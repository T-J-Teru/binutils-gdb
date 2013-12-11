/* Target-dependent code for the MRK3 CPU, for GDB. 

   Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
   2006, 2007, 2008, 2009, 2010 Free Software Foundation, Inc.
   Copyright (C) 2013 NXP Semiconductors Austria GmbH

   Contributor Jeremy Bennett <jeremy.bennett@embecosm.com>
   
   This file is part of GDB.
   
   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 3 of the License, or (at your option)
   any later version.
   
   This program is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.
   
   You should have received a copy of the GNU General Public License along
   with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "frame.h"
#include "frame-unwind.h"
#include "frame-base.h"
#include "trad-frame.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "gdbtypes.h"
#include "inferior.h"
#include "symfile.h"
#include "arch-utils.h"
#include "regcache.h"
#include "dis-asm.h"
#include "ui-file.h"

/*  Required for symbol switch handling. */
#include "gdb/defs.h"
#include "gdb/gdbcore.h"
#include "gdb/symfile.h"
#include "gdb/objfiles.h"
#include "gdb/gdb_obstack.h"
#include "gdb/progspace.h"
#include "gdb/breakpoint.h"

/*  Mrk3 specific headers. */
#include "p40/gdb_types.h"
#include "p40/gdb_mem_map.h"
#include "p40/gdb_registers.h"
#include "p40/debug.h"
#include "p40/P40_DLL.h"

#undef XMALLOC
#define XMALLOC(TYPE) ((TYPE*) xmalloc (sizeof (TYPE)))

/* Useful register numbers - CPU registers */
#define R0_REGNUM     0
#define PC_REGNUM     7
#define PSW_REGNUM    8
#define SSP_REGNUM    9
#define USP_REGNUM   10
#define R4E_REGNUM   11
#define R5E_REGNUM   12
#define R6E_REGNUM   13

/* Useful register numbers - SFRs
   TODO: For now we don't show the SFRs */
#define SFR_START   (R6E_REGNUM + 1)

/* Useful register numbers - pseudo registers */
#define PSEUDO_START (SFR_START)
#define SP_REGNUM       (PSEUDO_START +  0)
#define R0L_REGNUM      (PSEUDO_START +  1)
#define R1L_REGNUM      (PSEUDO_START +  2)
#define R2L_REGNUM      (PSEUDO_START +  3)
#define R3L_REGNUM      (PSEUDO_START +  4)
#define R0H_REGNUM      (PSEUDO_START +  5)
#define R1H_REGNUM      (PSEUDO_START +  6)
#define R2H_REGNUM      (PSEUDO_START +  7)
#define R3H_REGNUM      (PSEUDO_START +  8)
#define R4L_REGNUM      (PSEUDO_START +  9)
#define R5L_REGNUM      (PSEUDO_START + 10)
#define R6L_REGNUM      (PSEUDO_START + 11)
#define SYS_REGNUM      (PSEUDO_START + 12)
#define INT_REGNUM      (PSEUDO_START + 13)
#define ZERO_REGNUM     (PSEUDO_START + 14)
#define NEG_REGNUM      (PSEUDO_START + 15)
#define OVERFLOW_REGNUM (PSEUDO_START + 16)
#define CARRY_REGNUM    (PSEUDO_START + 17)
#define PSEUDO_END (CARRY_REGNUM)

/* TODO. These should be done through XML */
#define NUM_CPU_REGS    (SFR_START)
#define NUM_SFRS        (PSEUDO_START - SFR_START)
#define NUM_REAL_REGS   (NUM_CPU_REGS + NUM_SFRS)
#define NUM_PSEUDO_REGS (PSEUDO_END - PSEUDO_START + 1)
#define NUM_REGS        (NUM_REAL_REGS + NUM_PSEUDO_REGS)


/*  Define the breakpoint instruction which is inserted by */
/*  GDB into the target code. This must be exactly the same */
/*  as the simulator expects. */
/*  Per definition, a breakpoint instruction has 16 bits. */
/*  This should be sufficient for all purposes. */
static const uint16_t mrk3_sim_break_insn = 0xFFFF;

/* Structure describing architecture specific types. */
struct gdbarch_tdep
{
  /* Number of bytes stored to the stack by call instructions. */
  int call_length;

  /* Type for void.  */
  struct type *void_type;
  /* Type for a function returning void.  */
  struct type *func_void_type;
  /* Type for a pointer to a function.  Used for the type of PC.  */
  struct type *pc_type;
};


/* Wrapper around memcpy to make it legal argument to ui_file_put */
static void
mrk3_ui_memcpy (void *dest, const char *buffer, long len)
{
  memcpy (dest, buffer, (size_t) len);
  ((char *) dest)[len] = '\0';
}


/* Get the current memory space from the target.

   TODO: Is RCmd the best way to do this? */
