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

    CCYCLES0      32        Cycle count 0, read-only
    CCYCLES1      32        Cycle count 1, writable
    CCYCLES2      32        Cycle count 2, writable

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

MRK3 has multiple address spaces.  The system is divided in three ways.

- It has one or more logical cards: A, B, C etc.

- Each card may run code in one of three security modes: supersystem mode,
  system mode or user mode.

- Within each card and mode, there is one or more processes: 1, 2, 3 etc.

Each card/mode/process combination has its own pair of address spaces, one for
code (20-bit word-addressed) and one for data (32-bit byte-addressed).
Supersystem mode (the most secure mode) is special - it is common to all cards
and runs a single process.

For clarity, the nomenclature is as follows:

- SSM, SM and UM to refer to supersystem mode, system mode and user mode
  respectively;
- A, B, C, etc. to refer to logical cards; and
- 1, 2, 3, ... to refer to processes.

Thus SMA1 refers to system mode process 1 on logical card A.  Supersystem
mode, being common to all cards, and having a single process is referred to as
SSM.

GDB expects a single unified byte addressed memory. For Harvard architectures,
and/or those with word addressing, such as MRK3, this means that addresses on
the target need mapping.

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

We use this mechanism for MRK3.  All GDB addresses are extended to 64 bits,
with the least-signficant 32 bits used to refer to the *byte* address in a
particular address space and the most-significant 32-bits used as flags to
identify card, mode, process and code/data as follows:

  MMMM_CCCC_PPPP_PPPP_XXXX_XXXX_XXXX_AAAA

  - MMMM. bits 63-60.  Mode
    - 0000: Unused/invalid
    - 0001: Supersystem mode
    - 0010: System mode
    - 0011: User mode

  - CCCC. bits 59-56.  Card
    - 0000: Unused/invalid
    - 0001: Card A
    - 0010: Card B
    - ...
    - 1111: Card O

  - PPPP_PPPP. bits 55-48.  Process ID
    - 0000_0000: Unused/invalid
    - bbbb_bbbb: Process number (range 1 - 65,535)

  - XXXX. bits 47-36.  Unused bits (reserved)

  - AAAA. bits 35-32.  Code/data
    - 0000: Unused/invalid
    - 0001: Data
    - 0010: Code

Any unused bits should be set to zero. For SSM, this includes card and process
bits. */


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
#include "completer.h"
#include "readline/tilde.h"
#include "user-regs.h"
#include "stack.h"

/*  Required for symbol switch handling. */
#include "gdb/defs.h"
#include "gdb/gdbcore.h"
#include "gdb/symfile.h"
#include "gdb/objfiles.h"
#include "gdb/gdb_obstack.h"
#include "gdb/progspace.h"
#include "gdb/breakpoint.h"

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>


/* Useful register numbers - CPU registers */
#define MRK3_R0_REGNUM     0
#define MRK3_R1_REGNUM     1
#define MRK3_R2_REGNUM     2
#define MRK3_R3_REGNUM     3
#define MRK3_R4_REGNUM     4
#define MRK3_R5_REGNUM     5
#define MRK3_R6_REGNUM     6
#define MRK3_FP_REGNUM     MRK3_R6_REGNUM
#define MRK3_PC_REGNUM     7
#define MRK3_PSW_REGNUM    8
#define MRK3_SSSP_REGNUM   9
#define MRK3_SSP_REGNUM   10
#define MRK3_USP_REGNUM   11
#define MRK3_R4E_REGNUM   12
#define MRK3_R5E_REGNUM   13
#define MRK3_R6E_REGNUM   14
#define MRK3_CCYCLES0_REGNUM 15
#define MRK3_CCYCLES1_REGNUM 16
#define MRK3_CCYCLES2_REGNUM 17

#define MRK3_LAST_PHYSICAL_REGNUM MRK3_CCYCLES2_REGNUM

/* Useful register numbers - SFRs
   TODO: For now we don't show the SFRs */
#define SFR_START   (MRK3_LAST_PHYSICAL_REGNUM + 1)

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

#define MRK3_ELF_ADDRESS_SIZE 8
#define MRK3_FLAG_SHIFT ((MRK3_ELF_ADDRESS_SIZE - 4) * 8)

#define MRK3_MEM_FLAGS(B) (((CORE_ADDR) B) << MRK3_FLAG_SHIFT)

/* Memory spaces. A total of 32 bits are allocated for this.  See preamble
   for description.  */
#define MRK3_MEM_MODE_MASK MRK3_MEM_FLAGS (0xf0000000)
#define MRK3_MEM_MODE_NONE MRK3_MEM_FLAGS (0x00000000)
#define MRK3_MEM_MODE_SSM  MRK3_MEM_FLAGS (0x10000000)
#define MRK3_MEM_MODE_SM   MRK3_MEM_FLAGS (0x20000000)
#define MRK3_MEM_MODE_UM   MRK3_MEM_FLAGS (0x30000000)

#define MRK3_MEM_CARD_MASK MRK3_MEM_FLAGS (0x0f000000)
#define MRK3_MEM_CARD_ALL  MRK3_MEM_FLAGS (0x00000000)

#define MRK3_MEM_PROC_MASK MRK3_MEM_FLAGS (0x00ff0000)
#define MRK3_MEM_PROC_ALL  MRK3_MEM_FLAGS (0x00000000)

#define MRK3_MEM_TYPE_MASK MRK3_MEM_FLAGS (0x0000000f)
#define MRK3_MEM_TYPE_NONE MRK3_MEM_FLAGS (0x00000000)
#define MRK3_MEM_TYPE_DATA MRK3_MEM_FLAGS (0x00000001)
#define MRK3_MEM_TYPE_CODE MRK3_MEM_FLAGS (0x00000002)

#define MRK3_MEM_MASK      MRK3_MEM_FLAGS(0xffffffff)

#define PRIxMEM_SPACE "08lx"

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

/* The possible frame types.  */
enum mrk3_frame_type
  {
    frame_type_unknown = 0,
    frame_type_call,
    frame_type_ecall
  };

/* Information extracted from frame.  */
struct mrk3_frame_cache
{
  /* Number of bytes of stack allocated in this frame.  */
  int frame_size;

  /* The value of stack pointer on entry to this function.  This is after
     any artefacts written to the stack by CALL or ECALL.  This value is
     what we use for the FRAME_BASE address of each frame.  */
  CORE_ADDR frame_base;

  /* The value of stack pointer in the previous frame, at the point that
     THIS frame was called.  The difference between PREV_SP and FRAME_BASE
     depends on what type of call was used, CALL or ECALL.  */
  CORE_ADDR prev_sp;

  /* If the previous program counter is on the stack, which is almost
     certainly the case, then we should only access it as 16-bit.  */
  unsigned prev_pc_on_stack_p : 1;

  /* Set to true once the FRAME_BASE has been calculated.  */
  unsigned frame_base_p : 1;

  /* Set to true one the PREV_SP has been calculated.  */
  unsigned prev_sp_p : 1;

  /* What type of frame is this, CALL or ECALL.  We start of in the unknown
     state and try to figure out the frame type by looking at the code.  */
  enum mrk3_frame_type frame_type;

  /* Table indicating the location of each and every register.  */
  struct trad_frame_saved_reg *saved_regs;
};

/*! Global debug flag */
static int  mrk3_debug;

/*! Global flag indicating if memspace is valid. */
static int  mrk3_memspace_valid_p;

/* Global, logging level for nxpload command.  */
static int nxpload_logging = 1;

/* Issue warnings when making changes, or when issues are detected while
   analysing a frames CALL / ECALL type.  */
static int mrk3_frame_type_warnings = 1;

/* If a CALL / ECALL frame type is found by looking for a RET / ERET, but
   the call site suggests that the alternative frame type is more
   appropriate, should we toggle the frame type to match the call site?  */
static int mrk3_frame_type_toggling = 1;

/* If we found a CALL / ECALL frame type by looking for a RET / ERET, but
   we could not find a call site of any kind, should the frame be forced
   back to the unknown frame type?  */
