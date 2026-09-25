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

#include <stdlib.h>
#include <limits.h>

asm (".option arch, +v\n");

unsigned
do_vlenb_read ()
{
  unsigned vlenb;
  asm volatile ("csrr %[vlenb], vlenb" : [vlenb] "=r"(vlenb) : :);
  return vlenb;
}

unsigned
do_vsetvli ()
{
  unsigned vl;
  asm volatile ("vsetvli %[new_vl], x0, e8, m8, tu, mu"
		: [new_vl] "=r"(vl)
		:
		:);
  return vl;
}

char *STORAGE;

void
do_vector_stuff ()
{
  unsigned vlenb_value = do_vlenb_read ();
  STORAGE = (char *) calloc (1, vlenb_value * CHAR_BIT);
  do_vsetvli ();
  asm volatile ("vxor.vv v0, v0, v0");
  asm volatile ("vxor.vv v8, v8, v8");
  asm volatile ("vxor.vv v16, v16, v16");
  asm volatile ("vxor.vv v24, v24, v24");
  asm volatile ("vsetvli t0, x0, e8, m1, tu, mu" : : : "t0");
  asm volatile ("vadd.vi v1, v1, 0x1");
  asm volatile ("vadd.vi v2, v1, 0x2");
  asm volatile ("vs1r.v v1, (%0)"
		:
		: "r"(STORAGE)
		: "memory"); /* pre_vect_mem */
  asm volatile ("vl1re8.v v2, (%0)" : : "r"(STORAGE) : "memory");
}

int
main ()
{
  do_vector_stuff ();
  return 0; /* post_vector_op */
}