static uint8_t
mrk3_get_mem_space ()
{
  /* TODO: We can't tell if we have a valid target function here, because it
     is set to a value static within target.c (tcomplain). So we'll need to
     look at whether we have we have a valid value. A shame because we'll get
     an error message. */
  struct ui_file *mf = mem_fileopen ();
  char buf[64];
  target_rcmd ("getMemSpace", mf);
  ui_file_put (mf, mrk3_ui_memcpy, buf);
  /* Result is in mf->stream->buffer, of length mf->stream->length_buffer */
  if (strlen (buf) == 0)
    {
      /* TODO: We are presumably not connected to a target. Should we warn? Or
         should we return a default? */
      warning (_
	       ("Using default memory space since not connected to target."));
      return 0;			/* Arbitrary default */
    }
  else
    {
      return (uint8_t) strtol (buf, NULL, 16);
    }
}


/* Lookup the name of a register given it's number. */
static const char *
mrk3_register_name (struct gdbarch *gdbarch, int regnum)
{
  static char *regnames[NUM_REGS] = {
    /*  CPU Registers */
    "R0",
    "R1",
    "R2",
    "R3",
    "R4",
    "R5",
    "R6",
    "PC",
    "PSW",
    "SSP",
    "USP",
    "R4e",
    "R5e",
    "R6e",

    /* Special Function Registers.
       TODO: This should be done through XML description. */

    /* pseudo registers: */
    "SP",
    "R0L",
    "R1L",
    "R2L",
    "R3L",
    "R0H",
    "R1H",
    "R2H",
    "R3H",
    "R4l",
    "R5l",
    "R6l",
    "SYS",
    "INT",
    "ZERO",
    "NEG",
    "OVERFLOW",
    "CARRY"
  };

  if (regnum < NUM_REGS)
    return regnames[regnum];
  else
    {
      /* Moan */
      fprintf_unfiltered (gdb_stdlog,
			  "mrk3_register_name: unknown register number %d.\n",
			  regnum);
      return "";
    }
}


/* Return the GDB type object for the "standard" data type
   of data in register N.

   TODO. This should be done in XML. */
