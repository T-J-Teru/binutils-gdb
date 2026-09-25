/* Target-dependent header for the RISC-V architecture, for GDB, the
   GNU Debugger.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

#ifndef GDB_RISCV_REGS_H
#define GDB_RISCV_REGS_H

/* RiscV register numbers.  */
enum
{
  RISCV_ZERO_REGNUM = 0, /* Read-only register, always 0.  */
  RISCV_RA_REGNUM = 1,   /* Return Address.  */
  RISCV_SP_REGNUM = 2,   /* Stack Pointer.  */
  RISCV_GP_REGNUM = 3,   /* Global Pointer.  */
  RISCV_TP_REGNUM = 4,   /* Thread Pointer.  */
  RISCV_FP_REGNUM = 8,   /* Frame Pointer.  */
  RISCV_A0_REGNUM = 10,  /* First argument.  */
  RISCV_A1_REGNUM = 11,  /* Second argument.  */
  RISCV_A2_REGNUM = 12,  /* Third argument.  */
  RISCV_A3_REGNUM = 13,  /* Forth argument.  */
  RISCV_A4_REGNUM = 14,  /* Fifth argument.  */
  RISCV_A5_REGNUM = 15,  /* Sixth argument.  */
  RISCV_A7_REGNUM = 17,  /* Register to pass syscall number.  */
  RISCV_PC_REGNUM = 32,  /* Program Counter.  */

  RISCV_NUM_INTEGER_REGS = 32,

  RISCV_FIRST_FP_REGNUM = 33, /* First Floating Point Register */
  RISCV_FA0_REGNUM = 43,
  RISCV_FA1_REGNUM = RISCV_FA0_REGNUM + 1,
  RISCV_LAST_FP_REGNUM = 64, /* Last Floating Point Register */

  RISCV_FIRST_CSR_REGNUM = 65, /* First CSR */
#define DECLARE_CSR(name, num, class, define_version, abort_version) \
  RISCV_##num##_REGNUM = RISCV_FIRST_CSR_REGNUM + num,
#include "opcode/riscv-opc.h"
#undef DECLARE_CSR
  RISCV_LAST_CSR_REGNUM = 4160,
  RISCV_CSR_LEGACY_MISA_REGNUM = 0xf10 + RISCV_FIRST_CSR_REGNUM,

  RISCV_PRIV_REGNUM = 4161,

  RISCV_V0_REGNUM,

  RISCV_V31_REGNUM = RISCV_V0_REGNUM + 31,

  RISCV_LAST_REGNUM = RISCV_V31_REGNUM
};

/* RiscV DWARF register numbers.  */
enum
{
  RISCV_DWARF_REGNUM_X0 = 0,
  RISCV_DWARF_REGNUM_X31 = 31,
  RISCV_DWARF_REGNUM_F0 = 32,
  RISCV_DWARF_REGNUM_F31 = 63,
  RISCV_DWARF_REGNUM_V0 = 96,
  RISCV_DWARF_REGNUM_V31 = 127,
  RISCV_DWARF_FIRST_CSR = 4096,
  RISCV_DWARF_LAST_CSR = 8191,
};

#endif /* GDB_RISCV_REGS_H */
