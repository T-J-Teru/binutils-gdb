/* Target-dependent code for the RISC-V architecture, for GDB.

   Copyright (C) 1988-2015 Free Software Foundation, Inc.

   Contributed by Alessandro Forin(af@cs.cmu.edu) at CMU
   and by Per Bothner(bothner@cs.wisc.edu) at U.Wisconsin
   and by Todd Snyder <todd@bluespec.com>
   and by Mike Frysinger <vapier@gentoo.org>.

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

#include "defs.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "value.h"
#include "gdbcmd.h"
#include "language.h"
#include "gdbcore.h"
#include "symfile.h"
#include "objfiles.h"
#include "gdbtypes.h"
#include "target.h"
#include "arch-utils.h"
#include "regcache.h"
#include "osabi.h"
#include "riscv-tdep.h"
#include "block.h"
#include "reggroups.h"
#include "opcode/riscv.h"
#include "elf/riscv.h"
#include "elf-bfd.h"
#include "symcat.h"
#include "sim-regno.h"
#include "gdb/sim-riscv.h"
#include "dis-asm.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "infcall.h"
#include "floatformat.h"
#include "remote.h"
#include "target-descriptions.h"
#include "dwarf2-frame.h"
#include "user-regs.h"
#include "valprint.h"
#include "common-defs.h"
#include "opcode/riscv-opc.h"
#include <algorithm>

/* According to the ABI, the SP must be aligned to 16-byte boundaries.  */
#define SP_ALIGNMENT 16

#define DECLARE_INSN(INSN_NAME, INSN_MATCH, INSN_MASK) \
static inline bool is_ ## INSN_NAME ## _insn (long insn) \
{ \
  return (insn & INSN_MASK) == INSN_MATCH; \
}
#include "opcode/riscv-opc.h"
#undef DECLARE_INSN

struct riscv_frame_cache
{
  CORE_ADDR base;
  struct trad_frame_saved_reg *saved_regs;
};

static const char * const riscv_gdb_reg_names[RISCV_LAST_FP_REGNUM + 1] =
{
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",
  "pc",
  "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31",
};

struct register_alias
{
  const char *name;
  int regnum;
};

static const struct register_alias riscv_register_aliases[] =
{
  { "zero", 0 },
  { "ra", 1 },
  { "sp", 2 },
  { "gp", 3 },
  { "tp", 4 },
  { "t0", 5 },
  { "t1", 6 },
  { "t2", 7 },
  { "fp", 8 },
  { "s0", 8 },
  { "s1", 9 },
  { "a0", 10 },
  { "a1", 11 },
  { "a2", 12 },
  { "a3", 13 },
  { "a4", 14 },
  { "a5", 15 },
  { "a6", 16 },
  { "a7", 17 },
  { "s2", 18 },
  { "s3", 19 },
  { "s4", 20 },
  { "s5", 21 },
  { "s6", 22 },
  { "s7", 23 },
  { "s8", 24 },
  { "s9", 25 },
  { "s10", 26 },
  { "s11", 27 },
  { "t3", 28 },
  { "t4", 29 },
  { "t5", 30 },
  { "t6", 31 },
  /* pc is 32.  */
  { "ft0", 33 },
  { "ft1", 34 },
  { "ft2", 35 },
  { "ft3", 36 },
  { "ft4", 37 },
  { "ft5", 38 },
  { "ft6", 39 },
  { "ft7", 40 },
  { "fs0", 41 },
  { "fs1", 42 },
  { "fa0", 43 },
  { "fa1", 44 },
  { "fa2", 45 },
  { "fa3", 46 },
  { "fa4", 47 },
  { "fa5", 48 },
  { "fa6", 49 },
  { "fa7", 50 },
  { "fs2", 51 },
  { "fs3", 52 },
  { "fs4", 53 },
  { "fs5", 54 },
  { "fs6", 55 },
  { "fs7", 56 },
  { "fs8", 57 },
  { "fs9", 58 },
  { "fs10", 59 },
  { "fs11", 60 },
  { "ft8", 61 },
  { "ft9", 62 },
  { "ft10", 63 },
  { "ft11", 64 },
#define DECLARE_CSR(name, num) { #name, (num) + 65 },
#include "opcode/riscv-opc.h"
#undef DECLARE_CSR
};

static enum auto_boolean use_compressed_breakpoints;
/*
static void
show_use_compressed_breakpoints (struct ui_file *file, int from_tty,
			    struct cmd_list_element *c,
			    const char *value)
{
  fprintf_filtered (file,
		    _("Debugger's behavior regarding "
		      "compressed breakpoints is %s.\n"),
		    value);
}
*/

static struct cmd_list_element *setriscvcmdlist = NULL;
static struct cmd_list_element *showriscvcmdlist = NULL;

static void
show_riscv_command (const char *args, int from_tty)
{
  help_list (showriscvcmdlist, "show riscv ", all_commands, gdb_stdout);
}

static void
set_riscv_command (const char *args, int from_tty)
{
  printf_unfiltered
    ("\"set riscv\" must be followed by an appropriate subcommand.\n");
  help_list (setriscvcmdlist, "set riscv ", all_commands, gdb_stdout);
}

static uint32_t
cached_misa (bool *read_p)
{
  static bool read = false;
  static uint32_t value = 0;

  if (!read && target_has_registers)
    {
      struct frame_info *frame = get_current_frame ();
      TRY
	{
	  value = get_frame_register_unsigned (frame, RISCV_CSR_MISA_REGNUM);
	}
      CATCH (ex, RETURN_MASK_ERROR)
	{
	  // In old cores, $misa might live at 0xf10
	  value = get_frame_register_unsigned (frame,
			RISCV_CSR_MISA_REGNUM - 0x301 + 0xf10);
      }
      END_CATCH
      read = true;
    }

  if (read_p != nullptr)
    *read_p = read;

  return value;
}

/* Implement the breakpoint_kind_from_pc gdbarch method.  */

static int
riscv_breakpoint_kind_from_pc (struct gdbarch *gdbarch, CORE_ADDR *pcptr)
{
  if (use_compressed_breakpoints == AUTO_BOOLEAN_AUTO)
    {
      if (gdbarch_tdep (gdbarch)->supports_compressed_isa
	  == AUTO_BOOLEAN_AUTO)
	{
	  /* TODO: Because we try to read misa, it is not possible to set a
	     breakpoint before connecting to a live target. A suggested
	     workaround is to look at the ELF file in this case.  */
	  bool readp;
	  uint32_t misa = cached_misa(&readp);
	  if (readp)
	    gdbarch_tdep (gdbarch)->supports_compressed_isa
	      = ((misa & (1 << 2)) ? AUTO_BOOLEAN_TRUE : AUTO_BOOLEAN_FALSE);
	}

      if (gdbarch_tdep (gdbarch)->supports_compressed_isa == AUTO_BOOLEAN_TRUE)
	return 2;
      else
	return 4;
    }
  else if (use_compressed_breakpoints == AUTO_BOOLEAN_TRUE)
    return 2;
  else
    return 4;
}

/* Implement the sw_breakpoint_from_kind gdbarch method.  */

static const gdb_byte *
riscv_sw_breakpoint_from_kind (struct gdbarch *gdbarch, int kind, int *size)
{
  static const gdb_byte ebreak[] = { 0x73, 0x00, 0x10, 0x00, };
  static const gdb_byte c_ebreak[] = { 0x02, 0x90 };

  *size = kind;
  switch (kind)
    {
    case 2:
      return c_ebreak;
    case 4:
      return ebreak;
    default:
      gdb_assert(0);
    }
}

static struct value *
value_of_riscv_user_reg (struct frame_info *frame, const void *baton)
{
  const int *reg_p = (const int *)baton;

  return value_of_register (*reg_p, frame);
}

static const char *
register_name (struct gdbarch *gdbarch,
	       int regnum,
	       int prefer_alias)
{
  int i;
  static char buf[20];

  if (tdesc_has_registers (gdbarch_target_desc (gdbarch)))
    return tdesc_register_name (gdbarch, regnum);
  /* Prefer to use the alias. */
  if (prefer_alias &&
      regnum >= RISCV_ZERO_REGNUM && regnum <= RISCV_LAST_REGNUM)
    {
      for (i = 0; i < ARRAY_SIZE (riscv_register_aliases); ++i)
	if (regnum == riscv_register_aliases[i].regnum)
	  return riscv_register_aliases[i].name;
    }

  if (regnum >= RISCV_ZERO_REGNUM && regnum <= RISCV_LAST_FP_REGNUM)
      return riscv_gdb_reg_names[regnum];

  if (regnum >= RISCV_FIRST_CSR_REGNUM && regnum <= RISCV_LAST_CSR_REGNUM)
    {
      sprintf(buf, "csr%d", regnum - RISCV_FIRST_CSR_REGNUM);
      return buf;
    }

  if (regnum == RISCV_PRIV_REGNUM)
    {
      return "priv";
    }

  return NULL;
}

/* Implement the register_name gdbarch method.  */

static const char *
riscv_register_name (struct gdbarch *gdbarch,
		     int regnum)
{
  return register_name (gdbarch, regnum, 1);
}

/* Implement the pseudo_register_read gdbarch method.  */

static enum register_status
riscv_pseudo_register_read (struct gdbarch *gdbarch,
			    struct regcache *regcache,
			    int regnum,
			    gdb_byte *buf)
{
  return regcache_raw_read (regcache, regnum, buf);
}

/* Implement the pseudo_register_write gdbarch method.  */

