/* GNU/Linux/RISC-V specific low level interface, for the remote server
   for GDB.
   Copyright (C) 2020-2025 Free Software Foundation, Inc.

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


#include "linux-low.h"
#include "tdesc.h"
#include "elf/common.h"
#include "nat/riscv-linux-tdesc.h"
#include "opcode/riscv.h"

/* Work around glibc header breakage causing ELF_NFPREG not to be usable.  */
#ifndef NFPREG
# define NFPREG 33
#endif

/* Linux target op definitions for the RISC-V architecture.  */

class riscv_target : public linux_process_target
{
public:

  const regs_info *get_regs_info () override;

  int breakpoint_kind_from_pc (CORE_ADDR *pcptr) override;

  const gdb_byte *sw_breakpoint_from_kind (int kind, int *size) override;

protected:

  void low_arch_setup () override;

  bool low_cannot_fetch_register (int regno) override;

  bool low_cannot_store_register (int regno) override;

  bool low_fetch_register (regcache *regcache, int regno) override;

  bool low_supports_breakpoints () override;

  CORE_ADDR low_get_pc (regcache *regcache) override;

  void low_set_pc (regcache *regcache, CORE_ADDR newpc) override;

  bool low_breakpoint_at (CORE_ADDR pc) override;
};

/* The singleton target ops object.  */

static riscv_target the_riscv_target;

bool
riscv_target::low_cannot_fetch_register (int regno)
{
  gdb_assert_not_reached ("linux target op low_cannot_fetch_register "
			  "is not implemented by the target");
}

bool
riscv_target::low_cannot_store_register (int regno)
{
  gdb_assert_not_reached ("linux target op low_cannot_store_register "
			  "is not implemented by the target");
}

/* Implementation of linux target ops method "low_arch_setup".  */

void
riscv_target::low_arch_setup ()
{
  static const char *expedite_regs[] = { "sp", "pc", NULL };

  const riscv_gdbarch_features features
    = riscv_linux_read_features (current_thread->id.lwp ());
  target_desc_up tdesc = riscv_create_target_description (features);

  if (tdesc->expedite_regs.empty ())
    {
      init_target_desc (tdesc.get (), expedite_regs, GDB_OSABI_LINUX);
      gdb_assert (!tdesc->expedite_regs.empty ());
    }

  current_process ()->tdesc = tdesc.release ();
}

/* Collect GPRs from REGCACHE into BUF.  */

static void
riscv_fill_gregset (struct regcache *regcache, void *buf)
{
  const struct target_desc *tdesc = regcache->tdesc;
  elf_gregset_t *regset = (elf_gregset_t *) buf;
  int regno = find_regno (tdesc, "zero");
  int i;

  collect_register_by_name (regcache, "pc", *regset);
  for (i = 1; i < ARRAY_SIZE (*regset); i++)
    collect_register (regcache, regno + i, *regset + i);
}

/* Supply GPRs from BUF into REGCACHE.  */

static void
riscv_store_gregset (struct regcache *regcache, const void *buf)
{
  const elf_gregset_t *regset = (const elf_gregset_t *) buf;
  const struct target_desc *tdesc = regcache->tdesc;
  int regno = find_regno (tdesc, "zero");
  int i;

  supply_register_by_name (regcache, "pc", *regset);
  supply_register_zeroed (regcache, regno);
  for (i = 1; i < ARRAY_SIZE (*regset); i++)
    supply_register (regcache, regno + i, *regset + i);
}

/* Collect FPRs from REGCACHE into BUF.  */

static void
riscv_fill_fpregset (struct regcache *regcache, void *buf)
{
  const struct target_desc *tdesc = regcache->tdesc;
  int regno = find_regno (tdesc, "ft0");
  int flen = register_size (regcache->tdesc, regno);
  gdb_byte *regbuf = (gdb_byte *) buf;
  int i;

  for (i = 0; i < ELF_NFPREG - 1; i++, regbuf += flen)
    collect_register (regcache, regno + i, regbuf);
  collect_register_by_name (regcache, "fcsr", regbuf);
}

/* Supply FPRs from BUF into REGCACHE.  */

static void
riscv_store_fpregset (struct regcache *regcache, const void *buf)
{
  const struct target_desc *tdesc = regcache->tdesc;
  int regno = find_regno (tdesc, "ft0");
  int flen = register_size (regcache->tdesc, regno);
  const gdb_byte *regbuf = (const gdb_byte *) buf;
  int i;

  for (i = 0; i < ELF_NFPREG - 1; i++, regbuf += flen)
    supply_register (regcache, regno + i, regbuf);
  supply_register_by_name (regcache, "fcsr", regbuf);
}

/* Collect vector registers from REGCACHE into BUF.  */

