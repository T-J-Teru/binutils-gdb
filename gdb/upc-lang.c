/* UPC language support definitions for GDB, the GNU debugger.

   Contributed by: Gary Funck <gary@intrepid.com>

   Copyright (C) 2007 Free Software Foundation, Inc.

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
#include "arch-utils.h"
#include "value.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "target.h"
#include "parser-defs.h"
#include "symfile.h"
#include "language.h"
#include "upc-lang.h"
#include "inferior.h"
#include "gdbcmd.h"
#include "gdbthread.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "uda-types-client.h"
#include "uda-client.h"
#include "uda-defs.h"
#include "upc-thread.h"

#define UDA_SERVICE "/tmp/uda_service"

FILE *c_in, *c_out;
extern void _upc_initialize_upc_language (void);

/* UDA calls 
       An array of callouts initialized based on the
       plugin that is used: UDA server (with plugin)
       or UDA plugin directly. */
uda_callouts_t uda_calls;

/* upc language initilaized */
int upc_lang_initialized = 0;

uda_tword_t mythread = (uda_tword_t) -1;

extern int upc_threads;

#define UPC_MAIN_PROGRAM_SYMBOL_NAME "upc_main"

/* printf format strings */
#ifdef BFD64
#define LONGEST_FMT "l"
#else
#define LONGEST_FMT "ll"
#endif

/* If the main procedure is written in UPC, then return its name.
   The result is good until the next call.  Return NULL if the main
   procedure doesn't appear to be in UPC.  */

char *
upc_main_name (void)
{
  struct minimal_symbol *msym;
  msym = lookup_minimal_symbol (UPC_MAIN_PROGRAM_SYMBOL_NAME, NULL, NULL);
  if (msym != NULL)
    {
      return UPC_MAIN_PROGRAM_SYMBOL_NAME;
    }
  /* The main procedure doesn't seem to be in UPC.  */
  return NULL;
}

/* Return true if type is UPC shared qualified.  */
int
upc_shared_type_p (struct type *type)
{
  int is_shared = 0;
  if (type)
    {
      struct type *elem_type = check_typedef (type);
      /* follow array types down to their element type.  */
      while (TYPE_CODE (elem_type) == TYPE_CODE_ARRAY)
	elem_type = check_typedef (TYPE_TARGET_TYPE (elem_type));
      is_shared = TYPE_UPC_SHARED (elem_type);
    }
  return is_shared;
}

ULONGEST
upc_blocksizeof (struct type *type)
{
  ULONGEST block_size = 1;
  if (type)
    {
      struct type *elem_type = check_typedef (type);
      /* follow array types down to their element type.  */
      while (TYPE_CODE (elem_type) == TYPE_CODE_ARRAY)
	elem_type = check_typedef (TYPE_TARGET_TYPE (elem_type));
      block_size = TYPE_UPC_LAYOUT (elem_type);
    }
  return block_size;
}

ULONGEST
upc_elemsizeof (struct type *type)
{
  ULONGEST elem_size = 1;
  if (type)
    {
      struct type *elem_type = check_typedef (type);
      /* follow array types down to their element type.  */
      while (TYPE_CODE (elem_type) == TYPE_CODE_ARRAY)
	elem_type = check_typedef (TYPE_TARGET_TYPE (elem_type));
      if (TYPE_LENGTH (elem_type))
        elem_size = TYPE_LENGTH (elem_type);
    }
  return elem_size;
}

struct value *
upc_pts_index_add (struct type *ptrtype, struct value *ptrval,
                   struct value *indexval, LONGEST elem_size)
{
  int status;
  struct type *tt;
  ULONGEST block_size;
  const LONGEST index = value_as_long (indexval);
  const ULONGEST ptrtype_len = TYPE_LENGTH (ptrtype);
  const uda_target_pts_t *ptrval_raw = (const uda_target_pts_t *)
                                       value_contents_all (ptrval);
  uda_tword_t packed_pts_len;
  uda_target_pts_t packed_pts;
  uda_debugger_pts_t pts, sum;
  struct value *val;
  if (!uda_calls.uda_unpack_pts
      || !uda_calls.uda_calc_pts_index_add
      || !uda_calls.uda_pack_pts)
    error (_("UPC language support is not initialised"));  
  tt = TYPE_TARGET_TYPE (ptrtype); CHECK_TYPEDEF (tt);
  block_size = upc_blocksizeof (tt);
  status = (*uda_calls.uda_unpack_pts) (ptrtype_len, ptrval_raw, block_size, &pts);
  if (status != uda_ok)
    error (_("upc_pts_index_add: uda_unpack_pts error"));
  status = (*uda_calls.uda_calc_pts_index_add) (&pts, index, elem_size, block_size, &sum);
  if (status != uda_ok)
    error (_("upc_pts_index_add: uda_calc_pts_index_add error"));
  status = (*uda_calls.uda_pack_pts) (sum.addrfield, sum.thread, sum.phase,
                         block_size, (size_t *)&packed_pts_len, &packed_pts);
  if (status != uda_ok)
    error (_("upc_pts_index_add: uda_pack_pts error"));
  gdb_assert (ptrtype_len == packed_pts_len);
  val = allocate_value (ptrtype);
  memcpy (value_contents_all_raw (val), &packed_pts, packed_pts_len);
  return val;
}

