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

#include <vector>

asm (".option arch, +v\n");

enum VLMUL
{
  LMUL1 = 0,
  LMUL2 = 1,
  LMUL4 = 2,
  LMUL8 = 3,
  LMUL_F8 = 5,
  LMUL_F4 = 6,
  LMUL_F2 = 7
};

enum SEW
{
  SEW8 = 0,
  SEW16 = 1,
  SEW32 = 2,
  SEW64 = 3,
};

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

unsigned
do_vsetv (unsigned vl, VLMUL vlmul, SEW vsew, unsigned vta, unsigned vma)
{
  unsigned vtype = (unsigned) vlmul | ((unsigned) vsew << 3) | (vta << 6)
		   | (vma << 7);
  asm volatile ("vsetvl %[new_vl], %[new_vl], %[vtype]"
		: [new_vl] "+r"(vl)
		: [vtype] "r"(vtype)
		:);
  return vl; /* vsetvl_done */
}

int STORAGE[64];

void
do_vector_stuff ()
{
  std::vector<VLMUL> vlmul = {
    VLMUL::LMUL1,   VLMUL::LMUL2,   VLMUL::LMUL4,   VLMUL::LMUL8,
    VLMUL::LMUL_F8, VLMUL::LMUL_F4, VLMUL::LMUL_F2,
  };
  std::vector<SEW> vsew = {
    SEW::SEW8,
    SEW::SEW16,
    SEW::SEW32,
    SEW::SEW64,
  };
  for (auto vlmul : vlmul)
    for (auto sew : vsew)
      for (int vta = 0; vta < 2; ++vta)
	for (int vma = 0; vma < 2; ++vma)
	  for (int vl = 1; vl < 3; ++vl)
	    do_vsetv (vl, vlmul, sew, vta, vma);

  asm volatile ("csrw vxrm, %[rnd_m]" : : [rnd_m] "i"(0) :);
  asm volatile ("csrw vxrm, %[rnd_m]" : : [rnd_m] "i"(1) :);  /* vxrm_0 */
  asm volatile ("csrw vxrm, %[rnd_m]" : : [rnd_m] "i"(2) :);  /* vxrm_1 */
  asm volatile ("csrw vxrm, %[rnd_m]" : : [rnd_m] "i"(3) :);  /* vxrm_2 */
  asm volatile ("csrw vxsat, %[vxsat]" : : [vxsat] "i"(1) :); /* vxrm_3 */
  asm volatile ("csrw vxrm, %[rnd_m]" : : [rnd_m] "i"(0) :); /* vxrm_0_again */
  unsigned vtype = -1;
  unsigned vl = -1;
  asm volatile ("vsetvl %[new_vl], %[new_vl], %[vtype]"
		: [new_vl] "+r"(vl), [vtype] "=r"(vtype)
		:
		:); /* vcsr_done */
}

int
main ()
{
  do_vsetvli ();
  do_vector_stuff (); /* rvv_initialized */
  return 0;           /* do_vector_stuff_done */
}
