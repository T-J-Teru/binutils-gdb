/* GDB-friendly replacement for <assert.h>.
   Copyright (C) 2000-2021 Free Software Foundation, Inc.

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

#ifndef COMMON_GDB_ASSERT_H
#define COMMON_GDB_ASSERT_H

#include "errors.h"

/* A static assertion.  This will cause a compile-time error if EXPR,
   which must be a compile-time constant, is false.  */

#define gdb_static_assert(expr) static_assert (expr, "")

/* PRAGMATICS: "gdb_assert.h":gdb_assert() is a lower case (rather
   than upper case) macro since that provides the closest fit to the
   existing lower case macro <assert.h>:assert() that it is
   replacing.  */

#define gdb_assert(expr)                                                      \
  ((void) ((expr) ? 0 :                                                       \
	   (gdb_assert_fail (#expr, __FILE__, __LINE__, FUNCTION_NAME), 0)))

/* This prints an "Assertion failed" message, asking the user if they
   want to continue, dump core, or just exit.  */
#if defined (FUNCTION_NAME)
#define gdb_assert_fail(assertion, file, line, function)                      \
  internal_error (file, line, _("%s: Assertion `%s' failed."),                \
		  function, assertion)
#else
#define gdb_assert_fail(assertion, file, line, function)                      \
  internal_error (file, line, _("Assertion `%s' failed."),                    \
		  assertion)
#endif

/* The canonical form of gdb_assert (0).
   MESSAGE is a string to include in the error message.  */

#if defined (FUNCTION_NAME)
#define gdb_assert_not_reached(message) \
  internal_error (__FILE__, __LINE__, "%s: %s", FUNCTION_NAME, _(message))
#else
#define gdb_assert_not_reached(message) \
  internal_error (__FILE__, __LINE__, _(message))
#endif

/* If a pointer argument to a function is marked as nonnull (using the
   nonnull attribute) then GCC will warn about any attempts to compare the
   pointer to nullptr.  Even if you can silence the warning GCC will
   likely optimize out the check (and any associated code block)
   completely.

   Normally this would be what you want, but, marking an argument nonnull
   doesn't guarantee that nullptr can't be passed.  So it's not
   unreasonable that we might want to add an assert that the argument is
   not nullptr.

   This function should be used for this purpose, like:

   gdb_assert (nonnull_arg_is_not_nullptr (arg_name));  */

template<typename T>
bool _GL_ATTRIBUTE_NOINLINE
nonnull_arg_is_not_nullptr (const T *ptr)
{
  return ptr != nullptr;
}

#endif /* COMMON_GDB_ASSERT_H */
