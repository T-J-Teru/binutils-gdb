/* Copyright (C) 2023 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "gdbsupport/gdb_assert.h"
#include "gdbsupport/selftest.h"
#include "test-target.h"
#include "scoped-mock-context.h"
#include "break-cond-parse.h"
#include "tid-parse.h"
#include "ada-lang.h"

/* When parsing tokens from a string, which direction are we parsing?

   Given the following string and pointer 'ptr':

	ABC DEF GHI JKL
	       ^
	       ptr

   Parsing 'forward' will return the token 'GHI' and update 'ptr' to point
   between GHI and JKL.  Parsing 'backward' will return the token 'DEF' and
   update 'ptr' to point between ABC and DEF.
*/

enum class parse_direction
{
  /* Parse the next token forwards.  */
  forward,

  /* Parse the previous token backwards.  */
  backward
};

/* The tokens we parse are just views into the string from which the tokens
   are being parsed.  */

using token = std::string_view;

/* Return true if STR starts with PREFIX, otherwise return false.  */

static bool
startswith (const char *str, const token &prefix)
{
  return strncmp (str, prefix.data (), prefix.length ()) == 0;
}

/* Find the next token in DIRECTION from *CURR.  */

static token
find_next_token (const char **curr, parse_direction direction)
{
  const char *tok_start, *tok_end;

  gdb_assert (**curr != '\0');

  if (direction == parse_direction::forward)
    {
      *curr = skip_spaces (*curr);
      tok_start = *curr;
      *curr = skip_to_space (*curr);
      tok_end = *curr - 1;
    }
  else
    {
      gdb_assert (direction == parse_direction::backward);

      while (isspace (**curr))
	--(*curr);

      tok_end = *curr;

      while (!isspace (**curr))
	--(*curr);

      tok_start = (*curr) + 1;
    }

  return token (tok_start, tok_end - tok_start + 1);
}

/* See break-cond-parse.h.  */

