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
/* #include "p40/gdb_types.h" */
/* #include "p40/gdb_mem_map.h" */
/* #include "p40/gdb_registers.h" */
/* #include "p40/debug.h" */
/* #include "p40/P40_DLL.h" */

/* Useful register numbers - CPU registers */
#define MRK3_R0_REGNUM     0
#define MRK3_PC_REGNUM     7
#define MRK3_PSW_REGNUM    8
#define MRK3_SSSP_REGNUM   9
#define MRK3_SSP_REGNUM   10
#define MRK3_USP_REGNUM   11
#define MRK3_R4E_REGNUM   12
#define MRK3_R5E_REGNUM   13
#define MRK3_R6E_REGNUM   14

/* Useful register numbers - SFRs
   TODO: For now we don't show the SFRs */
#define SFR_START   (MRK3_R6E_REGNUM + 1)

/* Useful register numbers - pseudo registers */
#define PSEUDO_START (SFR_START)
#define MRK3_SP_REGNUM       (PSEUDO_START +  0)
#define MRK3_R0L_REGNUM      (PSEUDO_START +  1)
#define MRK3_R1L_REGNUM      (PSEUDO_START +  2)
#define MRK3_R2L_REGNUM      (PSEUDO_START +  3)
#define MRK3_R3L_REGNUM      (PSEUDO_START +  4)
#define MRK3_R0H_REGNUM      (PSEUDO_START +  5)
#define MRK3_R1H_REGNUM      (PSEUDO_START +  6)
#define MRK3_R2H_REGNUM      (PSEUDO_START +  7)
#define MRK3_R3H_REGNUM      (PSEUDO_START +  8)
#define MRK3_R4L_REGNUM      (PSEUDO_START +  9)
#define MRK3_R5L_REGNUM      (PSEUDO_START + 10)
#define MRK3_R6L_REGNUM      (PSEUDO_START + 11)
#define MRK3_SYS_REGNUM      (PSEUDO_START + 12)
#define MRK3_INT_REGNUM      (PSEUDO_START + 13)
#define MRK3_ZERO_REGNUM     (PSEUDO_START + 14)
#define MRK3_NEG_REGNUM      (PSEUDO_START + 15)
#define MRK3_OVERFLOW_REGNUM (PSEUDO_START + 16)
#define MRK3_CARRY_REGNUM    (PSEUDO_START + 17)
#define PSEUDO_END (MRK3_CARRY_REGNUM)

/* TODO. These should be done through XML */
#define NUM_CPU_REGS    (SFR_START)
#define NUM_SFRS        (PSEUDO_START - SFR_START)
#define NUM_REAL_REGS   (NUM_CPU_REGS + NUM_SFRS)
#define NUM_PSEUDO_REGS (PSEUDO_END - PSEUDO_START + 1)
#define NUM_REGS        (NUM_REAL_REGS + NUM_PSEUDO_REGS)

/* Memory spaces. A total of 4 bits are allocated for this. */
#define MRK3_MEM_SPACE_MASK 0xf0000000
#define MRK3_MEM_SPACE_SYS  0x10000000
#define MRK3_MEM_SPACE_APP1 0x20000000
#define MRK3_MEM_SPACE_APP2 0x30000000
#define MRK3_MEM_SPACE_SSYS 0x40000000

/* Memory types. One bit to indicate code or data. */
#define MRK3_MEM_TYPE_MASK 0x01000000
#define MRK3_MEM_TYPE_DATA 0x00000000
#define MRK3_MEM_TYPE_CODE 0x01000000

/* General mask */
#define MRK3_MEM_MASK 0xff000000

/*  Define the breakpoint instruction which is inserted by */
/*  GDB into the target code. This must be exactly the same */
/*  as the simulator expects. */
/*  Per definition, a breakpoint instruction has 16 bits. */
/*  This should be sufficient for all purposes. */
static const uint16_t mrk3_sim_break_insn = 0x0fc1;

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

/* A structure to store the objectfile filenames for dynammic objectfile
   switching. */
struct mrk3_objfile_info_t {
	char* name;
	char* full_name;
	uint32_t mem_size_code;
	uint16_t mem_size_globals;
	uint16_t mem_size_rodata;
};

/*! Global debug flag */
int mrk3_debug;


/* Wrapper around memcpy to make it legal argument to ui_file_put */
static void
mrk3_ui_memcpy (void *dest, const char *buffer, long len)
{
  memcpy (dest, buffer, (size_t) len);
  ((char *) dest)[len] = '\0';
}


/* Get the current memory space from the target.

   TODO: Is RCmd the best way to do this? */
static uint32_t
mrk3_get_mem_space (void)
{
  /* TODO: We can't tell if we have a valid target function here, because it
     is set to a value static within target.c (tcomplain). So we'll need to
     look at whether we have we have a valid value. A shame because we'll get
     an error message. */
  struct ui_file *mf = mem_fileopen ();
  struct cleanup *old_chain = make_cleanup_ui_file_delete (mf);
  char buf[64];
  target_rcmd ("SilentGetMemSpace", mf);
  ui_file_put (mf, mrk3_ui_memcpy, buf);
  /* Result is in mf->stream->buffer, of length mf->stream->length_buffer */
  if (strlen (buf) == 0)
    {
      /* TODO: We are presumably not connected to a target. Should we warn? Or
         should we return a default? */
      warning (_
	       ("mrk3-tdep.c: using default memory space (super system)."));
      do_cleanups (old_chain);
      return MRK3_MEM_SPACE_SSYS;
    }
  else
    {
      /* The value is returned as a 32 bit value, with the result in the top 8
	 bits. */
      long unsigned int res = strtol (buf, NULL, 16);
      if (mrk3_debug >= 2)
	fprintf_unfiltered (gdb_stdlog,
			    "mrk3-tdep.c: buf \"%s\", mem space 0x%08lx\n.",
			    buf, res);
      do_cleanups (old_chain);
      return (uint32_t) res & MRK3_MEM_SPACE_MASK;
    }
}