static void
riscv_pseudo_register_write (struct gdbarch *gdbarch,
			     struct regcache *regcache,
			     int cookednum,
			     const gdb_byte *buf)
{
  regcache_raw_write (regcache, cookednum, buf);
}

/* Implement the register_type gdbarch method.  */

static struct type *
riscv_register_type (struct gdbarch *gdbarch,
		     int regnum)
{
  int regsize;

  if (regnum < RISCV_FIRST_FP_REGNUM)
    {
      if (regnum == gdbarch_pc_regnum (gdbarch)
	  || regnum == RISCV_RA_REGNUM)
	return builtin_type (gdbarch)->builtin_func_ptr;

      if (regnum == RISCV_FP_REGNUM
	  || regnum == RISCV_SP_REGNUM
	  || regnum == RISCV_GP_REGNUM
	  || regnum == RISCV_TP_REGNUM)
	return builtin_type (gdbarch)->builtin_data_ptr;

      /* Remaining GPRs vary in size based on the current ISA.  */
      regsize = riscv_isa_regsize (gdbarch);
      switch (regsize)
	{
	case 4:
	  return builtin_type (gdbarch)->builtin_uint32;
	case 8:
	  return builtin_type (gdbarch)->builtin_uint64;
	case 16:
	  return builtin_type (gdbarch)->builtin_uint128;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("unknown isa regsize %i"), regsize);
	}
    }
  else if (regnum <= RISCV_LAST_FP_REGNUM)
    {
      regsize = riscv_isa_regsize (gdbarch);
      switch (regsize)
	{
	case 4:
	  return builtin_type (gdbarch)->builtin_float;
	case 8:
	case 16:
	  return builtin_type (gdbarch)->builtin_double;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("unknown isa regsize %i"), regsize);
	}
    }
  else if (regnum == RISCV_PRIV_REGNUM)
    {
      return builtin_type (gdbarch)->builtin_int8;
    }
  else
    {
      if (regnum == RISCV_CSR_FFLAGS_REGNUM
	  || regnum == RISCV_CSR_FRM_REGNUM
	  || regnum == RISCV_CSR_FCSR_REGNUM)
	return builtin_type (gdbarch)->builtin_int32;

      regsize = riscv_isa_regsize (gdbarch);
      switch (regsize)
	{
	case 4:
	  return builtin_type (gdbarch)->builtin_int32;
	case 8:
	  return builtin_type (gdbarch)->builtin_int64;
	case 16:
	  return builtin_type (gdbarch)->builtin_int128;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("unknown isa regsize %i"), regsize);
	}
    }
}

static void
riscv_print_one_register_info (struct gdbarch *gdbarch,
			       struct ui_file *file,
			       struct frame_info *frame,
			       int regnum)
{
  const char *name = register_name (gdbarch, regnum, 1);
  struct value *val = value_of_register (regnum, frame);
  struct type *regtype = value_type (val);
  int print_raw_format;

  fputs_filtered (name, file);
  print_spaces_filtered (15 - strlen (name), file);

  print_raw_format = (value_entirely_available (val)
		      && !value_optimized_out (val));

  if (TYPE_CODE (regtype) == TYPE_CODE_FLT)
    {
      struct value_print_options opts;
      const gdb_byte *valaddr = value_contents_for_printing (val);
      enum bfd_endian byte_order = gdbarch_byte_order (get_type_arch (regtype));

      get_user_print_options (&opts);
      opts.deref_ref = 1;

      val_print (regtype,
		 value_embedded_offset (val), 0,
		 file, 0, val, &opts, current_language);

      if (print_raw_format)
	{
	  fprintf_filtered (file, "\t(raw ");
	  print_hex_chars (file, valaddr, TYPE_LENGTH (regtype), byte_order,
			   true);
	  fprintf_filtered (file, ")");
	}
    }
  else
    {
      struct value_print_options opts;

      /* Print the register in hex.  */
      get_formatted_print_options (&opts, 'x');
      opts.deref_ref = 1;
      val_print (regtype,
		 value_embedded_offset (val), 0,
		 file, 0, val, &opts, current_language);

      if (print_raw_format)
	{
	  if (regnum == RISCV_CSR_MSTATUS_REGNUM)
	    {
	      LONGEST d;
	      int size = register_size (gdbarch, regnum);
	      unsigned xlen;

	      d = value_as_long (val);
	      xlen = size * 4;
	      fprintf_filtered (file,
				"\tSD:%X VM:%02X MXR:%X PUM:%X MPRV:%X XS:%X "
				"FS:%X MPP:%x HPP:%X SPP:%X MPIE:%X HPIE:%X "
				"SPIE:%X UPIE:%X MIE:%X HIE:%X SIE:%X UIE:%X",
				(int) ((d >> (xlen - 1)) & 0x1),
				(int) ((d >> 24) & 0x1f),
				(int) ((d >> 19) & 0x1),
				(int) ((d >> 18) & 0x1),
				(int) ((d >> 17) & 0x1),
				(int) ((d >> 15) & 0x3),
				(int) ((d >> 13) & 0x3),
				(int) ((d >> 11) & 0x3),
				(int) ((d >> 9) & 0x3),
				(int) ((d >> 8) & 0x1),
				(int) ((d >> 7) & 0x1),
				(int) ((d >> 6) & 0x1),
				(int) ((d >> 5) & 0x1),
				(int) ((d >> 4) & 0x1),
				(int) ((d >> 3) & 0x1),
				(int) ((d >> 2) & 0x1),
				(int) ((d >> 1) & 0x1),
				(int) ((d >> 0) & 0x1));
	    }
	  else if (regnum == RISCV_CSR_MISA_REGNUM)
	    {
	      int base;
	      unsigned xlen, i;
	      LONGEST d;

	      d = value_as_long (val);
	      base = d >> 30;
	      xlen = 16;

	      for (; base > 0; base--)
		xlen *= 2;
	      fprintf_filtered (file, "\tRV%d", xlen);

	      for (i = 0; i < 26; i++)
		{
		  if (d & (1 << i))
		    fprintf_filtered (file, "%c", 'A' + i);
		}
	    }
	  else if (regnum == RISCV_CSR_FCSR_REGNUM
		   || regnum == RISCV_CSR_FFLAGS_REGNUM
		   || regnum == RISCV_CSR_FRM_REGNUM)
	    {
	      LONGEST d;

	      d = value_as_long (val);

	      fprintf_filtered (file, "\t");
	      if (regnum != RISCV_CSR_FRM_REGNUM)
		fprintf_filtered (file,
				  "RD:%01X NV:%d DZ:%d OF:%d UF:%d NX:%d",
				  (int) ((d >> 5) & 0x7),
				  (int) ((d >> 4) & 0x1),
				  (int) ((d >> 3) & 0x1),
				  (int) ((d >> 2) & 0x1),
				  (int) ((d >> 1) & 0x1),
				  (int) ((d >> 0) & 0x1));

	      if (regnum != RISCV_CSR_FFLAGS_REGNUM)
		{
		  static const char * const sfrm[] =
		    {
		     "RNE (round to nearest; ties to even)",
		     "RTZ (Round towards zero)",
		     "RDN (Round down towards -∞)",
		     "RUP (Round up towards +∞)",
		     "RMM (Round to nearest; tiest to max magnitude)",
		     "INVALID[5]",
		     "INVALID[6]",
		     "dynamic rounding mode",
		    };
		  int frm = ((regnum == RISCV_CSR_FCSR_REGNUM)
			     ? (d >> 5) : d) & 0x3;

		  fprintf_filtered (file, "%sFRM:%i [%s]",
				    (regnum == RISCV_CSR_FCSR_REGNUM
				     ? " " : ""),
				    frm, sfrm[frm]);
		}
	    }
	  else if (regnum == RISCV_PRIV_REGNUM)
	    {
	      LONGEST d;
	      uint8_t priv;

	      d = value_as_long (val);
	      priv = d & 0xff;

	      if (priv >= 0 && priv < 4)
		{
		  static const char * const sprv[] = {
						      "User/Application",
						      "Supervisor",
						      "Hypervisor",
						      "Machine"
		  };
		  fprintf_filtered (file, "\tprv:%d [%s]",
				    priv, sprv[priv]);
		}
	      else
		fprintf_filtered (file, "\tprv:%d [INVALID]", priv);
	    }
	  else
	    {
	      /* If not a vector register, print it also according to its
		 natural format.  */
	      if (TYPE_VECTOR (regtype) == 0)
		{
		  get_user_print_options (&opts);
		  opts.deref_ref = 1;
		  fprintf_filtered (file, "\t");
		  val_print (regtype,
			     value_embedded_offset (val), 0,
			     file, 0, val, &opts, current_language);
		}
	    }
	}
    }
  fprintf_filtered (file, "\n");
}

static bool
riscv_has_fp_hardware (struct gdbarch *gdbarch)
{
  if (cached_misa (nullptr) & ((1 << ('F'-'A'))
			       | (1 << ('D'-'A'))
			       | (1 << ('Q'-'A'))))
    return true;

  /* TODO: This isn't completely correct.  A machine might have FP
     hardware, but choose not to use the FP ABI.  We really should check
     the architecture flags in the ELF header.  */
  if (HAS_FPU (gdbarch_tdep (gdbarch)->riscv_abi))
    return true;

  return false;
}

/* Implement the register_reggroup_p gdbarch method.  */