static struct type *
mrk3_register_type (struct gdbarch *gdbarch, int regnum)
{
  switch (regnum)
    {
      /* CPU registers */
    case (R0_REGNUM + 0):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 1):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 2):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 3):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 4):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 5):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0_REGNUM + 6):
      return builtin_type (gdbarch)->builtin_uint16;
    case (PC_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (PSW_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (SSP_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (USP_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R4E_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R5E_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R6E_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;

      /* Special Function Registers  - TODO through XML */

      /* Pseudo registers */
    case (SP_REGNUM):
      return builtin_type (gdbarch)->builtin_uint16;
    case (R0L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R1L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R2L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R3L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R0H_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R1H_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R2H_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R3H_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (R4L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint32;
    case (R5L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint32;
    case (R6L_REGNUM):
      return builtin_type (gdbarch)->builtin_uint32;
    case (SYS_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (INT_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (ZERO_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (NEG_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (OVERFLOW_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    case (CARRY_REGNUM):
      return builtin_type (gdbarch)->builtin_uint8;
    default:
      /*  Moan */
      fprintf_unfiltered (gdb_stdlog,
			  "mrk3_register_type: unknown register number %d.\n",
			  regnum);
      return builtin_type (gdbarch)->builtin_int0;
    };
}


static void
mrk3_pseudo_register_read (struct gdbarch *gdbarch,
			   struct regcache *regcache,
			   int cooked_regnum, gdb_byte * buf)
{
  gdb_byte raw_buf[8];
  int raw_regnum;

  /*  R0L is based on R0 */
  switch (cooked_regnum)
    {
    case SP_REGNUM:
      raw_regnum = (mrk3_get_mem_space () == 1) ? SSP_REGNUM : USP_REGNUM;
      regcache_raw_read (regcache, raw_regnum, buf);
      return;

    case R0L_REGNUM:
    case R1L_REGNUM:
    case R2L_REGNUM:
    case R3L_REGNUM:
      raw_regnum = cooked_regnum - R0L_REGNUM + R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf + 1, 1);
      else
	memcpy (buf, raw_buf, 1);
      return;

    case R0H_REGNUM:
    case R1H_REGNUM:
    case R2H_REGNUM:
    case R3H_REGNUM:
      raw_regnum = cooked_regnum - R0H_REGNUM + R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf, 1);
      else
	memcpy (buf, raw_buf + 1, 1);
      return;

    case R4L_REGNUM:
    case R5L_REGNUM:
    case R6L_REGNUM:
      /*  LO reg */
      raw_regnum = cooked_regnum - R4L_REGNUM + R0_REGNUM + 4;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf + 2, 2);
      else
	memcpy (buf, raw_buf, 2);
      /*  HI reg */
      raw_regnum = cooked_regnum - R4L_REGNUM + R4E_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf, 2);
      else
	memcpy (buf, raw_buf + 2, 2);
      return;

    case SYS_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[0] & 0x80) >> 7;
      else
	buf[0] = (raw_buf[1] & 0x80) >> 7;
      return;

    case INT_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[0] & 0x78) >> 3;
      else
	buf[0] = (raw_buf[1] & 0x78) >> 3;
      return;

    case ZERO_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 3;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 3;
      return;

    case NEG_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 2;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 2;
      return;

    case OVERFLOW_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 1;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 1;
      return;

    case CARRY_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 0;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 0;
      return;

    default:
      /*  Moan */
      fprintf_unfiltered (gdb_stdlog,
			  "mrk3_pseudo_register_read: Not a pseudo reg %d.\n",
			  cooked_regnum);
      return;
    }
}


static void
mrk3_pseudo_register_write (struct gdbarch *gdbarch,
			    struct regcache *regcache,
			    int cooked_regnum, const gdb_byte * buf)
{
  gdb_byte raw_buf[8];
  int raw_regnum;

  /*  R0L is based on R0 */
  switch (cooked_regnum)
    {
    case SP_REGNUM:
      raw_regnum = (mrk3_get_mem_space () == 1) ? SSP_REGNUM : USP_REGNUM;
      regcache_raw_write (regcache, raw_regnum, buf);
      return;

    case R0L_REGNUM:
    case R1L_REGNUM:
    case R2L_REGNUM:
    case R3L_REGNUM:
      raw_regnum = cooked_regnum - R0L_REGNUM + R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf + 1, buf, 1);
      else
	memcpy (raw_buf, buf, 1);
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case R0H_REGNUM:
    case R1H_REGNUM:
    case R2H_REGNUM:
    case R3H_REGNUM:
      raw_regnum = cooked_regnum - R0H_REGNUM + R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 1);
      else
	memcpy (raw_buf + 1, buf, 1);
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case R4L_REGNUM:
    case R5L_REGNUM:
    case R6L_REGNUM:
      /*  LO reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf + 2, buf, 2);
      else
	memcpy (raw_buf, buf, 2);
      raw_regnum = cooked_regnum - R4L_REGNUM + R0_REGNUM + 4;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      /*  HI reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 2);
      else
	memcpy (raw_buf + 2, buf, 2);
      raw_regnum = cooked_regnum - R4L_REGNUM + R4E_REGNUM;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case SYS_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[0] &= 0x7f;
	  raw_buf[0] |= (buf[0] & 0x01) << 7;
	}
      else
	{
	  raw_buf[1] &= 0x7f;
	  raw_buf[1] |= (buf[0] & 0x01) << 7;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case INT_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[0] &= 0x87;
	  raw_buf[0] |= (buf[0] & 0x0f) << 3;
	}
      else
	{
	  raw_buf[1] &= 0x87;
	  raw_buf[1] |= (buf[0] & 0x0f) << 3;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case ZERO_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xf7;
	  raw_buf[1] |= (buf[0] & 0x08) << 3;
	}
      else
	{
	  raw_buf[0] &= 0xf7;
	  raw_buf[0] |= (buf[0] & 0x08) << 3;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case NEG_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfb;
	  raw_buf[1] |= (buf[0] & 0x08) << 2;
	}
      else
	{
	  raw_buf[0] &= 0xfb;
	  raw_buf[0] |= (buf[0] & 0x08) << 2;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case OVERFLOW_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfd;
	  raw_buf[1] |= (buf[0] & 0x08) << 1;
	}
      else
	{
	  raw_buf[0] &= 0xfd;
	  raw_buf[0] |= (buf[0] & 0x08) << 1;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case CARRY_REGNUM:
      raw_regnum = PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfe;
	  raw_buf[1] |= (buf[0] & 0x08) << 0;
	}
      else
	{
	  raw_buf[0] &= 0xfe;
	  raw_buf[0] |= (buf[0] & 0x08) << 0;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    default:
      /*  Moan */
      fprintf_unfiltered (gdb_stdlog,
			  "mrk3_pseudo_register_write: Not a pseudo reg %d.\n",
			  cooked_regnum);
      return;
    }
}


static int
mrk3_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int dwarf2_regnr)
{
  /*  According to Target the following algorithm is used to determine */
  /*  dwarf2 locations: */
  /*    1. take the storages from file lib/isg/mrk3_regs.txt and sort */
  /*       alphabetically, first registers then memories */
  /*    2. number the locations in the resulting list */
  /*  */
  /*  The following values are taken from a list from Target: */
  uint32_t regnr;
  switch (dwarf2_regnr)
    {
    case 0:
      regnr = 0;
      break;			/*  AUSP (?) */
    case 1:
      regnr = mrk3_sim_reg_pc;
      break;			/*  PC */
    case 2:
      regnr = mrk3_sim_reg_pc;
      break;			/*  PC16 */
    case 3:
      regnr = mrk3_sim_reg_pc;
      break;			/*  PCh */
    case 4:
      regnr = mrk3_sim_reg_psw;
      break;			/*  PSW */
      /*  case  5:                                      // PU (?) */

    case 6:
      regnr = mrk3_sim_reg_r0;
      break;			/*  R0 */
    case 7:
      regnr = mrk3_sim_reg_r1;
      break;			/*  R1 */
    case 8:
      regnr = mrk3_sim_reg_r2;
      break;			/*  R2 */
    case 9:
      regnr = mrk3_sim_reg_r3;
      break;			/*  R3 */

      /*  R46 is a virtual 48 bit register */
      /*  having R4, R5 and R6 as real registers. */
    case 10:
      regnr = mrk3_sim_reg_r4;
      break;			/*  R46[0] */
    case 11:
      regnr = mrk3_sim_reg_r5;
      break;			/*  R46[1] */
    case 12:
      regnr = mrk3_sim_reg_r6;
      break;			/*  R46[2] */

      /*  R46e is a virtual 48 bit register */
      /*  having R4e, R5e and R6e as real registers. */
    case 13:
      regnr = mrk3_sim_pseudo_reg_r4e;
      break;			/*  R46e[0] */
    case 14:
      regnr = mrk3_sim_pseudo_reg_r5e;
      break;			/*  R46e[1] */
    case 15:
      regnr = mrk3_sim_pseudo_reg_r6e;
      break;			/*  R46e[2] */

      /*  R46L is a virtual 96 bit register */
      /*  having R4l, R5l and R6l as real registers. */
    case 16:
      regnr = mrk3_sim_pseudo_reg_r4l;
      break;			/*  R46e[0] */
    case 17:
      regnr = mrk3_sim_pseudo_reg_r5l;
      break;			/*  R46e[1] */
    case 18:
      regnr = mrk3_sim_pseudo_reg_r6l;
      break;			/*  R46e[2] */

    case 19:
      regnr = mrk3_sim_reg_sp;
      break;			/*  R7 */

      /*  case 20:                                      // RO (?) */

      /*  Rb is a virtual register, consisting of all */
      /*  byte registers. */
    case 21:
      regnr = mrk3_sim_pseudo_reg_r0l;
      break;			/*  Rb[0] */
    case 22:
      regnr = mrk3_sim_pseudo_reg_r1l;
      break;			/*  Rb[1] */
    case 23:
      regnr = mrk3_sim_pseudo_reg_r2l;
      break;			/*  Rb[2] */
    case 24:
      regnr = mrk3_sim_pseudo_reg_r3l;
      break;			/*  Rb[3] */
    case 25:
      regnr = mrk3_sim_pseudo_reg_r0h;
      break;			/*  Rb[4] */
    case 26:
      regnr = mrk3_sim_pseudo_reg_r1h;
      break;			/*  Rb[5] */
    case 27:
      regnr = mrk3_sim_pseudo_reg_r2h;
      break;			/*  Rb[6] */
    case 28:
      regnr = mrk3_sim_pseudo_reg_r3h;
      break;			/*  Rb[7] */

      /*  RbH is a virtual register, consisting of all */
      /*  high byte registers. */
    case 29:
      regnr = mrk3_sim_pseudo_reg_r0h;
      break;			/*  RbH[0] */
    case 30:
      regnr = mrk3_sim_pseudo_reg_r1h;
      break;			/*  RbH[1] */
    case 31:
      regnr = mrk3_sim_pseudo_reg_r2h;
      break;			/*  RbH[2] */
    case 32:
      regnr = mrk3_sim_pseudo_reg_r2h;
      break;			/*  RbH[3] */

      /*  RbL is a virtual register, consisting of all */
      /*  high byte registers. */
    case 33:
      regnr = mrk3_sim_pseudo_reg_r0l;
      break;			/*  RbL[0] */
    case 34:
      regnr = mrk3_sim_pseudo_reg_r1l;
      break;			/*  RbL[1] */
    case 35:
      regnr = mrk3_sim_pseudo_reg_r2l;
      break;			/*  RbL[2] */
    case 36:
      regnr = mrk3_sim_pseudo_reg_r2l;
      break;			/*  RbL[3] */

      /*  Rw is a virtual register, consisting of all */
      /*  word registers. */
    case 37:
      regnr = mrk3_sim_reg_r0;
      break;			/*  Rw[0] */
    case 38:
      regnr = mrk3_sim_reg_r1;
      break;			/*  Rw[1] */
    case 39:
      regnr = mrk3_sim_reg_r2;
      break;			/*  Rw[2] */
    case 40:
      regnr = mrk3_sim_reg_r3;
      break;			/*  Rw[3] */
    case 41:
      regnr = mrk3_sim_reg_r4;
      break;			/*  Rw[4] */
    case 42:
      regnr = mrk3_sim_reg_r5;
      break;			/*  Rw[5] */
    case 43:
      regnr = mrk3_sim_reg_r6;
      break;			/*  Rw[6] */
    case 44:
      regnr = mrk3_sim_pseudo_reg_r4e;
      break;			/*  Rw[7] */
    case 45:
      regnr = mrk3_sim_pseudo_reg_r5e;
      break;			/*  Rw[8] */
    case 46:
      regnr = mrk3_sim_pseudo_reg_r6e;
      break;			/*  Rw[9] */

      /*  RwL is probably a virtual register consisting */
      /*  of all word registers. */
    case 47:
      regnr = mrk3_sim_reg_r0;
      break;			/*  RwL[0] */
    case 48:
      regnr = mrk3_sim_reg_r1;
      break;			/*  RwL[1] */
    case 49:
      regnr = mrk3_sim_reg_r2;
      break;			/*  RwL[2] */
    case 50:
      regnr = mrk3_sim_reg_r3;
      break;			/*  RwL[3] */

    case 51:
      regnr = mrk3_sim_pseudo_reg_c_flag;
      break;			/*  c flag */
    case 52:
      regnr = mrk3_sim_pseudo_reg_i_lvl;
      break;			/*  interrupt level */
    case 53:
      regnr = mrk3_sim_pseudo_reg_n_flag;
      break;			/*  n flag */
    case 54:
      regnr = mrk3_sim_pseudo_reg_z_flag;
      break;			/*  z flag (actually nz (?)) */
    case 55:
      regnr = mrk3_sim_pseudo_reg_o_flag;
      break;			/*  o flag */
    case 56:
      regnr = 0;
      break;			/*  res (?) */
    case 57:
      regnr = mrk3_sim_pseudo_reg_s_flag;
      break;			/*  s flag */
    case 58:
      regnr = 0;
      break;			/*  tLSB (?) */
    case 59:
      regnr = mrk3_sim_pseudo_reg_z_flag;
      break;			/*  z flag */

    default:
      printf
	("mrk3_dwarf2_reg_to_regnum: Warning: unknown drwarf2 register number: %d\n",
	 dwarf2_regnr);
      regnr = mrk3_sim_reg_r0;
    }

  /* printf("mrk3-tdep.c:mrk3_dwarf2_reg_to_regnum gdbarch->num_regs=%d dwarf2_regnr(%d) maps to (%d)\n",gdbarch_num_regs(gdbarch),dwarf2_regnr,regnr); */
  return regnr;

  /* return Dll_Dwarf2RegToRegnum(dwarf2_regnr); */
}

uint32_t
mrk3_map_dwarf2_data_addr (const ULONGEST dwarf2_addr)
{
#define TARGET_DWARF_OFFSET_DM     0x00000003CULL
#define TARGET_DWARF_OFFSET_DM12   0x00001003CULL
#define TARGET_DWARF_OFFSET_DM12w  0x00001103CULL
#define TARGET_DWARF_OFFSET_DM32   0x00001183CULL
#define TARGET_DWARF_OFFSET_DM32w  0x10001183CULL
#define TARGET_DWARF_OFFSET_DM9    0x18001183CULL
#define TARGET_DWARF_OFFSET_DM9w   0x180011A3CULL
#define TARGET_DWARF_OFFSET_DMw    0x180011C92ULL
#define TARGET_DWARF_OFFSET_PM     0x180019C92ULL
#define TARGET_DWARF_OFFSET_PM1    0x180029C92ULL
#define TARGET_DWARF_OFFSET_ULP    0x180031C92ULL
  /*  Sorted offsets from low addr to high addr! */
  static const ULONGEST mem_offsets[] = {
    TARGET_DWARF_OFFSET_DM,
    TARGET_DWARF_OFFSET_DM12,
    TARGET_DWARF_OFFSET_DM12w,
    TARGET_DWARF_OFFSET_DM32,
    TARGET_DWARF_OFFSET_DM32w,
    TARGET_DWARF_OFFSET_DM9,
    TARGET_DWARF_OFFSET_DM9w,
    TARGET_DWARF_OFFSET_DMw,
    TARGET_DWARF_OFFSET_PM,
    TARGET_DWARF_OFFSET_PM1,
    TARGET_DWARF_OFFSET_ULP
  };
  static const size_t n_offsets =
    sizeof (mem_offsets) / sizeof (mem_offsets[0]);

  int n;
  for (n = n_offsets - 1; n >= 0; n--)
    {
      if (dwarf2_addr >= mem_offsets[n])
	{
	  uint32_t addr = (uint32_t) (dwarf2_addr - mem_offsets[n]);
	  /* printf("mrk3-tdep.c:mrk3_map_dwarf2_data_addr dwarf2_addr=0x%llX minus mem_offset[%d]=0x%llX is addr=0x%lX\n",dwarf2_addr,n,mem_offsets[n],addr);                      */
	  return addr;
	}
    }

  return (uint32_t) dwarf2_addr;

  /* return Dll_Dwarf2AddrToAddr(dwarf2_addr); */
}


/*  Save the full and base filename to an objfile info struct. */
void
mrk3_save_objfile_name (struct mrk3_objfile_info_t *of_info,
			const char *filename)
{
  if (filename != NULL)
    {
      char *base_name = basename (filename);
      mrk3_free_objfile_info (of_info);

      of_info->full_name = malloc (strlen (filename) + 1);
      strcpy (of_info->full_name, filename);

      of_info->name = malloc (strlen (base_name) + 1);
      strcpy (of_info->name, base_name);
    }
}


/*  Free an objfile_info structure. */
void
mrk3_free_objfile_info (struct mrk3_objfile_info_t *of_info)
{
  if (of_info->name != NULL)
    {
      /*  Remove the old symbol file. */
      free (of_info->name);
      of_info->name = NULL;
    }
  if (of_info->full_name != NULL)
    {
      /*  Remove the old symbol file. */
      free (of_info->full_name);
      of_info->full_name = NULL;
    }
}


#if 0
void
mrk3_load_symbol_info (const uint32_t mem_space)
{
  uint8_t mem_space_index = mrk3_mem_space_index (mem_space);
  uint8_t n = 0;
  struct objfile *of = object_files;
  while (of != NULL)
    {
      char *base_name = basename (of->name);
      if ((mrk3_objfile_info[mem_space_index].name != NULL) &&
	  (strcmp (base_name, mrk3_objfile_info[mem_space_index].name) == 0))
	{
	  objfile_to_front (of);
	  break;
	}
      of = of->next;
    }
}
#endif

#if 0
/*  When looking up symbols, switch priorities such that the */
/*  symbol file for the given memory space is preferred. */
uint32_t
mrk3_get_memspace_from_objfile_name (const char *base_name)
{
  uint8_t n = 0;
  if (base_name != NULL)
    {
      for (n = 0; n < MRK3_MAX_OBJFILES; n++)
	{
	  if (mrk3_objfile_info[n].name != NULL)
	    {
	      if (strcmp (base_name, mrk3_objfile_info[n].name) == 0)
		{
		  return mrk3_mem_space_from_mem_space_index (n);
		}
	    }
	}
      printf
	("mrk3_get_memspace_from_objfile_name: Warning: No memspace found for objectfile \"%s\".\n",
	 base_name);
      return 0;
    }
  printf
    ("mrk3_get_memspace_from_objfile_name: Error: No objectfile name specified.\n");
  return 0;
}
#endif


static const gdb_byte *
mrk3_breakpoint_from_pc (struct gdbarch *gdbarch,
			 CORE_ADDR * pcptr, int *lenptr)
{
  CORE_ADDR addr = mrk3_pc_to_address (*pcptr);

  /*  always use full addresses for breakpoints. */
  if (mrk3_is_map_addr (addr))
    {
      addr = mrk3_to_mem_space (addr, mrk3_get_mem_space ());
    }

  *pcptr = addr;
  *lenptr = sizeof (mrk3_sim_break_insn);
  return (gdb_byte *) & mrk3_sim_break_insn;
}

/* This is obsolete */
#if 0
static int
mrk3_memory_insert_breakpoint (struct gdbarch *gdbarch,
			       struct bp_target_info *bp_tgt)
{
  mrk3_breakpoint_from_pc (gdbarch, &(bp_tgt->placed_address),
			   &(bp_tgt->placed_size));
  Dll_InsertBreakpoint (bp_tgt->placed_address);
  return 0;
}

static int
mrk3_memory_remove_breakpoint (struct gdbarch *gdbarch,
			       struct bp_target_info *bp_tgt)
{
  Dll_RemoveBreakpoint (bp_tgt->placed_address);
  return 0;
}
#endif

static CORE_ADDR
mrk3_pointer_to_address (struct gdbarch *gdbarch,
			 struct type *type, const gdb_byte * buf)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  CORE_ADDR addr = extract_unsigned_integer (buf, TYPE_LENGTH (type),
					     byte_order);
  switch (TYPE_INSTANCE_FLAGS (type))
    {
    case TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1:
      addr &= 0xffff;
      break;

    default:
      break;
    }

  return addr;
}


