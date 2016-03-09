/* UDA plugin interface definiiion.

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
 * UPC Debug Agent (UDA) plugin definitions.
 *
 * This description of the interface between the UDA server and
 * the dynamically loaded UDA plugin is derived from
 * the Etnus upc_assistant.h header file originally
 * developed by James Cownie.
 * [Copyright (C) 2001-2002 Etnus, LLC]
 * See: http://www.etnus.com/Support/developers/upc_assistant.h
 *
 */
#ifndef _UDA_SERVER_PLUGIN_H_
#define _UDA_SERVER_PLUGIN_H_ 1

#include "uda-plugin-cb.h"

/***********************************************************************
 * Calls from the debugger into the plugin (DLL/shared library).
 ***********************************************************************/

/* Provide the library with the pointers to the the debugger functions
 * it needs The DLL need only save the pointer, the debugger promises
 * to maintain the table of functions valid for as long as
 * needed. (The table remains the property of the debugger, and should
 * not be messed with, or deallocated by the DLL). This applies to
 * all of the callback tables.
 */
typedef void (*uda_setup_basic_callbacks_fp_t) (const uda_basic_callbacks_t *);

/* Version handling */
/* Return a printable string which identifies the assistant library,
 * suitable for logging by the debugger, or reporting by a customer in
 * a fault report.
 */
typedef const char *(*uda_version_string_fp_t) (void);

/* Return the version compatinility UPCDA_INTERFACE_COMPATIBILITY
 * so that the debugger can check that it can use this assistant.
 */
typedef int (*uda_version_compatibility_fp_t) (void);

/* Provide a text string for an error value */
typedef const char *(*uda_error_string_fp_t) (int);

/* Destroy any information associated with a uda_job_t */
typedef void (*uda_destroy_job_info_fp_t) (uda_job_t *);
/* Destroy any information associated with a uda_thread_t */
typedef void (*uda_destroy_thread_info_fp_t) (uda_thread_t *);
/* Destroy any information associated with a uda_image_t */
typedef void (*uda_destroy_image_info_fp_t) (uda_image_t *);
/***********************************************************************
 * Useful calls which do real work, rather than just housekeeping.
 */

/* Let the assistant check that the target job really is suitable for
 *  the assistant.
 *
 * This call is made by the debugger into the assistant once per job
 * to allow the assistant to do whatever checking of the target and
 * information cacheing it wants to.
 *
 * The debugger will make the call _after_ the job has started and
 * created all its UPC threads, but before the debugger makes any
 * other call specific to this job (or its threads).  
 *
 * If this call fails (does not return uda_ok), then the debugger will
 * not use the assistant to help it when debugging this job.
 */
typedef int (*uda_initialize_job_fp_t) (uda_job_t *);

/* Return the value of MYTHREAD for a specific uda_thread_t object.
 * This allows the debugger to determine the mapping between an 
 * unordered bag of threads/processes and the UPC world.
 *
 * As ever the result of the function is the status, the value
 * of MYTHREAD is returned through the second argument.
 *
 * We need this function because MYTHREAD is a somewhat odd object in UPC
 * and may not be present in the debug information, therefore we require
 * help from the assistant library to extract its value.
 */
typedef int (*uda_get_threadno_fp_t) (uda_thread_t *, int *);

/* Convert the target process representation of a shared pointer to
 * the unpacked representation used by the debugger.
 *
 * Note :-
 *
 * 1) That the debugger is fetching those bits from the target and 
 *    placing them in a potentially unaligned buffer.
 * 2) It is the address of this buffer which is passed to the pack/unpack
 *    routines. 
 * 3) The data in the buffer is in target byte order, it has not been
 *    manipulated by the debugger in any way. (It's as if it had done
 *    a memcpy from the target).
 *
 * The debugger is responsible for fetching these objects because they may
 * reasonably be stored in registers, and constructing an interface to the
 * DLL which allows it to specify general addresses (which include registers)
 * is rather complicated (and unnecessary if the debugger can already
 * fetch the appropriate bits).
 * 
 * 4) The debugger passes in the UPC thread in case the assistant needs that 
 *    context.
 * 5) The debugger passes in the block_size of the PTS' target, in case
 *    the UPC implementation uses different PTS objects depending on the
 *    block size.
 *
 */
