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

struct token
{
  /* Create a new token.  START points to the first character of the new
     token, while END points at the last character of the new token.

     Neither START or END can be nullptr, and both START and END must point
     into the same C style string (i.e. there should be no null character
     between START and END).  END must be equal to, or greater than START,
     that is, it is not possible to create a zero length token.  */

  token (const char *start, const char *end)
    : m_start (start),
      m_end (end)
  {
    gdb_assert (m_end >= m_start);
    gdb_assert (m_start + strlen (m_start) > m_end);
  }

  /* Return a pointer to the first character of this token.  */
  const char *start () const
  {
    return m_start;
  }

  /* Return a pointer to the last character of this token.  */
  const char *end () const
  {
    return m_end;
  }

  /* Return the length of the token.  */
  size_t length () const
  {
    return m_end - m_start + 1;
  }

  /* Return true if this token matches STR.  If this token is shorter than
     STR then only a partial match is performed and true will be returned
     if the token length sub-string matches.  Otherwise, return false.  */
  bool matches (const char *str) const
  {
    return strncmp (m_start, str, length ()) == 0;
  }

private:
  /* The first character of this token.  */
  const char *m_start;

  /* The last character of this token.  */
  const char *m_end;
};

/* Find the next token in DIRECTION from *CURR.  */

static token
find_next_token (const char **curr, parse_direction direction)
{
  const char *tok_start, *tok_end;

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

  return token (tok_start, tok_end);
}

/* See break-cond-parse.h.  */

void
create_breakpoint_parse_arg_string
  (const char *tok, gdb::unique_xmalloc_ptr<char> *cond_string,
   int *thread, int *task, gdb::unique_xmalloc_ptr<char> *rest, bool *force)
{
  /* Set up the defaults.  */
  cond_string->reset ();
  rest->reset ();
  *thread = -1;
  *task = -1;
  *force = false;

  if (tok == nullptr)
    return;

  const char *cond_start = nullptr;
  const char *cond_end = nullptr;
  parse_direction direction = parse_direction::forward;
  while (*tok != '\0')
    {
      /* Find the next token.  If moving backward and this token starts at
	 the same location as the condition then we must have found the
	 other end of the condition string -- we're done.  */
      token t = find_next_token (&tok, direction);
      if (direction == parse_direction::backward && t.start () <= cond_start)
	{
	  cond_end = t.end () + 1;
	  break;
	}

      /* We only have a single flag option to check for.  All the other
	 options take a value so require an additional token to be found.  */
      if (t.length () > 0 && t.matches ("-force-condition"))
	{
	  *force = true;
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
	  tok = t.start ();
	  break;
	}

      /* As before, find the next token and, if we are scanning backwards,
	 check that we have not reached the start of the condition string.  */
      token v = find_next_token (&tok, direction);
      if (direction == parse_direction::backward && v.start () <= cond_start)
	{
	  /* Use token T here as that must also be part of the condition
	     string.  */
	  cond_end = t.end () + 1;
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
         string.  */
      if (direction == parse_direction::forward && t.length () > 0
	  && t.matches ("if"))
	{
	  cond_start = v.start ();
	  tok = tok + strlen (tok);
	  gdb_assert (*tok == '\0');
	  --tok;
	  direction = parse_direction::backward;
	  continue;
	}
      else if (t.length () > 0 && t.matches ("thread"))
	{
	  const char *tmptok;
	  struct thread_info *thr;

	  if (*thread != -1)
	    error(_("You can specify only one thread."));

	  if (*task != -1)
	    error (_("You can specify only one of thread or task."));

	  thr = parse_thread_id (v.start (), &tmptok);
	  const char *expected_end = skip_spaces (v.end () + 1);
	  if (tmptok != expected_end)
	    error (_("Junk after thread keyword."));
	  *thread = thr->global_num;
	}
      else if (t.length () > 0 && t.matches ("task"))
	{
	  char *tmptok;

	  if (*task != -1)
	    error(_("You can specify only one task."));

	  if (*thread != -1)
	    error (_("You can specify only one of thread or task."));

	  *task = strtol (v.start (), &tmptok, 0);
	  const char *expected_end = v.end () + 1;
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
	    tok = t.start ();
	  else
	    {
	      gdb_assert (direction == parse_direction::backward);
	      cond_end = v.end () + 1;
	    }
	  break;
	}
    }

  if (cond_start != nullptr)
    {
      /* If we found the start of a condition string then we should have
	 switched to backward scan mode, and found the end of the condition
	 string.  Capture the whole condition string into COND_STRING now.  */
      gdb_assert (direction == parse_direction::backward);
      gdb_assert (cond_end != nullptr);
      cond_string->reset (savestring (cond_start, cond_end - cond_start));
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
      rest->reset (savestring (tok, strlen (tok)));
    }
}

#if GDB_SELF_TEST

namespace selftests {

/* Run a single test of the create_breakpoint_parse_arg_string function.
   INPUT is passed to create_breakpoint_parse_arg_string while all other
   arguments are the expected output from
   create_breakpoint_parse_arg_string.  */

static void
test (const char *input, const char *condition, int thread = -1,
      int task = -1, bool force = false, const char *rest = nullptr)
{
  gdb::unique_xmalloc_ptr<char> extracted_condition;
  gdb::unique_xmalloc_ptr<char> extracted_rest;
  int extracted_thread, extracted_task;
  bool extracted_force_condition;
  std::string exception_msg;

  try
    {
      create_breakpoint_parse_arg_string (input, &extracted_condition,
					  &extracted_thread,
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
  gdbarch *arch = current_inferior ()->gdbarch;
  scoped_restore_current_pspace_and_thread restore;
  scoped_mock_context<test_target_ops> mock_target (arch);

  int global_thread_num = mock_target.mock_thread.global_num;

  test ("  if blah  ", "blah");
  test (" if blah thread 1", "blah", global_thread_num);
  test (" if blah thread 1  ", "blah", global_thread_num);
  test ("thread 1 woof", nullptr, global_thread_num, -1, false, "woof");
  test ("thread 1 X", nullptr, global_thread_num, -1, false, "X");
  test (" if blah thread 1 -force-condition", "blah", global_thread_num,
	-1, true);
  test (" -force-condition if blah thread 1", "blah", global_thread_num,
	-1, true);
  test (" -force-condition if blah thread 1  ", "blah", global_thread_num,
	-1, true);
  test ("thread 1 -force-condition if blah", "blah", global_thread_num,
	-1, true);
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