static void
mrk3_address_to_pointer (struct gdbarch *gdbarch,
			 struct type *type, gdb_byte * buf, CORE_ADDR addr)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  switch (TYPE_INSTANCE_FLAGS (type))
    {
    case TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1:
      store_unsigned_integer (buf, TYPE_LENGTH (type), byte_order,
			      addr & 0x0000FFFF);
      break;

    default:
      store_unsigned_integer (buf, TYPE_LENGTH (type), byte_order, addr);
      break;
    }
}


static void
mrk3_register_to_value (struct frame_info *frame, int regnum,
			struct type *type, gdb_byte * buf)
{
  /* printf("mrk3-tdep.c:mrk3_register_to_value frame=%p, regnum=%d, type=%p(length=%d)\n",frame,regnum,type,type->length);         */
  frame_unwind_register (frame, regnum, buf);
}


static CORE_ADDR
mrk3_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  /*  TODO: See alpha-tdep.c :: alpha_after_prologue */

  /*  Actually do not skip anything. */
  return pc;
}

extern bool enable_system_mode_stack_traces;


static void
mrk3_analyze_prologue (struct gdbarch *arch,
		       CORE_ADDR start, CORE_ADDR limit,
		       struct frame_info *this_frame,
		       struct trad_frame_cache *frame_cache)
{
  uint32_t pc;
  uint16_t sp;
  uint16_t ssp;
  uint16_t usp;
  uint16_t psw;
  uint16_t frame_base;
  bool write_to_frame_cache;