struct value *
upc_pts_diff (struct value *arg1, struct value *arg2)
{
  int status;
  struct gdbarch *gdbarch = get_type_arch (value_type (arg1));
  struct type *type1 = check_typedef (value_type (arg1));
  struct type *type2 = check_typedef (value_type (arg2));
  struct type *tt1 = check_typedef (TYPE_TARGET_TYPE (type1));
  struct type *tt2 = check_typedef (TYPE_TARGET_TYPE (type2));
  const ULONGEST elem_size = upc_elemsizeof (tt1);
  const ULONGEST block_size  = upc_blocksizeof (tt1);
  const ULONGEST ptrtype_len = TYPE_LENGTH (type1);
  const uda_target_pts_t *arg1_pts = (const uda_target_pts_t *)
                                     value_contents_all (arg1);
  const uda_target_pts_t *arg2_pts = (const uda_target_pts_t *)
                                     value_contents_all (arg2);
  uda_tword_t packed_pts_len;
  uda_target_pts_t packed_pts;
  uda_debugger_pts_t pts1, pts2;
  uda_tint_t diff;
  struct value *val;
  if (!uda_calls.uda_unpack_pts
      || !uda_calls.uda_calc_pts_diff)
    error (_("UPC language support is not initialised"));
  status = (*uda_calls.uda_unpack_pts) (ptrtype_len, arg1_pts, block_size, &pts1);
  if (status != uda_ok)
    error (_("upc_pts_diff: uda_unpack_pts(1) error"));
  status = (*uda_calls.uda_unpack_pts) (ptrtype_len, arg2_pts, block_size, &pts2);
  if (status != uda_ok)
    error (_("upc_pts_diff: uda_unpack_pts(2) error"));
  status = (*uda_calls.uda_calc_pts_diff) (&pts1, &pts2, elem_size, block_size, &diff);
  if (status != uda_ok)
    error (_("upc_pts_diff: uda_calc_pts_diff error"));
  val = value_from_longest (builtin_type (gdbarch)->builtin_long, diff);
  return val;
}

gdb_upc_pts_t
upc_shared_var_address (struct symbol *var)
{
  gdb_upc_pts_t shared_addr = {0, 0, 0, 0};
  CORE_ADDR sym_addr = 0;
  uda_tword_t elem_size = 1;
  uda_tword_t block_size = 1;
  struct type *elem_type = SYMBOL_TYPE (var);
  const char *sym_name = SYMBOL_LINKAGE_NAME (var);
  int status;
  if (!uda_calls.uda_symbol_to_pts)
    error (_("UPC language support is not initialised"));  
  if ((SYMBOL_CLASS (var) == LOC_STATIC) && !overlay_debugging)
    sym_addr = SYMBOL_VALUE_ADDRESS (var);
  else if (SYMBOL_CLASS (var) == LOC_UNRESOLVED)
    {
      struct minimal_symbol *msym;

      msym = lookup_minimal_symbol (SYMBOL_LINKAGE_NAME (var), NULL, NULL);
      if (msym == NULL)
          return shared_addr;
      sym_addr = SYMBOL_VALUE_ADDRESS (msym);
    }
  else
    error (_("upc_shared_var_address: wrong symbol class"));
  if (elem_type)
    {
      elem_size = upc_elemsizeof (elem_type);
      block_size = upc_blocksizeof (elem_type);
    }
  status = (*uda_calls.uda_symbol_to_pts) (elem_size, block_size, sym_addr,
                              sym_name, &shared_addr);
  if (status != uda_ok)
    error (_("UPC Shared Address of \"%s\" is unknown."), sym_name);
  return shared_addr;
}