static void
riscv_fill_vregset (struct regcache *regcache, void *buf)
{
  const struct target_desc *tdesc = regcache->tdesc;
  int regno = find_regno (tdesc, "v0");
  int vlenb = register_size (regcache->tdesc, regno);
  uint64_t u64_vlenb = vlenb;	/* pad to max XLEN for buffer conversion */
  uint64_t u64_vxsat = 0;
  uint64_t u64_vxrm = 0;
  uint64_t u64_vcsr = 0;
  gdb_byte *regbuf;
  int i;

  /* Since vxsat and equivalent bits in vcsr are aliases (and same for vxrm), we have a dilemma.
     For this gdb -> gdbserver topology, if the aliased pairs have values that disagree, then
     which value should take precedence?  We don't know which alias was most
     recently assigned.  We're just getting a block of register values including vxsat, vxrm,
     and vcsr.  We have to impose some kind of rule for predictable resolution to resolve any inconsistency.
     For now, let's say that vxsat and vxrm take precedence, and those values will be applied to the
     corresponding fields in vcsr.  Reconcile these 3 interdependent registers now:
  */
  regbuf = (gdb_byte *) & u64_vcsr;
  collect_register_by_name (regcache, "vcsr", regbuf);
  regbuf = (gdb_byte *) & u64_vxsat;
  collect_register_by_name (regcache, "vxsat", regbuf);
  regbuf = (gdb_byte *) & u64_vxrm;
  collect_register_by_name (regcache, "vxrm", regbuf);

  u64_vcsr &= ~((uint64_t)VCSR_MASK_VXSAT << VCSR_POS_VXSAT);
  u64_vcsr |= ((u64_vxsat & VCSR_MASK_VXSAT) << VCSR_POS_VXSAT);
  u64_vcsr &= ~((uint64_t)VCSR_MASK_VXRM << VCSR_POS_VXRM);
  u64_vcsr |= ((u64_vxrm & VCSR_MASK_VXRM) << VCSR_POS_VXRM);

  /* Replace the original vcsr value with the "cooked" value */
  regbuf = (gdb_byte *) & u64_vcsr;
  supply_register_by_name (regcache, "vcsr", regbuf);

  /* Now stage the ptrace buffer (it'll receive the cooked vcsr value) */

  regbuf = (gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vstart);
  collect_register_by_name (regcache, "vstart", regbuf);
  regbuf = (gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vl);
  collect_register_by_name (regcache, "vl", regbuf);
  regbuf = (gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vtype);
  collect_register_by_name (regcache, "vtype", regbuf);
  regbuf = (gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vcsr);
  collect_register_by_name (regcache, "vcsr", regbuf);
  regbuf = (gdb_byte *) & u64_vlenb;
  collect_register_by_name (regcache, "vlenb", regbuf);


  regbuf = (gdb_byte *) buf + offsetof (struct __riscv_vregs, data);
  for (i = 0; i < 32; i++, regbuf += vlenb)
    collect_register (regcache, regno + i, regbuf);
}

/* Supply vector registers from BUF into REGCACHE.  */

static void
riscv_store_vregset (struct regcache *regcache, const void *buf)
{
  const struct target_desc *tdesc = regcache->tdesc;
  int regno = find_regno (tdesc, "v0");
  int vlenb = register_size (regcache->tdesc, regno);
  uint64_t u64_vlenb = vlenb;	/* pad to max XLEN for buffer conversion */
  uint64_t vcsr;
  uint64_t vxsat;
  uint64_t vxrm;
  const gdb_byte *regbuf;
  int i;

  regbuf =
    (const gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vstart);
  supply_register_by_name (regcache, "vstart", regbuf);
  regbuf =
    (const gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vl);
  supply_register_by_name (regcache, "vl", regbuf);
  regbuf =
    (const gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vtype);
  supply_register_by_name (regcache, "vtype", regbuf);
  regbuf =
    (const gdb_byte *) buf + offsetof (struct __riscv_vregs, vstate.vcsr);
  supply_register_by_name (regcache, "vcsr", regbuf);
  /* also store off a non-byte-wise copy of vcsr, to derive values for vxsat and vxrm */
  vcsr = *(uint64_t*)regbuf;
  /* vlenb isn't part of vstate, but we have already inferred its value by running code on this
     hart, and we're assuming homogeneous VLENB if it's an SMP system */
  regbuf = (gdb_byte *) & u64_vlenb;
  supply_register_by_name (regcache, "vlenb", regbuf);

  /* vxsat and vxrm, are not part of vstate, so we have to extract from VCSR
     value */
  vxsat = ((vcsr >> VCSR_POS_VXSAT) & VCSR_MASK_VXSAT);
  regbuf = (gdb_byte *) &vxsat;
  supply_register_by_name (regcache, "vxsat", regbuf);
  vxrm = ((vcsr >> VCSR_POS_VXRM) & VCSR_MASK_VXRM);
  regbuf = (gdb_byte *) &vxrm;
  supply_register_by_name (regcache, "vxrm", regbuf);

  /* v0..v31 */
  regbuf = (const gdb_byte *) buf + offsetof (struct __riscv_vregs, data);
  for (i = 0; i < 32; i++, regbuf += vlenb)
    supply_register (regcache, regno + i, regbuf);
}