  /* Initialize. */
  if (this_frame != NULL)
    {
      pc = 0;
      sp = frame_unwind_register_unsigned (this_frame, mrk3_sim_reg_sp);
      ssp = frame_unwind_register_unsigned (this_frame, mrk3_sim_reg_ssp);
      usp = frame_unwind_register_unsigned (this_frame, mrk3_sim_reg_usp);
      psw = frame_unwind_register_unsigned (this_frame, mrk3_sim_reg_psw);
    }
  else
    {
      pc = 0;
      psw = 0x8000;
      sp = 0;
      ssp = 0;
      usp = 0;
      /* The mrk3 starts in system mode, so assume system mode here. */
      frame_base = sp;
    }

  /* For now miss out the target prologue analysis. */
#if 0
  Dll_GetPrologueAnalysis (start, limit, &pc, &psw, &ssp, &usp, &frame_base);
#endif

  /* After calling Dll_GetPrologueAnalysis, the parameters are actually
     the values of the outer frame (caller). We need to store them in
     the frame cache.

     We need to check whether the caller was executed in SYS or USR
     contect. Based on this info, the caller sp is either ssp or usp. */
  write_to_frame_cache = true;
  if (mrk3_is_sys_addr (pc))
    {
      sp = ssp;
      /* This is simulator specific. */
#if 0
      write_to_frame_cache = enable_system_mode_stack_traces;
#endif
    }
  else
    {
      sp = usp;
    }