static int
riscv_register_reggroup_p (struct gdbarch  *gdbarch,
			   int regnum,
			   struct reggroup *reggroup)
{
  int float_p;
  int raw_p;
  unsigned int i;

  /* Used by 'info registers' and 'info registers <groupname>'.  */

  if (gdbarch_register_name (gdbarch, regnum) == NULL
      || gdbarch_register_name (gdbarch, regnum)[0] == '\0')
    return 0;

  if (reggroup == all_reggroup)
    {
      if (regnum < RISCV_FIRST_CSR_REGNUM || regnum == RISCV_PRIV_REGNUM)
	return 1;
      /* Only include CSRs that have aliases.  */
      for (i = 0; i < ARRAY_SIZE (riscv_register_aliases); ++i)
	{
	  if (regnum == riscv_register_aliases[i].regnum)
	    return 1;
	}
      return 0;
    }
  else if (reggroup == float_reggroup)
    return ((regnum >= RISCV_FIRST_FP_REGNUM && regnum <= RISCV_LAST_FP_REGNUM)
	    || (regnum == RISCV_CSR_FCSR_REGNUM
		|| regnum == RISCV_CSR_FFLAGS_REGNUM
	        || regnum == RISCV_CSR_FRM_REGNUM));
  else if (reggroup == general_reggroup)
    return regnum < RISCV_FIRST_FP_REGNUM;
  else if (reggroup == restore_reggroup || reggroup == save_reggroup)
    {
      if (riscv_has_fp_hardware (gdbarch))
	return regnum <= RISCV_LAST_FP_REGNUM;
      else
	return regnum < RISCV_FIRST_FP_REGNUM;
    }
  else if (reggroup == system_reggroup)
    {
      if (regnum == RISCV_PRIV_REGNUM)
	return 1;
      if (regnum < RISCV_FIRST_CSR_REGNUM || regnum > RISCV_LAST_CSR_REGNUM)
	return 0;
      /* Only include CSRs that have aliases.  */
      for (i = 0; i < ARRAY_SIZE (riscv_register_aliases); ++i)
	{
	  if (regnum == riscv_register_aliases[i].regnum)
	    return 1;
	}
      return 0;
    }
  else if (reggroup == vector_reggroup)
    return 0;
  else
    internal_error (__FILE__, __LINE__, _("unhandled reggroup"));
}

/* Implement the print_registers_info gdbarch method.  This is used by
   'info registers' and 'info all-registers'.  */

static void
riscv_print_registers_info (struct gdbarch *gdbarch,
			    struct ui_file *file,
			    struct frame_info *frame,
			    int regnum, int print_all)
{
  if (regnum != -1)
    {
      /* Print one specified register.  */
      gdb_assert (regnum <= RISCV_LAST_REGNUM);
      if (gdbarch_register_name (gdbarch, regnum) == NULL
	  || *(gdbarch_register_name (gdbarch, regnum)) == '\0')
        error (_("Not a valid register for the current processor type"));
      riscv_print_one_register_info (gdbarch, file, frame, regnum);
    }
  else
    {
      struct reggroup *reggroup;

      if (print_all)
	reggroup = all_reggroup;
      else
	reggroup = general_reggroup;

      for (regnum = 0; regnum <= RISCV_LAST_REGNUM; ++regnum)
	{
	  /* Zero never changes, so might as well hide by default.  */
	  if (regnum == RISCV_ZERO_REGNUM && !print_all)
	    continue;

	  /* Registers with no name are not valid on this ISA.  */
	  if (gdbarch_register_name (gdbarch, regnum) == NULL
	      || *(gdbarch_register_name (gdbarch, regnum)) == '\0')
	    continue;

	  /* Is the register in the group we're interested in?  */
	  if (!riscv_register_reggroup_p (gdbarch, regnum, reggroup))
	    continue;

	  riscv_print_one_register_info (gdbarch, file, frame, regnum);
	}
    }
}

static ULONGEST
riscv_fetch_instruction (struct gdbarch *gdbarch, CORE_ADDR addr, int *len)
{
  enum bfd_endian byte_order = gdbarch_byte_order_for_code (gdbarch);
  gdb_byte buf[8];
  int instlen, status;

  /* All insns are at least 16 bits.  */
  status = target_read_memory (addr, buf, 2);
  if (status)
    memory_error (TARGET_XFER_E_IO, addr);

  /* If we need more, grab it now.  */
  instlen = riscv_insn_length (buf[0]);
  *len = instlen;
  if (instlen > sizeof (buf))
    internal_error (__FILE__, __LINE__, _("%s: riscv_insn_length returned %i"),
		    __func__, instlen);
  else if (instlen > 2)
    {
      status = target_read_memory (addr + 2, buf + 2, instlen - 2);
      if (status)
	memory_error (TARGET_XFER_E_IO, addr + 2);
    }

  return extract_unsigned_integer (buf, instlen, byte_order);
}

static void
set_reg_offset (struct gdbarch *gdbarch, struct riscv_frame_cache *this_cache,
		int regnum, CORE_ADDR offset)
{
  if (this_cache != NULL && this_cache->saved_regs[regnum].addr == -1)
    this_cache->saved_regs[regnum].addr = offset;
}

static void
reset_saved_regs (struct gdbarch *gdbarch, struct riscv_frame_cache *this_cache)
{
  const int num_regs = gdbarch_num_regs (gdbarch);
  int i;

  if (this_cache == NULL || this_cache->saved_regs == NULL)
    return;

  for (i = 0; i < num_regs; ++i)
    this_cache->saved_regs[i].addr = 0;
}

static int
riscv_decode_register_index (unsigned long opcode, int offset)
{
  return (opcode >> offset) & 0x1F;
}

/* TODO: Fix enum syntax for GDB.  */

enum riscv_insn_mnem
  {
   /* These instructions are all the ones we are interested in during the
      prologue scan.  */
   ADD,
   ADDI,
   ADDIW,
   ADDW,
   AUIPC,
   LUI,
   SD,
   SW,

   /* Other instructions are not interesting during the prologue scan, and
      are ignored.  */
   OTHER
  };

static const char *
riscv_opcode_to_string (enum riscv_insn_mnem opcode)
{
  switch (opcode)
    {
    case ADD:
      return "ADD";
    case ADDI:
      return "ADDI";
    case ADDIW:
      return "ADDIW";
    case ADDW:
      return "ADDW";
    case AUIPC:
      return "AUIPC";
    case LUI:
      return "LUI";
    case SD:
      return "SD";
    case SW:
      return "SW";
    case OTHER:
      return "OTHER";
    }

  abort ();
  return nullptr;
}

struct riscv_insn
{
  int length;

  enum riscv_insn_mnem opcode;

  int rd;
  int rs1;
  int rs2;

  union
  {
    int s;
    unsigned int u;
  } imm;
};

static void
riscv_decode_r_type_insn (enum riscv_insn_mnem opcode,
			  ULONGEST ival,
			  struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rd = riscv_decode_register_index (ival, 7);
  insn->rs1 = riscv_decode_register_index (ival, 15);
  insn->rs2 = riscv_decode_register_index (ival, 20);
}

static void
riscv_decode_cr_type_insn (enum riscv_insn_mnem opcode,
			   ULONGEST ival,
			   struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rd = insn->rs1 = riscv_decode_register_index (ival, 7);
  insn->rs2 = riscv_decode_register_index (ival, 2);
}

static void
riscv_decode_i_type_insn (enum riscv_insn_mnem opcode,
			  ULONGEST ival,
			  struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rd = riscv_decode_register_index (ival, 7);
  insn->rs1 = riscv_decode_register_index (ival, 15);
  insn->imm.s = EXTRACT_ITYPE_IMM (ival);
}

static void
riscv_decode_ci_type_insn (enum riscv_insn_mnem opcode,
			   ULONGEST ival,
			   struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rd = insn->rs1 = riscv_decode_register_index (ival, 7);
  insn->imm.s = EXTRACT_RVC_IMM (ival);
}

static void
riscv_decode_s_type_insn (enum riscv_insn_mnem opcode,
			  ULONGEST ival,
			  struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rs1 = riscv_decode_register_index (ival, 15);
  insn->rs2 = riscv_decode_register_index (ival, 20);
  insn->imm.s = EXTRACT_STYPE_IMM (ival);
}

static void
riscv_decode_u_type_insn (enum riscv_insn_mnem opcode,
			  ULONGEST ival,
			  struct riscv_insn *insn)
{
  insn->opcode = opcode;
  insn->rd = riscv_decode_register_index (ival, 7);
  insn->imm.s = EXTRACT_UTYPE_IMM (ival);
}

/* Fetch from target memory an instruction at PC and decode it into the
   INSN structure.  */

static void
riscv_decode_instruction (struct gdbarch *gdbarch,
			  CORE_ADDR pc,
			  struct riscv_insn *insn)
{
  ULONGEST ival;
  int len;

  /* Fetch the instruction, and the instructions length.  Compute the
     next pc we decode.  We don't support instructions longer than 4
     bytes yet.  */
  ival = riscv_fetch_instruction (gdbarch, pc, &len);
  insn->length = len;

