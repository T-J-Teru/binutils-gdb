/* MIPS simulator target description.

   Copyright (C) 2026 Free Software Foundation, Inc.

   This file is part of the GNU simulators.

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

/* This must come before any other includes.  */
#include "defs.h"

#include "sim-main.h"
#include "sim-tdesc.h"

/* MIPS CPU registers (org.gnu.gdb.mips.cpu).
   Registers r0-r31, lo, hi, and pc.  */

static const struct sim_tdesc_reg mips_cpu_regs_32[] = {
  { "r0", 32, 0, NULL },
  { "r1", 32, 1, NULL },
  { "r2", 32, 2, NULL },
  { "r3", 32, 3, NULL },
  { "r4", 32, 4, NULL },
  { "r5", 32, 5, NULL },
  { "r6", 32, 6, NULL },
  { "r7", 32, 7, NULL },
  { "r8", 32, 8, NULL },
  { "r9", 32, 9, NULL },
  { "r10", 32, 10, NULL },
  { "r11", 32, 11, NULL },
  { "r12", 32, 12, NULL },
  { "r13", 32, 13, NULL },
  { "r14", 32, 14, NULL },
  { "r15", 32, 15, NULL },
  { "r16", 32, 16, NULL },
  { "r17", 32, 17, NULL },
  { "r18", 32, 18, NULL },
  { "r19", 32, 19, NULL },
  { "r20", 32, 20, NULL },
  { "r21", 32, 21, NULL },
  { "r22", 32, 22, NULL },
  { "r23", 32, 23, NULL },
  { "r24", 32, 24, NULL },
  { "r25", 32, 25, NULL },
  { "r26", 32, 26, NULL },
  { "r27", 32, 27, NULL },
  { "r28", 32, 28, NULL },
  { "r29", 32, 29, NULL },
  { "r30", 32, 30, NULL },
  { "r31", 32, 31, NULL },
  { "lo", 32, 33, NULL },
  { "hi", 32, 34, NULL },
  { "pc", 32, 37, "code_ptr" },
};

static const struct sim_tdesc_reg mips_cpu_regs_64[] = {
  { "r0", 64, 0, NULL },
  { "r1", 64, 1, NULL },
  { "r2", 64, 2, NULL },
  { "r3", 64, 3, NULL },
  { "r4", 64, 4, NULL },
  { "r5", 64, 5, NULL },
  { "r6", 64, 6, NULL },
  { "r7", 64, 7, NULL },
  { "r8", 64, 8, NULL },
  { "r9", 64, 9, NULL },
  { "r10", 64, 10, NULL },
  { "r11", 64, 11, NULL },
  { "r12", 64, 12, NULL },
  { "r13", 64, 13, NULL },
  { "r14", 64, 14, NULL },
  { "r15", 64, 15, NULL },
  { "r16", 64, 16, NULL },
  { "r17", 64, 17, NULL },
  { "r18", 64, 18, NULL },
  { "r19", 64, 19, NULL },
  { "r20", 64, 20, NULL },
  { "r21", 64, 21, NULL },
  { "r22", 64, 22, NULL },
  { "r23", 64, 23, NULL },
  { "r24", 64, 24, NULL },
  { "r25", 64, 25, NULL },
  { "r26", 64, 26, NULL },
  { "r27", 64, 27, NULL },
  { "r28", 64, 28, NULL },
  { "r29", 64, 29, NULL },
  { "r30", 64, 30, NULL },
  { "r31", 64, 31, NULL },
  { "lo", 64, 33, NULL },
  { "hi", 64, 34, NULL },
  { "pc", 64, 37, "code_ptr" },
};

/* MIPS CP0 registers (org.gnu.gdb.mips.cp0).
   Registers status, badvaddr, and cause.  */

static const struct sim_tdesc_reg mips_cp0_regs_32[] = {
  { "status", 32, 32, NULL },
  { "badvaddr", 32, 35, "data_ptr" },
  { "cause", 32, 36, NULL },
};

static const struct sim_tdesc_reg mips_cp0_regs_64[] = {
  { "status", 64, 32, NULL },
  { "badvaddr", 64, 35, "data_ptr" },
  { "cause", 64, 36, NULL },
};

/* MIPS FPU registers (org.gnu.gdb.mips.fpu).
   Registers f0-f31, fcsr, and fir.  */

static const struct sim_tdesc_reg mips_fpu_regs_32[] = {
  { "f0", 32, 38, "ieee_single" },
  { "f1", 32, 39, "ieee_single" },
  { "f2", 32, 40, "ieee_single" },
  { "f3", 32, 41, "ieee_single" },
  { "f4", 32, 42, "ieee_single" },
  { "f5", 32, 43, "ieee_single" },
  { "f6", 32, 44, "ieee_single" },
  { "f7", 32, 45, "ieee_single" },
  { "f8", 32, 46, "ieee_single" },
  { "f9", 32, 47, "ieee_single" },
  { "f10", 32, 48, "ieee_single" },
  { "f11", 32, 49, "ieee_single" },
  { "f12", 32, 50, "ieee_single" },
  { "f13", 32, 51, "ieee_single" },
  { "f14", 32, 52, "ieee_single" },
  { "f15", 32, 53, "ieee_single" },
  { "f16", 32, 54, "ieee_single" },
  { "f17", 32, 55, "ieee_single" },
  { "f18", 32, 56, "ieee_single" },
  { "f19", 32, 57, "ieee_single" },
  { "f20", 32, 58, "ieee_single" },
  { "f21", 32, 59, "ieee_single" },
  { "f22", 32, 60, "ieee_single" },
  { "f23", 32, 61, "ieee_single" },
  { "f24", 32, 62, "ieee_single" },
  { "f25", 32, 63, "ieee_single" },
  { "f26", 32, 64, "ieee_single" },
  { "f27", 32, 65, "ieee_single" },
  { "f28", 32, 66, "ieee_single" },
  { "f29", 32, 67, "ieee_single" },
  { "f30", 32, 68, "ieee_single" },
  { "f31", 32, 69, "ieee_single" },
  { "fcsr", 32, 70, NULL },
  { "fir", 32, 71, NULL },
};