typedef int (*uda_unpack_pts_fp_t) (uda_thread_t *,
                                    const uda_target_pts_t *packed_pts,
				    const uda_tword_t block_size,
				    uda_debugger_pts_t *pts);

/* Convert the unpacked representation of a shared pointer used by the
 * debugger back to the packed representation used by the target.
 */
typedef int (*uda_pack_pts_fp_t) (uda_thread_t *,
                                  const uda_debugger_pts_t *pts,
				  const uda_tword_t block_size,
				  size_t *packed_pts_len,
				  uda_target_pts_t *packed_pts);

/* Return the size of a shared pointer for a target with the requested
 * block size.  The debugger may be able to work this out from the
 * debug information, but it is also convenient to have it here.
 * Note that this call is on an image, so the assistant must be able to 
 * work out the result without reading target store, since the debugger
 * may call this before the job has started. Since this is likely dependent
 * on a compilation mode (or is simply constant), that should be fine.
 */
typedef int (*uda_length_of_pts_fp_t) (uda_image_t *,
				       uda_tword_t block_size,
				       uda_tword_t *);

/* Tell the debugger whether to allow the user to see the "opaque" field
 * of a PTS with the given properties.
 * A result of uda_ok means show the opaque value, anything else means don't.
 */
typedef int (*uda_show_opaque_fp_t) (uda_image_t *,
				     uda_tword_t block_size,
				     uda_tword_t element_size);

/* Convert a PTS into a an absolute address. We assume that the thread in the
 * PTS does not change ! We pass in the block size and element size of the
 * pointer target in case that affects the calculation.
 */
typedef int (*uda_pts_to_addr_fp_t) (uda_thread_t *,
				     const uda_debugger_pts_t *,
				     uda_tword_t block_size,
				     uda_tword_t element_size, uda_taddr_t *);

/* Functions for PTS address arithmetic. 
 *
 * Index a PTS
 * Given a PTS, the target element size, the target blocking, the number of threads,
 * and the index required updates the PTS to point to that element.
 */
typedef int (*uda_index_pts_fp_t) (uda_thread_t *,
                                   const uda_debugger_pts_t *,
				   const uda_tword_t element_size,
				   const uda_tword_t block_size,
				   const uda_tword_t thread_count,
				   const uda_tword_t delta,
                                   uda_debugger_pts_t *);

/* Compute the value of p1 - p2 where p1 and p2 are compatible
 * pointers to shared with properties detailed in the other arguments,
 * the result is returned in delta
 */
typedef int (*uda_pts_difference_fp_t) (uda_thread_t *,
					const uda_debugger_pts_t * p1,
					const uda_debugger_pts_t * p2,
					const uda_tword_t block_size,
					const uda_tword_t element_size,
					const uda_tword_t thread_count,
					uda_tint_t * delta);

/* Given the name of a symbol of a shared type (therefore a static or
 * global symbol), and its address (as given by the debug information)
 * in a thread compute the pointer-to-shared which represents the
 * address of the symbol.
 */
typedef int (*uda_symbol_to_pts_fp_t) (uda_thread_t *, const char *,
				       uda_taddr_t sym_addr,
				       uda_tword_t block_size,
				       uda_tword_t element_size,
				       uda_debugger_pts_t *);
typedef int (*uda_read_upc_shared_mem_fp_t) (uda_thread_t *,
                                             const uda_taddr_t addrfield,
					     const uda_tword_t *actual_length,
					     const uda_tword_t length,
					     void *bytes);
typedef int (*uda_write_upc_shared_mem_fp_t) (uda_thread_t *,
					      const uda_taddr_t addrfield,
					      const uda_tword_t length,
					      uda_tword_t *bytes_written,
					      void *bytes);