  /* Do write the stack info to the frame cache. */
    if (write_to_frame_cache)
    {
      /* Save to frame cache.
         printf("---DEBUG--- saving to frame cache: pc 0x%x, psw: 0x%x, "
         "sp: 0x%x, ssp: 0x%x, usp: 0x%x, frame_base: 0x%x\n", 
         pc, psw, sp, ssp, usp, frame_base); */
      trad_frame_set_this_base (frame_cache, frame_base);
      trad_frame_set_reg_value (frame_cache, mrk3_sim_reg_pc, pc);
      trad_frame_set_reg_value (frame_cache, mrk3_sim_reg_psw, psw);
      trad_frame_set_reg_value (frame_cache, mrk3_sim_reg_sp, sp);
      trad_frame_set_reg_value (frame_cache, mrk3_sim_reg_ssp, ssp);
      trad_frame_set_reg_value (frame_cache, mrk3_sim_reg_usp, usp);
    }
  else
    {
      /* return an invalid frame. */
      trad_frame_set_id (frame_cache, null_frame_id);
    }
}


static struct trad_frame_cache *
mrk3_analyze_frame_prologue (struct frame_info *this_frame,
			     void **this_prologue_cache)
{
  if (!*this_prologue_cache)
    {
      CORE_ADDR func_start;
      CORE_ADDR stop_addr;

      *this_prologue_cache = trad_frame_cache_zalloc (this_frame);
      func_start = get_frame_func (this_frame);
      stop_addr = get_frame_pc (this_frame);


      /* If we couldn't find any function containing the PC, then
         just initialize the prologue cache, but don't do anything.  */
      if (!func_start)
	stop_addr = func_start;

      mrk3_analyze_prologue (get_frame_arch (this_frame),
			     func_start, stop_addr, this_frame,
			     *this_prologue_cache);
    }

  return *this_prologue_cache;
}

