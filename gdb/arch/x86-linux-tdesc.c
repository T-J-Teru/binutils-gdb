/* Target description related code for GNU/Linux x86 (i386 and x86-64).

   Copyright (C) 2024 Free Software Foundation, Inc.

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
#include "arch/x86-linux-tdesc.h"
#include "arch/x86-linux-tdesc-features.h"

/* See arch/x86-linux-tdesc.h.  */

int
x86_linux_xcr0_to_tdesc_idx (uint64_t xcr0)
{
  /* The following table shows which features are checked for when creating
     the target descriptions (see nat/x86-linux-tdesc.c), the feature order
     represents the bit order within the generated index number.

     i386  | x87 sse mpx avx avx512 pkru
     amd64 |         mpx avx avx512 pkru
     i32   |             avx avx512 pkru

     The features are ordered so that for each mode (i386, amd64, i32) the
     generated index will form a continuous range.  */

  int idx = 0;

  for (int i = 0; i < ARRAY_SIZE (x86_linux_all_tdesc_features); ++i)
    {
      if ((xcr0 & x86_linux_all_tdesc_features[i].feature) != 0)
	idx |= (1 << i);
    }

  return idx;
}