static const struct sim_tdesc_reg mips_fpu_regs_64[] = {
  { "f0", 64, 38, "ieee_double" },
  { "f1", 64, 39, "ieee_double" },
  { "f2", 64, 40, "ieee_double" },
  { "f3", 64, 41, "ieee_double" },
  { "f4", 64, 42, "ieee_double" },
  { "f5", 64, 43, "ieee_double" },
  { "f6", 64, 44, "ieee_double" },
  { "f7", 64, 45, "ieee_double" },
  { "f8", 64, 46, "ieee_double" },
  { "f9", 64, 47, "ieee_double" },
  { "f10", 64, 48, "ieee_double" },
  { "f11", 64, 49, "ieee_double" },
  { "f12", 64, 50, "ieee_double" },
  { "f13", 64, 51, "ieee_double" },
  { "f14", 64, 52, "ieee_double" },
  { "f15", 64, 53, "ieee_double" },
  { "f16", 64, 54, "ieee_double" },
  { "f17", 64, 55, "ieee_double" },
  { "f18", 64, 56, "ieee_double" },
  { "f19", 64, 57, "ieee_double" },
  { "f20", 64, 58, "ieee_double" },
  { "f21", 64, 59, "ieee_double" },
  { "f22", 64, 60, "ieee_double" },
  { "f23", 64, 61, "ieee_double" },
  { "f24", 64, 62, "ieee_double" },
  { "f25", 64, 63, "ieee_double" },
  { "f26", 64, 64, "ieee_double" },
  { "f27", 64, 65, "ieee_double" },
  { "f28", 64, 66, "ieee_double" },
  { "f29", 64, 67, "ieee_double" },
  { "f30", 64, 68, "ieee_double" },
  { "f31", 64, 69, "ieee_double" },
  { "fcsr", 64, 70, NULL },
  { "fir", 64, 71, NULL },
};

/* Features for 32-bit MIPS.  */

static const struct sim_tdesc_feature mips_features_32[] = {
  { "org.gnu.gdb.mips.cpu",
    NULL, 0,
    mips_cpu_regs_32,
    sizeof (mips_cpu_regs_32) / sizeof (mips_cpu_regs_32[0]) },
  { "org.gnu.gdb.mips.cp0",
    NULL, 0,
    mips_cp0_regs_32,
    sizeof (mips_cp0_regs_32) / sizeof (mips_cp0_regs_32[0]) },
  { "org.gnu.gdb.mips.fpu",
    NULL, 0,
    mips_fpu_regs_32,
    sizeof (mips_fpu_regs_32) / sizeof (mips_fpu_regs_32[0]) },
};

/* Features for 64-bit MIPS.  */

static const struct sim_tdesc_feature mips_features_64[] = {
  { "org.gnu.gdb.mips.cpu",
    NULL, 0,
    mips_cpu_regs_64,
    sizeof (mips_cpu_regs_64) / sizeof (mips_cpu_regs_64[0]) },
  { "org.gnu.gdb.mips.cp0",
    NULL, 0,
    mips_cp0_regs_64,
    sizeof (mips_cp0_regs_64) / sizeof (mips_cp0_regs_64[0]) },
  { "org.gnu.gdb.mips.fpu",
    NULL, 0,
    mips_fpu_regs_64,
    sizeof (mips_fpu_regs_64) / sizeof (mips_fpu_regs_64[0]) },
};

/* Target descriptions for 32-bit and 64-bit MIPS.

   We intentionally do not specify an architecture here.  The MIPS
   family has many machine variants (mips:3000, mips:4000, mips:isa32,
   etc.) and specifying one could conflict with the architecture of
   the loaded binary.  By leaving the architecture unspecified, GDB
   will use the architecture from the executable.  */

static const struct sim_tdesc mips_tdesc_32 = {
  NULL,
  mips_features_32,
  sizeof (mips_features_32) / sizeof (mips_features_32[0]),
};

static const struct sim_tdesc mips_tdesc_64 = {
  NULL,
  mips_features_64,
  sizeof (mips_features_64) / sizeof (mips_features_64[0]),
};

/* Return the target description for the current MIPS configuration.  */

const struct sim_tdesc *
mips_tdesc_get (SIM_DESC sd)
{
  if (WITH_TARGET_WORD_BITSIZE == 32)
    return &mips_tdesc_32;
  else if (WITH_TARGET_WORD_BITSIZE == 64)
    return &mips_tdesc_64;
  else
    return NULL;
}