  if (len == 4)
    {
      if (is_add_insn (ival))
	riscv_decode_r_type_insn (ADD, ival, insn);
      else if (is_addw_insn (ival))
	riscv_decode_r_type_insn (ADDW, ival, insn);
      else if (is_addi_insn (ival))
	riscv_decode_i_type_insn (ADDI, ival, insn);
      else if (is_addiw_insn (ival))
	riscv_decode_i_type_insn (ADDIW, ival, insn);
      else if (is_auipc_insn (ival))
	riscv_decode_u_type_insn (AUIPC, ival, insn);
      else if (is_lui_insn (ival))
	riscv_decode_u_type_insn (LUI, ival, insn);
      else if (is_sd_insn (ival))
	riscv_decode_s_type_insn (SD, ival, insn);
      else if (is_sw_insn (ival))
	riscv_decode_s_type_insn (SW, ival, insn);
      else
	/* None of the other fields of INSN are valid in this case.  */
	insn->opcode = OTHER;
    }
  else if (len == 2)
    {
      if (is_c_add_insn (ival))
	riscv_decode_cr_type_insn (ADD, ival, insn);
      else if (is_c_addw_insn (ival))
	riscv_decode_cr_type_insn (ADDW, ival, insn);
      else if (is_c_addi_insn (ival))
	riscv_decode_ci_type_insn (ADDI, ival, insn);
      else if (is_c_addiw_insn (ival))
	riscv_decode_ci_type_insn (ADDIW, ival, insn);
      else if (is_c_addi16sp_insn (ival))
	{
	  insn->opcode = ADDI;
	  insn->rd = insn->rs1 = riscv_decode_register_index (ival, 7);
	  insn->imm.s = EXTRACT_RVC_ADDI16SP_IMM (ival);
	}
      else if (is_lui_insn (ival))
	insn->opcode = OTHER;
      else if (is_c_sd_insn (ival))
	insn->opcode = OTHER;
      else if (is_sw_insn (ival))
	insn->opcode = OTHER;
      else
	/* None of the other fields of INSN are valid in this case.  */
	insn->opcode = OTHER;
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("unable to decode %d byte instructions in "
		      "prologue at %s"), len, core_addr_to_string (pc));
}

static CORE_ADDR
riscv_scan_prologue (struct gdbarch *gdbarch,
		     CORE_ADDR start_pc, CORE_ADDR limit_pc,
		     struct frame_info *this_frame,
		     struct riscv_frame_cache *this_cache)
{
  CORE_ADDR cur_pc, next_pc;
  CORE_ADDR frame_addr = 0;
  CORE_ADDR sp;
  long frame_offset;
  int frame_reg = RISCV_SP_REGNUM;

  CORE_ADDR end_prologue_addr = 0;
  int seen_sp_adjust = 0;
  int load_immediate_bytes = 0;

  /* Can be called when there's no process, and hence when there's no THIS_FRAME.  */
  if (this_frame != NULL)
    sp = get_frame_register_signed (this_frame, RISCV_SP_REGNUM);
  else
    sp = 0;

  if (limit_pc > start_pc + 200)
    limit_pc = start_pc + 200;

 restart:

  frame_offset = 0;
  /* TODO: Handle compressed extensions.  */
  for (next_pc = cur_pc = start_pc; cur_pc < limit_pc; cur_pc = next_pc)
    {
      struct riscv_insn insn;

      /* Decode the current instruction, and decide where the next
	 instruction lives based on the size of this instruction.  */
      riscv_decode_instruction (gdbarch, cur_pc, &insn);
      gdb_assert (insn.length > 0);
      next_pc = cur_pc + insn.length;

      if (getenv ("APB_PROLOGUE_DEBUG") != nullptr)
	fprintf (stderr, "APB: 0x%lx %s (%d)\n",
		 cur_pc, riscv_opcode_to_string (insn.opcode), insn.length);

      /* Look for common stack adjustment insns.  */
      if ((insn.opcode == ADDI || insn.opcode == ADDIW)
	  && insn.rd == RISCV_SP_REGNUM
	  && insn.rs1 == RISCV_SP_REGNUM)
	{
	  /* addi sp, sp, -i */
	  /* addiw sp, sp, -i */
	  if (insn.imm.s < 0)
	    frame_offset += insn.imm.s;
	  else
	    break;
	  seen_sp_adjust = 1;
	}
      else if ((insn.opcode == SW || insn.opcode == SD)
	       && (insn.rs1 == RISCV_SP_REGNUM
		   || insn.rs1 == RISCV_FP_REGNUM))
	{
	  /* sw reg, offset(sp)     OR
	     sd reg, offset(sp)     OR
	     sw reg, offset(s0)     OR
	     sd reg, offset(s0)  */
	  if (insn.rs1 == RISCV_SP_REGNUM)
	    set_reg_offset (gdbarch, this_cache, insn.rs1,
			    sp + insn.imm.s);
	  else
	    set_reg_offset (gdbarch, this_cache, insn.rs1,
			    frame_addr + insn.imm.s);
	}
      else if (insn.opcode == ADDI
	       && insn.rd == RISCV_FP_REGNUM
	       && insn.rs1 == RISCV_SP_REGNUM)
	{
	  /* addi s0, sp, size */
	  if ((long) insn.imm.s != frame_offset)
	    frame_addr = sp + insn.imm.s;
	}
      else if ((insn.opcode == ADD || insn.opcode == ADDW)
	       && insn.rd == RISCV_FP_REGNUM
	       && insn.rs1 == RISCV_SP_REGNUM
	       && RISCV_ZERO_REGNUM)
	{
	  /* add s0, sp, 0 */
	  /* addw s0, sp, 0 */
	  if (this_frame && frame_reg == RISCV_SP_REGNUM)
	    {
	      unsigned alloca_adjust;

	      frame_reg = RISCV_FP_REGNUM;
	      frame_addr
		= get_frame_register_signed (this_frame, RISCV_FP_REGNUM);

	      alloca_adjust = (unsigned) (frame_addr - sp);
	      if (alloca_adjust > 0)
		{
		  sp = frame_addr;
		  reset_saved_regs (gdbarch, this_cache);
		  goto restart;
		}
	    }
	}
      else if ((insn.rd == RISCV_GP_REGNUM
		&& (insn.opcode == AUIPC
		    || insn.opcode == LUI
		    || (insn.opcode == ADDI && insn.rs1 == RISCV_GP_REGNUM)
		    || (insn.opcode == ADD
			&& (insn.rs1 == RISCV_GP_REGNUM
			    || insn.rs2 == RISCV_GP_REGNUM))))
	       || (insn.opcode == ADDI
		   && insn.rd == RISCV_ZERO_REGNUM
		   && insn.rs1 == RISCV_ZERO_REGNUM
		   && insn.imm.s == 0))
	{
	  /* auipc gp, n */
	  /* addi gp, gp, n */
	  /* add gp, gp, reg */
	  /* add gp, reg, gp */
	  /* lui gp, n */
	  /* add x0, x0, 0   (NOP)   */
	  /* These instructions are part of the prologue, but we don't need to
	     do anything special to handle them.  */
	}
      else
	{
	  if (end_prologue_addr == 0)
	    end_prologue_addr = cur_pc;
	}
    }

  if (this_cache != NULL)
    {
      this_cache->base = (get_frame_register_signed (this_frame, frame_reg)
			  + frame_offset);
      this_cache->saved_regs[RISCV_PC_REGNUM]
	= this_cache->saved_regs[RISCV_RA_REGNUM];
    }

  if (end_prologue_addr == 0)
    end_prologue_addr = cur_pc;

  if (load_immediate_bytes && !seen_sp_adjust)
    end_prologue_addr -= load_immediate_bytes;

  return end_prologue_addr;
}

/* Implement the riscv_skip_prologue gdbarch method.  */

static CORE_ADDR
riscv_skip_prologue (struct gdbarch *gdbarch,
		     CORE_ADDR       pc)
{
  CORE_ADDR limit_pc;
  CORE_ADDR func_addr;

  if (getenv ("APB_NO_DWARF_PROLOGUE") == NULL)
    {
      /* See if we can determine the end of the prologue via the symbol
	 table.  If so, then return either PC, or the PC after the
	 prologue, whichever is greater.  */
      if (find_pc_partial_function (pc, NULL, &func_addr, NULL))
	{
	  CORE_ADDR post_prologue_pc
	    = skip_prologue_using_sal (gdbarch, func_addr);

	  if (post_prologue_pc != 0)
	    return std::max (pc, post_prologue_pc);
	}
    }

  /* Can't determine prologue from the symbol table, need to examine
     instructions.  */

  /* Find an upper limit on the function prologue using the debug
     information.  If the debug information could not be used to provide
     that bound, then use an arbitrary large number as the upper bound.  */
  limit_pc = skip_prologue_using_sal (gdbarch, pc);
  if (limit_pc == 0)
    limit_pc = pc + 100;   /* MAGIC! */

  return riscv_scan_prologue (gdbarch, pc, limit_pc, NULL, NULL);
}

static CORE_ADDR
riscv_push_dummy_code (struct gdbarch *gdbarch, CORE_ADDR sp, CORE_ADDR funaddr,
		       struct value **args, int nargs, struct type *value_type,
		       CORE_ADDR *real_pc, CORE_ADDR *bp_addr,
		       struct regcache *regcache)
{
  /* Allocate space for a breakpoint, and keep the stack correctly
     aligned.  */
  sp -= 16;
  *bp_addr = sp;
  *real_pc = funaddr;
  return sp;
}

/* Compute the alignment of the type T.  */