/* Convenience function for the super system memory space. */
static int
mrk3_is_ssys_mem_space (void)
{
  uint32_t ms = mrk3_get_mem_space ();
  return ms == MRK3_MEM_SPACE_SSYS;
}


/* Convenience function for the system memory space. */
static int
mrk3_is_sys_mem_space (void)
{
  uint32_t ms = mrk3_get_mem_space ();
  return ms == MRK3_MEM_SPACE_SYS;
}


/* Convenience function for the user memory space. */
static int
mrk3_is_usr_mem_space (void)
{
  uint32_t ms = mrk3_get_mem_space ();
  return (ms == MRK3_MEM_SPACE_APP1) || (ms == MRK3_MEM_SPACE_APP2);
}


/* Convenience function for the data memory type */
static int
mrk3_is_data_address (CORE_ADDR addr)
{
  return (addr & ~MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_DATA;

}	/* mrk3_is_data_address () */


/* Convenience function for the code memory type */
static int
mrk3_is_code_address (CORE_ADDR addr)
{
  return (addr & ~MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_CODE;

}	/* mrk3_is_code_address () */


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
    "SSSP",
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
      warning (_("mrk3_register_name: unknown register number %d.\n"), regnum);
      return "";
    }
}


/* Return the GDB type object for the "standard" data type
   of data in register N.

   TODO. This should be done in XML. */
static struct type *
mrk3_register_type (struct gdbarch *gdbarch, int regnum)
{
  static struct type *bt_uint8 = NULL;
  static struct type *bt_uint16 = NULL;
  static struct type *bt_uint32 = NULL;

  /* Initialize each type just once to avoid leaks. */
  if (NULL == bt_uint8)
    bt_uint8 = builtin_type (gdbarch)->builtin_uint8;
  if (NULL == bt_uint16)
    bt_uint16 = builtin_type (gdbarch)->builtin_uint16;
  if (NULL == bt_uint32)
    bt_uint32 = builtin_type (gdbarch)->builtin_uint32;

  switch (regnum)
    {
      /* CPU registers */
    case (MRK3_R0_REGNUM + 0):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 1):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 2):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 3):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 4):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 5):
      return bt_uint16;
    case (MRK3_R0_REGNUM + 6):
      return bt_uint16;
    case (MRK3_PC_REGNUM):
      return bt_uint32;
    case (MRK3_PSW_REGNUM):
      return bt_uint16;
    case (MRK3_SSSP_REGNUM):
      return bt_uint16;
    case (MRK3_SSP_REGNUM):
      return bt_uint16;
    case (MRK3_USP_REGNUM):
      return bt_uint16;
    case (MRK3_R4E_REGNUM):
      return bt_uint16;
    case (MRK3_R5E_REGNUM):
      return bt_uint16;
    case (MRK3_R6E_REGNUM):
      return bt_uint16;

      /* Special Function Registers  - TODO through XML */

      /* Pseudo registers */
    case (MRK3_SP_REGNUM):
      return bt_uint16;
    case (MRK3_R0L_REGNUM):
      return bt_uint8;
    case (MRK3_R1L_REGNUM):
      return bt_uint8;
    case (MRK3_R2L_REGNUM):
      return bt_uint8;
    case (MRK3_R3L_REGNUM):
      return bt_uint8;
    case (MRK3_R0H_REGNUM):
      return bt_uint8;
    case (MRK3_R1H_REGNUM):
      return bt_uint8;
    case (MRK3_R2H_REGNUM):
      return bt_uint8;
    case (MRK3_R3H_REGNUM):
      return bt_uint8;
    case (MRK3_R4L_REGNUM):
      return bt_uint32;
    case (MRK3_R5L_REGNUM):
      return bt_uint32;
    case (MRK3_R6L_REGNUM):
      return bt_uint32;
    case (MRK3_SYS_REGNUM):
      return bt_uint8;
    case (MRK3_INT_REGNUM):
      return bt_uint8;
    case (MRK3_ZERO_REGNUM):
      return bt_uint8;
    case (MRK3_NEG_REGNUM):
      return bt_uint8;
    case (MRK3_OVERFLOW_REGNUM):
      return bt_uint8;
    case (MRK3_CARRY_REGNUM):
      return bt_uint8;
    default:
      /*  Moan */
      warning (_("mrk3_register_type: unknown register number %d.\n"), regnum);
      return builtin_type (gdbarch)->builtin_int0;
    };
}


