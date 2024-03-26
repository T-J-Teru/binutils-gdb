/* Target description related code for GNU/Linux i386.

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
#include "arch/i386-linux-tdesc.h"
#include "arch/i386.h"
#include "arch/x86-linux-tdesc-features.h"

/* A cache of all possible i386 target descriptions.  */

static struct target_desc *i386_tdescs[x86_linux_i386_tdesc_count ()] = { };

/* See arch/i386-linux-tdesc.h.  */

const struct target_desc *
i386_linux_read_description (uint64_t xcr0)
{
  xcr0 &= x86_linux_i386_tdesc_feature_mask ();
  int idx = x86_linux_xcr0_to_tdesc_idx (xcr0);

  gdb_assert (idx >= 0 && idx < x86_linux_i386_tdesc_count ());

  target_desc **tdesc = &i386_tdescs[idx];

  if (*tdesc == nullptr)
    {
      *tdesc = i386_create_target_description (xcr0, true, false);

      x86_linux_post_init_tdesc (*tdesc, false);
    }

  return *tdesc;
}
