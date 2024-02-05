/* Self tests for the function extract_string_maybe_quoted.

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

namespace selftests {
namespace extract_string {

struct test_def
{
  test_def (const char *input,
	    const char *output,
	    size_t offset)
    : m_input (input),
      m_output (output),
      m_offset (offset)
  { /* Nothing.  */ }

  void run () const
  {
    const char *tmp = m_input;
    std::string test_out = extract_string_maybe_quoted (&tmp);

    if (run_verbose ())
      {
	debug_printf ("Input: %s\n", m_input);
	debug_printf ("Output [Got]: %s\n", test_out.c_str ());
	debug_printf ("Output [Exp]: %s\n", m_output);
	debug_printf ("Rest [Got]: %s\n", tmp);
	debug_printf ("Rest [Exp]: %s\n", (m_input + m_offset));
      }

    SELF_CHECK (test_out == m_output);
    SELF_CHECK (tmp == m_input + m_offset);
  }

private:
  const char *m_input;
  const char *m_output;
  size_t m_offset;
};

test_def tests[] = {
  { "abc def", "abc", 3 },
  { "'abc' def", "abc", 5 },
  { "\"abc\" def", "abc", 5 },
  { "ab\\ cd ef", "ab cd", 6 },
  { "\"abc\\\"def\" ghi", "abc\"def", 10 },
  { "\"'abc' 'def'\" ghi", "'abc' 'def'", 13 },
};

static void
run_tests ()
{
  for (const auto &test : tests)
    test.run ();
}

} /* namespace extract_string */
} /* namespace selftests */

void _initialize_extract_string_selftest ();
void
_initialize_extract_string_selftest ()
{
  selftests::register_test ("extract-string",
			    selftests::extract_string::run_tests);
}
