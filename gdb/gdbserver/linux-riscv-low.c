/* GNU/Linux/RISCV specific low level interface, for the remote server for GDB.

   Copyright (C) 2020 Free Software Foundation, Inc.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "server.h"
#include "linux-low.h"
#include <asm/ptrace.h>
#include "elf.h"
#include "riscv-tdep.h"


/* Defined in auto-generated file riscv32-linux-generated.c.  */
void init_registers_riscv32_linux (void);
extern const struct target_desc *tdesc_riscv32_linux;

/* Defined in auto-generated file riscv64-linux-generated.c.  */
void init_registers_riscv64_linux (void);
extern const struct target_desc *tdesc_riscv64_linux;

static int
riscv_cannot_fetch_register (int regno)
{
  if (regno >=0 && regno <= 63)
    return 0;
  else
    return 1;
}

static int
riscv_cannot_store_register (int regno)
{
  if (regno >=0 && regno <= 63)
    return 0;
  else
    return 1;
}

static void
riscv_fill_gregset (struct regcache *regcache, void *buf)
{
   elf_greg_t *regp = (elf_greg_t *)buf;

   /* We only support the integer registers and PC here.  */
   for (int i = RISCV_ZERO_REGNUM + 1; i < RISCV_PC_REGNUM; i++)
     collect_register (regcache, i, regp + i);

   collect_register (regcache, 32, regp + 0);
}

static void
riscv_store_gregset (struct regcache *regcache, const void *buf)
{
  const elf_greg_t *regp = (const elf_greg_t *)buf;

  /* We only support the integer registers and PC here.  */
  for (int i = RISCV_ZERO_REGNUM + 1; i < RISCV_PC_REGNUM; i++)
    supply_register (regcache, i, regp + i);

  /* GDB stores PC in reg 32.  Linux kernel stores it in reg 0.  */
  supply_register (regcache, 32, regp + 0);

  /* Fill the inaccessible zero register with zero.  */
  supply_register_zeroed (regcache, 0);
}

static void
riscv_fill_fpregset (struct regcache *regcache, void *buf)
{
  prfpregset_t *fpregs = (prfpregset_t *)buf;

  /* We only support the FP registers and FCSR here.  */
  for (int i = RISCV_FIRST_FP_REGNUM; i <= RISCV_LAST_FP_REGNUM; i++)
    collect_register (regcache, i, &fpregs->__d.__f[i - RISCV_FIRST_FP_REGNUM]);

  collect_register (regcache, RISCV_CSR_FCSR_REGNUM, &fpregs->__d.__fcsr);
}

static void
riscv_store_fpregset (struct regcache *regcache, const void *buf)
{
  const prfpregset_t *fpregs = (const prfpregset_t *)buf;

  /* We only support the FP registers and FCSR here.  */
  for (int i = RISCV_FIRST_FP_REGNUM; i <= RISCV_LAST_FP_REGNUM; i++)
    supply_register (regcache, i, &fpregs->__d.__f[i - RISCV_FIRST_FP_REGNUM]);

  supply_register (regcache, RISCV_CSR_FCSR_REGNUM, &fpregs->__d.__fcsr);
}

static struct regset_info riscv_regsets[] = {
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_PRSTATUS, sizeof (elf_gregset_t),
    GENERAL_REGS,
    riscv_fill_gregset, riscv_store_gregset },
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_FPREGSET, sizeof (elf_fpregset_t),
    FP_REGS,
    riscv_fill_fpregset, riscv_store_fpregset },
  NULL_REGSET
};


static void
riscv_arch_setup (void)
{
  int size = sizeof (elf_greg_t);

  if (size == 4)
    current_process ()->tdesc = tdesc_riscv32_linux;
  else
    current_process ()->tdesc = tdesc_riscv64_linux;
}

static struct regsets_info riscv_regsets_info =
  {
    riscv_regsets, /* regsets */
    0, /* num_regsets */
    NULL, /* disabled_regsets */
  };

static struct regs_info regs_info =
  {
    NULL, /* regset_bitmap */
    NULL, /* usrregs */
    &riscv_regsets_info
  };

static const struct regs_info *
riscv_regs_info (void)
{
  return &regs_info;
}

/* Implementation of linux_target_ops method "sw_breakpoint_from_kind".  */
static const gdb_byte ebreak[] = {0x73, 0x00, 0x10, 0x00};
static const gdb_byte c_ebreak[] = {0x02, 0x90};

static const gdb_byte *
riscv_sw_breakpoint_from_kind (int kind, int *size)
{
  *size = kind;

  switch (kind)
    {
      case 2:
        return c_ebreak;
      case 4:
        return ebreak;
      default:
        gdb_assert (0);
    }
}

static int
riscv_breakpoint_at (CORE_ADDR where)
{
  unsigned char insn[4];

  (*the_target->read_memory) (where, (unsigned char *) insn, 4);

  if (insn[0] == ebreak[0] && insn[1] == ebreak[1]
      && insn[2] == ebreak[2] && insn[3] == ebreak[3])
    return 1;
  else if (insn[0] == ebreak[0] && insn[1] == ebreak[1])
    return 1;
  else
    return 0;
}


struct linux_target_ops the_low_target = {
  riscv_arch_setup, /* arch_setup */
  riscv_regs_info, /* regs_info */
  riscv_cannot_fetch_register, /* cannot_fetch_register */
  riscv_cannot_store_register, /* cannot_store_register */
  NULL, /* fetch_register */
  linux_get_pc_64bit, /* get_pc */
  linux_set_pc_64bit, /* set_pc */
  NULL, /* breakpoint_kind_from_pc */
  riscv_sw_breakpoint_from_kind, /* sw_breakpoint_from_kind */
  NULL, /* get_next_pcs */
  0,    /* decr_pc_after_break */
  riscv_breakpoint_at, /* breakpoint_at */
  NULL, /* supports_z_point_type */
  NULL, /* insert_point */
  NULL, /* remove_point */
  NULL, /* stopped_by_watchpoint */
  NULL, /* stopped_data_address */
  NULL, /* collect_ptrace_register */
  NULL, /* supply_ptrace_register */
  NULL, /* siginfo_fixup */
  NULL, /* new_process */
  NULL, /* delete_process */
  NULL, /* new_thread */
  NULL, /* delete_thread */
  NULL, /* new_fork */
  NULL, /* prepare_to_resume */
  NULL, /* process_qsupported */
  NULL, /* supports_tracepoints */
  NULL, /* get_thread_area */
  NULL, /* install_fast_tracepoint_jump_pad */
  NULL, /* emit_ops */
  NULL, /* get_min_fast_tracepoint_insn_len */
  NULL, /* supports_range_stepping */
  NULL, /* breakpoint_kind_from_current_state */
  NULL, /* supports_hardware_single_step */
};


void
initialize_low_arch (void)
{
  init_registers_riscv32_linux ();
  init_registers_riscv64_linux ();
  initialize_regsets_info (&riscv_regsets_info);
}
