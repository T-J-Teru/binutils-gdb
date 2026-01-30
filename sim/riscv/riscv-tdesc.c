/* RISC-V simulator target description.

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
#include "sim/sim-riscv.h"
#include "riscv-sim.h"

/* Union type for 64-bit FPU registers that can hold float or double.
   Fields use -1 for start/end since they are not bitfields.  */

static const struct sim_tdesc_type_field riscv_double_fields[] = {
  { "float", "ieee_single", -1, -1 },
  { "double", "ieee_double", -1, -1 },
};

static const struct sim_tdesc_type riscv_fpu_types_64[] = {
  { "riscv_double", SIM_TDESC_TYPE_UNION, 0, NULL, 0,
    riscv_double_fields,
    sizeof (riscv_double_fields) / sizeof (riscv_double_fields[0]) },
};

/* RISC-V CPU registers (org.gnu.gdb.riscv.cpu).
   Registers x0-x31 and pc.  Uses ABI names.  */

static const struct sim_tdesc_reg riscv_cpu_regs_32[] = {
  { "zero", 32, SIM_RISCV_ZERO_REGNUM, NULL },
  { "ra", 32, SIM_RISCV_RA_REGNUM, NULL },
  { "sp", 32, SIM_RISCV_SP_REGNUM, NULL },
  { "gp", 32, SIM_RISCV_GP_REGNUM, NULL },
  { "tp", 32, SIM_RISCV_TP_REGNUM, NULL },
  { "t0", 32, SIM_RISCV_T0_REGNUM, NULL },
  { "t1", 32, SIM_RISCV_T1_REGNUM, NULL },
  { "t2", 32, SIM_RISCV_T2_REGNUM, NULL },
  { "fp", 32, SIM_RISCV_FP_REGNUM, NULL },
  { "s1", 32, SIM_RISCV_S1_REGNUM, NULL },
  { "a0", 32, SIM_RISCV_A0_REGNUM, NULL },
  { "a1", 32, SIM_RISCV_A1_REGNUM, NULL },
  { "a2", 32, SIM_RISCV_A2_REGNUM, NULL },
  { "a3", 32, SIM_RISCV_A3_REGNUM, NULL },
  { "a4", 32, SIM_RISCV_A4_REGNUM, NULL },
  { "a5", 32, SIM_RISCV_A5_REGNUM, NULL },
  { "a6", 32, SIM_RISCV_A6_REGNUM, NULL },
  { "a7", 32, SIM_RISCV_A7_REGNUM, NULL },
  { "s2", 32, SIM_RISCV_S2_REGNUM, NULL },
  { "s3", 32, SIM_RISCV_S3_REGNUM, NULL },
  { "s4", 32, SIM_RISCV_S4_REGNUM, NULL },
  { "s5", 32, SIM_RISCV_S5_REGNUM, NULL },
  { "s6", 32, SIM_RISCV_S6_REGNUM, NULL },
  { "s7", 32, SIM_RISCV_S7_REGNUM, NULL },
  { "s8", 32, SIM_RISCV_S8_REGNUM, NULL },
  { "s9", 32, SIM_RISCV_S9_REGNUM, NULL },
  { "s10", 32, SIM_RISCV_S10_REGNUM, NULL },
  { "s11", 32, SIM_RISCV_S11_REGNUM, NULL },
  { "t3", 32, SIM_RISCV_T3_REGNUM, NULL },
  { "t4", 32, SIM_RISCV_T4_REGNUM, NULL },
  { "t5", 32, SIM_RISCV_T5_REGNUM, NULL },
  { "t6", 32, SIM_RISCV_T6_REGNUM, NULL },
  { "pc", 32, SIM_RISCV_PC_REGNUM, "code_ptr" },
};

