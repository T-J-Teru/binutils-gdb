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
  asm volatile ("vsetvli %[new_vl], x0, e8, m1, ta, ma"
		: [new_vl] "=r"(vl)
		:
		:);
  return vl;
}

#ifdef READ_VLENB_BEFORE_MAIN
unsigned VLENB = do_vlenb_read ();
#endif // READ_VLENB_BEFORE_MAIN

#ifdef SET_VSETVLI_BEFORE_MAIN
unsigned VL = do_vsetvli ();
#endif // SET_VSETVLI_BEFORE_MAIN

int STORAGE[64];

void
do_vector_stuff ()
{
  do_vsetvli ();
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
