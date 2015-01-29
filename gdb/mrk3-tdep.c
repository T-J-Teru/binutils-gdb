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

/*! @file
		      GDB for the NXP MRK3 Architecture
		      =================================

The MRK3 is a "16-bit" Harvard architecture with a word addressed code
space.  Although a 16-bit architecture, the program counter is 32-bit,
allowing larger code spaces to be addressed.

The architecture also supports up to 16 separate address spaces (each with
a separate code and data space).  Of these address spaces, two are special, the
System address space and the Super-system adddress space.  Each address space
has its own stack pointer, but otherwise register context is shared.

Registers
---------

At all times GDB has visibility of the current user address space stack
pointer as well as the system and super system stack pointers.  The full set
of physical registers visible to GDB and their size in bits in order is:

    Register    Size        Notes
    --------    ----        -----
    R0            16
    R1            16
    R2            16
    R3            16
    R4            16
    R5            16
    R6            16        Can be used as frame pointer
    PC            32        Program counter @note Word address
    PSW           16        Status word
    SSSP          16        Super-system space stack pointer
    SSP           16        System space stack pointer
    USP           16        Current user space stack pointer
    R4E           16        Combined with R4 to create 32-bit value
    R5E           16        Combined with R5 to create 32-bit value
    R6E           16        Combined with R6 to create 32-bit value

GDB also defines some pseudo-registers which are combinations of the physical
registers.

    Register    Size        Notes
    --------    ----        -----
    SP            16        Stack pointer of current space (SSSP, SSP or USP)
    R0L            8        Low 8 bits of R0
    R1L            8        Low 8 bits of R1
    R2L            8        Low 8 bits of R2
    R3L            8        Low 8 bits of R3
    R0H            8        High 8 bits of R0
    R1H            8        High 8 bits of R1
    R2H            8        High 8 bits of R2
    R3H            8        High 8 bits of R3
    R4LONG        32        Concatenation of R4E and R4
    R5LONG        32        Concatenation of R5E and R5
    R6LONG        32        Concatenation of R6E and R6
    SYS            8        Bit 15 of PSW, 1 in System Mode, 0 otherwise
    INT            8        Bits 14-10 of PSW, interrupt level acceptance
    ZERO           8        Bit 3 of PSW, zero flag
    NEG            8        Bit 2 of PSW, negative (sign) flag
    OVERFLOW       8        Bit 1 of PSW, signed overflow flag
    CARRY          8        Bit 0 of PFS, carry flag

Memory
------

MRK3 has up to 16 address spaces, of which two are special, the System address
space and the Supersystem address space.  Each of these address spaces is
further divided into data and code spaces.  The code space is word addressed,
and can be only accessed by the program counter.

GDB expects a single unified byte addressed memory. For Harvard architectures,
and/or those with word addressing, this means that addresses on the target
need mapping.

To avoid confusion, GDB refers to "addresses" to mean the unified byte address
space used internally within GDB and "pointers" to refer to the values used on
the target (which need be neither unique, nor byte addressed).

This mapping is typically achieved by shifting to convert to/from word
addressing if required and adding high order bits to addresses to distinguish
multiple address spaces.

This is not quite perfect. GDB actually uses two flavours of *address*. Symbol
tables hold byte values, but without any higher order bits. So sometimes GDB
wants pointers converted to addresses, but without any higher order bits.

@note The GDB Remote Serial Protocol (RSP) does not distinguish address
      spaces when accessing memory (this is a change from the distant
      past).  So the communication protocol uses addresses, not pointers.

@note Therefore, the purpose of this address <-> pointer manipulation should
      be purely for presentation to the user.

We use this mechanism for MRK3. Although the PC is 32 bits, the achitecture
has a maximum addressability of 20-bits (word addressing). We use the top
8-bits to identify the address space (top 4 bits) and within the address
space, its code and data spaces (bit 24).  Code pointers are shifted to be
code addresses.

*/


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
#include "observer.h"
#include "safe-ctype.h"

/*  Required for symbol switch handling. */
#include "gdb/defs.h"
#include "gdb/gdbcore.h"
#include "gdb/symfile.h"
#include "gdb/objfiles.h"
#include "gdb/gdb_obstack.h"
#include "gdb/progspace.h"
#include "gdb/breakpoint.h"

/* Useful register numbers - CPU registers */
#define MRK3_R0_REGNUM     0
#define MRK3_FP_REGNUM     6
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
#define MRK3_R4LONG_REGNUM      (PSEUDO_START +  9)
#define MRK3_R5LONG_REGNUM      (PSEUDO_START + 10)
#define MRK3_R6LONG_REGNUM      (PSEUDO_START + 11)
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
static const uint32_t  MRK3_MEM_SPACE_MASK = 0xf0000000;
static const uint32_t  MRK3_MEM_SPACE_NONE = 0x00000000;
static const uint32_t  MRK3_MEM_SPACE_SYS  = 0x10000000;
static const uint32_t  MRK3_MEM_SPACE_APP1 = 0x20000000;
static const uint32_t  MRK3_MEM_SPACE_APP2 = 0x30000000;
static const uint32_t  MRK3_MEM_SPACE_SSYS = 0x40000000;

/* Memory types. One bit to indicate code or data. */
static const uint32_t  MRK3_MEM_TYPE_MASK = 0x01000000;
static const uint32_t  MRK3_MEM_TYPE_DATA = 0x00000000;
static const uint32_t  MRK3_MEM_TYPE_CODE = 0x01000000;

/* General mask */
static const uint32_t  MRK3_MEM_MASK = 0xff000000;

/* Define the breakpoint instruction which is inserted by GDB into the target
   code. This must be exactly the same as the simulator expects.

   Per definition, a breakpoint instruction has 16 bits.  This should be
   sufficient for all purposes. */
static const uint16_t mrk3_sim_break_insn = 0x0fc1;

/* Debug flag values */
static const int  MRK3_DEBUG_FLAG_GENERAL  = 0x0001;
static const int  MRK3_DEBUG_FLAG_PTRADDR  = 0x0002;
static const int  MRK3_DEBUG_FLAG_FRAME    = 0x0004;
static const int  MRK3_DEBUG_FLAG_MEMSPACE = 0x0008;
static const int  MRK3_DEBUG_FLAG_DWARF    = 0x0010;

/* Information extracted from frame.  */
struct mrk3_frame_cache
{
  /* Frame pointer base for frame.  */
  CORE_ADDR base;