static const struct sim_tdesc_reg riscv_cpu_regs_64[] = {
  { "zero", 64, SIM_RISCV_ZERO_REGNUM, NULL },
  { "ra", 64, SIM_RISCV_RA_REGNUM, NULL },
  { "sp", 64, SIM_RISCV_SP_REGNUM, NULL },
  { "gp", 64, SIM_RISCV_GP_REGNUM, NULL },
  { "tp", 64, SIM_RISCV_TP_REGNUM, NULL },
  { "t0", 64, SIM_RISCV_T0_REGNUM, NULL },
  { "t1", 64, SIM_RISCV_T1_REGNUM, NULL },
  { "t2", 64, SIM_RISCV_T2_REGNUM, NULL },
  { "fp", 64, SIM_RISCV_FP_REGNUM, NULL },
  { "s1", 64, SIM_RISCV_S1_REGNUM, NULL },
  { "a0", 64, SIM_RISCV_A0_REGNUM, NULL },
  { "a1", 64, SIM_RISCV_A1_REGNUM, NULL },
  { "a2", 64, SIM_RISCV_A2_REGNUM, NULL },
  { "a3", 64, SIM_RISCV_A3_REGNUM, NULL },
  { "a4", 64, SIM_RISCV_A4_REGNUM, NULL },
  { "a5", 64, SIM_RISCV_A5_REGNUM, NULL },
  { "a6", 64, SIM_RISCV_A6_REGNUM, NULL },
  { "a7", 64, SIM_RISCV_A7_REGNUM, NULL },
  { "s2", 64, SIM_RISCV_S2_REGNUM, NULL },
  { "s3", 64, SIM_RISCV_S3_REGNUM, NULL },
  { "s4", 64, SIM_RISCV_S4_REGNUM, NULL },
  { "s5", 64, SIM_RISCV_S5_REGNUM, NULL },
  { "s6", 64, SIM_RISCV_S6_REGNUM, NULL },
  { "s7", 64, SIM_RISCV_S7_REGNUM, NULL },
  { "s8", 64, SIM_RISCV_S8_REGNUM, NULL },
  { "s9", 64, SIM_RISCV_S9_REGNUM, NULL },
  { "s10", 64, SIM_RISCV_S10_REGNUM, NULL },
  { "s11", 64, SIM_RISCV_S11_REGNUM, NULL },
  { "t3", 64, SIM_RISCV_T3_REGNUM, NULL },
  { "t4", 64, SIM_RISCV_T4_REGNUM, NULL },
  { "t5", 64, SIM_RISCV_T5_REGNUM, NULL },
  { "t6", 64, SIM_RISCV_T6_REGNUM, NULL },
  { "pc", 64, SIM_RISCV_PC_REGNUM, "code_ptr" },
};

/* RISC-V FPU registers (org.gnu.gdb.riscv.fpu).
   Registers f0-f31.  Uses ABI names.  */