static enum register_status
mrk3_pseudo_register_read (struct gdbarch *gdbarch,
			   struct regcache *regcache,
			   int cooked_regnum, gdb_byte * buf)
{
  gdb_byte raw_buf[8];
  int raw_regnum;

  /*  R0L is based on R0 */
  switch (cooked_regnum)
    {
    case MRK3_SP_REGNUM:
      warning ("memspace is 0x%08x.", mrk3_get_mem_space ());
      if (mrk3_is_ssys_mem_space ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sys_mem_space ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_usr_mem_space ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("mrk3-tdep.c: invalid SP read mem space 0x%08x."),
		   mrk3_get_mem_space ());
	  raw_regnum = MRK3_SSSP_REGNUM;
	}

      regcache_raw_read (regcache, raw_regnum, buf);
      return REG_VALID;

    case MRK3_R0L_REGNUM:
    case MRK3_R1L_REGNUM:
    case MRK3_R2L_REGNUM:
    case MRK3_R3L_REGNUM:
      raw_regnum = cooked_regnum - MRK3_R0L_REGNUM + MRK3_R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf + 1, 1);
      else
	memcpy (buf, raw_buf, 1);
      return REG_VALID;

    case MRK3_R0H_REGNUM:
    case MRK3_R1H_REGNUM:
    case MRK3_R2H_REGNUM:
    case MRK3_R3H_REGNUM:
      raw_regnum = cooked_regnum - MRK3_R0H_REGNUM + MRK3_R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf, 1);
      else
	memcpy (buf, raw_buf + 1, 1);
      return REG_VALID;

    case MRK3_R4L_REGNUM:
    case MRK3_R5L_REGNUM:
    case MRK3_R6L_REGNUM:
      /*  LO reg */
      raw_regnum = cooked_regnum - MRK3_R4L_REGNUM + MRK3_R0_REGNUM + 4;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf + 2, 2);
      else
	memcpy (buf, raw_buf, 2);
      /*  HI reg */
      raw_regnum = cooked_regnum - MRK3_R4L_REGNUM + MRK3_R4E_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf, 2);
      else
	memcpy (buf, raw_buf + 2, 2);
      return REG_VALID;

    case MRK3_SYS_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[0] & 0x80) >> 7;
      else
	buf[0] = (raw_buf[1] & 0x80) >> 7;
      return REG_VALID;

    case MRK3_INT_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[0] & 0x78) >> 3;
      else
	buf[0] = (raw_buf[1] & 0x78) >> 3;
      return REG_VALID;

    case MRK3_ZERO_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 3;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 3;
      return REG_VALID;

    case MRK3_NEG_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 2;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 2;
      return REG_VALID;

    case MRK3_OVERFLOW_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 1;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 1;
      return REG_VALID;

    case MRK3_CARRY_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x08) >> 0;
      else
	buf[0] = (raw_buf[0] & 0x08) >> 0;
      return REG_VALID;

    default:
      /*  Moan */
      warning (_("mrk3_pseudo_register_read: Not a pseudo reg %d.\n"),
	       cooked_regnum);
      return REG_UNKNOWN;
    }
}


