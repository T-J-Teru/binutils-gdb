/* UDA type definitions.

Copyright 2007 by Intrepid Technology, Inc.
Copyright 2007 by Hewlett-Packard.

Written by; Gary Funck <gary@intrepid.com>
Reviewed by: Brian Wibecan and Tanya Klinchina, HP

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

(1) Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
(2) Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
(3) Neither the name of Intrepid Technology, or Hewlett-Packard
nor the names of its contributors may be used to endorse or promote
products derived from this software without specific prior written
permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  */

/*
 * UPC Debug Agent (UDA) type defitions.
 *
 * This data types used to define the API for the dynamically
 * loaded UDA library are derived from the Etnus upc_assistant.h
 * header file [Copyright (C) 2001-2002 Etnus, LLC] originally
 * developed by James Cownie.
 * See: http://www.etnus.com/Support/developers/upc_assistant.h
 *
 */

#ifndef _UDA_TYPES_H_
#define _UDA_TYPES_H_ 1

#include <stdint.h>
#include <sys/types.h>

/* Opaque types are used here to provide a degree of type checking
   through the prototypes of the interface functions. */

typedef struct uda_job_ uda_job_t;
typedef struct uda_thread_ uda_thread_t;

typedef struct uda_job_info_ uda_job_info_t;
typedef struct uda_thread_info_ uda_thread_info_t;

typedef struct uda_image_ uda_image_t;
typedef struct uda_image_info_ uda_image_info_t;

typedef struct uda_type_ uda_type_t;

/* Target types */

typedef uint64_t uda_taddr_t;	/* Target address */
typedef uint64_t uda_tword_t;	/* Target unsigned word */
typedef int64_t uda_tint_t;	/* Target signed integer */

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

/* Packed PTS */

/* Define storage sufficient to hold a shared pointer
   on the target.  */

typedef struct
{
  char bytes[2 * sizeof (uda_taddr_t)];
}
uda_target_pts_t;

/* Unpacked PTS */

typedef gdb_upc_pts_t uda_debugger_pts_t;

typedef uint8_t uda_byte_t;
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