static const struct sim_tdesc_reg riscv_fpu_regs_32[] = {
  { "ft0", 32, SIM_RISCV_FT0_REGNUM, "ieee_single" },
  { "ft1", 32, SIM_RISCV_FT1_REGNUM, "ieee_single" },
  { "ft2", 32, SIM_RISCV_FT2_REGNUM, "ieee_single" },
  { "ft3", 32, SIM_RISCV_FT3_REGNUM, "ieee_single" },
  { "ft4", 32, SIM_RISCV_FT4_REGNUM, "ieee_single" },
  { "ft5", 32, SIM_RISCV_FT5_REGNUM, "ieee_single" },
  { "ft6", 32, SIM_RISCV_FT6_REGNUM, "ieee_single" },
  { "ft7", 32, SIM_RISCV_FT7_REGNUM, "ieee_single" },
  { "fs0", 32, SIM_RISCV_FS0_REGNUM, "ieee_single" },
  { "fs1", 32, SIM_RISCV_FS1_REGNUM, "ieee_single" },
  { "fa0", 32, SIM_RISCV_FA0_REGNUM, "ieee_single" },
  { "fa1", 32, SIM_RISCV_FA1_REGNUM, "ieee_single" },
  { "fa2", 32, SIM_RISCV_FA2_REGNUM, "ieee_single" },
  { "fa3", 32, SIM_RISCV_FA3_REGNUM, "ieee_single" },
  { "fa4", 32, SIM_RISCV_FA4_REGNUM, "ieee_single" },
  { "fa5", 32, SIM_RISCV_FA5_REGNUM, "ieee_single" },
  { "fa6", 32, SIM_RISCV_FA6_REGNUM, "ieee_single" },
  { "fa7", 32, SIM_RISCV_FA7_REGNUM, "ieee_single" },
  { "fs2", 32, SIM_RISCV_FS2_REGNUM, "ieee_single" },
  { "fs3", 32, SIM_RISCV_FS3_REGNUM, "ieee_single" },
  { "fs4", 32, SIM_RISCV_FS4_REGNUM, "ieee_single" },
  { "fs5", 32, SIM_RISCV_FS5_REGNUM, "ieee_single" },
  { "fs6", 32, SIM_RISCV_FS6_REGNUM, "ieee_single" },
  { "fs7", 32, SIM_RISCV_FS7_REGNUM, "ieee_single" },
  { "fs8", 32, SIM_RISCV_FS8_REGNUM, "ieee_single" },
  { "fs9", 32, SIM_RISCV_FS9_REGNUM, "ieee_single" },
  { "fs10", 32, SIM_RISCV_FS10_REGNUM, "ieee_single" },
  { "fs11", 32, SIM_RISCV_FS11_REGNUM, "ieee_single" },
  { "ft8", 32, SIM_RISCV_FT8_REGNUM, "ieee_single" },
  { "ft9", 32, SIM_RISCV_FT9_REGNUM, "ieee_single" },
  { "ft10", 32, SIM_RISCV_FT10_REGNUM, "ieee_single" },
  { "ft11", 32, SIM_RISCV_FT11_REGNUM, "ieee_single" },
};

static const struct sim_tdesc_reg riscv_fpu_regs_64[] = {
  { "ft0", 64, SIM_RISCV_FT0_REGNUM, "riscv_double" },
  { "ft1", 64, SIM_RISCV_FT1_REGNUM, "riscv_double" },
  { "ft2", 64, SIM_RISCV_FT2_REGNUM, "riscv_double" },
  { "ft3", 64, SIM_RISCV_FT3_REGNUM, "riscv_double" },
  { "ft4", 64, SIM_RISCV_FT4_REGNUM, "riscv_double" },
  { "ft5", 64, SIM_RISCV_FT5_REGNUM, "riscv_double" },
  { "ft6", 64, SIM_RISCV_FT6_REGNUM, "riscv_double" },
  { "ft7", 64, SIM_RISCV_FT7_REGNUM, "riscv_double" },
  { "fs0", 64, SIM_RISCV_FS0_REGNUM, "riscv_double" },
  { "fs1", 64, SIM_RISCV_FS1_REGNUM, "riscv_double" },
  { "fa0", 64, SIM_RISCV_FA0_REGNUM, "riscv_double" },
  { "fa1", 64, SIM_RISCV_FA1_REGNUM, "riscv_double" },
  { "fa2", 64, SIM_RISCV_FA2_REGNUM, "riscv_double" },
  { "fa3", 64, SIM_RISCV_FA3_REGNUM, "riscv_double" },
  { "fa4", 64, SIM_RISCV_FA4_REGNUM, "riscv_double" },
  { "fa5", 64, SIM_RISCV_FA5_REGNUM, "riscv_double" },
  { "fa6", 64, SIM_RISCV_FA6_REGNUM, "riscv_double" },
  { "fa7", 64, SIM_RISCV_FA7_REGNUM, "riscv_double" },
  { "fs2", 64, SIM_RISCV_FS2_REGNUM, "riscv_double" },
  { "fs3", 64, SIM_RISCV_FS3_REGNUM, "riscv_double" },
  { "fs4", 64, SIM_RISCV_FS4_REGNUM, "riscv_double" },
  { "fs5", 64, SIM_RISCV_FS5_REGNUM, "riscv_double" },
  { "fs6", 64, SIM_RISCV_FS6_REGNUM, "riscv_double" },
  { "fs7", 64, SIM_RISCV_FS7_REGNUM, "riscv_double" },
  { "fs8", 64, SIM_RISCV_FS8_REGNUM, "riscv_double" },
  { "fs9", 64, SIM_RISCV_FS9_REGNUM, "riscv_double" },
  { "fs10", 64, SIM_RISCV_FS10_REGNUM, "riscv_double" },
  { "fs11", 64, SIM_RISCV_FS11_REGNUM, "riscv_double" },
  { "ft8", 64, SIM_RISCV_FT8_REGNUM, "riscv_double" },
  { "ft9", 64, SIM_RISCV_FT9_REGNUM, "riscv_double" },
  { "ft10", 64, SIM_RISCV_FT10_REGNUM, "riscv_double" },
  { "ft11", 64, SIM_RISCV_FT11_REGNUM, "riscv_double" },
};