static int mrk3_frame_type_require_call_site = 1;

/*! Enumeration to identify memory modes. These match the values in the bit
    field. */
enum mrk3_addr_mode {
  MODE_NONE = 0,
  MODE_SSM  = 1,
  MODE_SM   = 2,
  MODE_UM   = 3
};

/*! Enumeration to identify memory type (code/data). These match the values
    in the bit field. */
enum mrk3_addr_type {
  TYPE_NONE = 0,
  TYPE_DATA = 1,
  TYPE_CODE = 2
};

/*! Struct to record broken down information about a MRK3 address.  Not all
    fields need be used. */
struct mrk3_addr_info {
  enum mrk3_addr_mode  mode;
  uint8_t              card;
  uint16_t             proc;
  enum mrk3_addr_type  type;
  uint32_t             ptr;		/* Native value (GDB pointer) */
};


/*! Opaque data for nxpload_section_callback.  Reused from symfile.c */
struct nxpload_section_data {
  CORE_ADDR load_offset;
  struct nxpload_progress_data *progress_data;
  VEC(memory_write_request_s) *requests;
};

/*! Opaque data for nxpload_progress.  Reused from symfile.c */
struct nxpload_progress_data {
  /* Cumulative data.  */
  unsigned long write_count;
  unsigned long data_count;
  bfd_size_type total_size;
};

/*! Opaque data for load_progress for a single section.  Reused from
    symfile.c */
struct nxpload_progress_section_data {
  struct nxpload_progress_data *cumulative;

  /* Per-section data.  */
  const char *section_name;
  ULONGEST section_sent;
  ULONGEST section_size;
  CORE_ADDR lma;
  CORE_ADDR vma;
  flagword flags;
  gdb_byte *buffer;
};

/* Forward declarations.  */

static CORE_ADDR mrk3_analyze_prologue (struct gdbarch *gdbarch,
					CORE_ADDR start_pc, CORE_ADDR end_pc,
					struct mrk3_frame_cache *cache);

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
static struct mrk3_addr_info
mrk3_get_memspace (void)
{
  static struct mrk3_addr_info cached_memspace = {
    MODE_NONE, 0x0, 0x0, TYPE_NONE
  };
  struct ui_file *mf;
  struct cleanup *old_chain;
  char buf[64];

  if (mrk3_memspace_valid_p)
    return cached_memspace;

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
      warning (_("MRK3: no memory space found, using previous value"));
      do_cleanups (old_chain);
      /* Don't update cache in this circumstance. */
      return cached_memspace;
    }
  else
    {
      /* The value is returned as a 64 bit value, with the flags in the top 32
	 bits. */
      CORE_ADDR res =
	(CORE_ADDR) strtoll (buf, NULL, 16);
      if (mrk3_debug_memspace ())
	fprintf_unfiltered (gdb_stdlog, _("MRK3: memory space buf "
					  "\"%s\", val 0x%" PRIxMEM_SPACE
					  ".\n"), buf, res);
      do_cleanups (old_chain);

      switch (res & MRK3_MEM_MODE_MASK)
	{
	case MRK3_MEM_MODE_NONE: cached_memspace.mode = MODE_NONE; break;
	case MRK3_MEM_MODE_SSM:  cached_memspace.mode = MODE_SSM;  break;
	case MRK3_MEM_MODE_SM:   cached_memspace.mode = MODE_SM;   break;
	case MRK3_MEM_MODE_UM:   cached_memspace.mode = MODE_UM;   break;
	default:
	  warning (_("MRK3: Invalid mode flags: %s.\n"),
		   paddress (target_gdbarch (), res));
	  cached_memspace.mode = MODE_NONE;
	  break;
	}

      cached_memspace.card = (uint8_t) (res >> (MRK3_FLAG_SHIFT + 24)) & 0xf;
      cached_memspace.proc = (uint16_t) (res >> (MRK3_FLAG_SHIFT + 16))
	& 0xffff;

      switch (res & MRK3_MEM_TYPE_MASK)
	{
	case MRK3_MEM_TYPE_NONE: cached_memspace.type = TYPE_NONE; break;
	case MRK3_MEM_TYPE_DATA: cached_memspace.type = TYPE_DATA;  break;
	case MRK3_MEM_TYPE_CODE: cached_memspace.type = TYPE_CODE;  break;
	default:
	  warning (_("MRK3: Invalid type flags: %s.\n"),
		   paddress (target_gdbarch (), res));
	  cached_memspace.type = TYPE_NONE;
	  break;
	}

      mrk3_memspace_valid_p = 1;
      return cached_memspace;
    }
}	/* mrk3_get_memspace () */


/*! Turn a memory info struct into flag representation

    @param[in] info  Details of the memory
    @return  Flags for ORing to create a GDB address. */

static CORE_ADDR
mrk3_make_addr_flags (struct mrk3_addr_info  info)
{
  CORE_ADDR res = 0;

  res |= ((CORE_ADDR) info.mode) << (MRK3_FLAG_SHIFT + 28);
  res |= ((CORE_ADDR) info.card) << (MRK3_FLAG_SHIFT + 24);
  res |= ((CORE_ADDR) info.proc) << (MRK3_FLAG_SHIFT + 16);
  res |= ((CORE_ADDR) info.type) << (MRK3_FLAG_SHIFT +  0);

  return res;

}	/* mrk3_make_addr_flags () */


/*! Convenience function to print current  memory space as string. */
static char *
mrk3_print_memspace (void)
{
  /* Big enough for "{MODE, CARD, 0xPROC, CODE/DATA}.  Largest posssible is
     "{NONE, ALL, 0xffff, NONE}" (25 chars excl EOS char). */
  static char str [26];
  struct mrk3_addr_info info = mrk3_get_memspace ();
  const char *mode = (MODE_NONE == info.mode) ? "NONE"
    : (MODE_SSM == info.mode) ? "SSM"
    : (MODE_SM  == info.mode) ? "SM"
    : (MODE_UM  == info.mode) ? "UM" : "?";
  char card [4];			/* Large enough for "ALL" */
  const char *type = (TYPE_NONE == info.type) ? "NONE"
    : (TYPE_DATA == info.type) ? "DATA"
    : (TYPE_CODE== info.type) ? "CODE" : "?";

  if (0 == info.card)
    strcpy (card, "ALL");
  else
    {
      card[0] = 'A' + info.card - 1;
      card[1] = '\0';
    }

  if (0 == info.proc)
    sprintf (str, "{%s, %s, ALL, %s}", mode, card, type);
  else
    sprintf (str, "{%s, %s, 0x%hx, %s}", mode, card, info.proc, type);

  return (char *) str;

}	/* mrk3_print_memspace () */


/*! Convenience function for the super system memory space. */
static int
mrk3_is_ssm_memspace (void)
{
  return mrk3_get_memspace ().mode == MODE_SSM;

}	/* mrk3_is_ssm_memspace () */


/*! Convenience function for the system memory space. */
static int
mrk3_is_sm_memspace (void)
{
  return mrk3_get_memspace ().mode == MODE_SM;

}	/* mrk3_is_sm_memspace () */


/*! Convenience function for the user memory space. */
static int
mrk3_is_um_memspace (void)
{
  return mrk3_get_memspace ().mode == MODE_UM;

}	/* mrk3_is_um_memspace () */


/*! Convenience function for the code memory type */
static int
mrk3_is_code_address (CORE_ADDR addr)
{
  return (addr & MRK3_MEM_TYPE_MASK) == MRK3_MEM_TYPE_CODE;

}	/* mrk3_is_code_address () */

/* Read a user register.  We make use of this to implement register
   aliases, the baton is the real register number that should be used.  */

static struct value *
value_of_mrk3_user_reg (struct frame_info *frame, const void *baton)
{
  const int *reg_p = baton;
  return value_of_register (*reg_p, frame);
}

/* This table is used to create register aliases.  */