static void
mrk3_pseudo_register_write (struct gdbarch *gdbarch,
			    struct regcache *regcache,
			    int cooked_regnum, const gdb_byte * buf)
{
  gdb_byte raw_buf[8];
  int raw_regnum = 0;

  /*  R0L is based on R0 */
  switch (cooked_regnum)
    {
    case MRK3_SP_REGNUM:
      if (mrk3_is_ssys_mem_space ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sys_mem_space ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_usr_mem_space ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("mrk3-tdep.c: invalid SP write mem space 0x%08x."),
		   mrk3_get_mem_space ());
	  raw_regnum = MRK3_SSSP_REGNUM;
	}

      regcache_raw_write (regcache, raw_regnum, buf);
      return;

    case MRK3_R0L_REGNUM:
    case MRK3_R1L_REGNUM:
    case MRK3_R2L_REGNUM:
    case MRK3_R3L_REGNUM:
      raw_regnum = cooked_regnum - MRK3_R0L_REGNUM + MRK3_R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf + 1, buf, 1);
      else
	memcpy (raw_buf, buf, 1);
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_R0H_REGNUM:
    case MRK3_R1H_REGNUM:
    case MRK3_R2H_REGNUM:
    case MRK3_R3H_REGNUM:
      raw_regnum = cooked_regnum - MRK3_R0H_REGNUM + MRK3_R0_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 1);
      else
	memcpy (raw_buf + 1, buf, 1);
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_R4L_REGNUM:
    case MRK3_R5L_REGNUM:
    case MRK3_R6L_REGNUM:
      /*  LO reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf + 2, buf, 2);
      else
	memcpy (raw_buf, buf, 2);
      raw_regnum = cooked_regnum - MRK3_R4L_REGNUM + MRK3_R0_REGNUM + 4;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      /*  HI reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 2);
      else
	memcpy (raw_buf + 2, buf, 2);
      raw_regnum = cooked_regnum - MRK3_R4L_REGNUM + MRK3_R4E_REGNUM;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_SYS_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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

    case MRK3_INT_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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

    case MRK3_ZERO_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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

    case MRK3_NEG_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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

    case MRK3_OVERFLOW_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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

    case MRK3_CARRY_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
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
      warning (_("mrk3_pseudo_register_write: Not a pseudo reg %d.\n"),
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
      regnr = MRK3_PC_REGNUM;
      break;			/*  PC */
    case 2:
      regnr = MRK3_PC_REGNUM;
      break;			/*  PC16 */
    case 3:
      regnr = MRK3_PC_REGNUM;
      break;			/*  PCh */
    case 4:
      regnr = MRK3_PSW_REGNUM;
      break;			/*  PSW */
      /*  case  5:                                      // PU (?) */

    case 6:
      regnr = MRK3_R0_REGNUM;
      break;			/*  R0 */
    case 7:
      regnr = MRK3_R0_REGNUM + 1;
      break;			/*  R1 */
    case 8:
      regnr = MRK3_R0_REGNUM + 2;
      break;			/*  R2 */
    case 9:
      regnr = MRK3_R0_REGNUM + 3;
      break;			/*  R3 */

      /*  R46 is a virtual 48 bit register */
      /*  having R4, R5 and R6 as real registers. */
    case 10:
      regnr = MRK3_R0_REGNUM + 4;
      break;			/*  R46[0] */
    case 11:
      regnr = MRK3_R0_REGNUM + 5;
      break;			/*  R46[1] */
    case 12:
      regnr = MRK3_R0_REGNUM + 6;
      break;			/*  R46[2] */

      /*  R46e is a virtual 48 bit register */
      /*  having R4e, R5e and R6e as real registers. */
    case 13:
      regnr = MRK3_R4E_REGNUM;
      break;			/*  R46e[0] */
    case 14:
      regnr = MRK3_R5E_REGNUM;
      break;			/*  R46e[1] */
    case 15:
      regnr = MRK3_R6E_REGNUM;
      break;			/*  R46e[2] */

      /*  R46L is a virtual 96 bit register */
      /*  having R4l, R5l and R6l as real registers. */
    case 16:
      regnr = MRK3_R4L_REGNUM;
      break;			/*  R46e[0] */
    case 17:
      regnr = MRK3_R5L_REGNUM;
      break;			/*  R46e[1] */
    case 18:
      regnr = MRK3_R6L_REGNUM;
      break;			/*  R46e[2] */

    case 19:
      regnr = MRK3_SP_REGNUM;
      break;			/*  R7 */

      /*  case 20:                                      // RO (?) */

      /*  Rb is a virtual register, consisting of all */
      /*  byte registers. */
    case 21:
      regnr = MRK3_R0L_REGNUM;
      break;			/*  Rb[0] */
    case 22:
      regnr = MRK3_R1L_REGNUM;
      break;			/*  Rb[1] */
    case 23:
      regnr = MRK3_R2L_REGNUM;
      break;			/*  Rb[2] */
    case 24:
      regnr = MRK3_R3L_REGNUM;
      break;			/*  Rb[3] */
    case 25:
      regnr = MRK3_R0H_REGNUM;
      break;			/*  Rb[4] */
    case 26:
      regnr = MRK3_R1H_REGNUM;
      break;			/*  Rb[5] */
    case 27:
      regnr = MRK3_R2H_REGNUM;
      break;			/*  Rb[6] */
    case 28:
      regnr = MRK3_R3H_REGNUM;
      break;			/*  Rb[7] */

      /*  RbH is a virtual register, consisting of all */
      /*  high byte registers. */
    case 29:
      regnr = MRK3_R0H_REGNUM;
      break;			/*  RbH[0] */
    case 30:
      regnr = MRK3_R1H_REGNUM;
      break;			/*  RbH[1] */
    case 31:
      regnr = MRK3_R2H_REGNUM;
      break;			/*  RbH[2] */
    case 32:
      regnr = MRK3_R3H_REGNUM;
      break;			/*  RbH[3] */

      /*  RbL is a virtual register, consisting of all */
      /*  high byte registers. */
    case 33:
      regnr = MRK3_R0L_REGNUM;
      break;			/*  RbL[0] */
    case 34:
      regnr = MRK3_R1L_REGNUM;
      break;			/*  RbL[1] */
    case 35:
      regnr = MRK3_R2L_REGNUM;
      break;			/*  RbL[2] */
    case 36:
      regnr = MRK3_R3L_REGNUM;
      break;			/*  RbL[3] */

      /*  Rw is a virtual register, consisting of all */
      /*  word registers. */
    case 37:
      regnr = MRK3_R0_REGNUM;
      break;			/*  Rw[0] */
    case 38:
      regnr = MRK3_R0_REGNUM + 1;
      break;			/*  Rw[1] */
    case 39:
      regnr = MRK3_R0_REGNUM + 2;
      break;			/*  Rw[2] */
    case 40:
      regnr = MRK3_R0_REGNUM + 3;
      break;			/*  Rw[3] */
    case 41:
      regnr = MRK3_R0_REGNUM + 4;
      break;			/*  Rw[4] */
    case 42:
      regnr = MRK3_R0_REGNUM + 5;
      break;			/*  Rw[5] */
    case 43:
      regnr = MRK3_R0_REGNUM + 6;
      break;			/*  Rw[6] */
    case 44:
      regnr = MRK3_R4E_REGNUM;
      break;			/*  Rw[7] */
    case 45:
      regnr = MRK3_R5E_REGNUM;
      break;			/*  Rw[8] */
    case 46:
      regnr = MRK3_R6E_REGNUM;
      break;			/*  Rw[9] */

      /*  RwL is probably a virtual register consisting */
      /*  of all word registers. */
    case 47:
      regnr = MRK3_R0_REGNUM;
      break;			/*  RwL[0] */
    case 48:
      regnr = MRK3_R0_REGNUM + 1;
      break;			/*  RwL[1] */
    case 49:
      regnr = MRK3_R0_REGNUM + 2;
      break;			/*  RwL[2] */
    case 50:
      regnr = MRK3_R0_REGNUM + 3;
      break;			/*  RwL[3] */

    case 51:
      regnr = MRK3_CARRY_REGNUM;
      break;			/*  c flag */
    case 52:
      regnr = MRK3_INT_REGNUM;
      break;			/*  interrupt level */
    case 53:
      regnr = MRK3_NEG_REGNUM;
      break;			/*  n flag */
    case 54:
      regnr = MRK3_ZERO_REGNUM;
      break;			/*  z flag (actually nz (?)) */
    case 55:
      regnr = MRK3_OVERFLOW_REGNUM;
      break;			/*  o flag */
    case 56:
      regnr = 0;
      break;			/*  res (?) */
    case 57:
      regnr = MRK3_SYS_REGNUM;
      break;			/*  s flag */
    case 58:
      regnr = 0;
      break;			/*  tLSB (?) */
    case 59:
      regnr = MRK3_ZERO_REGNUM;
      break;			/*  z flag */

    default:
      warning (_("mrk3_dwarf2_reg_to_regnum: unknown drwarf2 regnum: %d."),
	       dwarf2_regnr);
      regnr = MRK3_R0_REGNUM;
    }

  if (mrk3_debug)
    fprintf_unfiltered (gdb_stdlog, "mrk3-tdep.c: gdbarch->num_regs=%d "
			"dwarf2_regnr(%d) maps to (%d)\n",
			gdbarch_num_regs( gdbarch), dwarf2_regnr, regnr);
  return regnr;

  /* return Dll_Dwarf2RegToRegnum(dwarf2_regnr); */
}