static int
riscv_type_alignment (struct type *t)
{
  t = check_typedef (t);
  switch (TYPE_CODE (t))
    {
    default:
      error (_("Could not compute alignment of type"));

    case TYPE_CODE_RVALUE_REF:
    case TYPE_CODE_PTR:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_INT:
    case TYPE_CODE_FLT:
    case TYPE_CODE_REF:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_BOOL:
      return TYPE_LENGTH (t);

    case TYPE_CODE_ARRAY:
    case TYPE_CODE_COMPLEX:
      return riscv_type_alignment (TYPE_TARGET_TYPE (t));

    case TYPE_CODE_STRUCT:
    case TYPE_CODE_UNION:
      {
	int i;
	int align = 1;

	for (i = 0; i < TYPE_NFIELDS (t); ++i)
	  {
	    if (TYPE_FIELD_LOC_KIND (t, i) == FIELD_LOC_KIND_BITPOS)
	      {
		int a = riscv_type_alignment (TYPE_FIELD_TYPE (t, i));
		if (a > align)
		  align = a;
	      }
	  }
	return align;
      }
    }
}

  struct argument_info
  {
    /* Contents of the argument.  */
    const gdb_byte *contents;

    /* Length of argument.  */
    int length;

    /* Alignment required, if on the stack.  */
    int align;

    /* The type for this argument.  */
    struct type *type;

    /* Each argument can have either 1 or 2 locations assigned to it.  Each
       location describes where part of the argument will be placed.  The
       second location is valid based on the LOC_TYPE and C_LENGTH fields
       of the first location (which is always valid).  */
    struct argument_location
    {
      /* What type of location this is.  */
      enum location_type
	{
	 /* Argument passed in a register.  */
	 in_reg,

	 /* Argument passed as an on stack argument.  */
	 on_stack,

	 /* Argument passed by reference.  The second location is always
	    valid for a BY_REF argument, and describes where the address
	    of the BY_REF argument should be placed.  */
	 by_ref
	} loc_type;

      /* Information that depends on the location type.  */
      union
      {
	/* Which register number to use.  */
	int regno;

	/* The offset into the stack region.  */
	int offset;
      } loc_data;

      /* The length of contents covered by this location.  If this is less
	 than the total length of the argument, then the second location
	 will be valid, and will describe where the rest of the argument
	 will go.  */
      int c_length;
    } argloc[2];
  };

  struct arg_reg
  {
    int next_regnum;
    int last_regnum;
  };


  /* Arguments can be passed as on stack arguments, or by reference.  The
     on stack arguments must be in a continuous region starting from $sp,
     while the by reference arguments can be anywhere, but we'll put them
     on the stack after (at higher address) the on stack arguments.

     This might not be the right approach to take.  The ABI is clear that
     an argument passed by reference can be modified by the callee, which
     us placing the argument (temporarily) onto the stack will not achieve
     (changes will be lost).  There's also the possibility that very large
     arguments could overflow the stack.

     This struct is used to track offset into these two areas for where
     arguments are to be placed.  */
  struct memory_offsets
  {
    /* Offset into on stack argument area.  */
    int arg_offset;

    /* Offset into the pass by reference area.  */
    int ref_offset;
  };

  struct abi_info
  {
    struct memory_offsets memory;

    struct arg_reg int_regs;
    struct arg_reg float_regs;

    int xlen;
    int flen;
  };

static int
riscv_arg_regs_available (struct arg_reg *reg)
{
  if (reg->next_regnum > reg->last_regnum)
    return 0;

  return (reg->last_regnum - reg->next_regnum + 1);
}

static bool
riscv_assign_reg_location (struct argument_info::argument_location *loc,
			   struct arg_reg *reg,
			   int length)
{
  if (reg->next_regnum <= reg->last_regnum)
    {
      loc->loc_type = argument_info::argument_location::in_reg;
      loc->loc_data.regno = reg->next_regnum;
      reg->next_regnum++;
      loc->c_length = length;
      return true;
    }

  return false;
}

static void
riscv_assign_stack_location (struct argument_info::argument_location *loc,
			     struct memory_offsets *memory,
			     int length, int align)
{
  loc->loc_type = argument_info::argument_location::on_stack;
  memory->arg_offset
    = align_up (memory->arg_offset, align);
  loc->loc_data.offset = memory->arg_offset;
  memory->arg_offset += length;
  loc->c_length = length;
}

static void
riscv_call_arg_scalar_int (struct argument_info *info,
			   struct abi_info *abi)
{
  if (info->length > (2 * abi->xlen))
    {
      /* Argument is going to be passed by reference.  */
      info->argloc[0].loc_type
	= argument_info::argument_location::by_ref;
      abi->memory.ref_offset
	= align_up (abi->memory.ref_offset, info->align);
      info->argloc[0].loc_data.offset = abi->memory.ref_offset;
      abi->memory.ref_offset += info->length;
      info->argloc[0].c_length = info->length;

      /* The second location for this argument is given over to holding the
	 address of the by-reference data.  */
      if (!riscv_assign_reg_location (&info->argloc[1],
				      &abi->int_regs, abi->xlen))
	riscv_assign_stack_location (&info->argloc[1],
				     &abi->memory, abi->xlen, abi->xlen);
    }
  else
    {
      int len = (info->length > abi->xlen) ? abi->xlen : info->length;

      if (!riscv_assign_reg_location (&info->argloc[0],
				      &abi->int_regs, len))
	riscv_assign_stack_location (&info->argloc[0],
				     &abi->memory, len, info->align);

      if (len < info->length)
	{
	  len = info->length - len;
	  if (!riscv_assign_reg_location (&info->argloc[1],
					  &abi->int_regs, len))
	    riscv_assign_stack_location (&info->argloc[1],
					 &abi->memory, len, abi->xlen);
	}
    }
}

static void
riscv_call_arg_scalar_float (struct argument_info *info,
			     struct abi_info *abi)
{
  if (info->length > abi->flen)
    return riscv_call_arg_scalar_int (info, abi);
  else
    {
      if (!riscv_assign_reg_location (&info->argloc[0],
				      &abi->float_regs, info->length))
	return riscv_call_arg_scalar_int (info, abi);
    }
}

static void
riscv_call_arg_complex_float (struct argument_info *info,
			      struct abi_info *abi)
{
  if (info->length <= (2 * abi->flen)
      && riscv_arg_regs_available (&abi->float_regs) >= 2)
    {
      bool result;
      int len = info->length / 2;

      result = riscv_assign_reg_location (&info->argloc[0],
					  &abi->float_regs, len);
      gdb_assert (result);

      result = riscv_assign_reg_location (&info->argloc[1],
					  &abi->float_regs, len);
      gdb_assert (result);
    }
  else
    return riscv_call_arg_scalar_int (info, abi);
}

struct xxx
{
  int number_of_fields;

  struct type *types[2];
};

static void
riscv_struct_analysis_for_call_1 (struct type *type,
				  struct xxx *xxx)
{
  unsigned int count = TYPE_NFIELDS (type);
  unsigned int i;

  for (i = 0; i < count; ++i)
    {
      if (TYPE_FIELD_LOC_KIND (type, i) != FIELD_LOC_KIND_BITPOS)
	continue;

      struct type *field_type = TYPE_FIELD_TYPE (type, i);
      field_type = check_typedef (field_type);

      switch (TYPE_CODE (field_type))
	{
	case TYPE_CODE_STRUCT:
	  riscv_struct_analysis_for_call_1 (field_type, xxx);
	  break;

	default:
	  if (xxx->number_of_fields < 2)
	    xxx->types[xxx->number_of_fields] = field_type;
	  xxx->number_of_fields++;
	  break;
	}

      if (xxx->number_of_fields > 2)
	return;
    }
}

static void
riscv_struct_analysis_for_call (struct type *type, struct xxx *xxx)
{
  xxx->number_of_fields = 0;
  xxx->types[0] = nullptr;
  xxx->types[1] = nullptr;
  riscv_struct_analysis_for_call_1 (type, xxx);
}