void
create_breakpoint_parse_arg_string
  (const char *tok, gdb::unique_xmalloc_ptr<char> *cond_string,
   int *thread, int *inferior, int *task,
   gdb::unique_xmalloc_ptr<char> *rest, bool *force)
{
  /* Set up the defaults.  */
  cond_string->reset ();
  rest->reset ();
  *thread = -1;
  *inferior = -1;
  *task = -1;
  *force = false;

  if (tok == nullptr)
    return;

  /* The "-force-condition" flag gets some special treatment.  If this
     token is immediately after the condition string then we treat this as
     part of the condition string rather than a separate token.  This is
     just a quirk of how this token used to be parsed, and has been
     retained for backwards compatibility.  Maybe this should be updated
     in the future.  */
  std::optional<token> force_condition_token;

  const char *cond_start = nullptr;
  const char *cond_end = nullptr;
  parse_direction direction = parse_direction::forward;
  while (*tok != '\0')
    {
      /* Find the next token.  If moving backward and this token starts at
	 the same location as the condition then we must have found the
	 other end of the condition string -- we're done.  */
      token t = find_next_token (&tok, direction);
      if (direction == parse_direction::backward && t.data () <= cond_start)
	{
	  cond_end = &t.back ();
	  break;
	}

      /* We only have a single flag option to check for.  All the other
	 options take a value so require an additional token to be found.
	 Additionally, we require that this flag be at least '-f', we
	 don't allow it to be abbreviated to '-'.  */
      if (t.length () > 1 && startswith ("-force-condition", t))
	{
	  force_condition_token.emplace (t);
	  continue;
	}

      /* Maybe the first token was the last token in the string.  If this
	 is the case then we definitely can't try to extract a value
	 token.  This also means that the token T is meaningless.  Reset
	 TOK to point at the start of the unknown content and break out of
	 the loop.  We'll record the unknown part of the string outside of
	 the scanning loop (below).  */
      if (direction == parse_direction::forward && *tok == '\0')
	{
	  tok = t.data ();
	  break;
	}

      /* As before, find the next token and, if we are scanning backwards,
	 check that we have not reached the start of the condition string.  */
      token v = find_next_token (&tok, direction);
      if (direction == parse_direction::backward && v.data () <= cond_start)
	{
	  /* Use token T here as that must also be part of the condition
	     string.  */
	  cond_end = &t.back ();
	  break;
	}

      /* When moving backward we will first parse the value token then the
	 keyword token, so swap them now.  */
      if (direction == parse_direction::backward)
	std::swap (t, v);

      /* Check for valid option in token T.  If we find a valid option then
	 parse the value from the token V.  Except for 'if', that's handled
	 differently.

	 For the 'if' token we need to capture the entire condition
	 string, so record the start of the condition string and then
	 start scanning backwards looking for the end of the condition
	 string.

	 The order of these checks is important, at least the check for
	 'thread' must occur before the check for 'task'.  We accept
	 abbreviations of these token names, and 't' should resolve to
	 'thread', which will only happen if we check 'thread' first.  */
      if (direction == parse_direction::forward && startswith ("if", t))
	{
	  cond_start = v.data ();
	  tok = tok + strlen (tok);
	  gdb_assert (*tok == '\0');
	  --tok;
	  direction = parse_direction::backward;
	  continue;
	}
      else if (startswith ("thread", t))
	{
	  if (*thread != -1)
	    error(_("You can specify only one thread."));

	  if (*task != -1 || *inferior != -1)
	    error (_("You can specify only one of thread, inferior, or task."));

	  const char *tmptok;
	  thread_info *thr = parse_thread_id (v.data (), &tmptok);
	  const char *expected_end = skip_spaces (v.data () + v.length ());
	  if (tmptok != expected_end)
	    error (_("Junk after thread keyword."));
	  *thread = thr->global_num;
	}
      else if (startswith ("inferior", t))
	{
	  if (*inferior != -1)
	    error(_("You can specify only one inferior."));

	  if (*task != -1 || *thread != -1)
	    error (_("You can specify only one of thread, inferior, or task."));

	  char *tmptok;
	  *inferior = strtol (v.data (), &tmptok, 0);
	  const char *expected_end = v.data () + v.length ();
	  if (tmptok != expected_end)
	    error (_("Junk after inferior keyword."));
	}
      else if (startswith ("task", t))
	{
	  if (*task != -1)
	    error(_("You can specify only one task."));

	  if (*thread != -1 || *inferior != -1)
	    error (_("You can specify only one of thread, inferior, or task."));

	  char *tmptok;
	  *task = strtol (v.data (), &tmptok, 0);
	  const char *expected_end = v.data () + v.length ();
	  if (tmptok != expected_end)
	    error (_("Junk after task keyword."));
	  if (!valid_task_id (*task))
	    error (_("Unknown task %d."), *task);
	}
      else
	{
	  /* An unknown token.  If we are scanning forward then reset TOK
	     to point at the start of the unknown content, we record this
	     outside of the scanning loop (below).

	     If we are scanning backward then unknown content is assumed to
	     be the other end of the condition string, obviously, this is
	     just a heuristic, we could be looking at a mistyped command
	     line, but this will be spotted when the condition is
	     eventually evaluated.

	     Either way, no more scanning is required after this.  */
	  if (direction == parse_direction::forward)
	    tok = t.data ();
	  else
	    {
	      gdb_assert (direction == parse_direction::backward);
	      cond_end = &v.back ();
	    }
	  break;
	}
    }

  if (cond_start != nullptr)
    {
      /* If we found the start of a condition string then we should have
	 switched to backward scan mode, and found the end of the condition
	 string.  Capture the whole condition string into COND_STRING
	 now.  */
      gdb_assert (direction == parse_direction::backward);
      gdb_assert (cond_end != nullptr);

      /* If the first token after the condition string is the
	 "-force-condition" token, then we merge the "-force-condition"
	 token with the condition string and forget ever seeing the
	 "-force-condition".  */
      if (force_condition_token.has_value ())
	{
	  const char *next_token_start = skip_spaces (cond_end + 1);

	  if (next_token_start == force_condition_token->data ())
	    {
	      cond_end = force_condition_token->end ();
	      force_condition_token.reset ();
	    }
	}

      /* The '+ 1' here is because COND_END points to the last character of
	 the condition string rather than the null-character at the end of
	 the condition string, and we need the string length here.  */
      cond_string->reset (savestring (cond_start, cond_end - cond_start + 1));
    }
  else if (*tok != '\0')
    {
      /* If we didn't have a condition start pointer then we should still
	 be in forward scanning mode.  If we didn't reach the end of the
	 input string (TOK is not at the null character) then the rest of
	 the input string is garbage that we didn't understand.

	 Record the unknown content into REST.  The caller of this function
	 will report this as an error later on.  We could report the error
	 here, but we prefer to allow the caller to run other checks, and
	 prioritise other errors before reporting this problem.  */
      gdb_assert (direction == parse_direction::forward);
      gdb_assert (cond_end == nullptr);
      rest->reset (savestring (tok, strlen (tok)));
    }

  /* If we saw "-force-condition" then set the *FORCE flag.  Depending on
     which path we took above we might have chosen to forget having seen
     the "-force-condition" token.  */
  if (force_condition_token.has_value ())
    *force = true;
}