#if 0
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
#endif


#if 0
/*  Free an objfile_info structure. */
static void
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


/*  Save the full and base filename to an objfile info struct. */
static void
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


/* The breakpoint will be set at a GDB address, but we need to convert it to a
   target (word) code address  */
static const gdb_byte *
mrk3_breakpoint_from_pc (struct gdbarch *gdbarch,
			 CORE_ADDR * pcptr, int *lenptr)
{
  CORE_ADDR addr = *pcptr & ~MRK3_MEM_MASK;
  CORE_ADDR ptr  = addr / 2;

  /*  always use full addresses for breakpoints, and it is code. */
  CORE_ADDR topbits = mrk3_get_mem_space () | MRK3_MEM_TYPE_CODE;
  addr |= topbits;
  ptr |= topbits;

  *pcptr = ptr;

  if (mrk3_debug)
    fprintf_unfiltered (gdb_stdlog, "mrk3-tdep.c: breakpoint at %s.\n",
			print_core_address (gdbarch, ptr));

  *lenptr = sizeof (mrk3_sim_break_insn);
  return (gdb_byte *) & mrk3_sim_break_insn;
}


/* Convert target pointer to GDB address.

   GDB expects a single unified byte addressed memory. For Harvard
   architectures, this means that addresses on the target need mapping. To
   avoid confusion, GDB refers to "addresses" to mean the unified byte address
   space used internally within GDB and "pointers" to refer to the values used
   on the target (which need be neither unique, nor byte addressing).

   This mapping is typically achieved by shifting to convert to/from word
   addressing if required and adding high order bits to addresses to
   distinguish multiple address spaces.

   MRK3 is a Harvard architecture, with a word-addressed instruction space.,
   so needs this mechanism.

   However...

   There is no mechanism (yet!) in Remote Serial Protocol to distinguish which
   address space is being used. So we cannot make the transformation. We must
   pass the higher order bits and leave it to the server.

   However we do the byte <-> word conversion, because otherwise symbols go
   'orribly wrong. We also do some validation of flags. */

static CORE_ADDR
mrk3_pointer_to_address (struct gdbarch *gdbarch,
			 struct type *type, const gdb_byte * buf)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  CORE_ADDR ptr = extract_unsigned_integer (buf, TYPE_LENGTH (type),
					    byte_order);

  if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC
      || TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD
      || TYPE_CODE_SPACE (TYPE_TARGET_TYPE (type)))
    {
      /* Word -> byte for code. */
      CORE_ADDR flags = ptr & MRK3_MEM_MASK;
      CORE_ADDR addr  = ((ptr & ~MRK3_MEM_MASK) * 2) | flags;

      /* Sanity check */
      if (mrk3_is_code_address (addr))
	{
	  warning (_("MRK3 code pointer 0x%s missing code flags - corrected"),
		   hex_string (addr));
	  addr = (addr & ~MRK3_MEM_TYPE_MASK) | MRK3_MEM_TYPE_CODE;
	}

      if (mrk3_debug >= 2)
	fprintf_unfiltered (gdb_stdlog,
			    "mrk3_pointer_to_address: code %s -> %s.\n",
			    hex_string (ptr), hex_string (addr));

      return addr;
    }
  else
    {
      /* No change for data. */
      CORE_ADDR addr = ptr;

      /* Sanity check */
      if (mrk3_is_data_address (addr))
	{
	  warning (_("MRK3 code pointer 0x%s missing code flags - corrected"),
		   hex_string (addr));
	  addr = (addr & ~MRK3_MEM_TYPE_MASK) | MRK3_MEM_TYPE_DATA;
	}

      if (mrk3_debug >= 2)
	fprintf_unfiltered (gdb_stdlog,
			    "mrk3_pointer_to_address: data %s -> %s.\n",
			    hex_string (ptr), hex_string (addr));
      return addr;
    }
}


