/* This file is part of GDB, the GNU debugger.

   Copyright 2026 Free Software Foundation, Inc.

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

asm (".option arch, +v\n");

#include <stdlib.h>
#include <limits.h>

unsigned
do_vlenb_read ()
{
  unsigned vlenb;
  asm volatile ("csrr %[vlenb], vlenb" : [vlenb] "=r"(vlenb) : :);
  return vlenb;
}

void
reset_vu ()
{
  unsigned vl;
  asm volatile ("vsetvli %[new_vl], x0, e8, m8, ta, ma"
		: [new_vl] "=r"(vl)
		:
		:);
  asm volatile ("vxor.vv v0, v0, v0\n"
		"vxor.vv v8, v8, v8\n"
		"vxor.vv v16, v16, v16\n"
		"vxor.vv v24, v24, v24\n"
		"vadd.vi v0, v0, 15\n"
		"vadd.vi v8, v8, 15\n"
		"vadd.vi v16, v16, 15\n"
		"vadd.vi v24, v24, 15\n"
		"csrrsi zero, vxrm, 3\n"
		"csrrsi zero, vxsat, 1\n");
  asm volatile ("nop"); /* vu_reset_end */
}

int
main ()
{
  unsigned vlenb_value = do_vlenb_read ();
  (void) vlenb_value;
  reset_vu ();
  /* vect_test_start */
  for (int i = 0; i < 777; ++i)
    reset_vu ();
  return 0; /* vect_test_end */
}
