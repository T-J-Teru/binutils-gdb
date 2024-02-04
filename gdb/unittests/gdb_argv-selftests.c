/* Self tests for the gdb_argv class from gdbsupport.

   Copyright (C) 2024 Free Software Foundation, Inc.

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

namespace selftests {
namespace gdb_argv_tests {

static extract_string_ctrl shell_extract_string_ctrl
  (nullptr, "", "\"$`\\", "\n", "", "\n");

struct test_def
{
  /* INPUT is the string to pass to the gdb_argv constructor.  OUTPUT is
     the vector of arguments that gdb_argv should split INPUT into.  */

  test_def (const char *input,
	    std::vector<const char *> output)
    : m_input (input),
      m_output (output)
  { /* Nothing.  */ }

  /* Run this test and check the results.  Throws an exception if the test
     fails.  */

  void run () const
  {
    gdb_argv argv (m_input);
    int count = argv.count ();

    if (run_verbose ())
      {
	debug_printf ("Input: %s\n", m_input);
	debug_printf ("Output [Count]: %d\n", count);
      }

    SELF_CHECK (count == m_output.size ());

    gdb::array_view<char *> view = argv.as_array_view ();
    SELF_CHECK (view.size () == count);

    const char *const *data = argv.get ();
    for (int i = 0; i < count; ++i)
      {
	const char *a = argv[i];
	SELF_CHECK (view[i] == a);
	SELF_CHECK (strcmp (a, m_output[i]) == 0);
	SELF_CHECK (a == *data);
	++data;
      }

    SELF_CHECK (*data == nullptr);
  }

private:
  /* What to pass to gdb_argv constructor.  */

  const char *m_input;

  /* The expected contents of gdb_argv after construction.  */

  std::vector<const char *> m_output;
};

/* The array of all tests.  */

test_def tests[] = {
  { "abc def", {"abc", "def"} },
  { "one two 3", {"one", "two", "3"} },
  { "one two\\ three", {"one", "two three"} },
  { "one\\ two three", {"one two", "three"} },
  { "'one two' three", {"one two", "three"} },
  { "one \"two three\"", {"one", "two three"} },
  { "'\"' \"''\"", {"\"", "''"} },
};

/* Test the creation of an empty gdb_argv object.  */

static void
empty_argv_tests ()
{
  {
    gdb_argv argv;

    SELF_CHECK (argv.get () == nullptr);
    SELF_CHECK (argv.count () == 0);

    gdb::array_view<char *> view = argv.as_array_view ();

    SELF_CHECK (view.data () == nullptr);
    SELF_CHECK (view.size () == 0);
  }
}

static void
run_tests ()
{
  empty_argv_tests ();

  for (const auto &test : tests)
    test.run ();
}

} /* namespace gdb_argv_tests */
} /* namespace selftests */

void _initialize_gdb_argv_selftest ();
void
_initialize_gdb_argv_selftest ()
{
  selftests::register_test ("gdb_argv",
			    selftests::gdb_argv_tests::run_tests);
}