/* Convert GDB address to target pointer.

   see mrk3_pointer_to_address for a description. */

static void
mrk3_address_to_pointer (struct gdbarch *gdbarch,
			 struct type *type, gdb_byte * buf, CORE_ADDR addr)
{
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  if (TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC
      || TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD)
    {
      /* Byte -> word for code. */
      CORE_ADDR flags = addr & MRK3_MEM_MASK;
      CORE_ADDR ptr   = ((addr & ~MRK3_MEM_MASK) / 2) | flags;

      /* Sanity check */
      if (mrk3_is_code_address (ptr))
	{
	  warning (_("MRK3 code address 0x%s missing code flags - corrected"),
		   hex_string (ptr));
	  ptr = (ptr & ~MRK3_MEM_TYPE_MASK) | MRK3_MEM_TYPE_CODE;
	}

      if (mrk3_debug >= 2)
	fprintf_unfiltered (gdb_stdlog,
			    "mrk3_address_to_pointer: code %s -> %s.\n",
			    hex_string (addr), hex_string (ptr));

      store_unsigned_integer (buf, TYPE_LENGTH (type), byte_order, ptr);
    }
  else
    {
      /* No change for data. */
      CORE_ADDR ptr = addr;

      /* Sanity check */
      if (mrk3_is_data_address (ptr))
	{
	  warning (_("MRK3 code address 0x%s missing code flags - corrected"),
		   hex_string (ptr));
	  ptr = (ptr & ~MRK3_MEM_TYPE_MASK) | MRK3_MEM_TYPE_DATA;
	}

      if (mrk3_debug >= 2)
	fprintf_unfiltered (gdb_stdlog,
			    "mrk3_address_to_pointer: data %s -> %s.\n",
			    hex_string (addr), hex_string (ptr));

      store_unsigned_integer (buf, TYPE_LENGTH (type), byte_order, ptr);
    }
}


/* Remove useless bits from addresses. */

static CORE_ADDR
mrk3_addr_bits_remove (struct gdbarch *gdbarch, CORE_ADDR val)
{
  return val & ~MRK3_MEM_MASK;

}	/* mrk3_addr_bits_remove () */


/* Read PC, which is a word pointer, converting it to a byte pointer, BUT
   DON'T ADD SPACE OR TYPE FLAGS!!! This seems only to be used for comparing
   against symbol tables, which are all byte addresses, but don't have the
   flags. */

static CORE_ADDR
mrk3_read_pc (struct regcache *regcache)
{
  CORE_ADDR pcptr;
  CORE_ADDR pcaddr;

  regcache_cooked_read_unsigned (regcache, MRK3_PC_REGNUM, &pcptr);
  pcaddr = pcptr * 2;

  if (mrk3_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "mrk3_read_pc: %s read as %s.\n",
			hex_string (pcptr), hex_string (pcaddr));
  return pcaddr;

}	/* mrk3_read_pc () */


/* Write PC, which is a word pointer, with a value supplied as a GDB byte
   address. */
static void
mrk3_write_pc (struct regcache *regcache, CORE_ADDR pcaddr)
{
  CORE_ADDR pcptr = (pcaddr & ~MRK3_MEM_MASK) / 2;
  regcache_cooked_write_unsigned (regcache, MRK3_PC_REGNUM, pcptr);

  if (mrk3_debug >= 2)
    fprintf_unfiltered (gdb_stdlog, "mrk3_write_pc: %s written as %s.\n",
			hex_string (pcaddr), hex_string (pcptr));

}	/* mrk3_write_pc () */


/* TODO. This has changed in the latest GDB, with more args. Need to
   understand what this does and why we need it. Result is a boolean
   indicating success or failure. */
static int
mrk3_register_to_value (struct frame_info *frame, int regnum,
			struct type *type, gdb_byte * buf,
			int *optimizedp, int *unavailablep)
{
  /* printf("mrk3-tdep.c:mrk3_register_to_value frame=%p, regnum=%d, type=%p(length=%d)\n",frame,regnum,type,type->length);         */
  frame_unwind_register (frame, regnum, buf);
  return  1;
}


static CORE_ADDR
mrk3_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  /*  TODO: See alpha-tdep.c :: alpha_after_prologue */

  /*  Actually do not skip anything. */
  return pc;
}


/* Determine the value of registers in the PREV frame and save them in the
   prologue cache for THIS frame. Note that in general we cannot just unwind
   registers here, or we'll end p with a circular dependency. */
static void
mrk3_analyze_prologue (struct frame_info *this_frame,
		       struct trad_frame_cache *this_cache)
{
  CORE_ADDR func_start = get_frame_func (this_frame);
  CORE_ADDR this_pc  = get_frame_pc (this_frame);
  CORE_ADDR this_sssp;
  CORE_ADDR this_ssp;
  CORE_ADDR this_usp;
  CORE_ADDR this_sp;