  /* If the previous program counter is on the stack, which is almost
     certainly the case, then we should only access it as 16-bit.  */
  unsigned prev_pc_on_stack : 1;

  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

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


/*! Global debug flag */
int  mrk3_debug;

/*! Global flag indicating if memspace is valid. */
int  mrk3_memspace_valid_p;


/*! Is a particular debug flag set?

  @param[in] target_flag  The flag to check
  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_have_debug_flag (const int  target_flag)
{
  return  (mrk3_debug & target_flag) == target_flag;

}	/* mrk3_have_debug_flag () */


/*! Are general debug messages enabled?

  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_debug_general (void)
{
  return  mrk3_have_debug_flag (MRK3_DEBUG_FLAG_GENERAL);

}	/* mrk3_debug_general () */


/*! Are debug messages concerning pointer-address conversion enabled?

  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_debug_ptraddr (void)
{
  return  mrk3_have_debug_flag (MRK3_DEBUG_FLAG_PTRADDR);

}	/* mrk3_debug_ptraddr () */


/*! Are debug messages concerning frame handling enabled?

  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_debug_frame (void)
{
  return  mrk3_have_debug_flag (MRK3_DEBUG_FLAG_FRAME);

}	/* mrk3_debug_frame () */


/*! Are debug messages concerning memory space handling enabled?

  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_debug_memspace (void)
{
  return  mrk3_have_debug_flag (MRK3_DEBUG_FLAG_MEMSPACE);

}	/* mrk3_debug_memspace () */


/*! Are debug messages concerning DWARF handling enabled?

  @return  Non-zero if these debug messages are enabled. Zero otherwise. */
static int
mrk3_debug_dwarf (void)
{
  return  mrk3_have_debug_flag (MRK3_DEBUG_FLAG_DWARF);

}	/* mrk3_debug_dwarf () */


/*! Wrapper around memcpy to make it legal argument to ui_file_put */
static void
mrk3_ui_memcpy (void *dest, const char *buffer, long len)
{
  memcpy (dest, buffer, (size_t) len);
  ((char *) dest)[len] = '\0';
}


/*! Invalidate the memory space cache. */
static void
mrk3_invalidate_memspace_cache (void)
{
  mrk3_memspace_valid_p = 0;
}

/*! Get the current memory space from the target.

   TODO: Is RCmd the best way to do this? */
static uint32_t
mrk3_get_memspace (void)
{
  static uint32_t  cached_memspace;
  struct ui_file *mf;
  struct cleanup *old_chain;
  char buf[64];

  if (mrk3_memspace_valid_p)
    return  cached_memspace;

  /* TODO: We can't tell if we have a valid target function here, because it
     is set to a value static within target.c (tcomplain). So we'll need to
     look at whether we have we have a valid value. A shame because we'll get
     an error message. */
  mf = mem_fileopen ();
  old_chain = make_cleanup_ui_file_delete (mf);

  target_rcmd ("SilentGetMemSpace", mf);
  ui_file_put (mf, mrk3_ui_memcpy, buf);

  /* Result is in mf->stream->buffer, of length mf->stream->length_buffer */
  if (strlen (buf) == 0)
    {
      /* TODO: We are presumably not connected to a target. Should we warn? Or
         should we return a default? */
      warning (_("MRK3: using default memory space (supersystem)."));
      do_cleanups (old_chain);
      /* Don't cache in this circumstance. */
      return MRK3_MEM_SPACE_SSYS;
    }
  else
    {
      /* The value is returned as a 32 bit value, with the flags in the top 4
	 bits. */
      long unsigned int res = strtol (buf, NULL, 16);
      if (mrk3_debug_memspace ())
	fprintf_unfiltered (gdb_stdlog,
			    _("MRK3: memory space buf \"%s\", val 0x%08lx.\n"),
			    buf, res);
      do_cleanups (old_chain);
      cached_memspace = res & MRK3_MEM_SPACE_MASK;
      mrk3_memspace_valid_p = 1;
      return  cached_memspace;
    }
}	/* mrk3_get_memspace () */


/*! Convenience function for the super system memory space. */
static int
mrk3_is_ssys_memspace (void)
{
  uint32_t ms = mrk3_get_memspace ();
  return ms == MRK3_MEM_SPACE_SSYS;
}


/*! Convenience function for the system memory space. */
static int
mrk3_is_sys_memspace (void)
{
  uint32_t ms = mrk3_get_memspace ();
  return ms == MRK3_MEM_SPACE_SYS;
}


/*! Convenience function for the user memory space. */
static int
mrk3_is_usr_memspace (void)
{
  uint32_t ms = mrk3_get_memspace ();
  return (ms == MRK3_MEM_SPACE_APP1) || (ms == MRK3_MEM_SPACE_APP2);
}


/*! Convenience function for the code memory type */
static int
mrk3_is_code_address (CORE_ADDR addr)
{
  return (addr & MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_CODE;

}	/* mrk3_is_code_address () */


/*! Lookup the name of a register given it's number. */
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
    "R4LONG",
    "R5LONG",
    "R6LONG",
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
      warning (_("MRK3 register name: unknown register number %d.\n"), regnum);
      return "";
    }
}


/*! Return the GDB type object for the "standard" data type
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
    case (MRK3_R4LONG_REGNUM):
      return bt_uint32;
    case (MRK3_R5LONG_REGNUM):
      return bt_uint32;
    case (MRK3_R6LONG_REGNUM):
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
      warning (_("MRK3 register type: unknown register number %d.\n"), regnum);
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

      if (mrk3_debug_memspace ())
	fprintf_unfiltered (gdb_stdlog,
			    _("MRK3: memspace for SP read is 0x%08x.\n"),
			    mrk3_get_memspace ());
      if (mrk3_is_ssys_memspace ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sys_memspace ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_usr_memspace ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("MRK3: invalid SP read mem space 0x%08x."),
		   mrk3_get_memspace ());
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

    case MRK3_R4LONG_REGNUM:
    case MRK3_R5LONG_REGNUM:
    case MRK3_R6LONG_REGNUM:
      /*  LO reg */
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R0_REGNUM + 4;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf + 2, 2);
      else
	memcpy (buf, raw_buf, 2);
      /*  HI reg */
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R4E_REGNUM;
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
      warning (_("MRK3: Not a valid pseudo reg to read %d.\n"),
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
      if (mrk3_debug_memspace ())
	fprintf_unfiltered (gdb_stdlog,
			    _("MRK3: memspace for SP write is 0x%08x.\n"),
			    mrk3_get_memspace ());
    case MRK3_SP_REGNUM:
      if (mrk3_is_ssys_memspace ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sys_memspace ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_usr_memspace ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("MRK3: invalid SP write mem space 0x%08x."),
		   mrk3_get_memspace ());
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

    case MRK3_R4LONG_REGNUM:
    case MRK3_R5LONG_REGNUM:
    case MRK3_R6LONG_REGNUM:
      /*  LO reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf + 2, buf, 2);
      else
	memcpy (raw_buf, buf, 2);
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R0_REGNUM + 4;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      /*  HI reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 2);
      else
	memcpy (raw_buf + 2, buf, 2);
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R4E_REGNUM;
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
      warning (_("MRK3: Not a valid pseudo reg to write %d.\n"),
	       cooked_regnum);
      return;
    }
}


/*! Map DWARF register number to GDB register number.

  NXP have advised the mapping used in this function.

  @note Despite the naming, GDB now uses DWARF4.

  @param[in] gdbarch  The current GDB architecture
  @param[in] dwarf2_regnum  The DWARF register number
  @return  The corresponding GDB register number. */

static int
mrk3_dwarf2_reg_to_regnum (struct gdbarch *gdbarch, int dwarf2_regnr)
{
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

      /*  R46 is a virtual 48 bit register having R4, R5 and R6 as real
	  registers. */
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
      /*  having R4LONG, R5LONG and R6LONG as real registers. */
    case 16:
      regnr = MRK3_R4LONG_REGNUM;
      break;			/*  R46e[0] */
    case 17:
      regnr = MRK3_R5LONG_REGNUM;
      break;			/*  R46e[1] */
    case 18:
      regnr = MRK3_R6LONG_REGNUM;
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
      warning (_("MRK3_dwarf2_reg_to_regnum: unknown drwarf2 regnum: %d."),
	       dwarf2_regnr);
      regnr = MRK3_R0_REGNUM;
    }

  if (mrk3_debug_dwarf ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3: DWARF2 reg %d -> reg %d\n"),
			dwarf2_regnr, regnr);
  return regnr;

}	/* mrk3_dwarf2_reg_to_regnum () */

/*! Convert target pointer to GDB address.

  @see mrk3-tdep.c for documentation of MRK3 addressing and how this is
  handled in GDB.

  This is the base routine providing the mapping from a pointer on the MRK3
  target to a unique byte address for GDB.

  @param[in] gdbarch    The current architecture
  @param[in] is_code    Non-zero (TRUE) if this is a code pointer
  @param[in] memspace  Memory space flags
  @param[in] ptr        The pointer to convert
  @return  The address. */

static CORE_ADDR
mrk3_p2a (struct gdbarch *gdbarch,
	  int             is_code,
	  uint32_t        memspace,
	  CORE_ADDR       ptr)
{
  CORE_ADDR addr;

  ptr  &= ~MRK3_MEM_MASK;		/* Just in case */
  addr  = is_code ? (ptr << 1) : ptr;
  addr |= memspace | (is_code ? MRK3_MEM_TYPE_CODE : MRK3_MEM_TYPE_DATA);

  if (mrk3_debug_ptraddr ())
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 p2a: %s %s -> %s.\n"),
			  (is_code ? "code" : "data"),
			  print_core_address (gdbarch, ptr),
			  print_core_address (gdbarch, addr));
    }

  return  addr;

}	/* mrk3_p2a () */