static CORE_ADDR
mrk3_frame_base_address (struct frame_info *this_frame,
			 void **this_prologue_cache)
{
  struct trad_frame_cache *frame_cache
    = mrk3_analyze_frame_prologue (this_frame, this_prologue_cache);
  return trad_frame_get_this_base (frame_cache);
}


static void
mrk3_frame_this_id (struct frame_info *this_frame,
		    void **this_prologue_cache, struct frame_id *this_id)
{
  CORE_ADDR base = mrk3_frame_base_address (this_frame, this_prologue_cache);
  if (base)
    {
      *this_id = frame_id_build (base, get_frame_func (this_frame));
    }
  /* Otherwise, leave it unset, and that will terminate the backtrace.  */
}


static struct value *
mrk3_frame_prev_register (struct frame_info *this_frame,
			  void **this_prologue_cache, int regnum)
{
  struct trad_frame_cache *frame_cache = *this_prologue_cache;
  if (frame_cache == NULL)
    {
      return frame_unwind_got_constant (this_frame, regnum, 0);
    }
  else
    {
      return trad_frame_get_register (frame_cache, this_frame, regnum);
    }
}


static const struct frame_unwind mrk3_frame_unwind = {
  NORMAL_FRAME,
  mrk3_frame_this_id,
  mrk3_frame_prev_register,
  NULL,
  default_frame_sniffer
};


static const struct frame_base mrk3_frame_base = {
  &mrk3_frame_unwind,
  mrk3_frame_base_address,
  mrk3_frame_base_address,
  mrk3_frame_base_address
};


static CORE_ADDR
mrk3_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;
  pc = frame_unwind_register_unsigned (next_frame, mrk3_sim_reg_pc);
  return gdbarch_addr_bits_remove (gdbarch, pc);
}

static CORE_ADDR
mrk3_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;
  sp = frame_unwind_register_unsigned (next_frame, mrk3_sim_reg_sp);
  return sp & 0xFFFF;
}

int
mrk3_convert_register_p (struct gdbarch *gdbarch, int regnum,
			 struct type *type)
{
  return mrk3_register_type (gdbarch, regnum)->length != type->length;
}

