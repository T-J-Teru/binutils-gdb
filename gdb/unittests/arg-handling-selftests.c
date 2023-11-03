/* Self tests for GDB's argument splitting and merging.

   Copyright (C) 2023 Free Software Foundation, Inc.

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
#include "gdbsupport/selftest.h"
#include "gdbsupport/buildargv.h"
#include "gdbsupport/common-inferior.h"
#include "gdbsupport/arg-passing.h"

namespace selftests {
namespace arg_handling_tests {

struct split_args_by_word
{
  split_args_by_word (const std::string &args)
  {
    const char *input = args.c_str ();
    bool squote = false, dquote = false, bsquote = false;
    std::string arg;

    input = skip_spaces (input);

    do
      {
	while (*input != '\0')
	  {
	    /* TODO: Use 'ISSPACE'.  */
	    if (isspace (*input) && !squote && !dquote && !bsquote)
	      {
		break;
	      }
	    else
	      {
		if (bsquote)
		  {
		    /* This is not used right now.  */
		    arg += *input;
		    bsquote = false;
		  }
		else if (*input == '\\' && !squote)
		  {
		    if ((input[1] == '"')
			|| (input[1] == '\'')
			|| (input[1] == '\\')
			|| (isspace (input[1])))
		      {
			arg += input[1];
			++input;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		else if (squote)
		  {
		    if (*input == '\'')
		      {
			squote = false;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		else if (dquote)
		  {
		    if (*input == '"')
		      {
			dquote = false;
		      }
		    else if (false && *input == '\'')
		      {
			arg += '\\';
			arg += *input;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		else
		  {
		    if (*input == '\'')
		      {
			squote = true;
		      }
		    else if (*input == '"')
		      {
			dquote = true;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		++input;
	      }
	  }

	m_argv.push_back (std::move (arg));
	arg.clear ();

	input = skip_spaces (input);
      }
    while (*input != '\0');
  }

private:

  std::vector<std::string> m_argv;

public:

  using iterator = decltype (m_argv)::iterator;

  iterator begin ()
  {
    return m_argv.begin ();
  }

  iterator end ()
  {
    return m_argv.end ();
  }
};

static
std::pair<std::vector<std::string>, std::vector<std::string>>
get_native_args (std::string args)
{
  std::vector<std::string> results_for_shell;
  std::vector<std::string> results_for_direct;

  {
    split_args_by_word argv (args);
    for (const auto &a : argv)
      results_for_shell.push_back (a);
  }

  {
    gdb_argv argv (args.c_str ());
    for (int i = 0; argv[i] != nullptr; i++)
      results_for_direct.push_back (std::string (argv[i]));
  }

  return { results_for_shell, results_for_direct };
}

static
std::pair<std::vector<std::string>, std::vector<std::string>>
get_remote_args (std::string args)
{
  remote_arg_handler *handler = remote_arg_handler_factory::get ();

  if (run_verbose ())
    debug_printf ("Remote args handler: %s\n", handler->name ());

  std::vector<std::string> split_args = handler->split (args);

  if (run_verbose ())
    {
      debug_printf ("Split remote  args:\n");
      for (const auto &a : split_args)
	debug_printf ("  (%s)\n", a.c_str ());
    }

  std::string merged_args = handler->join (split_args);

  if (run_verbose ())
    debug_printf ("Merged remote args (%s)\n", merged_args.c_str ());

  return get_native_args (merged_args);
}

struct arg_test_desc
{
  std::string input;
  std::vector<std::string> output;
};

arg_test_desc desc[] = {
  { "abc", { "abc" } },
  { "'\"' '\\\"'", { "\"", "\\\"" } },
  { "\"first arg\" \"\" \"third-arg\" \"'\" \"\\\"\" \"\\\\\\\"\" \" \" \"\"",
    { "first arg", "", "third-arg", "'", "\"", "\\\""," ", "" } },
  { "abc* abc\\* abc\\\\*", { "abc*", "abc*", "abc\\*" } },
  { "1 '\n' 3", { "1", "\n", "3" } },
};

#if 0
static int
check_output (const arg_test_desc &d,
	      const std::vector<std::string> &output,
	      const char *type_name,
	      bool more_tests)
{
  int failure_count = 0;

  if (run_verbose ())
    {
      if (output != d.output)
	{
	  debug_printf (" ... FAIL %s, output:\n", type_name);
	  int idx = 0;
	  for (const auto & a : output)
	    {
	      const std::string &expected = d.output[idx];
	      idx++;
	      debug_printf ("%c Got (%s), Expected (%s)\n",
			    (a == expected ? ' ' : '!'),
			    a.c_str (), expected.c_str ());
	    }
	  if (more_tests)
	    debug_printf ("Input (%s) ", d.input.c_str ());
	}
      else
	debug_printf (" ... PASS %s", type_name);
    }

  if (output != d.output)
    ++failure_count;

  return failure_count;
}
#endif

static void
dump_args (const char *left_name,
	   std::vector<std::string> left,
	   const char *right_name,
	   std::vector<std::string> right,
	   bool flag_differences_p)
{
  size_t len = std::max (left.size (), right.size ());
  for (size_t i = 0; i < len; ++i)
    {
      std::string r_arg = (i < left.size ()) ? left[i] : "*missing*";
      std::string n_arg = (i < right.size ()) ? right[i] : "*missing*";

      char marker = (flag_differences_p && r_arg != n_arg) ? '!' : ' ';

      debug_printf ("%c %s (%s), %s (%s)\n", marker,
		    left_name, r_arg.c_str (),
		    right_name, n_arg.c_str ());
    }
}

static void
self_test ()
{
  int failure_count = 0;
  for (const auto &d : desc)
    {
      std::vector<std::string> output;

      if (run_verbose ())
	{
	  debug_printf ("--------------------\n");
	  debug_printf ("Input (%s)\n", d.input.c_str ());
	}

      auto n = get_native_args (d.input);

      auto r = get_remote_args (d.input);

      if (run_verbose ())
	{
	  debug_printf ("Results, 'for shell' vs 'for direct':\n");
	  dump_args ("shell", r.first, "direct", r.second, false);
	}

      if (r.first != n.first
	  || r.second != n.second)
	{
	  ++failure_count;
	  if (run_verbose ())
	    {
	      if (r.first != n.first)
		{
		  debug_printf ("FAIL: difference in 'for shell' arguments:\n");
		  dump_args ("remote", r.first, "native", n.first, true);
		}

	      if (r.second != n.second)
		{
		  debug_printf ("FAIL: difference in 'for direct' arguments:\n");
		  dump_args ("remote", r.second, "native", n.second, true);
		}
	    }
	}
    }

  SELF_CHECK (failure_count == 0);
}

} /* namespace arg_handling_tests */
} /* namespace selftests */

void _initialize_arg_handling_selftests ();

void
_initialize_arg_handling_selftests ()
{
  selftests::register_test ("arg-handling",
			    selftests::arg_handling_tests::self_test);
}