static void
riscv_call_arg_struct (struct argument_info *info,
		       struct abi_info *abi)
{
  if (riscv_arg_regs_available (&abi->float_regs) >= 1)
    {
      struct xxx xxx;

      riscv_struct_analysis_for_call (info->type, &xxx);

      if (xxx.number_of_fields == 1
	  && TYPE_CODE (xxx.types[0]) == TYPE_CODE_COMPLEX)
	{
	  gdb_assert (TYPE_LENGTH (info->type) == TYPE_LENGTH (xxx.types[0]));
	  return riscv_call_arg_complex_float (info, abi);
	}

      if (xxx.number_of_fields == 1
	  && TYPE_CODE (xxx.types[0]) == TYPE_CODE_FLT)
	{
	  gdb_assert (TYPE_LENGTH (info->type) == TYPE_LENGTH (xxx.types[0]));
	  return riscv_call_arg_scalar_float (info, abi);
	}

      if (xxx.number_of_fields == 2
	  && TYPE_CODE (xxx.types[0]) == TYPE_CODE_FLT
	  && TYPE_LENGTH (xxx.types[0]) <= abi->flen
	  && TYPE_CODE (xxx.types[1]) == TYPE_CODE_FLT
	  && TYPE_LENGTH (xxx.types[1]) <= abi->flen
	  && riscv_arg_regs_available (&abi->float_regs) >= 2)
	{
	  int len;

	  gdb_assert (TYPE_LENGTH (info->type) <= (2 * abi->flen));

	  len = TYPE_LENGTH (xxx.types[0]);
	  if (!riscv_assign_reg_location (&info->argloc[0],
					  &abi->float_regs, len))
	    error (_("failed during argument setup"));

	  len = TYPE_LENGTH (xxx.types[1]);
	  gdb_assert (len == (TYPE_LENGTH (info->type)
			      - TYPE_LENGTH (xxx.types[0])));

	  if (!riscv_assign_reg_location (&info->argloc[1],
					  &abi->float_regs, len))
	    error (_("failed during argument setup"));

	  return;
	}

      if (xxx.number_of_fields == 2
	  && riscv_arg_regs_available (&abi->int_regs) >= 1
	  && (TYPE_CODE (xxx.types[0]) == TYPE_CODE_FLT
	      && TYPE_LENGTH (xxx.types[0]) <= abi->flen
	      && is_integral_type (xxx.types[1])
	      && TYPE_LENGTH (xxx.types[1]) <= abi->xlen))
	{
	  int len;

	  gdb_assert (TYPE_LENGTH (info->type) <= (abi->flen + abi->xlen));

	  len = TYPE_LENGTH (xxx.types[0]);
	  if (!riscv_assign_reg_location (&info->argloc[0],
					  &abi->float_regs, len))
	    error (_("failed during argument setup"));

	  len = TYPE_LENGTH (info->type) - TYPE_LENGTH (xxx.types[0]);
	  gdb_assert (len <= abi->xlen);
	  if (!riscv_assign_reg_location (&info->argloc[1],
					  &abi->int_regs, len))
	    error (_("failed during argument setup"));

	  return;
	}

      if (xxx.number_of_fields == 2
	  && riscv_arg_regs_available (&abi->int_regs) >= 1
	  && (is_integral_type (xxx.types[0])
	      && TYPE_LENGTH (xxx.types[0]) <= abi->xlen
	      && TYPE_CODE (xxx.types[1]) == TYPE_CODE_FLT
	      && TYPE_LENGTH (xxx.types[1]) <= abi->flen))
	{
	  int len1, len2;

	  gdb_assert (TYPE_LENGTH (info->type) <= (abi->flen + abi->xlen));

	  len2 = TYPE_LENGTH (xxx.types[1]);
	  len1 = TYPE_LENGTH (info->type) - len2;

	  gdb_assert (len1 <= abi->xlen);
	  gdb_assert (len2 <= abi->flen);

	  if (!riscv_assign_reg_location (&info->argloc[0],
					  &abi->int_regs, len1))
	    error (_("failed during argument setup"));

	  if (!riscv_assign_reg_location (&info->argloc[1],
					  &abi->float_regs, len2))
	    error (_("failed during argument setup"));

	  return;
	}
    }

  /* Non of the structure flattening cases apply, so we just pass using
     the integer ABI.  */
  info->length = align_up (info->length, abi->xlen);
  riscv_call_arg_scalar_int (info, abi);
}


static void
riscv_arg_location (struct gdbarch *gdbarch,
		    struct argument_info *info,
		    struct abi_info *abi,
		    struct type *type)
{
  info->type = type;
  info->length = TYPE_LENGTH (info->type);
  info->align = riscv_type_alignment (info->type);
  info->contents = nullptr;

  switch (TYPE_CODE (info->type))
    {
    case TYPE_CODE_INT:
    case TYPE_CODE_BOOL:
    case TYPE_CODE_CHAR:
    case TYPE_CODE_RANGE:
    case TYPE_CODE_ENUM:
    case TYPE_CODE_PTR:
      if (info->length <= abi->xlen)
	{
	  info->type = builtin_type (gdbarch)->builtin_long;
	  info->length = abi->xlen;
	}
      else if (info->length <= (2 * abi->xlen))
	{
	  info->type = builtin_type (gdbarch)->builtin_long_long;
	  info->length = 2 * abi->xlen;
	}

      /* Recalculate the alignment requirement.  */
      info->align = riscv_type_alignment (info->type);
      riscv_call_arg_scalar_int (info, abi);
      break;

    case TYPE_CODE_FLT:
      riscv_call_arg_scalar_float (info, abi);
      break;

    case TYPE_CODE_COMPLEX:
      riscv_call_arg_complex_float (info, abi);
      break;

    case TYPE_CODE_STRUCT:
      riscv_call_arg_struct (info, abi);
      break;

    default:
      riscv_call_arg_scalar_int (info, abi);
      break;
    }
}

static void
riscv_print_arg_location (FILE *stream, struct gdbarch *gdbarch,
			  struct argument_info *info,
			  CORE_ADDR sp_refs, CORE_ADDR sp_args)
{
  fprintf (stream, "type: '%s', length: 0x%x, alignment: 0x%x",
	   TYPE_NAME (info->type), info->length, info->align);
  switch (info->argloc[0].loc_type)
    {
    case argument_info::argument_location::in_reg:
      fprintf (stream, ", register %s",
	       riscv_register_name (gdbarch, info->argloc[0].loc_data.regno));
      if (info->argloc[0].c_length < info->length)
	{
	  switch (info->argloc[1].loc_type)
	    {
	    case argument_info::argument_location::in_reg:
	      fprintf (stream, ", register %s",
		       riscv_register_name (gdbarch,
					    info->argloc[1].loc_data.regno));
	      break;

	    case argument_info::argument_location::on_stack:
	      fprintf (stream, ", on stack at offset 0x%x",
		       info->argloc[1].loc_data.offset);
	      break;

	    case argument_info::argument_location::by_ref:
	    default:
	      /* The second location should never be a reference, any
		 argument being passed by reference just places its address
		 in the first location and is done.  */
	      error (_("invalid argument location"));
	      break;
	    }
	}
      break;

    case argument_info::argument_location::on_stack:
      fprintf (stream, ", on stack at offset 0x%x",
	       info->argloc[0].loc_data.offset);
      break;

    case argument_info::argument_location::by_ref:
      fprintf (stream, ", by reference, data at offset 0x%x (0x%lx)",
	       info->argloc[0].loc_data.offset,
	       (sp_refs + info->argloc[0].loc_data.offset));
      if (info->argloc[1].loc_type
	  == argument_info::argument_location::in_reg)
	fprintf (stream, ", address in register %s",
		 riscv_register_name (gdbarch,
				      info->argloc[1].loc_data.regno));
      else
	{
	  gdb_assert (info->argloc[1].loc_type
		      == argument_info::argument_location::on_stack);
	  fprintf (stream, ", address on stack at offset 0x%x (0x%lx)",
		   info->argloc[1].loc_data.offset,
		   (sp_args + info->argloc[1].loc_data.offset));
	}
      break;

    default:
      error ("unknown argument location type");
    }
}

static void
riscv_init_abi_info (struct gdbarch *gdbarch,
		     struct abi_info *abi)
{
  abi->memory.arg_offset = 0;
  abi->memory.ref_offset = 0;
  abi->int_regs.next_regnum = RISCV_A0_REGNUM;
  abi->int_regs.last_regnum = RISCV_A0_REGNUM + 7;
  abi->float_regs.next_regnum = RISCV_FA0_REGNUM;
  abi->float_regs.last_regnum = RISCV_FA0_REGNUM + 7;
  abi->xlen = riscv_isa_regsize (gdbarch);
  abi->flen = riscv_isa_fregsize (gdbarch);

  /* Disable use of floating point registers if needed.  */
  if (!HAS_FPU (gdbarch_tdep (gdbarch)->riscv_abi))
    abi->float_regs.next_regnum = abi->float_regs.last_regnum + 1;
}

