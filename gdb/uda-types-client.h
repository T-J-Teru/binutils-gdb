/* UPC Debug Agent (UDA) type defitions for GDB, the GNU debugger.

   Contributed by: Gary Funck <gary@intrepid.com>

   Derived from the upc_assistant.h header file, developed by
   James Cownie [Copyright (C) 2001-2002 Etnus, LLC, Permission
   is granted to use, reproduce, prepare derivative works,
   and to redistribute to others.]

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

#ifndef _UDA_TYPES_H_
#define _UDA_TYPES_H_ 1

#include "defs.h"

/* Target types */

typedef ULONGEST uda_taddr_t;	/* Target address */
typedef ULONGEST uda_tword_t;	/* Target unsigned word */
typedef LONGEST  uda_tint_t;	/* Target signed integer */

typedef struct
{
  uda_taddr_t value;
  uda_tword_t opaque;		/* Extra information which may be needed */
} uda_relocatable_addr_t;

/* A structure for (target) architectural information */

typedef struct
{
  u_int32_t short_size;		/* sizeof (short) */
  u_int32_t int_size;		/* sizeof (int)   */
  u_int32_t long_size;		/* sizeof (long)  */
  u_int32_t long_long_size;	/* sizeof (long long) */
  u_int32_t pointer_size;	/* sizeof (void *) */
} uda_target_type_sizes_t;


typedef gdb_byte uda_byte_t;

/* Packed PTS */

/* Define storage sufficient to hold a shared pointer
   on the target.  */

typedef struct
{
  uda_byte_t bytes[2  * sizeof (uda_taddr_t)];
}
uda_target_pts_t;

typedef gdb_upc_pts_t uda_debugger_pts_t;
typedef struct uda_binary_data_struct
{
  size_t len;
  uda_byte_t *bytes;
} uda_binary_data_t;

typedef char uda_string_t[4096];

/* Result codes. 
 * uda_ok is returned for success. 
 * Anything else implies a failure of some sort. 
 *
 * Most of the functions actually return one of these, however to avoid
 * any potential issues with different compilers implementing enums as
 * different sized objects, we actually use int as the result type.
 * 
 * Additional errors can be returned by the assistant or the debugger
 * in the appropriate ranges.
 *
 */
enum
{
  uda_unimplemented = -1,
  uda_ok = 0,
  uda_bad_assistant,
  uda_bad_job,
  uda_bad_num_threads,
  uda_bad_thread_index,
  uda_malloc_failed,
  uda_no_information,
  uda_no_symbol,
  uda_num_threads_already_set,
  uda_read_failed,
  uda_write_failed,
  uda_relocation_failed,
  uda_target_sizes_already_set,
  uda_first_assistant_code = 1000,
  uda_first_debugger_code = 2000,
  uda_incompatible_version = 3000,
  uda_init_already_done = 3001,
  uda_thread_busy = 3002,
  uda_need_init_first = 3003
};

#define uda_error_belongs_to_assistant(code) \
         (((code) >= uda_first_assistant_code) \
          && ((code) < uda_first_debugger_code))

#define uda_error_belongs_to_debugger(code) ((code) >= uda_first_debugger_code)


#endif /* _UDA_TYPES_H_ */