/* RISC-V CSR registers (org.gnu.gdb.riscv.csr).
   Generated from opcode/riscv-opc.h using DECLARE_CSR.  */

static const struct sim_tdesc_reg riscv_csr_regs_32[] = {
#define DECLARE_CSR(name, num, ...) { #name, 32, SIM_RISCV_ ## num ## _REGNUM, NULL },
#include "opcode/riscv-opc.h"
#undef DECLARE_CSR
};

static const struct sim_tdesc_reg riscv_csr_regs_64[] = {
#define DECLARE_CSR(name, num, ...) { #name, 64, SIM_RISCV_ ## num ## _REGNUM, NULL },
#include "opcode/riscv-opc.h"
#undef DECLARE_CSR
};

/* Features for RV32.  */

static const struct sim_tdesc_feature riscv_features_32[] = {
  { "org.gnu.gdb.riscv.cpu",
    NULL, 0,
    riscv_cpu_regs_32,
    sizeof (riscv_cpu_regs_32) / sizeof (riscv_cpu_regs_32[0]) },
  { "org.gnu.gdb.riscv.fpu",
    NULL, 0,
    riscv_fpu_regs_32,
    sizeof (riscv_fpu_regs_32) / sizeof (riscv_fpu_regs_32[0]) },
  { "org.gnu.gdb.riscv.csr",
    NULL, 0,
    riscv_csr_regs_32,
    sizeof (riscv_csr_regs_32) / sizeof (riscv_csr_regs_32[0]) },
};

/* Features for RV64.  */

static const struct sim_tdesc_feature riscv_features_64[] = {
  { "org.gnu.gdb.riscv.cpu",
    NULL, 0,
    riscv_cpu_regs_64,
    sizeof (riscv_cpu_regs_64) / sizeof (riscv_cpu_regs_64[0]) },
  { "org.gnu.gdb.riscv.fpu",
    riscv_fpu_types_64,
    sizeof (riscv_fpu_types_64) / sizeof (riscv_fpu_types_64[0]),
    riscv_fpu_regs_64,
    sizeof (riscv_fpu_regs_64) / sizeof (riscv_fpu_regs_64[0]) },
  { "org.gnu.gdb.riscv.csr",
    NULL, 0,
    riscv_csr_regs_64,
    sizeof (riscv_csr_regs_64) / sizeof (riscv_csr_regs_64[0]) },
};

/* Target descriptions for RV32 and RV64.  */

static const struct sim_tdesc riscv_tdesc_32 = {
  "riscv:rv32",
  riscv_features_32,
  sizeof (riscv_features_32) / sizeof (riscv_features_32[0]),
};

static const struct sim_tdesc riscv_tdesc_64 = {
  "riscv:rv64",
  riscv_features_64,
  sizeof (riscv_features_64) / sizeof (riscv_features_64[0]),
};

/* Return the target description for the current RISC-V configuration.
   See riscv-sim.h.  */

const struct sim_tdesc *
riscv_tdesc_get (SIM_DESC sd)
{
  SIM_CPU *cpu = STATE_CPU (sd, 0);
  int xlen = RISCV_XLEN (cpu);

  if (xlen == 32)
    return &riscv_tdesc_32;
  else if (xlen == 64)
    return &riscv_tdesc_64;
  else
    return NULL;
}
