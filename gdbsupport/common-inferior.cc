/* Functions to deal with the inferior being executed on GDB or
   GDBserver.

   Copyright (C) 2019-2024 Free Software Foundation, Inc.

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

#include "gdbsupport/common-inferior.h"

/* See common-inferior.h.  */

bool startup_with_shell = true;

/* See common-inferior.h.  */

std::string
construct_inferior_arguments (gdb::array_view<char * const> argv,
			      escape_args_func escape_func)
{
  std::string result;

  for (const char *a : argv)
    {
      if (!result.empty ())
	result += " ";

      result += escape_func (a);
    }

  return result;
}

/* Escape characters in ARG and return an updated string.  The string
   SPECIAL contains the set of characters that must be escaped.  SPECIAL
   must not be nullptr, and it is assumed that SPECIAL contains the newline
   '\n' character.  It is assumed that ARG is not nullptr, but ARG can
   be the empty string.  */

static std::string
escape_characters (const char *arg, const char *special)
{
  gdb_assert (special != nullptr);
  gdb_assert (arg != nullptr);

  std::string result;

#ifdef __MINGW32__
  static const char quote = '"';
#else
  static const char quote = '\'';
#endif

  /* Need to handle empty arguments specially.  */
  if (arg[0] == '\0')
    {
      result += quote;
      result += quote;
    }
  /* The special character handling code here assumes that if SPECIAL is
     not nullptr, then SPECIAL will contain '\n'.  This is true for all our
     current usages, but if this ever changes in the future the following
     might need reworking.  */
  else
    {
#ifdef __MINGW32__
      bool quoted = false;

      if (strpbrk (argv[i], special))
	{
	  quoted = true;
	  result += quote;
	}
#endif
      for (const char *cp = arg; *cp; ++cp)
	{
	  if (*cp == '\n')
	    {
	      /* A newline cannot be quoted with a backslash (it just
		 disappears), only by putting it inside quotes.  */
	      result += quote;
	      result += '\n';
	      result += quote;
	    }
	  else
	    {
#ifdef __MINGW32__
	      if (*cp == quote)
#else
	      if (strchr (special, *cp) != nullptr)
#endif
		result += '\\';
	      result += *cp;
	    }
	}
#ifdef __MINGW32__
      if (quoted)
	result += quote;
#endif
    }

  return result;
}

/* See common-inferior.h.  */

std::string
escape_shell_characters (const char *arg)
{
#ifdef __MINGW32__
  /* This holds all the characters considered special to the
     Windows shells.  */
  static const char special[] = "\"!&*|[]{}<>?`~^=;, \t\n";
#else
  /* This holds all the characters considered special to the
     typical Unix shells.  We include `^' because the SunOS
     /bin/sh treats it as a synonym for `|'.  */
  static const char special[] = "\"!#$&*()\\|[]{}<>?'`~^; \t\n";
#endif

  return escape_characters (arg, special);
}