static CORE_ADDR
riscv_push_dummy_call (struct gdbarch *gdbarch,
		       struct value *function,
		       struct regcache *regcache,
		       CORE_ADDR bp_addr,
		       int nargs,
		       struct value **args,
		       CORE_ADDR sp,
		       int struct_return,
		       CORE_ADDR struct_addr)
{
  int riscv_debug = (getenv ("APB_DEBUG") != NULL);
  int i;
  CORE_ADDR sp_args, sp_refs;
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);

  struct argument_info *arg_info =
    (struct argument_info *) alloca (nargs * sizeof (struct argument_info));
  struct argument_info *info;

  struct abi_info abi_info;
  riscv_init_abi_info (gdbarch, &abi_info);

  CORE_ADDR osp = sp;

  /* We'll use register $a0 if we're returning a struct.  */
  if (struct_return)
    ++abi_info.int_regs.next_regnum;

  for (i = 0, info = &arg_info[0];
       i < nargs;
       ++i, ++info)
    {
      struct value *arg_value;
      struct type *arg_type;

      arg_value = args[i];
      arg_type = check_typedef (value_type (arg_value));

      riscv_arg_location (gdbarch, info, &abi_info, arg_type);

      if (info->type != arg_type)
	arg_value = value_cast (info->type, arg_value);
      info->contents = value_contents (arg_value);
    }

  /* Adjust the stack pointer and align it.  */
  sp = sp_refs = align_down (sp - abi_info.memory.ref_offset, SP_ALIGNMENT);
  sp = sp_args = align_down (sp - abi_info.memory.arg_offset, SP_ALIGNMENT);

  if (riscv_debug)
    {
      fprintf (stderr, "dummy call args:\n");
      fprintf (stderr, ": floating point ABI %s use\n",
	       ((HAS_FPU (gdbarch_tdep (gdbarch)->riscv_abi))
		? "is" : "is not"));
      fprintf (stderr, ": xlen: %d\n: flen: %d\n",
	       abi_info.xlen, abi_info.flen);
      if (struct_return)
	fprintf (stderr, "[*] struct return pointer in register $A0\n");
      for (i = 0; i < nargs; ++i)
	{
	  struct argument_info *info = &arg_info [i];

	  fprintf (stderr, "[%2d] ", i);
	  riscv_print_arg_location (stderr, gdbarch, info, sp_refs, sp_args);
	  fprintf (stderr, "\n");
	}
      if (abi_info.memory.arg_offset > 0
	  || abi_info.memory.ref_offset > 0)
	{
	  fprintf (stderr, "              Original sp: 0x%lx\n", osp);
	  fprintf (stderr, "Stack required (for args): 0x%x\n",
		   abi_info.memory.arg_offset);
	  fprintf (stderr, "Stack required (for refs): 0x%x\n",
		   abi_info.memory.ref_offset);
	  fprintf (stderr, "          Stack allocated: 0x%lx\n",
		   (osp - sp));
	}
    }

  /* Now load the argument into registers, or onto the stack.  */

  if (struct_return)
    {
      gdb_byte buf[sizeof (LONGEST)];

      store_unsigned_integer (buf, abi_info.xlen, byte_order, struct_addr);
      regcache_cooked_write (regcache, RISCV_A0_REGNUM, buf);
    }

  for (i = 0; i < nargs; ++i)
    {
      CORE_ADDR dst;
      int second_arg_length = 0;
      const gdb_byte *second_arg_data;
      struct argument_info *info = &arg_info [i];

      gdb_assert (info->length > 0);

      switch (info->argloc[0].loc_type)
	{
	case argument_info::argument_location::in_reg:
	  {
	    gdb_byte tmp [sizeof (ULONGEST)];

	    gdb_assert (info->argloc[0].c_length <= info->length);
	    memset (tmp, 0, sizeof (tmp));
	    memcpy (tmp, info->contents, info->argloc[0].c_length);
	    regcache_cooked_write (regcache,
				   info->argloc[0].loc_data.regno,
				   tmp);
	    second_arg_length = info->length - info->argloc[0].c_length;
	    second_arg_data = info->contents + info->argloc[0].c_length;
	  }
	  break;

	case argument_info::argument_location::on_stack:
	  dst = sp_args + info->argloc[0].loc_data.offset;
	  write_memory (dst, info->contents, info->length);
	  second_arg_length = 0;
	  break;

	case argument_info::argument_location::by_ref:
	  dst = sp_refs + info->argloc[0].loc_data.offset;
	  write_memory (dst, info->contents, info->length);

	  second_arg_length = abi_info.xlen;
	  second_arg_data = (gdb_byte *) &dst;
	  break;

	default:
	  error ("unknown argument location type");
	}

      if (second_arg_length > 0)
	{
	  switch (info->argloc[1].loc_type)
	    {
	    case argument_info::argument_location::in_reg:
	      {
		gdb_byte tmp [sizeof (ULONGEST)];

		gdb_assert (second_arg_length <= abi_info.xlen);
		memset (tmp, 0, sizeof (tmp));
		memcpy (tmp, second_arg_data, second_arg_length);
		regcache_cooked_write (regcache,
				       info->argloc[1].loc_data.regno,
				       tmp);
	      }
	      break;

	    case argument_info::argument_location::on_stack:
	      {
		CORE_ADDR arg_addr;

		arg_addr = sp_args + info->argloc[1].loc_data.offset;
		write_memory (arg_addr, second_arg_data, second_arg_length);
		break;
	      }

	    case argument_info::argument_location::by_ref:
	    default:
	      /* The second location should never be a reference, any
		 argument being passed by reference just places its address
		 in the first location and is done.  */
	      error (_("invalid argument location"));
	      break;
	    }
	}
    }

  /* Set the dummy return value to bp_addr.
     A dummy breakpoint will be setup to execute the call.  */

  if (riscv_debug)
    fprintf (stderr, "Writing $ra = 0x%lx\n", bp_addr);
  regcache_cooked_write_unsigned (regcache, RISCV_RA_REGNUM, bp_addr);

  /* Finally, update the stack pointer.  */

  if (riscv_debug)
    fprintf (stderr, "Writing $sp = 0x%lx\n", sp);
  regcache_cooked_write_unsigned (regcache, RISCV_SP_REGNUM, sp);

  return sp;
}

/* Implement the return_value gdbarch method.  */

static enum return_value_convention
riscv_return_value (struct gdbarch  *gdbarch,
		    struct value *function,
		    struct type *type,
		    struct regcache *regcache,
		    gdb_byte *readbuf,
		    const gdb_byte *writebuf)
{
  int riscv_debug = (getenv ("APB_DEBUG") != NULL);
  enum type_code rv_type = TYPE_CODE (type);
  unsigned int rv_size = TYPE_LENGTH (type);
  int fp, regnum, flen;
  ULONGEST tmp;
  struct abi_info abi_info;
  struct argument_info info;
  struct type *arg_type;

  if (riscv_debug)
    fprintf (stderr, "Entering: %s\n", __PRETTY_FUNCTION__);

  riscv_init_abi_info (gdbarch, &abi_info);
  arg_type = check_typedef (type);
  riscv_arg_location (gdbarch, &info, &abi_info, arg_type);

  if (riscv_debug)
    {
      fprintf (stderr, "[R] ");
      riscv_print_arg_location (stderr, gdbarch, &info, 0, 0);
      fprintf (stderr, "\n");
    }

  if (readbuf != nullptr || writebuf != nullptr)
    {
        int regnum;

	switch (info.argloc[0].loc_type)
	  {
	    /* Return value in register(s).  */
	  case argument_info::argument_location::in_reg:
	    regnum = info.argloc[0].loc_data.regno;

	    if (readbuf)
	      {
		regcache_cooked_read (regcache, regnum, readbuf);
		readbuf += info.argloc[0].c_length;
	      }

	    if (writebuf)
	      {
		regcache_cooked_write (regcache, regnum, writebuf);
		writebuf += info.argloc[0].c_length;
	      }

	    /* A return value in register can have a second part in a
	       second register.  */
	    if (info.argloc[0].c_length < info.length)
	      {
		switch (info.argloc[1].loc_type)
		  {
		  case argument_info::argument_location::in_reg:
		    regnum = info.argloc[1].loc_data.regno;

		    if (readbuf)
		      {
			regcache_cooked_read (regcache, regnum, readbuf);
			readbuf += info.argloc[1].c_length;
		      }

		    if (writebuf)
		      {
			regcache_cooked_write (regcache, regnum, writebuf);
			writebuf += info.argloc[1].c_length;
		      }
		    break;

		  case argument_info::argument_location::by_ref:
		  case argument_info::argument_location::on_stack:
		  default:
		    error (_("invalid argument location"));
		    break;
		  }
	      }
	    break;

	    /* Return value by reference will have its address in A0.  */
	  case argument_info::argument_location::by_ref:
	    {
	      CORE_ADDR addr;

	      regcache_cooked_read_unsigned (regcache, RISCV_A0_REGNUM,
					     &addr);
	      if (readbuf != nullptr)
		read_memory (addr, readbuf, info.length);
	      if (writebuf != nullptr)
		write_memory (addr, writebuf, info.length);
	    }
	    break;

	  case argument_info::argument_location::on_stack:
	  default:
	    error (_("invalid argument location"));
	    break;
	  }
    }

  switch (info.argloc[0].loc_type)
    {
    case argument_info::argument_location::in_reg:
      return RETURN_VALUE_REGISTER_CONVENTION;
    case argument_info::argument_location::by_ref:
      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    case argument_info::argument_location::on_stack:
    default:
      error (_("invalid argument location"));
    }
}

/* Implement the frame_align gdbarch method.  */

static CORE_ADDR
riscv_frame_align (struct gdbarch *gdbarch, CORE_ADDR addr)
{
  return align_down (addr, 16);
}

/* Implement the unwind_pc gdbarch method.  */

static CORE_ADDR
riscv_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, RISCV_PC_REGNUM);
}

/* Implement the unwind_sp gdbarch method.  */

static CORE_ADDR
riscv_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, RISCV_SP_REGNUM);
}

/* Implement the dummy_id gdbarch method.  */

static struct frame_id
riscv_dummy_id (struct gdbarch *gdbarch, struct frame_info *this_frame)
{
  return frame_id_build (get_frame_register_signed (this_frame, RISCV_SP_REGNUM),
			 get_frame_pc (this_frame));
}

static struct trad_frame_cache *
riscv_frame_cache (struct frame_info *this_frame, void **this_cache)
{
  CORE_ADDR pc;
  CORE_ADDR start_addr;
  CORE_ADDR stack_addr;
  struct trad_frame_cache *this_trad_cache;
  struct gdbarch *gdbarch = get_frame_arch (this_frame);

  if ((*this_cache) != NULL)
    return (struct trad_frame_cache *) *this_cache;
  this_trad_cache = trad_frame_cache_zalloc (this_frame);
  (*this_cache) = this_trad_cache;

  trad_frame_set_reg_realreg (this_trad_cache, gdbarch_pc_regnum (gdbarch),
			      RISCV_RA_REGNUM);

  pc = get_frame_pc (this_frame);
  find_pc_partial_function (pc, NULL, &start_addr, NULL);
  stack_addr = get_frame_register_signed (this_frame, RISCV_SP_REGNUM);
  trad_frame_set_id (this_trad_cache, frame_id_build (stack_addr, start_addr));

  trad_frame_set_this_base (this_trad_cache, stack_addr);

  return this_trad_cache;
}

static void
riscv_frame_this_id (struct frame_info *this_frame,
		     void              **prologue_cache,
		     struct frame_id   *this_id)
{
  struct trad_frame_cache *info = riscv_frame_cache (this_frame, prologue_cache);
  trad_frame_get_id (info, this_id);
}