struct value *
upc_value_from_pts (struct type *ptrtype, gdb_upc_pts_t pts)
{
  int status;
  struct type *tt = check_typedef (TYPE_TARGET_TYPE (ptrtype));
  const ULONGEST block_size = upc_blocksizeof (tt);
  const ULONGEST ptrtype_len = TYPE_LENGTH (ptrtype);
  uda_tword_t packed_pts_len;
  uda_target_pts_t packed_pts;
  struct value *val;
  if (!uda_calls.uda_pack_pts)
    error (_("UPC language support is not initialised"));  
  status = (*uda_calls.uda_pack_pts) (pts.addrfield, pts.thread, pts.phase,
                         block_size, (size_t *)&packed_pts_len, &packed_pts);
  if (status != uda_ok)
    error (_("upc_value_from_pts: uda_pack_pts error"));
  gdb_assert (ptrtype_len == packed_pts_len);
  val = allocate_value (ptrtype);
  memcpy (value_contents_all_raw (val), &packed_pts, packed_pts_len);
  return val;
}

gdb_upc_pts_t
upc_value_as_pts (struct value *val)
{
  int status;
  struct type *type = check_typedef (value_type (val));
  struct type *tt = check_typedef (TYPE_TARGET_TYPE (type));
  ULONGEST block_size = upc_blocksizeof (tt);
  unsigned pts_len = upc_pts_len (type);
  gdb_upc_pts_t pts;
  uda_target_pts_t *pts_raw;
  if (!uda_calls.uda_unpack_pts)
    error (_("UPC language support is not initialised"));    
  pts_raw = (uda_target_pts_t *) value_contents_all (val);
  status = (*uda_calls.uda_unpack_pts) (pts_len, pts_raw, block_size, &pts);
  if (status != uda_ok)
    error (_("upc_value_as_pts: uda_unpack_pts error"));
  return pts;
}

struct value *
upc_value_at_lazy (struct type *type, gdb_upc_pts_t pts)
{
  struct value *val;

  CHECK_TYPEDEF (type);
  if (TYPE_CODE (type) == TYPE_CODE_VOID)
    error (_("Attempt to dereference a generic pointer-to-shared."));

  val = allocate_value (type);

  VALUE_LVAL (val) = lval_upc_shared;
  VALUE_SHARED_ADDR (val) = pts;
  set_value_lazy (val, 1);

  return val;
}

void
upc_value_fetch_lazy (struct value *val)
{
  struct type *type = check_typedef (value_type (val));
  unsigned length = TYPE_LENGTH (value_enclosing_type (val));
  ULONGEST block_size = upc_blocksizeof (type);
  gdb_upc_pts_t pts = VALUE_SHARED_ADDR (val);
  int status;
  if (!uda_calls.uda_calc_pts_index_add)
    error (_("UPC language support is not initialised"));  
  if (TYPE_CODE (type) == TYPE_CODE_ARRAY)
    {
      struct type *elem_type = check_typedef (TYPE_TARGET_TYPE (type));
      unsigned elem_size = TYPE_LENGTH (elem_type);
      unsigned n_elems = length / elem_size;
      int i;
      for (i = 0; i < n_elems; ++i)
        {
          gdb_upc_pts_t elem_pts;
          status = (*uda_calls.uda_calc_pts_index_add) (&pts, i, elem_size, block_size, &elem_pts);
	  if (status != uda_ok)
	    error (_("upc_value_fetch_lazy: uda_calc_pts_index_add error"));
	  upc_read_shared_mem (elem_pts.addrfield, elem_pts.thread,
			       value_contents_all_raw (val) + i * elem_size,
			       elem_size);
	}
    }
  else
    {
      pts.addrfield += value_offset (val);
      upc_read_shared_mem (pts.addrfield, pts.thread,
			   value_contents_all_raw (val), length);
    }
}

unsigned
upc_pts_len (struct type *target_type)
{
  int status;
  struct type *tt = target_type;
  const ULONGEST block_size = upc_blocksizeof (tt);
  uda_tword_t pts_len;
  CHECK_TYPEDEF (tt);
  status = (*uda_calls.uda_length_of_pts) (block_size, &pts_len);
  if (status != uda_ok)
    error (_("upc_pts_len: uda_length_of_pts error"));
  return pts_len;
}