static const struct
{
  const char *name;
  int regnum;
} mrk3_register_aliases[] = {
  { "R7", 15 },
};

/* This table is used to map register number onto register names.  The
   names that appear in this table are the preferred name for each
   register.  */

static const char *const mrk3_register_names [] =
  {
    /* The "real" registers.  */
    "R0", "R1", "R2", "R3", "R4",		/*  0  1  2  3  4 */
    "R5", "R6", "PC", "PSW", "SSSP",		/*  5  6  7  8  9 */
    "SSP", "USP", "R4e", "R5e", "R6e",		/* 10 11 12 13 14 */
    "CPU_CYCLES0", "CPU_CYCLES1", "CPU_CYCLES2",/* 15 16 17 */

    /* The "pseudo" registers.  */
    "SP", "R0L", "R1L", "R2L", "R3L",
    "R0H", "R1H", "R2H", "R3H", "R4LONG",
    "R5LONG", "R6LONG", "SYS", "INT", "ZERO",
    "NEG", "OVERFLOW", "CARRY"
  };

/*! Lookup the name of a register given it's number. */
static const char *
mrk3_register_name (struct gdbarch *gdbarch, int regnum)
{
  gdb_assert (ARRAY_SIZE (mrk3_register_names) == NUM_REGS);
  if (regnum < ARRAY_SIZE (mrk3_register_names))
    return mrk3_register_names [regnum];
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
    case (MRK3_CCYCLES0_REGNUM):
      return bt_uint32;
    case (MRK3_CCYCLES1_REGNUM):
      return bt_uint32;
    case (MRK3_CCYCLES2_REGNUM):
      return bt_uint32;

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
			    _("MRK3: memspace for SP read is %s.\n"),
			      mrk3_print_memspace ());
      if (mrk3_is_ssm_memspace ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sm_memspace ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_um_memspace ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("MRK3: invalid SP read mem space %s"),
		     mrk3_print_memspace ());
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
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R4_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf + 2, raw_buf, 2);
      else
	memcpy (buf, raw_buf, 2);
      /*  HI reg */
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R4E_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (buf, raw_buf, 2);
      else
	memcpy (buf + 2, raw_buf, 2);
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
	buf[0] = (raw_buf[1] & 0x04) >> 2;
      else
	buf[0] = (raw_buf[0] & 0x04) >> 2;
      return REG_VALID;

    case MRK3_OVERFLOW_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x02) >> 1;
      else
	buf[0] = (raw_buf[0] & 0x02) >> 1;
      return REG_VALID;

    case MRK3_CARRY_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	buf[0] = (raw_buf[1] & 0x01) >> 0;
      else
	buf[0] = (raw_buf[0] & 0x01) >> 0;
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
			    _("MRK3: memspace for SP write is %s.\n"),
			    mrk3_print_memspace ());
    case MRK3_SP_REGNUM:
      if (mrk3_is_ssm_memspace ())
	raw_regnum = MRK3_SSSP_REGNUM;
      else if (mrk3_is_sm_memspace ())
	raw_regnum = MRK3_SSP_REGNUM;
      else if (mrk3_is_um_memspace ())
	raw_regnum = MRK3_USP_REGNUM;
      else
	{
	  warning (_("MRK3: invalid SP write mem space %s"),
		     mrk3_print_memspace ());
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
	memcpy (raw_buf, buf + 2, 2);
      else
	memcpy (raw_buf, buf, 2);
      raw_regnum = cooked_regnum - MRK3_R4LONG_REGNUM + MRK3_R4_REGNUM;
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      /*  HI reg */
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	memcpy (raw_buf, buf, 2);
      else
	memcpy (raw_buf, buf + 2, 2);
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
	  raw_buf[1] |= (buf[0] & 0x01) << 3;
	}
      else
	{
	  raw_buf[0] &= 0xf7;
	  raw_buf[0] |= (buf[0] & 0x01) << 3;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_NEG_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfb;
	  raw_buf[1] |= (buf[0] & 0x01) << 2;
	}
      else
	{
	  raw_buf[0] &= 0xfb;
	  raw_buf[0] |= (buf[0] & 0x01) << 2;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_OVERFLOW_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfd;
	  raw_buf[1] |= (buf[0] & 0x01) << 1;
	}
      else
	{
	  raw_buf[0] &= 0xfd;
	  raw_buf[0] |= (buf[0] & 0x01) << 1;
	}
      regcache_raw_write (regcache, raw_regnum, raw_buf);
      return;

    case MRK3_CARRY_REGNUM:
      raw_regnum = MRK3_PSW_REGNUM;
      regcache_raw_read (regcache, raw_regnum, raw_buf);
      if (gdbarch_byte_order (gdbarch) == BFD_ENDIAN_BIG)
	{
	  raw_buf[1] &= 0xfe;
	  raw_buf[1] |= (buf[0] & 0x01) << 0;
	}
      else
	{
	  raw_buf[0] &= 0xfe;
	  raw_buf[0] |= (buf[0] & 0x01) << 0;
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
  int regnr;
  switch (dwarf2_regnr)
    {
    case 0:  regnr = MRK3_R0L_REGNUM; break;
    case 1:  regnr = MRK3_R0H_REGNUM; break;
    case 2:  regnr = MRK3_R1L_REGNUM; break;
    case 3:  regnr = MRK3_R1H_REGNUM; break;
    case 4:  regnr = MRK3_R2L_REGNUM; break;
    case 5:  regnr = MRK3_R2H_REGNUM; break;
    case 6:  regnr = MRK3_R3L_REGNUM; break;
    case 7:  regnr = MRK3_R3H_REGNUM; break;
    case 8:  regnr = MRK3_R4E_REGNUM; break;
    case 9:  regnr = MRK3_R5E_REGNUM; break;
    case 10: regnr = MRK3_R6E_REGNUM; break;
    case 11: regnr = MRK3_R4LONG_REGNUM; break;
    case 12: regnr = MRK3_R5LONG_REGNUM; break;
    case 13: regnr = MRK3_R6LONG_REGNUM; break;
    case 14: regnr = MRK3_R0_REGNUM;  break;
    case 15: regnr = MRK3_R1_REGNUM;  break;
    case 16: regnr = MRK3_R2_REGNUM;  break;
    case 17: regnr = MRK3_R3_REGNUM;  break;
    case 18: regnr = MRK3_R4_REGNUM;  break;
    case 19: regnr = MRK3_R5_REGNUM;  break;
    case 20: regnr = MRK3_R6_REGNUM;  break;
    case 21: regnr = MRK3_SP_REGNUM;  break; /* SP = R7 */
    case 22: regnr = MRK3_PSW_REGNUM; break;
    case 23: regnr = MRK3_PC_REGNUM;  break;

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
  @param[in] memspace   Memory space info
  @param[in] ptr        The pointer to convert
  @return  The address. */

static CORE_ADDR
mrk3_p2a (struct gdbarch *gdbarch, int is_code,
	  struct mrk3_addr_info memspace, CORE_ADDR ptr)
{
  CORE_ADDR addr;

  ptr  &= ~MRK3_MEM_MASK;		/* Just in case */
  addr  = is_code ? (ptr << 1) : ptr;
  memspace.type = is_code ? TYPE_CODE : TYPE_DATA;
  addr |= mrk3_make_addr_flags (memspace);

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
mrk3_addr_bits_add (int is_code,
		    struct mrk3_addr_info memspace,
		    CORE_ADDR addr)
{
  memspace.type = is_code ? TYPE_CODE : TYPE_DATA;
  return (addr & ~MRK3_MEM_MASK) | mrk3_make_addr_flags (memspace);

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
  struct mrk3_addr_info memspace = mrk3_get_memspace ();
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
  struct mrk3_addr_info memspace = mrk3_get_memspace ();
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

  if ((pc & 1) != 0)
    error ("Attempt to set breakpoint at misaligned pc %s",
	   print_core_address (gdbarch, *pcptr));
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
mrk3_skip_prologue (struct gdbarch *gdbarch, CORE_ADDR pc)
{
  CORE_ADDR after_prologue_pc;
  struct mrk3_addr_info memspace = mrk3_get_memspace ();

  if (mrk3_debug_frame ())
    fprintf_unfiltered (gdb_stdlog,
			_("MRK3 skip_prologue: pc %s (initially).\n"),
			print_core_address (gdbarch, pc));

  /* TODO: I believe that PC already have the address bits set.  */
  pc = mrk3_addr_bits_add (1, memspace, pc);

  /* We can pass PC for both start and end address here.  We only care
     about the value that is returned, and that is not dependent on a
     sensible value of end address being passed in.  */
  after_prologue_pc = mrk3_analyze_prologue (gdbarch, pc, pc, NULL);

  if (mrk3_debug_frame ())
    fprintf_unfiltered (gdb_stdlog,
			_("MRK3 skip_prologue: pc %s (after prologue).\n"),
			print_core_address (gdbarch, after_prologue_pc));

  return after_prologue_pc;
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

   @param[in]  gdbarch     The architecture for the frame being analysed.
   @param[in]  start_pc    The address from which to start prologue
                           analysis.
   @param[in]  end_pc      The address after which we no longer care about
                           the effects of the prologue.  When we are
                           scanning the prologue for the current function
                           this will be the current pc, otherwise it will
                           either be the end address of the function, or
                           an address a suitable distance from START_PC
                           such that the entire prologue should fit
			   between START_PC and END_PC.

   @param[out] cache       The cache of registers in the PREV frame, which
                           we should initialize.

   @return                 The address of the end of the prologue.  */

static CORE_ADDR
mrk3_analyze_prologue (struct gdbarch *gdbarch,
		       CORE_ADDR start_pc, CORE_ADDR end_pc,
		       struct mrk3_frame_cache *cache)
{
  int have_fp_p;		/* If we update the FP */
  int have_sp_p;		/* If we update the SP */
  int fp_pushed_p;		/* If we have pushed the FP for this frame */
  int fp_updated_p;		/* If we have updated the FP for this frame */
  int sp_updated_p;		/* If we have updated the SP for this frame */
  int frame_size;		/* Computed frame size.  */

  CORE_ADDR fp_offset;		/* How much FP is incremented (if used) */
  CORE_ADDR sp_offset;		/* How much SP is incremented (if used) */

  CORE_ADDR pc;			/* Temp address use to scan prologue.  */

  /* We need to count through the code from the start of the function. */
  uint16_t insn16;

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

  pc = start_pc;
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
      fp_pushed_p = (end_pc >= pc);

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
	  fp_updated_p = (end_pc >= pc);
	}
      else if (0x0486 == (insn16 & 0xff87))
	{
	  /* Short SUB.w. Offset is in bits [6:3] */
	  fp_offset = mrk3_decode_const4 ((insn16 & 0x78) >> 3, TRUE);

	  pc += 2;
	  fp_updated_p = (end_pc >= pc);
	}
      else
	{
	  warning (_("FP pushed, but no new value set."));
	  fp_offset = 0;
	  fp_updated_p = 0;
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
      sp_updated_p = (end_pc >= pc);
    }
  else if (0x0487 == (insn16 & 0xff87))
    {
      /* Short SUB.w. Offset is in bits [6:3]. */
      have_sp_p = 1;
      sp_offset = mrk3_decode_const4 ((insn16 & 0x78) >> 3, TRUE);
      pc += 2;

      /* Have we yet updated the SP? */
      sp_updated_p = (end_pc >= pc);
    }
  else
    sp_offset = 0;			/* Default initialization. */

  /* Calculate the frame size based on the prologue.  */
  frame_size = ((have_fp_p ? (fp_pushed_p ? 2 : 0) : 0)
		+ ((sp_updated_p) ? sp_offset : 0));

  /* Now some debug output.  */
  if (mrk3_debug_frame ())
    {
      if (have_sp_p)
	{
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Have SP.\n"));
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: SP offset %s.\n"),
			      hex_string_custom ((LONGEST) sp_offset, 4));
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: SP updated: %s.\n"),
			      sp_updated_p ? "yes" : "no");
	}
      else
	fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Don't have SP.\n"));

      if (have_fp_p)
	{
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Have FP.\n"));
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: FP pushed: %s.\n"),
			      fp_pushed_p ? "yes" : "no");
	}
      else
	fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Don't have FP.\n"));

      fprintf_unfiltered (gdb_stdlog, _("MRK3 prologue: Frame size: %d\n"),
			  frame_size);
    }

  /* Write the results into the cache, if we have one.  */
  if (cache != NULL)
    {
      /* Populate the cache. */
      cache->frame_size = frame_size;

      /* PC is saved on the start of this stack.  */
      cache->prev_pc_on_stack_p = 1;

      /* If we have a FP it may be pushed onto the stack. */
      if (have_fp_p)
	{
	  if (fp_pushed_p)
	    /* Frame pointer is always pushed first, so is stored in the two
	       bytes below the stack pointer value on entry to this
	       function.  */
	    cache->saved_regs [MRK3_FP_REGNUM].addr = cache->frame_size - 2;
	  else
	    /* The frame pointer is currently still in the FP register.  */
	    cache->saved_regs [MRK3_FP_REGNUM].realreg = MRK3_FP_REGNUM;
	}

      /* All other registers are just in themselves for now. */
    }

  return pc;
}	/* mrk3_analyse_prologue */

/*! Extract the previous pc value from the stack.

   @param[in]  gdbarch     The architecture for the frame being analysed.
   @param[in]  frame_type  Is this a CALL or ECALL frame.
   @param[in]  frame_base  The GDB address for the stack pointer as it was
                           on entry to this function.
   @param[in]  frame_pc    The address within this function.  Used to
                           supply the upper 4 bits of address for a CALL
                           style function.

   @return                 The GDB address for the return pc.  This is
                           generally referred to as the previous pc in GDB,
			   but is actually the return address.  */

static CORE_ADDR
mrk3_extract_prev_pc (struct gdbarch *gdbarch,
		      enum mrk3_frame_type frame_type,
		      CORE_ADDR frame_base,
		      CORE_ADDR frame_pc)
{
  CORE_ADDR prev_pc;

  /* This function takes no responsibility for guessing.  */
  gdb_assert (frame_type != frame_type_unknown);

  if (frame_type == frame_type_ecall)
    {
      uint16_t pc, psw;

      /* For an ECALL function we load bits [15:0] from the first stack
	 location, while bits [19:16] are loaded from the second stack
	 location; these bits are actually merged into the PSW value.  */
      read_memory (frame_base + 2, (gdb_byte *) &pc, 2);
      read_memory (frame_base + 0, (gdb_byte *) &psw, 2);
      prev_pc = ((frame_pc & ~((CORE_ADDR) 0x1fffff))
		 | ((CORE_ADDR) pc) << 1
		 | (((((CORE_ADDR) psw) >> 4) & 0xf) << 17));
    }
  else
    {
      /* This handles frames of type CALL, and is also the default if
	 we don't know what type of frame this is.  */
      uint16_t pc;

      /* For a CALL function we load bits [15:0] from the stack, while
	 bits [19:16] are kept from the frame pc.  */
      read_memory (frame_base + 0, (gdb_byte *) &pc, 2);
      prev_pc = ((frame_pc & ~((CORE_ADDR) 0x1ffff))
		 | (((CORE_ADDR) pc) << 1));
    }

  if (mrk3_debug_frame ())
    fprintf_unfiltered (gdb_stdlog, _("MRK3 mrk3_extract_prev_pc frame_base = %s, frame_pc = %s, prev_pc = %s\n"),
				      print_core_address (gdbarch, frame_base),
				      print_core_address (gdbarch, frame_pc),
				      print_core_address (gdbarch, prev_pc));

  return prev_pc;
}

/*! Return a string that is the name of the function for FRAME.

   @param[in]  frame       The frame for which the function name is
                           required.

   @return  A malloc'd string that is the name of the function.  This
            string should be xfree'd when it is no longer required.  */

static char *
mrk3_get_frame_function_name (struct frame_info *frame)
{
  char *name = NULL;
  enum language funlang = language_unknown;
  struct symbol *func;

  find_frame_funname (frame, &name, &funlang, &func);
  if (name == NULL)
    name = xstrdup ("??");

  return name;
}

/*! Calculate the frame type (CALL or ECALL) by looking at frame details.

   @param[in]  gdbarch     The architecture for the frame being analysed.
   @param[in]  this_frame  The frame to analyse.

   @param[out] cache       The cache of registers in the PREV frame, which
                           we should initialize.  */

static void
mrk3_analyze_frame_type (struct gdbarch *gdbarch,
			 struct frame_info *this_frame,
			 struct mrk3_frame_cache *cache)
{
  int have_func_bounds_p;	/* If we have function start/end addresses.  */
  CORE_ADDR func_start, func_end, pc, prev_pc;
  uint16_t insn16;
  uint32_t insn32;
  enum mrk3_frame_type frame_type = frame_type_unknown;
  int found_call_p, found_ecall_p;

  gdb_assert (cache->frame_base_p);
  gdb_assert (cache->frame_type == frame_type_unknown);

  pc = get_frame_pc (this_frame);
  if (find_pc_partial_function (pc, NULL, &func_start, &func_end))
    have_func_bounds_p = 1;
  else
    have_func_bounds_p = 0;

  if (have_func_bounds_p && (func_end - func_start) > 2)
    {
      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog,
			      _("MRK3 check for RET/ERET at: %s.\n"),
			      print_core_address (gdbarch, (func_end - 2)));
	}
      mrk3_read_code_memory (gdbarch, func_end - 2, (gdb_byte *) &insn16,
			     sizeof (insn16));
      if (insn16 == /* ERET */ 0x1bc7)
	frame_type = frame_type_ecall;
      else if (insn16 == /* RET */ 0xc41b)
	frame_type = frame_type_call;
    }

  found_call_p = found_ecall_p = 0;

  prev_pc = mrk3_extract_prev_pc (gdbarch, frame_type_ecall,
				  cache->frame_base, pc);
  mrk3_read_code_memory (gdbarch, prev_pc - 4,
			 (gdb_byte *) &insn16, sizeof (insn16));
  if ((insn16 & 0xfff0) == 0x0fe0)
    found_ecall_p = 1;

  prev_pc = mrk3_extract_prev_pc (gdbarch, frame_type_call,
				  cache->frame_base, pc);
  mrk3_read_code_memory (gdbarch, prev_pc - 4,
			 (gdb_byte *) &insn16, sizeof (insn16));
  if ((insn16 & 0xffff) == 0x0fc0)
    found_call_p = 1;

  mrk3_read_code_memory (gdbarch, prev_pc - 2,
			 (gdb_byte *) &insn16, sizeof (insn16));
  if (((insn16 & 0xfff8) == 0x07d0)
      || ((insn16 & 0xc000) == 0x8000))
    found_call_p = 1;

  if ((frame_type == frame_type_ecall && found_ecall_p)
      || (frame_type == frame_type_call && found_call_p))
    /* Nothing to do.  */ ;
  else if (frame_type == frame_type_ecall && found_call_p)
    {
      if (mrk3_frame_type_toggling)
	{
	  if (mrk3_frame_type_warnings)
	    {
	      char *name = mrk3_get_frame_function_name (this_frame);

	      warning (_("Function `%s' (at %s) ends in ERET but it "
			 "looks like\n"
			 "it was called with CALL.  The function will "
			 "be unwound assuming CALL."),
		       name, print_core_address (gdbarch, pc));
	      xfree (name);
	    }

	  frame_type = frame_type_call;
	}
    }
  else if (frame_type == frame_type_call && found_ecall_p)
    {
      if (mrk3_frame_type_toggling)
	{
	  if (mrk3_frame_type_warnings)
	    {
	      char *name = mrk3_get_frame_function_name (this_frame);

	      warning (_("Function `%s' (at %s) ends in RET but it "
			 "looks like\n"
			 "it was called with ECALL.  The function will "
			 "be unwound assuming ECALL."),
		       name, print_core_address (gdbarch, pc));
	      xfree (name);
	    }

	  frame_type = frame_type_ecall;
	}
    }
  else if (frame_type != frame_type_unknown)
    {
      if (mrk3_frame_type_require_call_site)
	{
	  if (mrk3_frame_type_warnings)
	    {
	      char *name = mrk3_get_frame_function_name (this_frame);

	      warning (_("Function `%s' (at %s) ends in %s but no\n"
			 "CALL or ECALL call site could be found.  The stack "
			 "unwinding will stop here."),
		       name,
		       print_core_address (gdbarch, pc),
		       (frame_type == frame_type_call ? "RET" : "ERET"));
	      xfree (name);
	    }

	  frame_type = frame_type_unknown;
	}
    }
  else if ((found_ecall_p ^ found_call_p) != 0)
    {
      if (mrk3_frame_type_warnings)
	{
	  char *name = mrk3_get_frame_function_name (this_frame);

	  warning (_("Function `%s' (at %s) found call-site, but no\n"
		     "RET or ERET instruction at end."),
		   name, print_core_address (gdbarch, pc));
	  xfree (name);
	}

      frame_type = found_ecall_p ? frame_type_ecall : frame_type_call;
    }
  else if (mrk3_frame_type_warnings)
    {
      char *name = mrk3_get_frame_function_name (this_frame);

      warning (_("Unable to determine CALL / ECALL type for `%s' (at %s)."),
	       name, print_core_address (gdbarch, pc));
      xfree (name);
    }

  /* Finally, update the cache.  */
  cache->frame_type = frame_type;
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
      CORE_ADDR this_sp, prev_sp, prev_pc, frame_pc;
      struct mrk3_frame_cache *cache;
      struct gdbarch *gdbarch = get_frame_arch (this_frame);
      struct mrk3_addr_info memspace = mrk3_get_memspace ();
      CORE_ADDR func_start;
      int reg;

      /* Allocate and zero all fields.  */
      cache = FRAME_OBSTACK_ZALLOC (struct mrk3_frame_cache);

      /* Initialise the traditional frame cache component.  */
      gdb_assert (cache->saved_regs == NULL);
      cache->saved_regs = trad_frame_alloc_saved_regs (this_frame);
      *this_cache = cache;

      if (mrk3_debug_frame ())
	fprintf_unfiltered (gdb_stdlog,
			    _("MRK3 build frame cache for frame level %d\n"),
			    frame_relative_level (this_frame));

      func_start = get_frame_func (this_frame);
      func_start = mrk3_addr_bits_add (1, memspace, func_start);
      frame_pc = get_frame_pc (this_frame);
      mrk3_analyze_prologue (gdbarch, func_start, frame_pc, cache);

      /* Compute this before we try to figure out the frame type.  */
      this_sp = get_frame_register_unsigned (this_frame, MRK3_SP_REGNUM);
      this_sp = mrk3_p2a (gdbarch, 0, memspace, this_sp);
      cache->frame_base = this_sp + cache->frame_size;
      cache->frame_base_p = 1;

      /* Now we can figure out the frame type.  */
      mrk3_analyze_frame_type (gdbarch, this_frame, cache);

      if (cache->frame_type == frame_type_ecall)
	prev_sp = cache->frame_base + 4;
      else if (cache->frame_type == frame_type_call)
	prev_sp = cache->frame_base + 2;
      else
	/* TODO: Is there anything better we could do here?  */
	prev_sp = 0;
      /* Now store the value of the previous stack pointer.  */
      cache->prev_sp = prev_sp;
      cache->prev_sp_p = 1;
      trad_frame_set_value (cache->saved_regs, MRK3_SP_REGNUM, prev_sp);

      /* Calculate actual addresses of saved registers using offsets
	 determined by mrk3_analyze_prologue.  */
      for (reg = 0; reg < gdbarch_num_regs (get_frame_arch (this_frame)); reg++)
	if (trad_frame_addr_p (cache->saved_regs, reg))
	  cache->saved_regs[reg].addr += prev_sp;

      /* Get the previous pc, and write it into the register cache.  */
      if (cache->frame_type != frame_type_unknown)
	prev_pc = mrk3_extract_prev_pc (gdbarch, cache->frame_type,
					cache->frame_base, frame_pc);
      else
	/* TODO: Is there something better we could do here?  */
	prev_pc = 0;

      trad_frame_set_value (cache->saved_regs, MRK3_PC_REGNUM,
			    mrk3_a2p (gdbarch, prev_pc));

      /* Some debug output.  */
      if (mrk3_debug_frame ())
	{
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 frame base: %s\n"),
			      print_core_address (gdbarch, cache->frame_base) );
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 previous stack pointer: %s\n"),
			      print_core_address (gdbarch, prev_sp) );
	  fprintf_unfiltered (gdb_stdlog, _("MRK3 previous pc: %s\n"),
			      print_core_address (gdbarch, prev_pc) );
	}
    }

  return *this_cache;
}