  /* Get the stack pointers if we can. */
  if (this_frame)
    {
      this_sssp = get_frame_register_unsigned (this_frame, MRK3_SSSP_REGNUM);
      this_ssp  = get_frame_register_unsigned (this_frame, MRK3_SSP_REGNUM);
      this_usp  = get_frame_register_unsigned (this_frame, MRK3_USP_REGNUM);
      this_sp   = mrk3_is_ssys_mem_space () ? this_sssp
	: mrk3_is_sys_mem_space () ? this_ssp : this_usp;
    }
  else
    {
      /* Default is to start in super system mode. */
      this_sssp = 0;
      this_ssp = 0;
      this_usp = 0;
      this_sp  = this_sssp;
    }

  /* Set the frame ID and frame base */
  trad_frame_set_id (this_cache, frame_id_build (this_sp, this_pc));
  trad_frame_set_this_base (this_cache, this_sp);

  /* TODO: We'll leave register initialization to another time.
     trad_frame_set_reg_value (this_cache, MRK3_PC_REGNUM, pc);
     etc... */
}


/* Populate the frame cache if it doesn't exist. */
static struct trad_frame_cache *
mrk3_frame_cache (struct frame_info *this_frame,
		  void **this_cache)
{
  if (mrk3_debug)
    fprintf_unfiltered (gdb_stdlog,
			"mrk3-tdep.c: frame_cache = 0x%p\n", *this_cache);

  if (!*this_cache)
    {
      CORE_ADDR func_start;
      CORE_ADDR stop_addr;

      *this_cache = trad_frame_cache_zalloc (this_frame);
      func_start = get_frame_func (this_frame);
      stop_addr = get_frame_pc (this_frame);

      /* If we couldn't find any function containing the PC, then just
         initialize the prologue cache, but don't do anything.  */
      if (!func_start)
	stop_addr = func_start;

      mrk3_analyze_prologue (this_frame, *this_cache);
    }

  return *this_cache;
}

static CORE_ADDR
mrk3_frame_base_address (struct frame_info *this_frame,
			 void **this_cache)
{
  struct trad_frame_cache *frame_cache = mrk3_frame_cache (this_frame,
							   this_cache);
  return trad_frame_get_this_base (frame_cache);
}


static void
mrk3_frame_this_id (struct frame_info *this_frame,
		    void **this_cache, struct frame_id *this_id)
{
  CORE_ADDR base = mrk3_frame_base_address (this_frame, this_cache);
  if (base)
    {
      *this_id = frame_id_build (base, get_frame_func (this_frame));
    }
  /* Otherwise, leave it unset, and that will terminate the backtrace.  */
}


static struct value *
mrk3_frame_prev_register (struct frame_info *this_frame,
			  void **this_cache, int regnum)
{
  struct trad_frame_cache *frame_cache = mrk3_frame_cache (this_frame,
							   this_cache);
  return trad_frame_get_register (frame_cache, this_frame, regnum);
}


static const struct frame_unwind mrk3_frame_unwind = {
  .type          = NORMAL_FRAME,
  .stop_reason   = default_frame_unwind_stop_reason,
  .this_id       = mrk3_frame_this_id,
  .prev_register = mrk3_frame_prev_register,
  .unwind_data   = NULL,
  .sniffer       = default_frame_sniffer,
  .dealloc_cache = NULL,
  .prev_arch     = NULL
};


static const struct frame_base mrk3_frame_base = {
  .unwind      = &mrk3_frame_unwind,
  .this_base   = mrk3_frame_base_address,
  .this_locals = mrk3_frame_base_address,
  .this_args   = mrk3_frame_base_address
};


/* When unwinding the PC we turn into a byte address and add the flags for
   code type. This is different from mrk3_read_pc, where we don't worry about
   the flags. */

static CORE_ADDR
mrk3_unwind_pc (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST pc;
  pc = frame_unwind_register_unsigned (next_frame, MRK3_PC_REGNUM);
  pc = pc * 2 | MRK3_MEM_SPACE_SSYS | MRK3_MEM_TYPE_CODE;
  return gdbarch_addr_bits_remove (gdbarch, pc);
}


/* When unwinding the PC we turn into a byte address and add the flags for
   data type. */

static CORE_ADDR
mrk3_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  ULONGEST sp;
  sp = frame_unwind_register_unsigned (next_frame, MRK3_SP_REGNUM);
  return sp | MRK3_MEM_SPACE_SSYS | MRK3_MEM_TYPE_DATA;
}

static int
mrk3_convert_register_p (struct gdbarch *gdbarch, int regnum,
			 struct type *type)
{
  return mrk3_register_type (gdbarch, regnum)->length != type->length;
}


static int
mrk3_address_class_type_flags (int byte_size, int dwarf2_addr_class)
{
  if (byte_size == 2)
    return TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1;
  else
    return 0;
}

#if 0
/* This appears not to be used for now. */

static char *
mrk3_address_class_type_flags_to_name (int type_flags)
{
  if (type_flags & TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1)
    return "short";
  else
    return 0;
}

/* Not used, so disabled. */
static int
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
#endif


/* Diassembler

   Get the target to disassemble if possible. Otherwise, just dump out the
   hex. */