/*! Convert GDB address to target pointer (direct version).

  @see mrk3-tdep.c for documentation of MRK3 addressing and how this is
  handled in GDB.

  This is the base routine providing the mapping from a unique byte address in
  GDB to a pointer on the MRK3 target.

  @note  This routine assumes addr has its flags correctly set.

  @param[in] gdbarch    The current architecture
  @param[in] addr  The address to convert
  @return  The pointer */

static CORE_ADDR
mrk3_a2p (struct gdbarch *gdbarch,
	  CORE_ADDR       addr)
{
  int is_code = mrk3_is_code_address (addr);
  CORE_ADDR ptr = addr & ~MRK3_MEM_MASK;
  ptr = is_code ? (ptr >> 1) : ptr;

  if (mrk3_debug_ptraddr ())
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 a2p: %s %s -> %s.\n"),
			  is_code ? "code" : "data",
			  print_core_address (gdbarch, addr),
			  print_core_address (gdbarch, ptr));
    }

  return  ptr;

}	/* mrk3_a2p () */


/*! Add flags to a byte "address"

   We are given an address, presumed to be without flags bits (such as is
   found in symbol tables), and set the flag bits as specified.

   This gives us a true GDB "address".

   @param[in] is_code    Non-zero (TRUE) if this is a code pointer
   @param[in] memspace  Memory space flags
   @param[in] addr       The address to which flags are added

   @return  Address withouts its address space and code/data bits. */
static CORE_ADDR
mrk3_addr_bits_add (int        is_code,
		     uint32_t   memspace,
		     CORE_ADDR  addr)
{
  return (addr & ~MRK3_MEM_MASK) | memspace
    | (is_code ? MRK3_MEM_TYPE_CODE : MRK3_MEM_TYPE_DATA);

}	/* mrk3_addr_bits_add () */


/*! Convert target pointer to GDB address (buffer version).

  @see mrk3-tdep.c for documentation of MRK3 addressing and how this is
  handled in GDB.

  This is a wrapper to take the pointer in a buffer, since this is the format
  needed for the GDB architecture routine.

  @param[in] gdbarch  The current architecture
  @param[in] type     The type of the pointer/address
  @param[in] buf      The pointer to convert

  @return  The address. */

static CORE_ADDR
mrk3_pointer_to_address (struct gdbarch *gdbarch,
			 struct type    *type,
			 const gdb_byte *buf)
{
  enum bfd_endian  byte_order = gdbarch_byte_order (gdbarch);
  CORE_ADDR  ptr = extract_unsigned_integer (buf, TYPE_LENGTH (type),
					    byte_order) & ~MRK3_MEM_MASK;
  int  is_code = TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC
    || TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD
    || TYPE_CODE_SPACE (TYPE_TARGET_TYPE (type));

  return mrk3_p2a (gdbarch, is_code, mrk3_get_memspace (), ptr);

}	/* mrk3_pointer_to_address () */


/*! Convert GDB address to target pointer (buffer version).

  @see mrk3-tdep.c for documentation of MRK3 addressing and how this is
  handled in GDB.

  This is a wrapper to supply the pointer in a buffer, since this is the format
  needed for the GDB architecture routine.

  @note  Although it is not documented officially, it is the type supplied by
         the TYPE argument that matters, not any bits in the address.  To
         keep things really clear, we explicitly add the bits before passing
         on.

  @param[in]  gdbarch  The current architecture
  @param[in]  type     The type of the pointer/address
  @param[out] buf      The converted pointer
  @param[in]  addr     The address to convert */

static void
mrk3_address_to_pointer (struct gdbarch *gdbarch,
			 struct type    *type,
			 gdb_byte       *buf,
			 CORE_ADDR       addr)
{
  int  is_code = TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_FUNC
    || TYPE_CODE (TYPE_TARGET_TYPE (type)) == TYPE_CODE_METHOD
    || TYPE_CODE_SPACE (TYPE_TARGET_TYPE (type));
  uint32_t  memspace = mrk3_get_memspace ();
  CORE_ADDR ptr = mrk3_a2p (gdbarch,
			    mrk3_addr_bits_add (is_code, memspace, addr));
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);

  store_unsigned_integer (buf, TYPE_LENGTH (type), byte_order, ptr);

}	/* mrk3_address_to_pointer () */


/*! Read PC as byte address

   The PC is a word "pointer".  This function converts it to a byte "address"
   before it is used, BUT DOESN'T ADD SPACE OR TYPE FLAGS!!!

   This seems only to be used for comparing against symbol tables, which are
   all byte addresses, but don't have the flags.

   @param[in] regcache  The register cache holding the raw value.

   @return  Program counter as a byte "address" without flags. */

static CORE_ADDR
mrk3_read_pc (struct regcache *regcache)
{
  CORE_ADDR pcptr;
  CORE_ADDR pcaddr;

  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  uint32_t  memspace = mrk3_get_memspace ();
  regcache_cooked_read_unsigned (regcache, MRK3_PC_REGNUM, &pcptr);
  pcaddr = mrk3_p2a (gdbarch, 1, memspace, pcptr);

  if (mrk3_debug_ptraddr ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 read pc: ptr %s -> addr %s.\n"),
			print_core_address (gdbarch, pcptr),
			print_core_address (gdbarch, pcaddr));

  return pcaddr;

}	/* mrk3_read_pc () */


/*! Write PC supplied as a byte "address"

   The PC is a word "pointer".  This function converts it from a byte pointer
   before it is used.

   This appears to be used when setting the PC from symbol table data.  By
   comparison (@see mrk3_read_pc()), we must assume that the address supplied
   does not have flags bits set.

   @param[out] regcache  The register cache holding the raw value.
   @param[in]  pcaddr    The PC as a byte "address" without flags. */

static void
mrk3_write_pc (struct regcache *regcache, CORE_ADDR pcaddr)
{
  struct gdbarch *gdbarch = get_regcache_arch (regcache);
  CORE_ADDR pcptr;

  pcaddr = mrk3_addr_bits_add (1, mrk3_get_memspace (), pcaddr);
  pcptr = mrk3_a2p (gdbarch, pcaddr);

  regcache_cooked_write_unsigned (regcache, MRK3_PC_REGNUM, pcptr);

  if (mrk3_debug_ptraddr ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 write pc: "
				      "addr %s written as ptr %s.\n"),
			print_core_address (gdbarch, pcaddr),
			print_core_address (gdbarch, pcptr));

}	/* mrk3_write_pc () */


/*! Read from code memory.

   We are given a GDB byte address without flag bits (i.e. suitable for symbol
   table lookup), not a word pointer.

   We also need to deal with the case of reading a code location that has been
   replaced by a breakpoint instruction.

   @param[in]  gdbarch  Current architecture
   @param[in]  addr     Address in code to read from
   @param[out] buf      Where to put the result
   @param[in]  len      Number of bytes to read.
*/
static void
mrk3_read_code_memory (struct gdbarch *gdbarch,
		       CORE_ADDR       addr,
		       gdb_byte       *buf,
		       ssize_t         len)
{
  enum bfd_endian  byte_order = gdbarch_byte_order (gdbarch);
  ULONGEST  val;

  read_memory (mrk3_addr_bits_add (1, mrk3_get_memspace (), addr), buf, len);
  val = extract_unsigned_integer (buf, len, byte_order);

}	/* mrk3_read_code_memory () */


/*! Where should we actually set a breakpoint?

  The addresses are all byte addresses, not word pointers and without flags
  (i.e. from a symbol table). We can do a sanity check that it is a valid word
  address and add appropriate flag bits.

  @param[in]     gdbarch  The current architecture.
  @param[in,out] pcptr    Pointer to the desired PC address on input, and the
                          value to use on output.
  @param[out]    lenptr   Pointer to store the length of the breakpoint
                          instruction (in bytes).
  @return  Pointer to a buffer with the breakpoint instruction. */

static const gdb_byte *
mrk3_breakpoint_from_pc (struct gdbarch *gdbarch,
			 CORE_ADDR      *pcptr,
			 int            *lenptr)
{
  CORE_ADDR pc = *pcptr;

  if (mrk3_debug_general ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 breakpoint: requested at %s.\n"),
			print_core_address (gdbarch, *pcptr));

  gdb_assert ((pc & 1) == 0);
  *pcptr = pc;

  if (mrk3_debug_general ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 breakpoint: proposed at %s.\n"),
			print_core_address (gdbarch, pc));

  *lenptr = sizeof (mrk3_sim_break_insn);
  return (gdb_byte *) &mrk3_sim_break_insn;

}	/* mrk3_breakpoint_from_pc () */