#if GDB_SELF_TEST

namespace selftests {

/* Run a single test of the create_breakpoint_parse_arg_string function.
   INPUT is passed to create_breakpoint_parse_arg_string while all other
   arguments are the expected output from
   create_breakpoint_parse_arg_string.  */

static void
test (const char *input, const char *condition, int thread = -1,
      int inferior = -1, int task = -1, bool force = false,
      const char *rest = nullptr)
{
  gdb::unique_xmalloc_ptr<char> extracted_condition;
  gdb::unique_xmalloc_ptr<char> extracted_rest;
  int extracted_thread, extracted_inferior, extracted_task;
  bool extracted_force_condition;
  std::string exception_msg;

  try
    {
      create_breakpoint_parse_arg_string (input, &extracted_condition,
					  &extracted_thread,
					  &extracted_inferior,
					  &extracted_task, &extracted_rest,
					  &extracted_force_condition);
    }
  catch (const gdb_exception_error &ex)
    {
      string_file buf;

      exception_print (&buf, ex);
      exception_msg = buf.release ();
    }

  if ((condition == nullptr) != (extracted_condition.get () == nullptr)
      || (condition != nullptr
	  && strcmp (condition, extracted_condition.get ()) != 0)
      || (rest == nullptr) != (extracted_rest.get () == nullptr)
      || (rest != nullptr && strcmp (rest, extracted_rest.get ()) != 0)
      || thread != extracted_thread
      || inferior != extracted_inferior
      || task != extracted_task
      || force != extracted_force_condition
      || !exception_msg.empty ())
    {
      if (run_verbose ())
	{
	  debug_printf ("input: '%s'\n", input);
	  debug_printf ("condition: '%s'\n", extracted_condition.get ());
	  debug_printf ("rest: '%s'\n", extracted_rest.get ());
	  debug_printf ("thread: %d\n", extracted_thread);
	  debug_printf ("inferior: %d\n", extracted_inferior);
	  debug_printf ("task: %d\n", extracted_task);
	  debug_printf ("forced: %s\n",
			extracted_force_condition ? "true" : "false");
	  debug_printf ("exception: '%s'\n", exception_msg.c_str ());
	}

      /* Report the failure.  */
      SELF_CHECK (false);
    }
}

/* Test the create_breakpoint_parse_arg_string function.  Just wraps
   multiple calls to the test function above.  */

static void
create_breakpoint_parse_arg_string_tests (void)
{
  gdbarch *arch = current_inferior ()->arch ();
  scoped_restore_current_pspace_and_thread restore;
  scoped_mock_context<test_target_ops> mock_target (arch);

  int global_thread_num = mock_target.mock_thread.global_num;

  test ("  if blah  ", "blah");
  test (" if blah thread 1", "blah", global_thread_num);
  test (" if blah inferior 1", "blah", -1, 1);
  test (" if blah thread 1  ", "blah", global_thread_num);
  test ("thread 1 woof", nullptr, global_thread_num, -1, -1, false, "woof");
  test ("thread 1 X", nullptr, global_thread_num, -1, -1, false, "X");
  test (" if blah thread 1 -force-condition", "blah", global_thread_num,
	-1, -1, true);
  test (" -force-condition if blah thread 1", "blah", global_thread_num,
	-1, -1, true);
  test (" -force-condition if blah thread 1  ", "blah", global_thread_num,
	-1, -1, true);
  test ("thread 1 -force-condition if blah", "blah", global_thread_num,
	-1, -1, true);
  test ("if (A::outer::func ())", "(A::outer::func ())");
}

} // namespace selftests
#endif /* GDB_SELF_TEST */

void _initialize_break_cond_parse ();
void
_initialize_break_cond_parse ()
{
#if GDB_SELF_TEST
  selftests::register_test
    ("create_breakpoint_parse_arg_string",
     selftests::create_breakpoint_parse_arg_string_tests);
#endif
}