static int
mrk3_print_insn (bfd_vma addr,
		 disassemble_info *info)
{
  if (target_has_execution)
    {
      // This is a bit dodgy. We assume that if we have an executable target
      // it knows how to disassemble. We are also dealing direct with the
      // target, so we need the address, not the pointer.
      struct ui_file *mf = mem_fileopen ();
      struct cleanup *old_chain = make_cleanup_ui_file_delete (mf);
      char cmd[40];
      char buf[80];
      sprintf (cmd, "silent-disas %s\n", hex_string (addr));
      target_rcmd (cmd, mf);
      /* Result is in mf->stream->buffer, of length
	 mf->stream->length_buffer. Extract it into buf. */
      ui_file_put (mf, mrk3_ui_memcpy, buf);

      if (strlen (buf) == 0)
	{
	  /* TODO: What do we do if something goes wrong? */
	  warning (_("mrk3-tdep.c: Unable to diassemble."));
	  do_cleanups (old_chain);
	  return -1;
	}
      else
	{
	  // Print the instruction if we had one.
	  int size = buf [0] - '0';		// Yuk - what if we have EBCDIC
	  if (size > 0)
	    (*info->fprintf_func) (info->stream, "%s", &(buf[2]));

	  do_cleanups (old_chain);
	  return size > 0 ? size : -1;
	}
    }
  else
    {
      // Not executing, so simple hex dump
      uint16_t  insn16;
      uint32_t  insn32;

      CORE_ADDR ptr = ((addr & ~MRK3_MEM_TYPE_MASK) / 2) | MRK3_MEM_TYPE_CODE;
      read_memory (ptr, (gdb_byte *) &insn16, sizeof (insn16));
      read_memory (ptr, (gdb_byte *) &insn32, sizeof (insn32));

      /* Because of the way we read things, we have to use a middle-endian
	 presentation of 32-bit instructions. */
      (*info->fprintf_func) (info->stream, "%04x%04x %04hx", insn32 >> 16,
			     insn32 &0xffff, insn16);
      return sizeof (insn16);	/* Assume 16-bit instruction. */
    }
}	/* mrk3_print_insn () */


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

  set_gdbarch_read_pc (gdbarch, mrk3_read_pc);
  set_gdbarch_write_pc (gdbarch, mrk3_write_pc);

  set_gdbarch_num_regs (gdbarch, NUM_REAL_REGS);
  set_gdbarch_num_pseudo_regs (gdbarch, NUM_PSEUDO_REGS);

  set_gdbarch_sp_regnum (gdbarch, MRK3_SP_REGNUM);
  set_gdbarch_pc_regnum (gdbarch, MRK3_PC_REGNUM);

  set_gdbarch_register_name (gdbarch, mrk3_register_name);
  set_gdbarch_register_type (gdbarch, mrk3_register_type);

  set_gdbarch_pseudo_register_read (gdbarch, mrk3_pseudo_register_read);
  set_gdbarch_pseudo_register_write (gdbarch, mrk3_pseudo_register_write);

  /* @todo: reading the stack and address to pointer conversion is not
     supported atm. */
  /* set_gdbarch_return_value (gdbarch, mrk3_return_value); */
  /* We don't currently have a proper disassembler, so we'll provide our own
     locally. The real one should be in opcodes/mrk3-dis.c (part of
     binutils). */
  /* set_gdbarch_print_insn (gdbarch, print_insn_mrk3); */
  set_gdbarch_print_insn (gdbarch, mrk3_print_insn);

/*   set_gdbarch_push_dummy_call (gdbarch, dummy_push_dummy_call); */
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mrk3_dwarf2_reg_to_regnum);

  set_gdbarch_address_to_pointer (gdbarch, mrk3_address_to_pointer);
  set_gdbarch_pointer_to_address (gdbarch, mrk3_pointer_to_address);
  set_gdbarch_addr_bits_remove (gdbarch, mrk3_addr_bits_remove);

  /*  IMPORTANT - We need to be able to convert register contents to different length */
  /*  gdb default will use 1:1 which is false in case we have a 16 bit register and need 32 bit values */
  set_gdbarch_convert_register_p (gdbarch, mrk3_convert_register_p);
  set_gdbarch_register_to_value (gdbarch, mrk3_register_to_value);
  set_gdbarch_skip_prologue (gdbarch, mrk3_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_decr_pc_after_break (gdbarch, 2);
  set_gdbarch_breakpoint_from_pc (gdbarch, mrk3_breakpoint_from_pc);

  frame_unwind_append_unwinder (gdbarch, &mrk3_frame_unwind);
  frame_base_set_default (gdbarch, &mrk3_frame_base);

/*   set_gdbarch_dummy_id (gdbarch, mrk3_dummy_id); */

  set_gdbarch_unwind_pc (gdbarch, mrk3_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mrk3_unwind_sp);

  return gdbarch;
}

/*! Dump out the target specific information. Currently we have none. */
static void
mrk3_dump_tdep (struct gdbarch *gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  fprintf_unfiltered (file, "mrk3_dump_tdep: Nothing to show\n");

}	/* mrk3_dump_tdep () */


extern initialize_file_ftype _initialize_mrk3_tdep;

void
_initialize_mrk3_tdep (void)
{
  gdbarch_register (bfd_arch_mrk3, mrk3_gdbarch_init, mrk3_dump_tdep);

  /* Debug internals for MRK3 GDB.  */
  add_setshow_zinteger_cmd ("mrk3", class_maintenance,
			    &mrk3_debug,
			    _("Set MRK3 specific debugging."),
			    _("Show MRK3 specific debugging."),
			    _("Non-zero enables MRK3 specific debugging."),
			    NULL,
			    NULL,
			    &setdebuglist,
			    &showdebuglist);
}
