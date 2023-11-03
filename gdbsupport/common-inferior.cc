/* Functions to deal with the inferior being executed on GDB or
   GDBserver.

   Copyright (C) 2019-2023 Free Software Foundation, Inc.

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

#include "gdbsupport/common-defs.h"
#include "gdbsupport/common-inferior.h"

/* See common-inferior.h.  */

bool startup_with_shell = true;

/* ... */
static std::string
escape_characters (const char *arg, const char *special)
{
  std::string result;

#ifdef __MINGW32__
  static const char quote = '"';
#else
  static const char quote = '\'';
#endif

  /* Need to handle empty arguments specially.  */
  if (arg[0] == '\0')
    result = result + quote + quote;
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

/* See common-inferior.h.  */

std::string
escape_white_space (const char * arg)
{
#ifdef __MINGW32__
  /* This holds all the characters considered special to the
     Windows shells.  */

  // TODO: Need to figure out what goes here!!
  static const char special[] = "\"!&*|[]{}<>?`~^=;, \t\n";
#else
  /* ...  */
  static const char special[] = " \t\n";
#endif

  return escape_characters (arg, special);
}

/* See common-inferior.h.  */

std::string
escape_some_stuff (const char * arg)
{
#ifdef __MINGW32__
  /* This holds all the characters considered special to the
     Windows shells.  */

  // TODO: Need to figure out what goes here!!
  static const char special[] = "\"!&*|[]{}<>?`~^=;, \t\n";
#else
  /* ...  */
  const char *special;

  if (getenv ("APB_WITH_BS") != NULL)
    special = "\\\"' \t\n";
  else
    special = "\"' \t\n";
#endif

  return escape_characters (arg, special);
}


std::string
escape_nothing (const char *arg)
{
  return arg;
}

/* See common-inferior.h.  */

std::string
construct_inferior_arguments (gdb::array_view<char * const> argv,
			      escape_string_func_t escape_func)
{
  std::string result;

  for (int i = 0; i < argv.size (); ++i)
    {
      if (i > 0)
	result += " ";

      result += escape_func (argv[i]);
    }

  return result;
}