CORE_ADDR
mrk3_address_bits_remove (struct gdbarch * gdbarch, CORE_ADDR addr)
{
  return addr & 0x0FFFFFFF;
}

int
mrk3_address_class_type_flags (int byte_size, int dwarf2_addr_class)
{
  if (byte_size == 2)
    return TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1;
  else
    return 0;
}

/* This appears not to be used for now. */
#if 0
static char *
mrk3_address_class_type_flags_to_name (int type_flags)
{
  if (type_flags & TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1)
    return "short";
  else
    return 0;
}
#endif

int
mrk3_address_class_name_to_type_flags (char *name, int *type_flags_ptr)
{
  if (strcmp (name, "short") == 0)
    {
      *type_flags_ptr = TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1;
      return 1;
    }
  else
    return 0;
}



/* Initialize the gdbarch structure for the mrk3. */
static struct gdbarch *
mrk3_gdbarch_init (struct gdbarch_info info, struct gdbarch_list *arches)
{
  struct gdbarch *gdbarch;
  struct gdbarch_tdep *tdep;

  /* This is a horrible temporary kludge to deal with the problem that
     the Target compiler generates a big-endian ELF file for a
     little-endian architecture. */
  info.byte_order = BFD_ENDIAN_LITTLE;

  /* Check to see if we've already built an appropriate architecture
     object for this executable.  */
  arches = gdbarch_list_lookup_by_info (arches, &info);
  if (arches)
    return arches->gdbarch;

  /* Create a new architecture from the information provided. */
  tdep = XMALLOC (struct gdbarch_tdep);
  gdbarch = gdbarch_alloc (&info, tdep);	/*  TODO: Can we use null for tdep? We do never use tdep anyways. */

  /* set_gdbarch_addr_bits_remove(gdbarch,mrk3_address_bits_remove); */

  set_gdbarch_address_class_type_flags (gdbarch,
					mrk3_address_class_type_flags);
  set_gdbarch_short_bit (gdbarch, 1 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 4 * TARGET_CHAR_BIT);

  set_gdbarch_float_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_double_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_double_bit (gdbarch, 2 * TARGET_CHAR_BIT);

  set_gdbarch_float_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_double_format (gdbarch, floatformats_ieee_single);
  set_gdbarch_long_double_format (gdbarch, floatformats_ieee_single);

/*
	set_gdbarch_read_pc (gdbarch, mrk3_read_pc);
	set_gdbarch_write_pc (gdbarch, mrk3_write_pc);
*/
  set_gdbarch_num_regs (gdbarch, NUM_REAL_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, NUM_PSEUDO_REGS);

  set_gdbarch_sp_regnum (gdbarch, SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, PC_REGNUM);

  set_gdbarch_register_name (gdbarch, mrk3_register_name);
  set_gdbarch_register_type (gdbarch, mrk3_register_type);

  set_gdbarch_pseudo_register_read (gdbarch, mrk3_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, mrk3_pseudo_register_write);

/* TODO: reading the stack and address to pointer conversion is not supported atm. */
/*   set_gdbarch_return_value (gdbarch, mrk3_return_value); */
  set_gdbarch_print_insn (gdbarch, print_insn_mrk3);

/*   set_gdbarch_push_dummy_call (gdbarch, dummy_push_dummy_call); */
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mrk3_dwarf2_reg_to_regnum);

  set_gdbarch_address_to_pointer (gdbarch, mrk3_address_to_pointer);
  set_gdbarch_pointer_to_address (gdbarch, mrk3_pointer_to_address);
  /*  IMPORTANT - We need to be able to convert register contents to different length */
  /*  gdb default will use 1:1 which is false in case we have a 16 bit register and need 32 bit values */
  set_gdbarch_convert_register_p (gdbarch, mrk3_convert_register_p);
  set_gdbarch_register_to_value (gdbarch, mrk3_register_to_value);
  set_gdbarch_skip_prologue (gdbarch, mrk3_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

#if 0
  set_gdbarch_memory_insert_breakpoint (gdbarch,
					mrk3_memory_insert_breakpoint);
  set_gdbarch_memory_remove_breakpoint (gdbarch,
					mrk3_memory_remove_breakpoint);
#endif
/*       set_gdbarch_decr_pc_after_break (gdbarch, mrk3_sim_break_insn_pc_decrease); */
  set_gdbarch_breakpoint_from_pc (gdbarch, mrk3_breakpoint_from_pc);

  frame_unwind_append_unwinder (gdbarch, &mrk3_frame_unwind);
  frame_base_set_default (gdbarch, &mrk3_frame_base);

/*   set_gdbarch_dummy_id (gdbarch, avr_dummy_id); */

  set_gdbarch_unwind_pc (gdbarch, mrk3_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mrk3_unwind_sp);

  return gdbarch;
}

extern initialize_file_ftype _initialize_mrk3_tdep;	/* -Wmissing-prototypes */

void
_initialize_mrk3_tdep (void)
{
  register_gdbarch_init (bfd_arch_mrk3, mrk3_gdbarch_init);
}
