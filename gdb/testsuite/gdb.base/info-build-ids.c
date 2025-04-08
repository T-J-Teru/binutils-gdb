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

#ifdef NO_SHLIB
#include <stdlib.h>

static int
foo (int dump_core_p)
{
  if (dump_core_p)
    abort ();

  return 0;
}
#else
extern int foo (int);
#endif

#ifdef DUMP_CORE
static int dump_core_flag = 1;
#else
static int dump_core_flag = 0;
#endif

int
main (void)
{
  int result = foo (dump_core_flag);

  return result;
}