void
upc_print_pts (struct ui_file *stream,
               int format,
               struct type *target_type,
               const gdb_byte *pts_bytes)
{
  int status;
  struct type *tt;
  ULONGEST block_size, pts_len;
  char buf[100];
  uda_debugger_pts_t pts;
  tt = target_type; CHECK_TYPEDEF (tt);
  block_size = upc_blocksizeof (tt);
  pts_len = upc_pts_len (tt);
  gdb_assert (pts_len <= sizeof(uda_target_pts_t));
  if (!uda_calls.uda_unpack_pts)
    error (_("UPC language support is not initialised"));  
  status = (*uda_calls.uda_unpack_pts) (pts_len, (const uda_target_pts_t *)pts_bytes,
                           block_size, &pts);
  if (status != uda_ok)
    error (_("upc_print_pts: uda_unpack_pts error"));
  if (!format && pts.thread < 10 && (ULONGEST)pts.phase < 10)
    format = 'd';
  if (block_size <= 1 && pts.phase == 0)
    {
      if (!format || format == 'x')
	{
	  sprintf (buf, "(0x%0"LONGEST_FMT"x,0x%"LONGEST_FMT"x)",(ULONGEST)pts.addrfield, 
				         (ULONGEST)pts.thread);
	}
      else
	{
	  sprintf (buf, "(0x%0"LONGEST_FMT"x,%"LONGEST_FMT"d)",(ULONGEST)pts.addrfield, 
				       (ULONGEST)pts.thread);
	}
    }
  else
    {
      if (!format || format == 'x')
	{
	  sprintf (buf, "(0x%0"LONGEST_FMT"x,0x%"LONGEST_FMT"x,0x%"LONGEST_FMT"x)",(ULONGEST)pts.addrfield, 
					       (ULONGEST)pts.thread,
					       (ULONGEST)pts.phase);
	}
      else
	{
	  sprintf (buf, "(0x%0"LONGEST_FMT"x,%"LONGEST_FMT"d,%"LONGEST_FMT"d)",(ULONGEST)pts.addrfield, 
					   (ULONGEST)pts.thread,
					   (ULONGEST)pts.phase);
	}
    }
  fputs_filtered (buf, stream);
}

int
upc_read_shared_mem (ULONGEST address, ULONGEST thread,
                     gdb_byte *data, int length)
{
  int status;
  uda_binary_data_t rdata;
  if (!uda_calls.uda_read_shared_mem)
    error (_("UPC language support is not initialised"));  
  status = (*uda_calls.uda_read_shared_mem) (address, thread, length, &rdata);

  if (status != uda_ok)
    {
      error (_("Cannot read shared memory"));
      abort ();
    }
  gdb_assert (rdata.len == length);
  memcpy (data, rdata.bytes, length);
  xfree (rdata.bytes);
  return 0;
}

struct value *
upc_read_var_value (struct symbol *var, struct frame_info *frame)
{
  struct value *v;
  struct type *type = SYMBOL_TYPE (var);

  if (!upc_shared_type_p (type))
    return default_read_var_value (var, frame);

  v = allocate_value (type);
  VALUE_SHARED_ADDR (v) = upc_shared_var_address (var);
  VALUE_LVAL (v) = lval_upc_shared;
  set_value_lazy (v, 1);
  return v;
}

void
upc_expand_threads_factor (struct type *type)
{
  int threads;
  if (TYPE_CODE (type) != TYPE_CODE_RANGE
      || !TYPE_UPC_HAS_THREADS_FACTOR (type))
    return;
  threads = upc_thread_count();
  if (threads == 0)
    return;
  TYPE_HIGH_BOUND (type) = (TYPE_HIGH_BOUND (type) + 1) * threads - 1;
  TYPE_INSTANCE_FLAGS (type) &= ~TYPE_INSTANCE_FLAG_UPC_HAS_THREADS_FACTOR;  
}

static void
thread_value_read (struct value *v)
{
  pack_long (value_contents_raw (v), value_type (v), upc_threads);
}

static struct lval_funcs thread_value_funcs =
  {
    thread_value_read,
  };

static struct value *
thread_make_value (struct gdbarch *gdbarch, struct internalvar *var, void *data)
{
  struct type *type = builtin_type (gdbarch)->builtin_int;
  return allocate_computed_value (type, &thread_value_funcs, NULL);
}