static struct value *
riscv_frame_prev_register (struct frame_info *this_frame,
			   void              **prologue_cache,
			   int                regnum)
{
  struct trad_frame_cache *info = riscv_frame_cache (this_frame, prologue_cache);
  return trad_frame_get_register (info, this_frame, regnum);
}

static const struct frame_unwind riscv_frame_unwind =
{
  /*.type          =*/ NORMAL_FRAME,
  /*.stop_reason   =*/ default_frame_unwind_stop_reason,
  /*.this_id       =*/ riscv_frame_this_id,
  /*.prev_register =*/ riscv_frame_prev_register,
  /*.unwind_data   =*/ NULL,
  /*.sniffer       =*/ default_frame_sniffer,
  /*.dealloc_cache =*/ NULL,
  /*.prev_arch     =*/ NULL,
};

static struct gdbarch *
riscv_gdbarch_init (struct gdbarch_info info,
		    struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;
  const struct bfd_arch_info *binfo = info.bfd_arch_info;
  enum auto_boolean supports_compressed_isa = AUTO_BOOLEAN_AUTO;

  int abi, i;

  /* For now, base the abi on the elf class.  */
  /* Allow the ELF class to override the register size. Ideally the target
   * (OpenOCD/spike/...) would communicate the register size to gdb instead. */
  abi = RISCV_ABI_FLAG_RV32I;
  if (info.abfd && bfd_get_flavour (info.abfd) == bfd_target_elf_flavour)
    {
      unsigned char eclass = elf_elfheader (info.abfd)->e_ident[EI_CLASS];
      int e_flags = elf_elfheader (info.abfd)->e_flags;

      if (eclass == ELFCLASS32)
	abi = RISCV_ABI_FLAG_RV32I;
      else if (eclass == ELFCLASS64)
	abi = RISCV_ABI_FLAG_RV64I;
      else
        internal_error (__FILE__, __LINE__,
			_("unknown ELF header class %d"), eclass);

      if (e_flags & EF_RISCV_RVC)
	supports_compressed_isa = AUTO_BOOLEAN_TRUE;

      if (e_flags & EF_RISCV_FLOAT_ABI_DOUBLE)
	abi |= RISCV_ABI_FLAG_D;

      if (e_flags & EF_RISCV_FLOAT_ABI_SINGLE)
	abi |= RISCV_ABI_FLAG_F;
    }
  else
    {
      if (binfo->bits_per_word == 32)
        abi = RISCV_ABI_FLAG_RV32I;
      else if (binfo->bits_per_word == 64)
        abi = RISCV_ABI_FLAG_RV64I;
      else
        internal_error (__FILE__, __LINE__, _("unknown bits_per_word %d"),
            binfo->bits_per_word);
    }

  /* Find a candidate among the list of pre-declared architectures.  */
  for (arches = gdbarch_list_lookup_by_info (arches, &info);
       arches != NULL;
       arches = gdbarch_list_lookup_by_info (arches->next, &info))
    if (gdbarch_tdep (arches->gdbarch)->riscv_abi == abi)
      return arches->gdbarch;

  /* None found, so create a new architecture from the information provided.
     Can't initialize all the target dependencies until we actually know which
     target we are talking to, but put in some defaults for now.  */

  tdep = (struct gdbarch_tdep *) xmalloc (sizeof *tdep);
  gdbarch = gdbarch_alloc (&info, tdep);

  tdep->riscv_abi = abi;
  tdep->supports_compressed_isa = supports_compressed_isa;

  /* Target data types.  */
  set_gdbarch_short_bit (gdbarch, 16);
  set_gdbarch_int_bit (gdbarch, 32);
  set_gdbarch_long_bit (gdbarch, riscv_isa_regsize (gdbarch) * 8);
  set_gdbarch_long_long_bit (gdbarch, 64);
  set_gdbarch_float_bit (gdbarch, 32);
  set_gdbarch_double_bit (gdbarch, 64);
  set_gdbarch_long_double_bit (gdbarch, 128);
  set_gdbarch_long_double_format (gdbarch, floatformats_ia64_quad);
  set_gdbarch_ptr_bit (gdbarch, riscv_isa_regsize (gdbarch) * 8);
  set_gdbarch_char_signed (gdbarch, 0);

  /* Information about the target architecture.  */
  set_gdbarch_return_value (gdbarch, riscv_return_value);
  set_gdbarch_breakpoint_kind_from_pc (gdbarch, riscv_breakpoint_kind_from_pc);
  set_gdbarch_sw_breakpoint_from_kind (gdbarch, riscv_sw_breakpoint_from_kind);

  /* Register architecture.  */
  set_gdbarch_pseudo_register_read (gdbarch, riscv_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, riscv_pseudo_register_write);
  set_gdbarch_num_regs (gdbarch, RISCV_NUM_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, RISCV_NUM_REGS);
  set_gdbarch_sp_regnum (gdbarch, RISCV_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, RISCV_PC_REGNUM);
  set_gdbarch_ps_regnum (gdbarch, RISCV_FP_REGNUM);
  set_gdbarch_deprecated_fp_regnum (gdbarch, RISCV_FP_REGNUM);

  /* Functions to supply register information.  */
  set_gdbarch_register_name (gdbarch, riscv_register_name);
  set_gdbarch_register_type (gdbarch, riscv_register_type);
  set_gdbarch_print_registers_info (gdbarch, riscv_print_registers_info);
  set_gdbarch_register_reggroup_p (gdbarch, riscv_register_reggroup_p);

  /* Functions to analyze frames.  */
  set_gdbarch_decr_pc_after_break (gdbarch,
				   (supports_compressed_isa
				    == AUTO_BOOLEAN_TRUE ? 2 : 4));
  set_gdbarch_skip_prologue (gdbarch, riscv_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);
  set_gdbarch_frame_align (gdbarch, riscv_frame_align);

  /* Functions to access frame data.  */
  set_gdbarch_unwind_pc (gdbarch, riscv_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, riscv_unwind_sp);

  /* Functions handling dummy frames.  */
  set_gdbarch_call_dummy_location (gdbarch, ON_STACK);
  set_gdbarch_push_dummy_code (gdbarch, riscv_push_dummy_code);
  set_gdbarch_push_dummy_call (gdbarch, riscv_push_dummy_call);
  set_gdbarch_dummy_id (gdbarch, riscv_dummy_id);

  /* Frame unwinders.  Use DWARF debug info if available, otherwise use our own
     unwinder.  */
  dwarf2_append_unwinders (gdbarch);
  frame_unwind_append_unwinder (gdbarch, &riscv_frame_unwind);

  /* Check any target description for validity.  */
  if (tdesc_has_registers (info.target_desc))
    {
      const struct tdesc_feature *feature;
      struct tdesc_arch_data *tdesc_data;
      int valid_p;

      feature = tdesc_find_feature (info.target_desc, "org.gnu.gdb.riscv.cpu");
      if (feature == NULL)
	goto no_tdata;

      tdesc_data = tdesc_data_alloc ();

      valid_p = 1;
      for (i = RISCV_ZERO_REGNUM; i <= RISCV_LAST_FP_REGNUM; ++i)
        valid_p &= tdesc_numbered_register (feature, tdesc_data, i,
                                            riscv_gdb_reg_names[i]);
      for (i = RISCV_FIRST_CSR_REGNUM; i <= RISCV_LAST_CSR_REGNUM; ++i)
        {
          char buf[20];
          sprintf (buf, "csr%d", i - RISCV_FIRST_CSR_REGNUM);
          valid_p &= tdesc_numbered_register (feature, tdesc_data, i, buf);
        }

      valid_p &= tdesc_numbered_register (feature, tdesc_data, i++, "priv");

      if (!valid_p)
	tdesc_data_cleanup (tdesc_data);
      else
	tdesc_use_registers (gdbarch, info.target_desc, tdesc_data);
    }
 no_tdata:

  for (i = 0; i < ARRAY_SIZE (riscv_register_aliases); ++i)
    user_reg_add (gdbarch, riscv_register_aliases[i].name,
		  value_of_riscv_user_reg, &riscv_register_aliases[i].regnum);

  return gdbarch;
}

extern initialize_file_ftype _initialize_riscv_tdep; /* -Wmissing-prototypes */

void
_initialize_riscv_tdep (void)
{
  gdbarch_register (bfd_arch_riscv, riscv_gdbarch_init, NULL);

  /* Add root prefix command for all "set riscv"/"show riscv" commands.  */
  add_prefix_cmd ("riscv", no_class, set_riscv_command,
      _("RISC-V specific commands."),
      &setriscvcmdlist, "set riscv ", 0, &setlist);

  add_prefix_cmd ("riscv", no_class, show_riscv_command,
      _("RISC-V specific commands."),
      &showriscvcmdlist, "show riscv ", 0, &showlist);

  use_compressed_breakpoints = AUTO_BOOLEAN_AUTO;
  add_setshow_auto_boolean_cmd ("use_compressed_breakpoints", no_class,
      &use_compressed_breakpoints,
      _("Configure whether to use compressed breakpoints."),
      _("Show whether to use compressed breakpoints."),
      _("\
Debugging compressed code requires compressed breakpoints to be used. If left\n\
to 'auto' then gdb will use them if $misa indicates the C extension is\n\
supported. If that doesn't give the correct behavior, then this option can be\n\
used."),
      NULL,
      NULL,
      &setriscvcmdlist,
      &showriscvcmdlist);
}