/* RISC-V/Linux regsets.  FPRs are optional and come in different sizes,
   so define multiple regsets for them marking them all as OPTIONAL_REGS
   rather than FP_REGS, so that "regsets_fetch_inferior_registers" picks
   the right one according to size.  */
static struct regset_info riscv_regsets[] = {
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_PRSTATUS,
    sizeof (elf_gregset_t), GENERAL_REGS,
    riscv_fill_gregset, riscv_store_gregset },
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_FPREGSET,
    sizeof (struct __riscv_mc_q_ext_state), OPTIONAL_REGS,
    riscv_fill_fpregset, riscv_store_fpregset },
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_FPREGSET,
    sizeof (struct __riscv_mc_d_ext_state), OPTIONAL_REGS,
    riscv_fill_fpregset, riscv_store_fpregset },
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_FPREGSET,
    sizeof (struct __riscv_mc_f_ext_state), OPTIONAL_REGS,
    riscv_fill_fpregset, riscv_store_fpregset },
  { PTRACE_GETREGSET, PTRACE_SETREGSET, NT_RISCV_VECTOR,
    sizeof (struct __riscv_vregs), OPTIONAL_REGS,
    riscv_fill_vregset, riscv_store_vregset },
  NULL_REGSET
};

/* RISC-V/Linux regset information.  */
static struct regsets_info riscv_regsets_info =
  {
    riscv_regsets, /* regsets */
    0, /* num_regsets */
    NULL, /* disabled_regsets */
  };

/* Definition of linux_target_ops data member "regs_info".  */
static struct regs_info riscv_regs =
  {
    NULL, /* regset_bitmap */
    NULL, /* usrregs */
    &riscv_regsets_info,
  };

/* Implementation of linux target ops method "get_regs_info".  */

const regs_info *
riscv_target::get_regs_info ()
{
  return &riscv_regs;
}

/* Implementation of linux target ops method "low_fetch_register".  */

bool
riscv_target::low_fetch_register (regcache *regcache, int regno)
{
  const struct target_desc *tdesc = regcache->tdesc;

  if (regno != find_regno (tdesc, "zero"))
    return false;
  supply_register_zeroed (regcache, regno);
  return true;
}

bool
riscv_target::low_supports_breakpoints ()
{
  return true;
}

/* Implementation of linux target ops method "low_get_pc".  */

CORE_ADDR
riscv_target::low_get_pc (regcache *regcache)
{
  elf_gregset_t regset;

  if (sizeof (regset[0]) == 8)
    return linux_get_pc_64bit (regcache);
  else
    return linux_get_pc_32bit (regcache);
}

/* Implementation of linux target ops method "low_set_pc".  */

void
riscv_target::low_set_pc (regcache *regcache, CORE_ADDR newpc)
{
  elf_gregset_t regset;

  if (sizeof (regset[0]) == 8)
    linux_set_pc_64bit (regcache, newpc);
  else
    linux_set_pc_32bit (regcache, newpc);
}

/* Correct in either endianness.  */
static const uint16_t riscv_ibreakpoint[] = { 0x0073, 0x0010 };
static const uint16_t riscv_cbreakpoint = 0x9002;

/* Implementation of target ops method "breakpoint_kind_from_pc".  */

int
riscv_target::breakpoint_kind_from_pc (CORE_ADDR *pcptr)
{
  union
    {
      gdb_byte bytes[2];
      uint16_t insn;
    }
  buf;

  if (target_read_memory (*pcptr, buf.bytes, sizeof (buf.insn)) == 0
      && riscv_insn_length (buf.insn == sizeof (riscv_ibreakpoint)))
    return sizeof (riscv_ibreakpoint);
  else
    return sizeof (riscv_cbreakpoint);
}

/* Implementation of target ops method "sw_breakpoint_from_kind".  */

const gdb_byte *
riscv_target::sw_breakpoint_from_kind (int kind, int *size)
{
  *size = kind;
  switch (kind)
    {
      case sizeof (riscv_ibreakpoint):
	return (const gdb_byte *) &riscv_ibreakpoint;
      default:
	return (const gdb_byte *) &riscv_cbreakpoint;
    }
}

/* Implementation of linux target ops method "low_breakpoint_at".  */

bool
riscv_target::low_breakpoint_at (CORE_ADDR pc)
{
  union
    {
      gdb_byte bytes[2];
      uint16_t insn;
    }
  buf;

  if (target_read_memory (pc, buf.bytes, sizeof (buf.insn)) == 0
      && (buf.insn == riscv_cbreakpoint
	  || (buf.insn == riscv_ibreakpoint[0]
	      && target_read_memory (pc + sizeof (buf.insn), buf.bytes,
				     sizeof (buf.insn)) == 0
	      && buf.insn == riscv_ibreakpoint[1])))
    return true;
  else
    return false;
}

/* The linux target ops object.  */

linux_process_target *the_linux_target = &the_riscv_target;

/* Initialize the RISC-V/Linux target.  */

void
initialize_low_arch ()
{
  initialize_regsets_info (&riscv_regsets_info);
}
