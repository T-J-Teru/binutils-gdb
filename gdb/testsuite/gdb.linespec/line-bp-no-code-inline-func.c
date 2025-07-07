/* This testcase is part of GDB, the GNU debugger.

   Copyright 2025 Free Software Foundation, Inc.

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

#include "attributes.h"

volatile int global_var = 3;

/* IMPORTANT: The blank line within the function is a _critical_ part of
   this test.  Don't delete it, or add any content to the line.  */

static inline int __attribute__ ((always_inline))
inline_func (int i)
{

  return i + 1;	/* Break on previous line.  */
}

int __attribute__((noinline)) ATTRIBUTE_NOCLONE
func ()
{
  int v = inline_func (global_var);

  return v + global_var;
}

int
main ()
{
  int res = func ();

  return res;
}