/*! Skip the prologue.

   If the input address, PC, is in a function prologue, return the address of
   the end of the prologue, otherwise return the input address.

   We attempt to use the source and line data from the symtab and line data.

   MRK3 has a very simple prologue (if any).

   PUSH   R6                   ;; Only if using FP (16-bit instruction)
   SUB.w  R6,#<frame_size>     ;; Only if using FP (16- or 32-bit instruction)
   SUB.w  R7,#<stack_size>     ;;                  (16- or 32-bit instruction)

   We let prologue analysis do any validation. We just count bytes here.

   @param[in] gdbarch  The current architecture.
   @param[in] pc   The current current program counter as GDB _address_,
                       but without flag bits.

   @return  Updated PC, pointing after the prologue if the PC supplied was in
            the prologue. */

static CORE_ADDR
mrk3_skip_prologue (struct gdbarch *gdbarch,
		    CORE_ADDR       pc)
{
  CORE_ADDR  func_start;
  CORE_ADDR  func_end;

  if (mrk3_debug_frame ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 skip_prologue: pc %s.\n"),
			print_core_address (gdbarch, pc));

  if (find_pc_partial_function (pc, NULL, &func_start, &func_end))
    {
      CORE_ADDR  prologue_end = skip_prologue_using_sal (gdbarch, func_start);
      uint16_t  insn16;

      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog,
			      _("MRK3 skip_prologue: func_start %s.\n"),
			      print_core_address (gdbarch, func_start));
	  fprintf_unfiltered (gdb_stdlog,
			      _("MRK3 skip_prologue: prologue_end %s.\n"),
			      print_core_address (gdbarch, prologue_end));
	}

      /* @todo SAL is broken at present. This means that we rely on reading
	       the code to work out the prologue.  So we had better have the
	       program loaded in memory to get this right. */

      /* if (0 != prologue_end) */
      /* 	return  prologue_end;	/\* SAL gave us the answer. *\/ */

      /* SAL didn't give us what we want, so count forward by steam from the
	 PC, which is now at the start of the function. If the first
	 instruction is PUSH R6, we have a FP, and a 5 word (10 byte)
	 prologue, if SUB.w R7,#<offset> we have a simple stack frame and a 2
	 word (4 byte) prologue, else we have no stack frame and no
	 prologue. */
      prologue_end = pc;
      mrk3_read_code_memory (gdbarch, prologue_end, (gdb_byte *) &insn16,
			     sizeof (insn16));

      if (0x57a0 == insn16)
	{
	  /* We have a FP being set up. We skip either 2 or 3 words, depending
	     on whether we use a 16- or 32-bit sub instruction to set up the
	     new FP. */
	  prologue_end += 2;
	  mrk3_read_code_memory (gdbarch, prologue_end, (gdb_byte *) &insn16,
				 sizeof (insn16));

	  /* 16- or 32-bit sub.w */
	  prologue_end += (0x0486 == (insn16 & 0xff87)) ? 2 : 4;
	  mrk3_read_code_memory (gdbarch, prologue_end, (gdb_byte *) &insn16,
				 sizeof (insn16));
	}

      prologue_end += (0x0407 == insn16)
	? 4					/* 32-bit SP set up */
	: (0x0487 == (insn16 & 0xff87)) ? 2	/* 16-bit SP set up */
	: 0;					/* No stack frame */

      if (mrk3_debug_frame ())
	fprintf_unfiltered (gdb_stdlog, _("MRK3 skip prolog to %s.\n"),
			    print_core_address (gdbarch, prologue_end));

      return  prologue_end;
    }
  else
    {
      /* Can't find the function. Give up.

         @todo Should we consider disassembling to look for a prologue
	 pattern? */
      if (mrk3_debug_frame ())
	fprintf_unfiltered (gdb_stdlog, _("MRK3 no function found.\n"));

      return  pc;
    }
}	/* mrk3_skip_prologue () */

/*! Decode a const4 from an instruction.

  The const4 immediates, as used in short instructions are encoded using a
  special mapping table, this allows for some special values to be placed
  in the table at high offsets.

  This function takes a value as encoded in an instruction, and returns the
  const4 value this represents.

  @param[in]   indx   The offset into the const4 table as encoded in the
                      instruction.

  @parma[in]   is_word_insn_p   If this is true (non zero) then the source
                                instruction is word based (has the word bit
                                set), and the immediate returned will be
                                16-bits long.  If this is false (zero) then
				only the lower 8 bits of the immediate
                                returned will be valid.

  @return   Returns a value which is a const4 for use in instructions.  */

static int16_t
mrk3_decode_const4 (uint8_t indx, int is_word_insn_p)
{
  static uint16_t const4_table []
    = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, -1, 16, 32, 64, 128 };
  uint16_t const4_mask = (is_word_insn_p ? 0xffff : 0x00ff);

  gdb_assert (indx < sizeof (const4_table) / sizeof (const4_table [0]));

  return const4_table [indx] & const4_mask;
}


/*! Determine the value of registers in the PREV frame and save them in the
   prologue cache for THIS frame. Note that in general we cannot just unwind
   registers here, or we'll end up with a circular dependency.

   The MRK3 prologue is simple. @see mrk3_skip_prologue() for details.

   @param[in]  this_frame  THIS frame info, from which we can get THIS program
                           counter and THIS stack pointer.  May be null, in
                           which case we use defaults values.
   @param[out] this_cache  The cache of registers in the PREV frame, which we
                           should initialize. */
static void
mrk3_analyze_prologue (struct frame_info *this_frame,
		       struct mrk3_frame_cache *this_cache)
{
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  uint32_t  memspace = mrk3_get_memspace ();

  /* Function starts are symbols, so GDB addresses, _without_ flag bits. */
  CORE_ADDR  func_start = get_frame_func (this_frame);
  /* Similarly frame PCs are symbols, so GDB addresses, _without_ flag bits */
  CORE_ADDR  this_pc = get_frame_pc (this_frame);
  /* The value in the SP is a data pointer. */
  CORE_ADDR  this_sp = get_frame_register_unsigned (this_frame, MRK3_SP_REGNUM);

  int  have_fp_p;		/* If we update the FP */
  int  have_sp_p;		/* If we update the SP */
  int  fp_pushed_p;		/* If we have pushed the FP for this frame */
  int  fp_updated_p;		/* If we have updated the FP for this frame */
  int  sp_updated_p;		/* If we have updated the SP for this frame */

  CORE_ADDR fp_offset;		/* How much FP is incremented (if used) */
  CORE_ADDR sp_offset;		/* How much SP is incremented (if used) */

  CORE_ADDR  prev_sp;
  CORE_ADDR  prev_pc_addr;

  /* We need to count through the code from the start of the function. */
  CORE_ADDR  pc;
  uint16_t  insn16;

  /* Turn values into full GDB addresses. */
  func_start = mrk3_addr_bits_add (1, memspace, func_start);
  this_pc = mrk3_addr_bits_add (1, memspace, this_pc);
  this_sp = mrk3_p2a (gdbarch, 0, memspace, this_sp);

  pc = func_start;

  /* Initialise the traditional frame cache component.  */
  gdb_assert (this_cache->saved_regs == NULL);
  this_cache->saved_regs = trad_frame_alloc_saved_regs (this_frame);

  /* For now, always mark the previous pc as on the stack.  */
  this_cache->prev_pc_on_stack = 1;

