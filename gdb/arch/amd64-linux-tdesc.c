/* Target description related code for GNU/Linux x86-64.

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
#include "arch/amd64-linux-tdesc.h"
#include "arch/amd64.h"
#include "arch/x86-linux-tdesc-features.h"

/* A cache of all possible amd64 target descriptions.  */

static target_desc *amd64_tdescs[x86_linux_amd64_tdesc_count ()] = { };

/* A cache of all possible x32 target descriptions.  */

static target_desc *x32_tdescs[x86_linux_x32_tdesc_count ()] = { };

/* See arch/amd64-linux-tdesc.h.  */

const struct target_desc *
amd64_linux_read_description (uint64_t xcr0, bool is_x32)
{
  if (is_x32)
    xcr0 &= x86_linux_x32_tdesc_feature_mask ();
  else
    xcr0 &= x86_linux_amd64_tdesc_feature_mask ();

  int idx = x86_linux_xcr0_to_tdesc_idx (xcr0);

  if (is_x32)
    gdb_assert (idx >= 0 && idx < x86_linux_x32_tdesc_count ());
  else
    gdb_assert (idx >= 0 && idx < x86_linux_amd64_tdesc_count ());

  target_desc **tdesc = nullptr;

  if (is_x32)
    tdesc = &x32_tdescs[idx];
  else
    tdesc = &amd64_tdescs[idx];

  if (*tdesc == nullptr)
    {
      *tdesc = amd64_create_target_description (xcr0, is_x32, true, true);

      x86_linux_post_init_tdesc (*tdesc, true);
    }
  return *tdesc;
}
