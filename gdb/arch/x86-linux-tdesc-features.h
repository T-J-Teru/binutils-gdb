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

#ifndef ARCH_X86_LINUX_TDESC_FEATURES_H
#define ARCH_X86_LINUX_TDESC_FEATURES_H

#include "gdbsupport/x86-xstate.h"
#include "gdbsupport/gdb_assert.h"

/* A structure used to describe a single cpu feature that might, or might
   not, be checked for when creating a target description for one of i386,
   amd64, or x32.  */

struct x86_tdesc_feature {
  /* The cpu feature mask.  This is a mask against an xcr0 value.  */
  uint64_t feature;

  /* Is this feature checked when creating an i386 target description.  */
  bool is_i386;

  /* Is this feature checked when creating an amd64 target description.  */
  bool is_amd64;

  /* Is this feature checked when creating an x32 target description.  */
  bool is_x32;
};

/* A constant table that describes all of the cpu features that are
   checked when building a target description for i386, amd64, or x32.  */

static constexpr x86_tdesc_feature x86_linux_all_tdesc_features[] = {
  /* Feature,           i386,	amd64,	x32.  */
  { X86_XSTATE_PKRU,	true,	true, 	true },
  { X86_XSTATE_AVX512,	true,	true, 	true },
  { X86_XSTATE_AVX,	true,	true, 	true },
  { X86_XSTATE_MPX,	true,	true, 	false },
  { X86_XSTATE_SSE,	true,	false, 	false },
  { X86_XSTATE_X87,	true,	false, 	false }
};

/* Return a compile time constant which is a mask of all the cpu features
   that are checked for when building an i386 target description.  */

static constexpr uint64_t
x86_linux_i386_tdesc_feature_mask ()
{
  uint64_t mask = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_i386)
      mask |= entry.feature;

  return mask;
}

/* Return a compile time constant which is a mask of all the cpu features
   that are checked for when building an amd64 target description.  */

static constexpr uint64_t
x86_linux_amd64_tdesc_feature_mask ()
{
  uint64_t mask = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_amd64)
      mask |= entry.feature;

  return mask;
}

/* Return a compile time constant which is a mask of all the cpu features
   that are checked for when building an x32 target description.  */

static constexpr uint64_t
x86_linux_x32_tdesc_feature_mask ()
{
  uint64_t mask = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_x32)
      mask |= entry.feature;

  return mask;
}

/* Return a compile time constant which is a count of the number of cpu
   features that are checked for when building an i386 target description.  */

static constexpr int
x86_linux_i386_tdesc_count ()
{
  uint64_t count = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_i386)
      ++count;

  gdb_assert (count > 0);

  return (1 << count);
}

/* Return a compile time constant which is a count of the number of cpu
   features that are checked for when building an amd64 target description.  */

static constexpr int
x86_linux_amd64_tdesc_count ()
{
  uint64_t count = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_amd64)
      ++count;

  gdb_assert (count > 0);

  return (1 << count);
}

/* Return a compile time constant which is a count of the number of cpu
   features that are checked for when building an x32 target description.  */

static constexpr int
x86_linux_x32_tdesc_count ()
{
  uint64_t count = 0;

  for (const auto &entry : x86_linux_all_tdesc_features)
    if (entry.is_x32)
      ++count;

  gdb_assert (count > 0);

  return (1 << count);
}

#endif /* ARCH_X86_LINUX_TDESC_FEATURES_H */