  if (mrk3_debug_frame ())
    {
      gdb_flush (gdb_stdout);
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: frame_cache %p.\n"),
			  this_cache);
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: at entry func_start %s.\n"),
			  print_core_address (gdbarch, func_start));
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: at entry this_pc %s.\n"),
			  print_core_address (gdbarch, this_pc));
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: at entry this_sp %s.\n"),
			  print_core_address (gdbarch, this_sp));
    }

  /* If we don't have a function start, then we can't do any meaningful
     analysis. We can set the ID and base, but we just leave the register
     cache "as is", with its default values being that the reg in this frame
     is the reg in the previous frame. */
  if (0 == pc)
    {
      /* Set the frame ID (which uses full GDB addresses) and frame base */
      this_cache->base = this_sp;

      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog,
			      _("MRK3 prologue: No function start.\n"));
	}

      return;
    }

  /* Set up the cache. Generally previous frame's registers are unknown. The
     execptions are:
     - the previous PC, which is either the last thing on the previous stack
       frame, or if we have a FP the penultimate thing on the previous stack
       frame.
     - the previous R6, in case where we are using this as a FP, in which case
       it is the last thing on the previous stack frame.
     - the previous SP, which can be calculated as a fixed amount from the
       current SP.

     To work these out, we need to know exactly where we have stopped, and
     whether we have a FP, so we can deduce the value of the previous frame
     SP. */

  /* Default values until we find out where we are. */
  have_fp_p = 0;
  have_sp_p = 0;
  fp_pushed_p = 0;
  fp_updated_p = 0;
  sp_updated_p = 0;

  /* Walk the prologue. */

  mrk3_read_code_memory (gdbarch, pc, (gdb_byte *) &insn16,
			 sizeof (insn16));

  if (0x57a0 == insn16)
    {
      /* PUSH R6: Have FP. In this case, the prev FP is pushed on the stack. */
      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog,
			      _("MRK3 prologue: Found PUSH R6 at %s.\n"),
			      print_core_address (gdbarch, pc));
	}

      have_fp_p = 1;
      pc += 2;
      fp_pushed_p = (this_pc >= pc);

      /* Check we really have SUB.w R6,#<frame_size> here. This could be the
	 short or long version */
      mrk3_read_code_memory (gdbarch, pc, (gdb_byte *) &insn16,
			     sizeof (insn16));
      if (0x0406 == insn16)
	{
	  /* Long SUB.w. Offset is in the second word */
	  pc += 2;
	  mrk3_read_code_memory (gdbarch, pc, (gdb_byte *) &insn16,
				 sizeof (insn16));
	  fp_offset = (CORE_ADDR) insn16;
	  pc += 2;			/* Full instr has 16-bit const */
	  fp_updated_p = (this_pc >= pc);
	}
      else if (0x0486 == (insn16 & 0xff87))
	{
	  /* Short SUB.w. Offset is in bits [6:3] */
	  fp_offset = mrk3_decode_const4 ((insn16 & 0x78) >> 3, TRUE);

	  pc += 2;
	  fp_updated_p = (this_pc >= pc);
	}
      else
	{
	  warning (_("FP pushed, but no new value set."));
	  fp_offset = 0;
	  fp_updated_p = 0;
	}

      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Have FP.\n"));
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: FP pushed: %s.\n"),
			      fp_pushed_p ? "yes" : "no");
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: FP updated: %s.\n"),
			      fp_updated_p ? "yes" : "no");
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: FP offset: %s.\n"),
			      hex_string_custom ((LONGEST) fp_offset, 4));
	}

      /* Update the instruction we are looking at. */
      mrk3_read_code_memory (gdbarch, pc, (gdb_byte *) &insn16,
			     sizeof (insn16));
    }

  /* Check we have SUB.w R7,#<stack_size> (this may be short or long form).
     If not we may not have a stack frame at all, so give up, setting ID and
     frame base and leaving the register cache "as is". */
  if (0x0407 == insn16)
    {
      /* Long SUB.w. Offset is in second word. */
      have_sp_p = 1;
      pc += 2;
      /* We seem to have a valid prologue, so get the stack size */
      mrk3_read_code_memory (gdbarch, pc, (gdb_byte *) &insn16,
			     sizeof (insn16));
      sp_offset = (CORE_ADDR) insn16;
      pc += 2;

      /* Have we yet updated the SP? */
      sp_updated_p = (this_pc >= pc);
    }
  else if (0x0487 == (insn16 & 0xff87))
    {
      /* Short SUB.w. Offset is in bits [6:3]. */
      have_sp_p = 1;
      sp_offset = mrk3_decode_const4 ((insn16 & 0x78) >> 3, TRUE);
      pc += 2;

      /* Have we yet updated the SP? */
      sp_updated_p = (this_pc >= pc);
    }
  else
    sp_offset = 0;			/* Default initialization. */

  if (mrk3_debug_frame () && have_sp_p)
    {
      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Have SP.\n"));
      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: SP offset %s.\n"),
			  hex_string_custom ((LONGEST) sp_offset, 4));
      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: SP updated: %s.\n"),
			  sp_updated_p ? "yes" : "no");
    }
  if (mrk3_debug_frame () && have_fp_p)
    {
      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Have FP.\n"));
      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: FP pushed: %s.\n"),
			  fp_pushed_p ? "yes" : "no");
    }

  /* Populate the cache. */

  /* this_sp and this_fp hold the values currently in the registers. Are these
     the PREV or THIS values. Don't forget the 2 bytes of stack from the call */
  if (have_sp_p)
    {
      if (sp_updated_p)
	{
	  if (have_fp_p)
	    prev_sp = this_sp + 2 + (fp_pushed_p ? 2 : 0) + sp_offset;
	  else
	    prev_sp = this_sp + 2 + sp_offset;
	}
      else
	{
	  if (have_fp_p)
	    {
	      prev_sp = this_sp + 2 + (fp_pushed_p ? 2 : 0);
	      this_sp = prev_sp - 2 - sp_offset - (fp_pushed_p ? 2 : 0);
	    }
	  else
	    {
	      prev_sp = this_sp + 2;
	      this_sp = prev_sp - 2 - sp_offset;
	    }
	}
    }
  else
    {
      /* Could conceivably have FP but no SP? */
      prev_sp = this_sp + 2 + (fp_pushed_p ? 2 : 0);
    }

  if (mrk3_debug_frame ())
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: Prev SP has value 0x%s.\n"),
			  print_core_address (gdbarch, prev_sp));
    }

  /* PC is saved on the start of this stack */
  prev_pc_addr = prev_sp - 2;
  this_cache->saved_regs [MRK3_PC_REGNUM].addr = prev_pc_addr;

  if (mrk3_debug_frame ())
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: Prev PC at addr 0x%s.\n"),
			  print_core_address (gdbarch, prev_pc_addr));
    }

  /* If we have a FP it may be pushed onto the stack. */
  if (have_fp_p)
    {
      if (fp_pushed_p)
	{
	  CORE_ADDR prev_fp_addr = prev_pc_addr - 2;
	  this_cache->saved_regs [MRK3_FP_REGNUM].addr = prev_fp_addr;

	  if (mrk3_debug_frame ())
	    {
	      fprintf_unfiltered (gdb_stdlog,
				  _("MRK3 prologue: Prev FP at addr 0x%s.\n"),
				  print_core_address (gdbarch, prev_fp_addr));
	    }
	}
      else
	{
	  this_cache->saved_regs [MRK3_FP_REGNUM].realreg = MRK3_FP_REGNUM;
	  if (mrk3_debug_frame ())
	    {
	      fprintf_unfiltered (gdb_stdlog,
				  _("MRK3 prologue: Prev FP in reg %d.\n"),
				  MRK3_FP_REGNUM);
	    }
	}
    }

  /* All other registers are just in themselves for now. */

  /* Set the frame ID and frame base */
  if (mrk3_debug_frame ())
    {
      fprintf_unfiltered (gdb_stdlog,
			  _("MRK3 prologue: frame_id (%s, %s)\n"),
			  print_core_address (gdbarch, this_sp),
			  print_core_address (gdbarch, func_start));
    }

  /* The frame ID uses full GDB addresses */
  this_cache->base = prev_sp;

  /* gdbarch_sp_regnum contains the value and not the address.  */
  trad_frame_set_value (this_cache->saved_regs,
                        MRK3_SP_REGNUM,
                        this_cache->base);

}	/* mrk3_analyse_prologue */

static struct mrk3_frame_cache *
mrk3_alloc_frame_cache (void)
{
  struct mrk3_frame_cache *cache;

  cache = FRAME_OBSTACK_ZALLOC (struct mrk3_frame_cache);
  cache->saved_regs = NULL;

  return cache;
}