static const struct internalvar_funcs thread_funcs =
{
  thread_make_value,
  NULL,
  NULL
};

static void
mythread_value_read (struct value *v)
{
  if (upcsingle)
    pack_long (value_contents_raw (v), value_type (v), mythread);
  else
    pack_long (value_contents_raw (v), value_type (v), upc_current_thread_num ());
}

static struct lval_funcs mythread_value_funcs =
  {
    mythread_value_read,
  };

static struct value *
mythread_make_value (struct gdbarch *gdbarch, struct internalvar *var, void *data)
{
  struct type *type = builtin_type (gdbarch)->builtin_int;
  return allocate_computed_value (type, &mythread_value_funcs, NULL);
}

static const struct internalvar_funcs mythread_funcs =
{
  mythread_make_value,
  NULL,
  NULL
};

void
upc_lang_init (char *cmd, int from_tty)
{
  uda_target_type_sizes_t targ_info;
  struct gdbarch *arch = target_gdbarch ();
  int is_big_endian = gdbarch_byte_order (arch) == BFD_ENDIAN_BIG;
  uda_tword_t num_threads;
  ptid_t  current_ptid = inferior_ptid;
  int status;
  char *uda_service;
  char *uda_path;

  /* Select UDA plugin you wish to use. By default try to
   * connect to UDA server. */

  /* Check for GNU UPC (GCCUPC) plugin first */
  uda_path = getenv ("UDA_GUPC_PLUGIN_LIBRARY");
  if (uda_path)
   {
     init_uda_plugin (&uda_calls, uda_path);
     printf_filtered("upc_lang: using GUPC plugin.\n");
   }
  else
   {
     /* Default - connect to the UDA server. */
     uda_service = getenv ("UDA_SERVICE");
     if (!uda_service)
       uda_service = UDA_SERVICE;
       uda_client_connect (uda_service);
       printf_filtered("upc_lang: connected to UDA server.\n");
     init_uda_client (&uda_calls);
   }

  /* Send traget info. to server. */
  targ_info.short_size = gdbarch_short_bit (arch) / 8;
  targ_info.int_size = gdbarch_int_bit (arch) / 8;
  targ_info.long_size = gdbarch_long_bit (arch) / 8;
  targ_info.long_long_size = gdbarch_long_long_bit (arch) / 8;
  targ_info.pointer_size = gdbarch_ptr_bit (arch) / 8;
  if (from_tty)
    printf_filtered("upc_lang_init: set type information for UDA.\n");
  status = (*uda_calls.uda_set_type_sizes_and_byte_order) (targ_info, is_big_endian);
  if (status != uda_ok)
    error (_("uda_set_type_sizes_and_byte_order() failed."));
  if (from_tty)
    printf_filtered("upc_lang_init: set THREADS value for UDA.\n");
  status = (*uda_calls.uda_get_num_threads) (&num_threads);
  if (status == uda_ok && num_threads > 0)
    {
      upc_threads = num_threads;
    }
  else
    {
      struct symbol *threads_sym;
      struct value *threads_val;

      if (from_tty)
        printf_filtered("upc_lang_init: send THREADS value to UDA server.\n");
      
      /* UPC program? */
      threads_sym = lookup_symbol ("THREADS", 0, VAR_DOMAIN, NULL);
      if (threads_sym)
        {
          threads_val = read_var_value (threads_sym, NULL);
          if (threads_val)
	    upc_threads = value_as_long (threads_val);
        }
      else
        {
          error (_("upc_lang_init: Can't find THREADS variable. Is this a UPC program?"));
        }
      status = (*uda_calls.uda_set_num_threads) (upc_threads);
      if (status != uda_ok)
        error (_("upc_lang_init: uda_set_num_threads() failed."));
    }
  if (upcsingle)
    {
      status = (*uda_calls.uda_get_thread_num) (&mythread);
      if (status != uda_ok)
	error (_("upc_lang_init: uda_get_thread_num() failed."));
    }
  if (from_tty)
    printf_filtered("upc_lang_init: done.\n");
  create_internalvar_type_lazy ("THREADS", &thread_funcs, NULL);
  create_internalvar_type_lazy ("MYTHREAD", &mythread_funcs, NULL);
  upc_lang_initialized = 1;
}

extern void _initialize_upc_language (void);

void
_initialize_upc_language (void)
{
  add_com ("upc-init", no_class, upc_lang_init, _("Test: connect to UDA server"));
}