static CORE_ADDR
mrk3_frame_base_address (struct frame_info *this_frame,
			 void **this_cache)
{
  struct mrk3_frame_cache *frame_cache;

  frame_cache = mrk3_frame_cache (this_frame, this_cache);
  return frame_cache->frame_base;
}

static void
mrk3_frame_this_id (struct frame_info *this_frame,
		    void **this_cache, struct frame_id *this_id)
{
  CORE_ADDR base = mrk3_frame_base_address (this_frame, this_cache);

  if (base)
    {
      struct mrk3_addr_info memspace = mrk3_get_memspace ();
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
  struct gdbarch *gdbarch = get_frame_arch (this_frame);
  struct mrk3_frame_cache *frame_cache =
    mrk3_frame_cache (this_frame, this_cache);

  val = trad_frame_get_prev_register (this_frame,
				      frame_cache->saved_regs,
				      regnum);
  if (mrk3_debug_frame ())
    fprintf_unfiltered (gdb_stdlog,
			_("MRK3 reg %s (%d) previous value = %s\n"),
			mrk3_register_name (gdbarch, regnum), regnum,
			print_core_address (gdbarch, value_as_address (val)));

  gdb_assert (val != NULL);
  return val;
}

/* Figure out if unwinding should stop at THIS_FRAME.  Return
   UNWIND_NO_REASON is unwinding should continue, or a suitable reason code
   if unwinding should no go beyond THIS_FRAME.  */

static enum unwind_stop_reason
mrk3_frame_unwind_stop_reason (struct frame_info *this_frame,
			       void **this_cache)
{
  struct frame_info *next_frame;
  struct frame_id this_id = get_frame_id (this_frame);
  struct mrk3_frame_cache *cache =
    mrk3_frame_cache (this_frame, this_cache);
  struct gdbarch *gdbarch = get_frame_arch (this_frame);

  /* If we've hit a wall, stop.  */
  if (cache->prev_sp == 0 || frame_id_eq (this_id, outer_frame_id))
    return UNWIND_OUTERMOST;

  if (cache->frame_type == frame_type_unknown)
    return UNWIND_OUTERMOST;

  /* This catches the error case where run into unused memory and slowly
     creep up the stack 2 bytes at a time.  As each frame looks different
     (different stack pointer) gdb does not stop us.  This will break if
     we ever have a recursive function that uses no stack frame.... */
  next_frame = get_next_frame (this_frame);
  if (next_frame
      && get_frame_pc (next_frame) == get_frame_pc (this_frame)
      && (get_frame_base_address (next_frame) ==
	  get_frame_base_address (this_frame) - 2))
    return UNWIND_OUTERMOST;

  return UNWIND_NO_REASON;
}

static const struct frame_unwind mrk3_frame_unwind = {
  .type          = NORMAL_FRAME,
  .stop_reason   = mrk3_frame_unwind_stop_reason,
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
  int i;

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
  gdbarch = gdbarch_alloc (&info, NULL);
  set_gdbarch_address_class_type_flags (gdbarch,
					mrk3_address_class_type_flags);
  set_gdbarch_short_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_int_bit (gdbarch, 2 * TARGET_CHAR_BIT);
  set_gdbarch_long_bit (gdbarch, 4 * TARGET_CHAR_BIT);
  set_gdbarch_long_long_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_ptr_bit (gdbarch, 8 * TARGET_CHAR_BIT);
  set_gdbarch_addr_bit (gdbarch, 8 * TARGET_CHAR_BIT);

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

  for (i = 0; i < ARRAY_SIZE (mrk3_register_aliases); i++)
    user_reg_add (gdbarch, mrk3_register_aliases[i].name,
		  value_of_mrk3_user_reg, &mrk3_register_aliases[i].regnum);

  return gdbarch;
}

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


/*! Clean up an entire memory request vector, including load
    data and progress records.

  Reused from symfile.c

  @param arg  Generic vector pointer. */

static void
mrk3_clear_memory_write_data (void *arg)
{
  VEC(memory_write_request_s) **vec_p = arg;
  VEC(memory_write_request_s) *vec = *vec_p;
  int i;
  struct memory_write_request *mr;

  for (i = 0; VEC_iterate (memory_write_request_s, vec, i, mr); ++i)
    {
      xfree (mr->data);
      xfree (mr->baton);
    }
  VEC_free (memory_write_request_s, vec);

}	/* mrk3_clear_memory_write_data () */


/*! Callback service function for generic_load (bfd_map_over_sections).

  Copied from the generic version in symfile.c

  @param[in]  abfd  BFD for the section
  @param[in]  asec  The section
  @param[out] data  The running size total. */

static void
mrk3_add_section_size_callback (bfd *abfd,
				asection *asec,
				void *data)
{
  bfd_size_type *sum = data;

  *sum += bfd_get_section_size (asec);

}	/* mrk3_add_section_size_callback () */

/* Callback to decide if SEC from ABFD should be transfered to the remote
   as part of nxpload command.

   @param[in] abfd BFD for the section.
   @param[in] asec The section.

   @return Boolean, true if asec should be transfered, otherwise false.  */

static int
mrk3_load_transfer_section (bfd *abfd, asection *asec)
{
  /* For now we send everything over.  In the future we might want to
     filter out some sections that we know the remote will never be
     interested in, for example debug content, or relocations.  */
  return 1;
}

/*! Callback service function for mrk3_nxpload (bfd_map_over_sections).

  Based on the generic version in symfile.c

  @param[in] abfd  BFD for the section
  @param[in] asec  The section
  @param[in] data  The arguments. */

static void
mrk3_load_section_callback (bfd *abfd,
			    asection *asec,
			    void *data)
{
  struct memory_write_request *new_request;
  struct nxpload_section_data *args = data;
  struct nxpload_progress_section_data *section_data;
  bfd_size_type size = bfd_get_section_size (asec);
  gdb_byte *buffer;
  const char *sect_name = bfd_get_section_name (abfd, asec);

  if (!mrk3_load_transfer_section (abfd, asec))
    return;

  new_request = VEC_safe_push (memory_write_request_s,
			       args->requests, NULL);
  memset (new_request, 0, sizeof (struct memory_write_request));
  section_data = xcalloc (1, sizeof (struct nxpload_progress_section_data));
  new_request->begin = bfd_section_lma (abfd, asec) + args->load_offset;
  new_request->end = new_request->begin + size; /* FIXME Should size
						   be in instead?  */
  new_request->data = (size > 0) ? xmalloc (size) : NULL;
  new_request->baton = section_data;

  buffer = new_request->data;

  section_data->cumulative = args->progress_data;
  section_data->section_name = sect_name;
  section_data->section_size = size;
  section_data->lma = new_request->begin;
  section_data->vma = bfd_section_vma (abfd, asec);
  section_data->flags = bfd_get_section_flags (abfd, asec);
  section_data->buffer = buffer;

  bfd_get_section_contents (abfd, asec, buffer, 0, size);

}	/* mrk3_load_section_callback () */


/*! Write callback routine for progress reporting.

  Based on the generic version in symfile.c

  @param[in] bytes        Number of bytes written
  @param[in] untyped_arg  The arguments */

static void
mrk3_nxpload_progress (ULONGEST bytes, void *untyped_arg)
{
  struct nxpload_progress_section_data *args = untyped_arg;
  struct nxpload_progress_data *totals;

  if (args == NULL)
    /* Writing padding data.  No easy way to get at the cumulative
       stats, so just ignore this.  */
    return;

  totals = args->cumulative;

  if (bytes == 0 && args->section_sent == 0)
    {
      int log_this_load = 0;

      if (nxpload_logging == 1
	  && (args->flags & SEC_LOAD) && (args->section_size > 0))
	log_this_load = 1;
      else if (nxpload_logging > 1)
	log_this_load = 1;

      /* The write is just starting.  Let the user know we've started
	 this section.  */
      if (log_this_load)
	{
	  struct ui_out *uiout = current_uiout;

	  ui_out_text (uiout, "Loading section ");
	  ui_out_field_fmt (uiout, "section-name", "%s", args->section_name);
	  ui_out_text (uiout, ", size ");
	  ui_out_field_fmt (uiout, "section-size", "%s", hex_string (args->section_size));
	  ui_out_text (uiout, ", lma ");
	  ui_out_field_fmt (uiout, "section-lma", "%s", paddress (target_gdbarch (), args->lma));
	  ui_out_text (uiout, "\n");
	  return;
	}
    }

  /* The original has code to validate downloads here, which as far as I can
     see is never used. */

  totals->data_count += bytes;
  args->lma += bytes;
  args->buffer += bytes;
  totals->write_count += 1;
  args->section_sent += bytes;
  if (check_quit_flag ()
      || (deprecated_ui_load_progress_hook != NULL
	  && deprecated_ui_load_progress_hook (args->section_name,
					       args->section_sent)))
    error (_("Canceled the download"));

  if (deprecated_show_load_progress != NULL)
    deprecated_show_load_progress (args->section_name,
				   args->section_sent,
				   args->section_size,
				   totals->data_count,
				   totals->total_size);

}	/* mrk3_nxpload_progress () */

/* Sort load requests gathered as part of the nxpload command.
   Non-loadable sections are sorted to the front of the list, and loadable
   sections are sorted to the end.  Loadable and non loadable sections are
   then sorted by start address.  */

static int
mrk3_load_sort_write_requests (const void *a, const void *b)
{
  const struct memory_write_request *a_req = a;
  const struct memory_write_request *b_req = b;

  struct nxpload_progress_section_data *a_secdata =
    (struct nxpload_progress_section_data *) a_req->baton;
  struct nxpload_progress_section_data *b_secdata =
    (struct nxpload_progress_section_data *) b_req->baton;

  if ((a_secdata->flags & SEC_LOAD) != (b_secdata->flags & SEC_LOAD))
    {
      if (a_secdata->flags & SEC_LOAD)
	return 1;
      else
	return -1;
    }
  else
    {
      if (a_req->begin < b_req->begin)
	return -1;
      else if (a_req->begin == b_req->begin)
	return 0;
      else
	return 1;
    }
}

/*! Custom load command for MRK3

  Largely based on the generic_load function, but implementing key differences
  for MRK3.

  Specifically the contents of *all* sections is transferred,  with
  unallocated ones sent first.  These may include encryption information for
  the allocated sections.

   @param[in] args      The args to the command
   @param[in] from_tty  True (1) if GDB is running from a TTY, false (0)
   otherwise. */

static void
mrk3_nxpload (char *args,
	      int from_tty)
{
  bfd *loadfile_bfd;
  struct timeval start_time, end_time;
  char *filename;
  struct cleanup *old_cleanups = make_cleanup (null_cleanup, 0);
  struct nxpload_section_data cbdata;
  struct nxpload_progress_data total_progress;
  struct ui_out *uiout = current_uiout;

  struct memory_write_request *r;
  unsigned i;
  CORE_ADDR entry;
  char **argv;

  memset (&cbdata, 0, sizeof (cbdata));
  memset (&total_progress, 0, sizeof (total_progress));
  cbdata.progress_data = &total_progress;

  make_cleanup (mrk3_clear_memory_write_data, &cbdata.requests);

  if (args == NULL)
    error_no_arg (_("file to load"));

  argv = gdb_buildargv (args);
  make_cleanup_freeargv (argv);

  filename = tilde_expand (argv[0]);
  make_cleanup (xfree, filename);

  if (argv[1] != NULL)
    {
      const char *endptr;

      cbdata.load_offset = strtoulst (argv[1], &endptr, 0);

      /* If the last word was not a valid number then
         treat it as a file name with spaces in.  */
      if (argv[1] == endptr)
        error (_("Invalid download offset:%s."), argv[1]);

      if (argv[2] != NULL)
	error (_("Too many parameters."));
    }

  /* Open the file for loading.  */
  loadfile_bfd = gdb_bfd_open (filename, gnutarget, -1);
  if (loadfile_bfd == NULL)
    {
      perror_with_name (filename);
      return;
    }

  make_cleanup_bfd_unref (loadfile_bfd);

  if (!bfd_check_format (loadfile_bfd, bfd_object))
    {
      error (_("\"%s\" is not an object file: %s"), filename,
	     bfd_errmsg (bfd_get_error ()));
    }

  bfd_map_over_sections (loadfile_bfd, mrk3_add_section_size_callback,
			 (void *) &total_progress.total_size);

  bfd_map_over_sections (loadfile_bfd, mrk3_load_section_callback, &cbdata);

  gettimeofday (&start_time, NULL);

  /* Sort the section list, non-loadable sections first, secondary sort by
     start address.  */
  qsort (VEC_address (memory_write_request_s, cbdata.requests),
	 VEC_length (memory_write_request_s, cbdata.requests),
	 sizeof (struct memory_write_request),
	 mrk3_load_sort_write_requests);

  /* Load all of the sections.  */
  for (i = 0; VEC_iterate (memory_write_request_s, cbdata.requests, i, r); ++i)
    {
      char buf [1024]; /* Hack */
      LONGEST len;
      struct nxpload_progress_section_data *secdata =
	(struct nxpload_progress_section_data *) r->baton;

      snprintf (buf, 1024, "nxpload:%s:%s:%x:%s:%s",
		core_addr_to_string (secdata->vma),
		core_addr_to_string (secdata->lma),
		secdata->flags,
		pulongest (secdata->section_size),
		secdata->section_name);
      target_rcmd (buf, NULL);

      len = target_write_with_progress (current_target.beneath,
					TARGET_OBJECT_RAW_MEMORY, NULL,
					r->data, 0, r->end - r->begin,
					mrk3_nxpload_progress,
					r->baton);
      if (len < (LONGEST) (r->end - r->begin))
	error (_("Load failed"));
    }

  gettimeofday (&end_time, NULL);

  entry = bfd_get_start_address (loadfile_bfd);
  entry = gdbarch_addr_bits_remove (target_gdbarch (), entry);
  ui_out_text (uiout, "Start address ");
  ui_out_field_fmt (uiout, "address", "%s", paddress (target_gdbarch (), entry));
  ui_out_text (uiout, ", load size ");
  ui_out_field_fmt (uiout, "load-size", "%lu", total_progress.data_count);
  ui_out_text (uiout, "\n");
  /* We were doing this in remote-mips.c, I suspect it is right
     for other targets too.  */
  regcache_write_pc (get_current_regcache (), entry);

  /* Reset breakpoints, now that we have changed the load image.  For
     instance, breakpoints may have been set (or reset, by
     post_create_inferior) while connected to the target but before we
     loaded the program.  In that case, the prologue analyzer could
     have read instructions from the target to find the right
     breakpoint locations.  Loading has changed the contents of that
     memory.  */

  breakpoint_re_set ();

  /* FIXME: are we supposed to call symbol_file_add or not?  According
     to a comment from remote-mips.c (where a call to symbol_file_add
     was commented out), making the call confuses GDB if more than one
     file is loaded in.  Some targets do (e.g., remote-vx.c) but
     others don't (or didn't - perhaps they have all been deleted).  */

  print_transfer_performance (gdb_stdout, total_progress.data_count,
			      total_progress.write_count,
			      &start_time, &end_time);

  do_cleanups (old_cleanups);

}	/* mrk3_nxpload () */


/*! NXP specific load command

   MRK3 needs a custom command to load files, because of the encrypted
   sections, and the need to pass in unallocated info sections which contain
   information on handling the encryption etc.

   Largely taken from the default load_command function

   @param[in] arg       The arg to the command
   @param[in] from_tty  True (1) if GDB is running from a TTY, false (0)
   otherwise. */

static void
mrk3_nxpload_command (char *arg,
		      int   from_tty)
{
  struct cleanup *cleanup = make_cleanup (null_cleanup, NULL);

  dont_repeat ();		/* Don't repeat command by null line */

  /* The user might be reloading because the binary has changed.  Take
     this opportunity to check.  */
  reopen_exec_file ();
  reread_symbols ();

  if (arg == NULL)
    {
      char *parg;
      int count = 0;

      parg = arg = get_exec_file (1);

      /* Count how many \ " ' tab space there are in the name.  */
      while ((parg = strpbrk (parg, "\\\"'\t ")))
	{
	  parg++;
	  count++;
	}

      if (count)
	{
	  /* We need to quote this string so buildargv can pull it apart.  */
	  char *temp = xmalloc (strlen (arg) + count + 1 );
	  char *ptemp = temp;
	  char *prev;

	  make_cleanup (xfree, temp);

	  prev = parg = arg;
	  while ((parg = strpbrk (parg, "\\\"'\t ")))
	    {
	      strncpy (ptemp, prev, parg - prev);
	      ptemp += parg - prev;
	      prev = parg++;
	      *ptemp++ = '\\';
	    }
	  strcpy (ptemp, prev);

	  arg = temp;
	}
    }

  mrk3_nxpload (arg, from_tty);

  /* After re-loading the executable, we don't really know which overlays are
     mapped any more.  This almost certainly will never apply to MRK3, but
     keep it just in case. */
  overlay_cache_invalid = 1;

  do_cleanups (cleanup);

}	/* mrk3_nxpload_command() */

/* Command-list for the "set/show mrk3" prefix command.  */
static struct cmd_list_element *set_mrk3_list;
static struct cmd_list_element *show_mrk3_list;

/* Implement the "set mrk3" prefix command.  */

static void
set_mrk3_command (char *arg, int from_tty)
{
  printf_unfiltered (_(\
"\"set mrk3\" must be followed by the name of a setting.\n"));
  help_list (set_mrk3_list, "set mrk3 ", all_commands, gdb_stdout);
}

/* Implement the "show mrk3" prefix command.  */

static void
show_mrk3_command (char *args, int from_tty)
{
  cmd_show_list (show_mrk3_list, from_tty, "");
}

/* Command-list for the "set/show mrk3 frame-type" prefix command.  */
static struct cmd_list_element *set_mrk3_frame_type_list;
static struct cmd_list_element *show_mrk3_frame_type_list;

/* Implement the "set mrk3 frame-type" prefix command.  */

static void
set_mrk3_frame_type_command (char *arg, int from_tty)
{
  printf_unfiltered (_(\
"\"set mrk3 frame-type\" must be followed by the name of a setting.\n"));
  help_list (set_mrk3_frame_type_list, "set mrk3 frame-type ",
	     all_commands, gdb_stdout);
}

/* Implement the "show mrk3 frame-type" prefix command.  */

static void
show_mrk3_frame_type_command (char *args, int from_tty)
{
  cmd_show_list (show_mrk3_frame_type_list, from_tty, "");
}

extern initialize_file_ftype _initialize_mrk3_tdep;

void
_initialize_mrk3_tdep (void)
{
  struct cmd_list_element *c;

  gdbarch_register (bfd_arch_mrk3, mrk3_gdbarch_init, NULL);

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

  c = add_com ("nxpload", class_files, mrk3_nxpload_command,
	       "Customized load command for NXP");
  set_cmd_completer (c, filename_completer);

  add_setshow_zinteger_cmd ("nxpload-logging", class_maintenance,
			    &nxpload_logging,
			    _("Set nxpload logging level."),
			    _("Show nxpload logging level."),
			    _("How much logging to report from nxpload command."),
			    NULL,
			    NULL,
			    &setlist,
			    &showlist);

  add_prefix_cmd ("mrk3", no_class, set_mrk3_command,
                  _("Prefix command for changing MRK3 specfic settings"),
                  &set_mrk3_list, "set mrk3 ", 0, &setlist);

  add_prefix_cmd ("mrk3", no_class, show_mrk3_command,
                  _("Generic command for showing MRK3 specific settings."),
                  &show_mrk3_list, "show mrk3 ", 0, &showlist);

  add_prefix_cmd ("frame-type", no_class, set_mrk3_frame_type_command,
                  _("Prefix command for changing MRK3 frame-type "
		    "specfic settings"),
                  &set_mrk3_frame_type_list, "set mrk3 frame-type ",
		  0, &set_mrk3_list);

  add_prefix_cmd ("frame-type", no_class, show_mrk3_frame_type_command,
                  _("Generic command for showing MRK3 frame-type "
		    "specific settings."),
                  &show_mrk3_frame_type_list, "show mrk3 frame-type ",
		  0, &show_mrk3_list);

  add_setshow_boolean_cmd ("require-call-site", class_obscure,
			   &mrk3_frame_type_require_call_site, _("\
Enable or disable if a call site is needed to establish frame type"), _("\
Show whether establishing frame type requires a call site"), _("\
When establishing the CALL / ECALL type of a frame GDB will look look at\n\
the return address on the stack, and try to find the call site.  If GDB\n\
can't find a call site, and this option is on, then GDB will force the\n\
frame to have unknown CALL / ECALL type.  If this option is off then GDB\n\
will rely on any frame type information it acquired by looking for a RET /\n\
ERET at the end of a function."),
			/* set */ NULL,
			/* show */ NULL,
			&set_mrk3_frame_type_list,
			&show_mrk3_frame_type_list);

  add_setshow_boolean_cmd ("toggling", class_obscure,
			   &mrk3_frame_type_toggling, _("\
Enable or disable frame type toggling"), _("\
Show whether frame type toggling is enabled"), _("\
When establishing the CALL / ECALL type of a frame GDB will look for a RET\n\
or ERET instruction at the end of a function.  However, GDB also looks on\n\
the stack at the return address and then examines the call site looking for\n\
a CALL or ECALL instruction.  If the function ends in ERET, but appears to\n\
be called with CALL (or vice versa) then, when this option is enabled, the\n\
frame CALL / ECALL type will be toggled to match the call site.  When this\n\
option is off, the frame type will be left based on the RET / ERET."),
			/* set */ NULL,
			/* show */ NULL,
			&set_mrk3_frame_type_list,
			&show_mrk3_frame_type_list);

  add_setshow_boolean_cmd ("warnings", class_obscure,
                           &mrk3_frame_type_warnings, _("\
Enable or disable warnings when analyzing frame CALL / ECALL type"), _("\
Show whether warnings are enabled for CALL / ECALL analysis"),
                           _("\
When establishing the CALL / ECALL type of a frame, sometime\n\
inconsistencies are found.  In these cases GDB tries to do what will\n\
give the most helpful information, however, turning this option on will\n\
cause GDB to also issue a warning whenever something unexpected is found\n\
while establishing the CALL / ECALL type.  Turn this option off to\n\
silence the warnings and have GDB just get on with it."),
			   NULL, NULL,
			   &set_mrk3_frame_type_list,
			   &show_mrk3_frame_type_list);

}