/*! Populate the frame cache if it doesn't exist. */
static struct mrk3_frame_cache *
mrk3_frame_cache (struct frame_info *this_frame,
		  void **this_cache)
{
  if (mrk3_debug_general ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3: frame_cache = %p\n"), *this_cache);

  if (!*this_cache)
    {
      struct gdbarch *gdbarch = get_frame_arch (this_frame);
      CORE_ADDR func_start;
      CORE_ADDR stop_addr;

      *this_cache = mrk3_alloc_frame_cache ();
      func_start = get_frame_func (this_frame);
      stop_addr = mrk3_p2a (gdbarch, 1, mrk3_get_memspace (),
			    get_frame_pc (this_frame));

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
  struct mrk3_frame_cache *frame_cache =
    mrk3_frame_cache (this_frame, this_cache);
  return frame_cache->base;
}


static void
mrk3_frame_this_id (struct frame_info *this_frame,
		    void **this_cache, struct frame_id *this_id)
{
  CORE_ADDR base = mrk3_frame_base_address (this_frame, this_cache);

  if (base)
    {
      uint32_t  memspace = mrk3_get_memspace ();
      CORE_ADDR pc = get_frame_func (this_frame);

      /* Frame ID's always use full GDB addresses */
      base = mrk3_addr_bits_add (0, memspace, base);
      pc   = mrk3_addr_bits_add (1, memspace, pc);

      *this_id = frame_id_build (base, pc);
    }

  /* Otherwise, leave it unset, and that will terminate the backtrace.  */
}


static struct value *
mrk3_frame_prev_register (struct frame_info *this_frame,
			  void **this_cache, int regnum)
{
  struct value *val = NULL;
  struct mrk3_frame_cache *frame_cache =
    mrk3_frame_cache (this_frame, this_cache);

  if (regnum == MRK3_PC_REGNUM
      && trad_frame_addr_p (frame_cache->saved_regs, MRK3_PC_REGNUM)
      && frame_cache->prev_pc_on_stack)
    {
      uint16_t pc;

      read_memory (frame_cache->saved_regs[MRK3_PC_REGNUM].addr,
		   (gdb_byte *) &pc, 2); /* Read 2 bytes only.  */

      val = frame_unwind_got_constant (this_frame, regnum, pc);
    }
  else
    val = trad_frame_get_prev_register (this_frame,
					frame_cache->saved_regs,
					regnum);

  gdb_assert (val != NULL);
  return val;
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


/*! Unwind the program counter for THIS frame.

  This is a nightmare. What we want is a value that can be looked up in a
  symbol table. I.e. a GDB address, but without any high order bits.

  @note  When printing a register we get the raw (word address) value, which
         is a relief.

  @todo  This is a complete (upstream) mess.

  @param[in] gdbarch     The current architecture.
  @param[in] next_frame  Frame info for NEXT frame.

  @return  The program counter in THIS frame as a GDB _address_, but without
           any higher order bits set. */

static CORE_ADDR
mrk3_unwind_pc (struct gdbarch    *gdbarch,
		struct frame_info *next_frame)
{
  CORE_ADDR pcptr = frame_unwind_register_unsigned (next_frame, MRK3_PC_REGNUM);
  CORE_ADDR pcaddr = mrk3_p2a (gdbarch, 1, mrk3_get_memspace (), pcptr);

  if (mrk3_debug_ptraddr ())
    fprintf_unfiltered (gdb_stdlog, _("unwind PC value %s.\n"),
			print_core_address (gdbarch, pcaddr));

  return  pcaddr;

}	/* mrk3_unwind_pc () */


/*! Unwind the stack pointer for THIS frame.

   @todo Should we instead convert the raw pointer to a GDB "address",
         i.e. with byte address and flags?

   @param[in] gdbarch     The current architecture.
   @param[in] next_frame  Frame info for NEXT frame.

   @return  The stack pointer in THIS frame as a GDB "pointer". */

static CORE_ADDR
mrk3_unwind_sp (struct gdbarch *gdbarch, struct frame_info *next_frame)
{
  return frame_unwind_register_unsigned (next_frame, MRK3_SP_REGNUM);

}	/* mrk3_unwind_sp () */


static int
mrk3_address_class_type_flags (int byte_size, int dwarf2_addr_class)
{
  if (byte_size == 2)
    return TYPE_INSTANCE_FLAG_ADDRESS_CLASS_1;
  else
    return 0;
}

/* Skip whitespace in BUF, update BUF.  Return non-zero if at least one
   whitespace is skipped, otherwise return zero.  */
static int
skip_whitespace (char **buf)
{
  char *tmp = *buf;

  while (ISSPACE (*tmp))
    tmp++;

  if (tmp == *buf)
    return 0;

  *buf = tmp;
  return 1;
}

/* Copy 4 digit hex string from string pointed to by BUF into DST, and add
   a null terminator to DST.  Buffer DST is SIZE bytes long, including
   space for null character, this must be 5 or more then.  This is
   checked (by an assertion).  Afterwards BUF is updated past the hex
   string.  The hex string can optionally have a '0x' prefix, this is
   skipped, and not copied.  Return non-zero if a hex string is copied
   successfully, in which case DST is valid, otherwise return zero, and DST
   is not valid.  */
static int
parse_opcode_hex (char **buf, char *dst, size_t size)
{
  char *tmp = *buf;
  size_t can_copy;

  /* Skip '0x' prefix if there is one.  */
  if (tmp [0] == '0' && tmp [1] == 'x')
    tmp += 2;

  /* Should now have 4 hex characters, that we copy into DST, then add a
     NULL pointer at the end (of DST).  */
  gdb_assert (size > 4);
  for (can_copy = 4;
       can_copy > 0 && ISXDIGIT (*tmp);
       --can_copy)
    *dst++ = *tmp++;
  *dst = '\0';

  if (can_copy > 0)
    return 0; /* Failed to extract 4 hex digits.  */

  *buf = tmp;
  return 1;
}

/* Parse opcode string and arguments out of string pointed to by BUF, and
   update BUF to skip the characters read.
   See the comment on mrk3_fancy_print_insn for a description of the
   incoming instruction format.
   The OPCODE, ARG1, and ARG2 buffers are filled, and null characters added
   to the end of each, with no more than OPCODE_LEN, ARG1_LEN, and ARG2_LEN
   bytes being written to each buffer respectively.
   If the instruction string in BUF is parsed then non-zero is returned and
   the output buffers are valid, otherwise zero is returned and the output
   buffers are invalid.
   The NARGS is update to either 0, 1, or 2 to indicate how many of ARG1
   and ARG2 are valid.  NARGS only applies when this function returns
   non-zero, if this function returns zero then NARGS is meaningless and
   should not be checked.  */
static int
parse_opcode_and_args (char **buf, char *opcode, size_t opcode_len,
		       char *arg1, size_t arg1_len,
		       char *arg2, size_t arg2_len,
		       int *nargs)
{
  size_t can_copy;
  char *tmp;

  /* Now copy everything up to the next whitespace into OPCODE, however
     don't copy more than (OPCODE_LEN - 1) characters, then add a \0 to
     the end of OPCODE.  */
  for (can_copy = opcode_len - 1;
       can_copy > 0 && !ISSPACE (**buf) && **buf != '\0';
       --can_copy)
    *opcode++ = *(*buf)++;
  *opcode = '\0';

  /* Ooops, looks like the opcode is too long.  */
  if (can_copy == 0 && !(ISSPACE (**buf) || **buf == '\0'))
    return 0;

  /* End of the string?  No arguments then.  */
  if (**buf == '\0')
    {
      *nargs = 0;
      return 1;
    }

  /* OK, we're expecting some arguments.  */
  if (!skip_whitespace (buf))
    return 0;

  /* Now parse first argument, up to either whitespace, end of string, or
     comma between arguments.  Copy the argument into the ARG1 buffer, and
     add a null character to the ARG1 buffer.  */
  for (can_copy = arg1_len - 1;
       can_copy > 0 && !ISSPACE (**buf) && **buf != '\0' && **buf != ',';
       --can_copy)
    *arg1++ = *(*buf)++;
  *arg1 = '\0';

  /* Ooops, looks like arg1 is too long.  */
  if (can_copy == 0 && !(ISSPACE (**buf) || **buf == '\0' || **buf == ','))
    return 0;

  /* Skip any whitespace. */
  skip_whitespace (buf);

  /* End of string?  No second argument then.  */
  if (**buf == '\0')
    {
      *nargs = 1;
      return 1;
    }

  /* We're expecting a second argument, skip the comma.  */
  if (**buf != ',')
    return 0;
  (*buf)++;

  /* And skip any whitespace.  */
  skip_whitespace (buf);

  /* Now copy the second argument, just like the first.  To deal with
     instructions like POP, we copy everything up to the newline, ignoring
     any spaces. */
  for (can_copy = arg2_len - 1; can_copy > 0 && **buf != '\0';)
    if (!ISSPACE (**buf))
      {
	*arg2++ = *(*buf)++;
	--can_copy;
      }
    else
      {
	(*buf)++;
      }

  *arg2 = '\0';

  /* Ooops, looks like arg2 is too long.  */
  if (can_copy == 0 && !(ISSPACE (**buf) || **buf == '\0'))
    return 0;

  /* Skip any whitespace. */
  skip_whitespace (buf);

  /* Should be at the end of the string now.  */
  if (**buf == '\0')
    {
      *nargs = 2;
      return 1;
    }

  return 0;
}


/*! Fancy disassembler

   We are passed the size of an instruction to disassemble and the text
   string. We break out the string and try to print if with symbolic
   information.

   General format:

     <word1> [<word2>] [<word3>] <opcode> [<arg1>] [, <arg2>]

   @param[in] addr  Address being disassembled
   @param[in] info  Details of the disassembly
   @param[in] size  Number of bytes in the instruction
   @param[in] buf   The instruction string */
static void
mrk3_fancy_print_insn (CORE_ADDR         addr,
		       disassemble_info *info,
		       int               size,
		       char              buf[])
{
  int  nargs;			/* Number of args to opcode */
  char hw1[5];			/* First half word of instr */
  char hw2[5];			/* Second half word of instr */
  char hw3[5];			/* Third half word of instr */
  char opc[9];			/* Opcode */
  char argsep[2];		/* Argument separator */
  char arg1[14];		/* First argument, inc separator */
  char arg2[18];		/* Second argument */
  char allargs[27];             /* For neat printing */
  char supstr[32];		/* Supplementary info about instr */
  char *orig_buf = buf;

  /* Set defaults for optional elements */
  strcpy (hw2, "    ");
  strcpy (hw3, "    ");
  argsep[0] = '\0';
  arg1[0] = '\0';
  arg2[0] = '\0';
  supstr[0] = '\0';

  switch (size)
    {
    case 2:
      skip_whitespace (&buf);		// Optional.
      if (!(parse_opcode_hex (&buf, hw1, 5)
	    && skip_whitespace (&buf)
	    && parse_opcode_and_args (&buf, opc, 9,
				      arg1, 14, arg2, 18,
				      &nargs)))
	goto parse_failure;
      break;

    case 4:
      skip_whitespace (&buf);		// Optional.
      if (!(parse_opcode_hex (&buf, hw1, 5)
      	    && skip_whitespace (&buf)
      	    && parse_opcode_hex (&buf, hw2, 5)
      	    && skip_whitespace (&buf)
      	    && parse_opcode_and_args (&buf, opc, 9,
      				      arg1, 14, arg2, 18,
      				      &nargs)))
      	goto parse_failure;
      break;

    case 6:
      skip_whitespace (&buf);		// Optional.
      if (!(parse_opcode_hex (&buf, hw1, 5)
	    && skip_whitespace (&buf)
	    && parse_opcode_hex (&buf, hw2, 5)
	    && skip_whitespace (&buf)
	    && parse_opcode_hex (&buf, hw3, 5)
	    && skip_whitespace (&buf)
	    && parse_opcode_and_args (&buf, opc, 9,
				      arg1, 14, arg2, 18,
				      &nargs)))
	goto parse_failure;
      break;

    parse_failure:
      warning (_("Unable to parse `%s'"), orig_buf);
      return;

    default:
      warning (_("Invalid inst length to disassemble %d\n"), size);
      return;
    }

  if (2 == nargs)
    strcpy (argsep, ",");

  /* Work out any useful supplementary information. */
  if ((0 == strncmp ("BEQ", opc, strlen ("BRA")))
      || (0 == strncmp ("BNE", opc, strlen ("BRA")))
      || (0 == strncmp ("BLE", opc, strlen ("BRA")))
      || (0 == strncmp ("BG", opc, strlen ("BRA")))
      || (0 == strncmp ("BL", opc, strlen ("BRA")))
      || (0 == strncmp ("BGE", opc, strlen ("BRA")))
      || (0 == strncmp ("BLE", opc, strlen ("BRA")))
      || (0 == strncmp ("BA", opc, strlen ("BRA")))
      || (0 == strncmp ("BB", opc, strlen ("BRA")))
      || (0 == strncmp ("BAE", opc, strlen ("BRA")))
      || (0 == strncmp ("BRA", opc, strlen ("BRA")))
      || (0 == strncmp ("BO", opc, strlen ("BRA")))
      || (0 == strncmp ("BN", opc, strlen ("BRA")))
      || (0 == strncmp ("BP", opc, strlen ("BRA"))))
    {
      /* A branch instruction. Work out the absolute address. The argument
	 will be of the form 0xhhhh and is *word* offset. */
      long unsigned int argval = strtoul (arg1, NULL, 0);
      CORE_ADDR target_addr;

      target_addr = (argval > 0x7fff)
	? addr - 2 * (0x10000 - argval)
	: addr + 2 * argval;
      /* Can't use print_core_address, since no gdbarch */
      sprintf (supstr, "<%s>", hex_string_custom (target_addr, 8));
    }
  else if (0 == strcmp ("FCALL", opc))
    {
      /* Call a function. Try to find a symbolic representation of its
	 name. The argument is a word address. */
      CORE_ADDR target = strtoul (arg1, NULL, 0) * 2;
      struct obj_section *section = find_pc_section (target);
      struct symbol *f = find_pc_sect_function (target, section);

      if (f != NULL)
	strncpy (arg1, SYMBOL_LINKAGE_NAME (f), 13);
    }

  sprintf (allargs, "%s%s%s", arg1, argsep, arg2);
  (*info->fprintf_func) (info->stream, "%4s %4s %4s %-8s  %-16s %s", hw1, hw2,
			 hw3, opc, allargs, supstr);

}	/* mark3_fancy_print_insn () */


/*! Diassembler

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
      /* Can't use print_core_address, since no gdbarch */
      sprintf (cmd, "silent-disas %s\n", hex_string_custom (addr, 8));
      target_rcmd (cmd, mf);
      /* Result is in mf->stream->buffer, of length
	 mf->stream->length_buffer. Extract it into buf. */
      ui_file_put (mf, mrk3_ui_memcpy, buf);

      if (strlen (buf) == 0)
	{
	  /* TODO: What do we do if something goes wrong? */
	  warning (_("MRK3: Unable to diassemble."));
	  do_cleanups (old_chain);
	  return -1;
	}
      else
	{
	  // Print the instruction if we had one.
	  int size = buf [0] - '0';		// Yuk - what if we have
						// EBCDIC
	  if (size > 0)
	    mrk3_fancy_print_insn ((CORE_ADDR) addr, info, size, &(buf[2]));

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

static int
mrk3_convert_register_p (struct gdbarch *gdbarch, int regnum,
			 struct type *type)
{
  return (mrk3_register_type (gdbarch, regnum)->length != type->length
	  && TYPE_CODE (type) == TYPE_CODE_PTR);
}

static int
mrk3_register_to_value (struct frame_info *frame, int regnum,
			struct type *valtype, gdb_byte *out,
			int *optimizedp, int *unavailablep)
{
  struct gdbarch *gdbarch = get_frame_arch (frame);

  memset (out, 0, TYPE_LENGTH (valtype));
  if (!get_frame_register_bytes (frame, regnum, 0,
				 register_size (gdbarch, regnum),
				 out, optimizedp, unavailablep))
    return 0;

  return 1;
}

static void
mrk3_value_to_register (struct frame_info *frame, int regnum,
			struct type *valtype, const gdb_byte *from)
{
  int len;
  struct gdbarch *gdbarch;
  struct type *reg_type;

  len = TYPE_LENGTH (valtype);
  gdbarch = get_frame_arch (frame);
  reg_type = gdbarch_register_type (gdbarch, regnum);

  gdb_assert (len == 4);
  gdb_assert (TYPE_LENGTH (reg_type) == 2);

  if (from [2] != 0 || from [3] != 0)
    error ("setting 2-byte register using 4-byte value");

  put_frame_register (frame, regnum, from);
}

/* Extract from REGCACHE a function return value of type TYPE and copy that
   into VALBUF.  */

static void
mrk3_extract_return_value (struct type *type, struct regcache *regcache,
			   gdb_byte *valbuf)
{
  switch (TYPE_LENGTH (type))
    {
    case 1:
      regcache_raw_read (regcache, MRK3_R0L_REGNUM, valbuf);
      break;
    case 2:
      regcache_raw_read (regcache, MRK3_R0_REGNUM, valbuf);
      break;
    case 4:
      /* Need to read from R0 and R1.  */
      regcache_raw_read (regcache, MRK3_R0_REGNUM, valbuf);
      regcache_raw_read (regcache, MRK3_R0_REGNUM + 1, valbuf + 2);
      break;
    default:
      error ("unable to extract return value of size %d bytes",
	     TYPE_LENGTH (type));
    }
}

/* Stores a function return value of type TYPE, where VALBUF is the address
   of the value to be stored, into REGCACHE.  */

static void
mrk3_store_return_value (struct type *type, struct regcache *regcache,
			 const gdb_byte *valbuf)
{
  switch (TYPE_LENGTH (type))
    {
    case 1:
      regcache_raw_write (regcache, MRK3_R0L_REGNUM, valbuf);
      break;
    case 2:
      regcache_raw_write (regcache, MRK3_R0_REGNUM, valbuf);
      break;
    case 4:
      /* Need to write to R0 and R1.  */
      regcache_raw_write (regcache, MRK3_R0_REGNUM, valbuf);
      regcache_raw_write (regcache, MRK3_R0_REGNUM + 1, valbuf + 2);
      break;
    default:
      error ("unable to store return value of size %d bytes",
	     TYPE_LENGTH (type));
    }
}

/* Determine, for architecture GDBARCH, how a return value of TYPE
   should be returned.  If it is supposed to be returned in registers,
   and READBUF is non-zero, read the appropriate value from REGCACHE,
   and copy it into READBUF.  If WRITEBUF is non-zero, write the value
   from WRITEBUF into REGCACHE.  */

static enum return_value_convention
mrk3_return_value (struct gdbarch *gdbarch, struct value *function,
		   struct type *valtype, struct regcache *regcache,
		   gdb_byte *readbuf, const gdb_byte *writebuf)
{
  if (TYPE_CODE (valtype) == TYPE_CODE_STRUCT
      || TYPE_CODE (valtype) == TYPE_CODE_UNION
      || TYPE_CODE (valtype) == TYPE_CODE_ARRAY)
    {
      if (readbuf)
	{
	  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
	  struct gdbarch *gdbarch = get_regcache_arch (regcache);
	  ULONGEST addr;
	  bfd_byte buf[MAX_REGISTER_SIZE];

	  mrk3_pseudo_register_read (gdbarch, regcache,
				     MRK3_SP_REGNUM, buf);
	  addr = extract_unsigned_integer
	    (buf, register_size (gdbarch, MRK3_SP_REGNUM), byte_order);
	  read_memory (addr, readbuf, TYPE_LENGTH (valtype));
	}

      return RETURN_VALUE_ABI_RETURNS_ADDRESS;
    }

  if (readbuf)
    mrk3_extract_return_value (valtype, regcache, readbuf);
  if (writebuf)
    mrk3_store_return_value (valtype, regcache, writebuf);

  return RETURN_VALUE_REGISTER_CONVENTION;
}

static const char *
mrk3_unwind_stop_at_frame_p (struct gdbarch *gdbarch,
			     struct frame_info *this_frame)
{
  CORE_ADDR frame_pc;
  int frame_pc_p;

  gdb_assert (this_frame != NULL);
  frame_pc_p = get_frame_pc_if_available (this_frame, &frame_pc);

  if (frame_pc_p && mrk3_a2p (gdbarch, frame_pc) == 0)
    return "zero PC";

  return NULL; /* False, don't stop here.  */
}


/*! Initialize the gdbarch structure for the mrk3. */
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
  gdbarch = gdbarch_alloc (&info, tdep);  /* @todo: Can we use NULL for tdep?
					     We do never use tdep anyway. */
  set_gdbarch_address_class_type_flags (gdbarch,
					mrk3_address_class_type_flags);
  set_gdbarch_short_bit (gdbarch, 1 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
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

  set_gdbarch_return_value (gdbarch, mrk3_return_value);
  /* We don't currently have a proper disassembler, so we'll provide our own
     locally. The real one should be in opcodes/mrk3-dis.c (part of
     binutils). */
  set_gdbarch_print_insn (gdbarch, mrk3_print_insn);

  /* set_gdbarch_push_dummy_call (gdbarch, dummy_push_dummy_call); */
  set_gdbarch_dwarf2_reg_to_regnum (gdbarch, mrk3_dwarf2_reg_to_regnum);

  set_gdbarch_address_to_pointer (gdbarch, mrk3_address_to_pointer);
  set_gdbarch_pointer_to_address (gdbarch, mrk3_pointer_to_address);

  set_gdbarch_skip_prologue (gdbarch, mrk3_skip_prologue);
  set_gdbarch_inner_than (gdbarch, core_addr_lessthan);

  set_gdbarch_decr_pc_after_break (gdbarch, 2);
  set_gdbarch_breakpoint_from_pc (gdbarch, mrk3_breakpoint_from_pc);

  frame_unwind_append_unwinder (gdbarch, &mrk3_frame_unwind);
  frame_base_set_default (gdbarch, &mrk3_frame_base);

  /* set_gdbarch_dummy_id (gdbarch, mrk3_dummy_id); */

  set_gdbarch_unwind_pc (gdbarch, mrk3_unwind_pc);
  set_gdbarch_unwind_sp (gdbarch, mrk3_unwind_sp);

  set_gdbarch_convert_register_p (gdbarch, mrk3_convert_register_p);
  set_gdbarch_register_to_value (gdbarch, mrk3_register_to_value);
  set_gdbarch_value_to_register (gdbarch, mrk3_value_to_register);

  set_gdbarch_unwind_stop_at_frame_p (gdbarch, mrk3_unwind_stop_at_frame_p);

  return gdbarch;
}

/*! Dump out the target specific information. Currently we have none. */
static void
mrk3_dump_tdep (struct gdbarch *gdbarch, struct ui_file *file)
{
  struct gdbarch_tdep *tdep = gdbarch_tdep (gdbarch);
  fprintf_unfiltered (file, "MRK3: No target dependencies to dump\n");

}	/* mrk3_dump_tdep () */


/*! Handler for the inferior stopping.

  We use this to flush the cache of the memory space.

  @param[in]  Breakpoint statistics (unused)
  @param[in]  Flag to indicate whether the frame should be printed (unused) */

static void
mrk3_handle_frame_cache_cleared (void)
{
  if (mrk3_debug_memspace ())
    fprintf_unfiltered (gdb_stdlog,
			_("MRK3: Frame cache cleared handler triggered.\n"));
  mrk3_invalidate_memspace_cache ();

}	/* mrk3_handle_normal_stop () */


extern initialize_file_ftype _initialize_mrk3_tdep;

void
_initialize_mrk3_tdep (void)
{
  gdbarch_register (bfd_arch_mrk3, mrk3_gdbarch_init, mrk3_dump_tdep);

  /* Add ourselves to normal_stop event chain, which we can use to invalidate
     the cached address space value. */
  observer_attach_frame_cache_cleared (mrk3_handle_frame_cache_cleared);

  /* Debug internals for MRK3 GDB.  */
  add_setshow_zinteger_cmd ("mrk3", class_maintenance,
			    &mrk3_debug,
			    _("Set MRK3 specific debugging."),
			    _("Show MRK3 specific debugging."),
			    _("Bits of value enable MRK3 specific debugging."),
			    NULL,
			    NULL,
			    &setdebuglist,
			    &showdebuglist);
}
