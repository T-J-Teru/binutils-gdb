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
#include <stdint.h>

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
		"csrrci zero, vxrm, 3\n"
		"csrrci zero, vxsat, 1\n");
  asm volatile ("vsetvli %[new_vl], x0, e8, m1, tu, mu"
		: [new_vl] "=r"(vl)
		:
		:);
  asm volatile ("nop"); /* vu_reset_end */
}

void
do_workload ()
{
  unsigned long long app_vtype;
  unsigned app_vl;
  unsigned app_vlenb;
  asm volatile ("csrr %[vtype], vtype\n" : [vtype] "=r"(app_vtype) : :);
  asm volatile ("csrr %[vl], vl\n"
		: [vl] "=r"(app_vl) /* vect_test_vtype_read */
		:
		:);
  asm volatile ("csrr %[vlenb], vlenb\n" : [vlenb] "=r"(app_vlenb) : :);
  asm volatile ("vxor.vv v24, v16, v8\n" : : :);
  asm volatile ("nop"); /* workload_end */
}

int
main ()
{
  unsigned vlenb_value = do_vlenb_read ();
  (void) vlenb_value;
  reset_vu ();
  /* vect_test_start */
  for (int i = 0; i < 777; ++i)
    do_workload ();
  return 0; /* vect_test_end */
}