typedef struct uda_plugin_struct
{
  uda_setup_basic_callbacks_fp_t uda_setup_basic_callbacks_fp;
  uda_version_string_fp_t uda_version_string_fp;
  uda_version_compatibility_fp_t uda_version_compatibility_fp;
  uda_error_string_fp_t uda_error_string_fp;
  uda_destroy_job_info_fp_t uda_destroy_job_info_fp;
  uda_destroy_thread_info_fp_t uda_destroy_thread_info_fp;
  uda_destroy_image_info_fp_t uda_destroy_image_info_fp;
  uda_initialize_job_fp_t uda_initialize_job_fp;
  uda_get_threadno_fp_t uda_get_threadno_fp;
  uda_unpack_pts_fp_t uda_unpack_pts_fp;
  uda_pack_pts_fp_t uda_pack_pts_fp;
  uda_length_of_pts_fp_t uda_length_of_pts_fp;
  uda_show_opaque_fp_t uda_show_opaque_fp;
  uda_pts_to_addr_fp_t uda_pts_to_addr_fp;
  uda_index_pts_fp_t uda_index_pts_fp;
  uda_pts_difference_fp_t uda_pts_difference_fp;
  uda_symbol_to_pts_fp_t uda_symbol_to_pts_fp;
  uda_read_upc_shared_mem_fp_t uda_read_upc_shared_mem_fp;
  uda_write_upc_shared_mem_fp_t uda_write_upc_shared_mem_fp;
} uda_plugin_t;

extern uda_plugin_t uda_plugin;

/* Define short hand functions to call plugin routines.  */
#define uda_setup_basic_callbacks(a)((*uda_plugin.uda_setup_basic_callbacks_fp)(a))
#define uda_version_stringl()((*uda_plugin.uda_version_string_fp)())
#define uda_version_compatibility()((*uda_plugin.uda_version_compatibility_fp)())
#define uda_error_string(a)((*uda_plugin.uda_error_string_fp)((a)))
#define uda_destroy_job_info(a)((*uda_plugin.uda_destroy_job_info_fp)((a)))
#define uda_destroy_thread_info(a)((*uda_plugin.uda_destroy_thread_info_fp)((a)))
#define uda_destroy_image_info(a)((*uda_plugin.uda_destroy_image_info_fp)((a)))
#define uda_initialize_job(a)((*uda_plugin.uda_initialize_job_fp)((a)))
#define uda_get_threadno(a,b)((*uda_plugin.uda_get_threadno_fp)((a),(b)))
#define uda_unpack_pts(a,b,c,d)((*uda_plugin.uda_unpack_pts_fp)((a),(b),(c),(d)))
#define uda_pack_pts(a,b,c,d,e)((*uda_plugin.uda_pack_pts_fp)((a),(b),(c),(d),(e)))
#define uda_length_of_pts(a,b,c)((*uda_plugin.uda_length_of_pts_fp)((a),(b),(c)))
#define uda_show_opaque(a,b,c)((*uda_plugin.uda_show_opaque_fp)((a),(b),(c)))
#define uda_pts_to_addr(a,b,c,d,e)((*uda_plugin.uda_pts_to_addr_fp)((a),(b),(c),(d),(e)))
#define uda_index_pts(a,b,c,d,e,f,g)((*uda_plugin.uda_index_pts_fp)((a),(b),(c),(d),(e),(f),(g)))
#define uda_pts_difference(a,b,c,d,e,f,g)((*uda_plugin.uda_pts_difference_fp)((a),(b),(c),(d),(e),(f),(g)))
#define uda_symbol_to_pts(a,b,c,d,e,f)((*uda_plugin.uda_symbol_to_pts_fp)((a),(b),(c),(d),(e),(f)))
#define uda_read_upc_shared_mem(a,b,c,d,e)((*uda_plugin.uda_read_upc_shared_mem_fp)((a),(b),(c),(d),(e)))
#define uda_write_upc_shared_mem(a,b,c,d,e)((*uda_plugin.uda_write_upc_shared_mem_fp)((a),(b),(c),(d),(e)))

#endif /* !_UDA_SERVER_PLUGIN_H_ */
