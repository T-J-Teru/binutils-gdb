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

char *STORAGE;

void
zero_out_vu ()
{
  unsigned vl;
  asm volatile ("vsetvli %[new_vl], x0, e8, m8, tu, mu"
		: [new_vl] "=r"(vl)
		:
		:);
  asm volatile ("vxor.vv v0, v0, v0");
  asm volatile ("vxor.vv v8, v8, v8");
  asm volatile ("vxor.vv v16, v16, v16");
  asm volatile ("vxor.vv v24, v24, v24");
}

void
do_wide_operations ()
{
  unsigned vl;
  asm volatile ("vsetvli %[new_vl], x0, e8, m8, tu, mu"
		: [new_vl] "=r"(vl)
		:
		:);
  asm volatile ("vadd.vi v0, v0, 0x1");  /* vect_wide_op_start */
  asm volatile ("vadd.vi v24, v0, 0x2"); /* vect_op_v0_add1 */
  asm volatile ("vadd.vi v16, v8, 0x2"); /* vect_op_v24_v0_add2 */
  asm volatile ("vadd.vi v10, v9, 0x3"); /* vect_op_v16_v8_add2 */
  asm volatile ("nop");                  /* vect_wide_op_end */
}

void
do_controlled_vadd ()
{
  unsigned vl;
  asm volatile ("vsetvli %[new_vl], x0, e8, m1, tu, mu"
		: [new_vl] "=r"(vl)
		:
		:);
  asm volatile ("vadd.vv v2, v1, v0"); /* vect_control_vadd_start */
  asm volatile ("nop");                /* controlled_vadd_done */
}

int
main ()
{
  unsigned vlenb_value = do_vlenb_read ();
  STORAGE = (char *) calloc (1, vlenb_value * CHAR_BIT);

  zero_out_vu ();
  /* vect_test_start */
  do_controlled_vadd ();
  zero_out_vu ();
  do_wide_operations ();
  return 0; /* vect_test_end */
}
