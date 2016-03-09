/* UDA plugin interface.

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

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "defs.h"
#include "value.h"
#include "target.h"
#include "symtab.h"
#include "objfiles.h"
#include "uda-types.h"
#include "uda-defs.h"
#include "uda-plugin-cb.h"
#include "uda-plugin.h"
#include "uda-plugin-iface.h"
#include "upc-thread.h"

/* UDA job information.  */
uda_job_t *uda_job;

/* Local callback implementation routines.  */
static void *uda_malloc(size_t size);
static void uda_free(void *);
static void uda_prints (const char *);
static const char *uda_db_error_string (int error_code);
static int uda_job_thread_count (uda_job_t *, size_t *);
static int uda_job_get_thread (uda_job_t *, uda_tword_t, uda_thread_t **);
static int uda_job_get_image (uda_job_t *, uda_image_t **);
static int uda_thread_get_job (uda_thread_t *, uda_job_t **);
static int uda_job_set_info (uda_job_t *, uda_job_info_t *);
static int uda_job_get_info (uda_job_t *, uda_job_info_t **);
static int uda_thread_set_info (uda_thread_t *, uda_thread_info_t *);
static int uda_thread_get_info (uda_thread_t *, uda_thread_info_t **);
static int uda_image_set_info (uda_image_t *, uda_image_info_t *);
static int uda_image_get_info (uda_image_t *, uda_image_info_t **);
static int uda_get_type_sizes (uda_image_t *, uda_target_type_sizes_t *);
static int uda_variable_lookup (uda_image_t *, const char *,
				uda_taddr_t *);
static int uda_type_lookup (uda_image_t *, const char *, uda_type_t **);
static int uda_type_length (uda_type_t *, uda_tword_t *);
static int uda_type_get_member_info (uda_type_t *, const char *,
				     uda_tword_t * bit_offset,
				     uda_tword_t * bit_length, uda_type_t **);
static int uda_relocate_address (uda_thread_t *,
				 const uda_taddr_t *,
				 uda_taddr_t *);
static int uda_thread_variable_lookup (uda_thread_t *, const char *,
				       uda_taddr_t *);
static int uda_thread_type_lookup (uda_thread_t *, const char *,
				   uda_type_t **);
static int uda_read_store (uda_thread_t *, uda_taddr_t, uda_tword_t, void *);
static int uda_write_store (uda_thread_t *, uda_taddr_t, uda_tword_t,
			    uda_tword_t *, void *);
static int uda_target_to_big_end (uda_thread_t *, uda_tword_t, const void *,
				  void *);
static int uda_big_end_to_target (uda_thread_t *, uda_tword_t, const void *,
				  void *);

/* Define the call back vector that is passed to the
   UDA plugin.  */
static const uda_basic_callbacks_t uda_callbacks = {
  uda_malloc,
  uda_free,
  uda_prints,
  uda_db_error_string,
  uda_get_type_sizes,
  uda_variable_lookup,
  uda_type_lookup,
  uda_relocate_address,
  uda_job_thread_count,
  uda_job_get_thread,
  uda_job_get_image,
  uda_thread_get_job,
  uda_job_set_info,
  uda_job_get_info,
  uda_thread_set_info,
  uda_thread_get_info,
  uda_image_set_info,
  uda_image_get_info,
  uda_type_length,
  uda_type_get_member_info,
  uda_read_store,
  uda_write_store,
  uda_target_to_big_end,
  uda_big_end_to_target,
  uda_thread_type_lookup,
  uda_thread_variable_lookup
};

static uda_type_t *uda_type_id_to_type (uda_tword_t);

/* TODO: move these procedures into a separate file that
 * can be used for uda-client.c too */

#define TYPE_TBL_INIT_ALLOC 256

static const struct type **type_tbl;
static size_t type_tbl_alloc_size;
static size_t type_tbl_size;

static CORE_ADDR
lookup_symbol_address (const char *symbol)
{
  const struct minimal_symbol *msym;
  CORE_ADDR retaddr;
  struct obj_section *obj_section;
  msym = lookup_minimal_symbol (symbol, NULL, NULL);
  if (!msym)
    return 0;
  retaddr = SYMBOL_VALUE_ADDRESS(msym);
  obj_section = SYMBOL_OBJ_SECTION (msym);
  if (obj_section && (obj_section->the_bfd_section->flags & SEC_THREAD_LOCAL) != 0)
    retaddr = target_translate_tls_address (obj_section->objfile, retaddr);
  return retaddr;
}

static
int
get_type_id (const struct type *type)
{
  int i;
  for (i = 1; i <= type_tbl_size; ++i)
    {
      if (type_tbl[i-1] == type)
        return i;
    }
  if (type_tbl_size == type_tbl_alloc_size)
    {
      type_tbl_alloc_size *= 2;
      type_tbl = (const struct type **) xrealloc ((char *) type_tbl,
                    type_tbl_alloc_size * sizeof (*type_tbl));
    }
  type_tbl[type_tbl_size++] = type;
  return type_tbl_size;
}

static
int
lookup_type_by_name (const char *type_name)
{
  const struct symbol *sym;
  struct type *type;
  int type_id;
  struct symbol_search *matches;
  /* FIXME: add ^$ anchors to front/back of type_name, so
 *      that the regex matches only the desired type name.  */
  search_symbols ((char *)type_name, TYPES_DOMAIN,
                   0 /* nfiles */, NULL /* files */, &matches);
  if (!matches)
    return 0;
  /* Arbitrarily use the first match.  */
  sym = matches->symbol;
  free_search_symbols (matches);
  type = SYMBOL_TYPE (sym);
  CHECK_TYPEDEF (type);
  type_id = get_type_id ( type);
  return type_id;
}

static
const struct type *
lookup_type_by_id (const int type_id)
{
  if (type_id < 1 || type_id > type_tbl_size)
    return NULL;
  return type_tbl[type_id-1];
}

static
const struct type *
lookup_type_member (const struct type *parent_type, const char *field_name,
                    uda_tword_t *bit_offset, uda_tword_t *bit_length)
{
  struct type *ptype = (struct type *)parent_type;
  struct type *ftype = NULL;
  int n_fields, i;
  *bit_offset = 0;
  *bit_length = 0;
  if (!(ptype && (TYPE_CODE (ptype) == TYPE_CODE_STRUCT
                  || TYPE_CODE (ptype) == TYPE_CODE_UNION)))
    return NULL;
  CHECK_TYPEDEF (ptype);
  n_fields = TYPE_NFIELDS (ptype);
  for (i = 0; i < n_fields; ++i)
    {
      const char *t_field_name = TYPE_FIELD_NAME (ptype, i);
      if (t_field_name && (strcmp (t_field_name, field_name) == 0))
        break;
    }
  if (i >= n_fields)
    return NULL;
  *bit_offset = TYPE_FIELD_BITPOS(ptype, i);
  *bit_length = TYPE_FIELD_BITSIZE(ptype, i);
  ftype = TYPE_FIELD_TYPE(ptype, i);
  CHECK_TYPEDEF (ftype);
  if (!*bit_length)
    *bit_length = TYPE_LENGTH (ftype) * 8;
  return ftype;
}

/* TODO: END */

static int
uda_iface_set_num_threads (const uda_tword_t num_threads)
{
  size_t n;
  if (num_threads < 1 || num_threads > 65535)
    return uda_bad_num_threads;
  if (uda_job->num_threads != 0)
    return uda_num_threads_already_set;
  uda_job->num_threads = num_threads;
  uda_job->threads = (uda_thread_t *) calloc (num_threads,
					      sizeof (uda_thread_t));
  if (!uda_job->threads)
    {
      perror ("UDA threads table allocation failed");
      abort ();
    }
  for (n = 0; n < num_threads; ++n)
    {
      uda_thread_t *thread = &uda_job->threads[n];
      thread->mark = UDA_THREAD_MARK;
      thread->id = n;
      thread->job = uda_job;
    }
  uda_job->current_thread = &uda_job->threads[0];
  uda_initialize_job (uda_job);
  return uda_ok;
}

static int
uda_iface_set_thread_num (const uda_tword_t thread_num)
{
  if (thread_num >= uda_job->num_threads)
    return uda_bad_thread_index;
  uda_job->current_thread = &uda_job->threads[thread_num];
  return uda_ok;
}

static int
uda_iface_get_num_threads (uda_tword_t *num_threads)
{
  *num_threads = uda_job->num_threads;
  return uda_ok;
}

static int
uda_iface_get_thread_num (uda_tword_t *thread_num)
{
  int status;
  int n = 0;
  status = uda_get_threadno (uda_job->current_thread, &n);
  *thread_num = n;
  /* current_thread may be set to thread 0 but we may actually be debugging
     a different thread. Destroy the thread info so we don't retain the
     incorrect thread no.  */
  uda_destroy_thread_info (uda_job->current_thread);
  uda_job->current_thread->info = NULL;
  return status;
}

static
int
uda_iface_set_type_sizes_and_byte_order (const uda_target_type_sizes_t sizes,
		const uda_tword_t byte_order)
{
  uda_image_t *image = uda_job->image;
  if (image->target_sizes.int_size != 0)
    return uda_target_sizes_already_set;
  image->target_sizes = sizes;
  image->target_is_big_end = byte_order;
  image->target_pts_has_opaque = 0;
  return uda_ok;
}

static
int
uda_iface_symbol_to_pts (
		      const uda_tword_t elem_size,
		      const uda_tword_t block_size,
		      const uda_taddr_t addrfield,
                      const char *symbol,
		      uda_debugger_pts_t * pts)
{
  uda_thread_t *thread = uda_job->current_thread;
  int status;
  memset (pts, '\0', sizeof (*pts));
  status = uda_symbol_to_pts (thread,
			      symbol, addrfield, block_size, elem_size, pts);
  return status;
}

static
int
uda_iface_unpack_pts (const size_t packed_pts_len,
                   const uda_target_pts_t * packed_pts,
		   const uda_tword_t block_size,
		   const uda_tword_t elem_size,		   
		   uda_debugger_pts_t * pts)
{
  uda_thread_t *thread = uda_job->current_thread;
  int status;
  status = uda_unpack_pts (thread, packed_pts, block_size, pts);
  return status;
}

static
int
uda_iface_pack_pts (const uda_taddr_t addrfield,
                 const uda_tword_t thread,
		 const uda_tword_t phase,
		 const uda_tword_t block_size,
		 const uda_tword_t elem_size,		 
		 size_t * packed_pts_len,
		 uda_target_pts_t * packed_pts)
{
  uda_debugger_pts_t pts;
  uda_thread_t *thread0 = NULL;
  int status;
  pts.addrfield = addrfield;
  pts.thread = thread;
  pts.phase = phase;
  uda_job_get_thread (uda_job, 0, &thread0);
  memset (packed_pts, '\0', sizeof (uda_target_pts_t));
  *packed_pts_len = 0;
  status = uda_pack_pts (thread0, &pts, block_size,
			 packed_pts_len, packed_pts);
  return status;
}

static
int
uda_iface_length_of_pts (const uda_tword_t block_size,
		      uda_tword_t * pts_len)
{
  uda_image_t *image = uda_job->image;
  int status;
  *pts_len = 0;
  status = uda_length_of_pts (image, block_size, pts_len);
  return status;
}

static
int
uda_iface_calc_pts_index_add (const uda_debugger_pts_t * pts_operand,
			      const uda_tint_t index,
			      const uda_tword_t elem_size,
			      const uda_tword_t block_size,
			      uda_debugger_pts_t * pts)
{
  uda_thread_t *thread = uda_job->current_thread;
  uda_tword_t thread_cnt = uda_job->num_threads;
  int status;
  ULONGEST tmp;
  status = uda_index_pts (thread, pts_operand, index,
			  elem_size, block_size, thread_cnt, pts);
  return status;
}

static
int
uda_iface_calc_pts_diff (const uda_debugger_pts_t * pts_oprnd_1,
			 const uda_debugger_pts_t * pts_oprnd_2,
			 const uda_tword_t elem_size,
			 const uda_tword_t block_size, 
                         uda_tint_t * diff)
{
  uda_thread_t *thread = uda_job->current_thread;
  const uda_tword_t thread_count = uda_job->num_threads;
  int status;
  status = uda_pts_difference (thread, pts_oprnd_1, pts_oprnd_2,
			       elem_size, block_size, thread_count, diff);
  return status;
}

static
int
uda_iface_read_shared_mem (const uda_taddr_t addrfield,
                           const uda_tword_t thread_num,
			   const uda_tword_t phase,
                           uda_tword_t block_size,
                           uda_tword_t element_size,
			   const uda_tword_t length,
			   uda_binary_data_t *data)
{
  uda_thread_t *thread;
  int status;
  uda_tword_t actual_length;
  if (thread_num >= uda_job->num_threads)
    return uda_bad_assistant;
  thread = &uda_job->threads[thread_num];
  data->bytes = (uda_byte_t *) xmalloc ((size_t) length);
  status = uda_read_upc_shared_mem (thread, addrfield, &actual_length,
				    length, data->bytes);
  data->len = actual_length;
  return status;
}

static
int
uda_iface_pts_to_addr (const uda_debugger_pts_t * pts,
		    const uda_tword_t block_size,
		    const uda_tword_t elem_size, 
                    uda_taddr_t * addr)
{
  int status;
  const uda_tword_t thread_num = pts->thread;
  uda_thread_t *thread;
  if (thread_num >= uda_job->num_threads)
    return uda_bad_assistant;
  thread = &uda_job->threads[thread_num];
  status = uda_pts_to_addr (thread, pts, block_size, elem_size, addr);
  return status;
}

/* TODO: this procedure does not seem to be used - check if
 * length needs to be passed in or not - it is aalready contained
 * in data. */
static
int
uda_iface_write_shared_mem (const uda_taddr_t addrfield,
                            const uda_tword_t thread_num,
			    const uda_tword_t phase,
                            uda_tword_t block_size,
                            uda_tword_t element_size,
			    const uda_tword_t length,
			    uda_tword_t *bytes_written,
			    const uda_binary_data_t *data)
{
  int status;  
  uda_thread_t *thread;
  if (thread_num >= uda_job->num_threads)
    return uda_bad_assistant;  
  thread = &uda_job->threads[thread_num];
  status = uda_write_upc_shared_mem (thread, addrfield, length,
				     bytes_written, data->bytes);
  return status;
}


/* UDA callouts */

static
void *
uda_malloc(size_t size)
{
  void *ptr;
  ptr = malloc (size);
  assert (ptr);
  return ptr;
}

static
void
uda_free(void *ptr)
{
  if (ptr)
    free (ptr);
}

/* Print a message (intended for debugging use *ONLY*).  */

static void
uda_prints (const char *str)
{
  fputs (str, stderr);
}

/* Convert an error code from the debugger into an error message
 *  * (this cannot fail since it returns a string including the error
 *   * number if it is unknown */

static
const char *
uda_db_error_string (int error_code)
{
  const char *msg;
  static char mbuf[21];
  switch (error_code)
    {
    case uda_unimplemented:
      msg = "UDA: unimplemented operation"; break;
    case uda_ok:
      msg = "UDA: OK"; break;
    case uda_bad_assistant:
      msg = "UDA: bad assistant"; break;
    case uda_bad_job:
      msg = "UDA: bad uda_job"; break;
    case uda_bad_num_threads:
      msg = "UDA: bad num threads"; break;
    case uda_bad_thread_index:
      msg = "UDA: bad thread index"; break;
    case uda_no_information:
      msg = "UDA: no information"; break;
    case uda_no_symbol:
      msg = "UDA: no symbol"; break;
    case uda_num_threads_already_set:
      msg = "UDA: num threads already set"; break;
    case uda_read_failed:
      msg = "UDA: read failed"; break;
    case uda_write_failed:
      msg = "UDA: write failed"; break;
    case uda_relocation_failed:
      msg = "UDA: relocation failed"; break;
    case uda_target_sizes_already_set:
      msg = "UDA: target sizes already set"; break;
    case uda_incompatible_version:
      msg = "UDA: incompatible version"; break;
    case uda_init_already_done:
      msg = "UDA: init already done"; break;
    case uda_thread_busy:
      msg = "UDA: thread busy"; break;
    case uda_need_init_first:
      msg = "UDA: need init first"; break;
    default:
      sprintf (mbuf, "UDA: error %d",  error_code);
      msg = mbuf;
      break;
    }
  return msg;
}

static
void
uda_rmt_swap_bytes (uint8_t *dest,
                    const uint8_t *src,
                    const size_t n_bytes)
{
  int i, k;
  const int mid = (n_bytes + 1) / 2;
  for (i = 0, k = n_bytes - 1; i < mid; ++i, --k)
    {
      gdb_byte t = src[i];
      dest[i] = src[k];
      dest[k] = t;
    }
}

/* Given a job return the number of UPC threads in it. */
static int
uda_job_thread_count (uda_job_t * job, size_t * n_threads)
{
  if (!(job && job->mark == UDA_JOB_MARK))
    return uda_bad_job;
  *n_threads = job->num_threads;
  return uda_ok;
}

/* Given a job return the requested UPC thread within it. */
static int
uda_job_get_thread (uda_job_t * job, uda_tword_t thread_id,
		    uda_thread_t ** thread)
{
  if (!(job && job->mark == UDA_JOB_MARK))
    return uda_bad_job;
  if (thread_id >= job->num_threads)
    return uda_bad_thread_index;
  *thread = &job->threads[thread_id];
  if (!*thread)
    return uda_no_information;
  return uda_ok;
}

/* Given a job return the image associated with it. */
static int
uda_job_get_image (uda_job_t * job, uda_image_t ** image)
{
  if (!(job && job->mark == UDA_JOB_MARK))
    return uda_bad_job;
  *image = job->image;
  if (!*image)
    return uda_no_information;
  return uda_ok;
}

/* Given a thread return the job it belongs to. */
static int
uda_thread_get_job (uda_thread_t * thread, uda_job_t ** job)
{
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  *job = thread->job;
  return uda_ok;
}

/* These functions let the assistant library associate information
 *with specific jobs and threads and images. The definition of
 *uda_*_info is up to the assistant; the debugger only ever has a
 *pointer to it and doesn't look inside, so the assistant can store
 *whatever it wants.
 *
 *If a uda_get_*_info function is called before the corresponding
 *uda_set_*_info function has been called (or after the info has been
 *reset to (void *)0), then the get function will return
 *uda_no_information and nullify the result pointer.
 */
/* Associate information with a UPC job object */
static int
uda_job_set_info (uda_job_t * job, uda_job_info_t * info)
{
  if (!(job && job->mark == UDA_JOB_MARK))
    return uda_bad_job;
  job->info = info;
  return uda_ok;
}

static int
uda_job_get_info (uda_job_t * job, uda_job_info_t ** info)
{
  if (!(job && job->mark == UDA_JOB_MARK))
    return uda_bad_job;
  *info = job->info;
  if (!*info)
    return uda_no_information;
  return uda_ok;
}

/* Associate information with a UPC thread object */
static int
uda_thread_set_info (uda_thread_t * thread, uda_thread_info_t * info)
{
  if (!thread)
    return uda_bad_assistant;
  if (thread->mark != UDA_THREAD_MARK)
    return uda_bad_assistant;
  thread->info = info;
  return uda_ok;
}

static int
uda_thread_get_info (uda_thread_t * thread, uda_thread_info_t ** info)
{
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  *info = thread->info;
  if (!*info)
    return uda_no_information;
  return uda_ok;
}

/* Associate information with an image object */
static int
uda_image_set_info (uda_image_t * image, uda_image_info_t * info)
{
  if (!(image && image->mark == UDA_IMAGE_MARK))
    return uda_bad_assistant;
  image->info = info;
  return uda_ok;
}

static int
uda_image_get_info (uda_image_t * image, uda_image_info_t ** info)
{
  if (!(image && image->mark == UDA_IMAGE_MARK))
    return uda_bad_assistant;
  *info = image->info;
  if (!*info)
    return uda_no_information;
  return uda_ok;
}

/* Calls on an image */
/* Fill in the sizes of target types for this image */
static int
uda_get_type_sizes (uda_image_t * image, uda_target_type_sizes_t * sizes)
{
  if (!(image && image->mark == UDA_IMAGE_MARK))
    return uda_bad_assistant;
  *sizes = image->target_sizes;
  return uda_ok;
}

/* Lookup a global variable and return its relocatable address */
static int
uda_variable_lookup (uda_image_t * image,
		     const char *symbol, uda_taddr_t * addr)
{
  int status;
  if (!(image && image->mark == UDA_IMAGE_MARK))
    return uda_bad_assistant;
  *addr = lookup_symbol_address (symbol);
  if (!*addr)
    return uda_no_symbol;
  return uda_ok;
}

/* Lookup a type and return it */
static int
uda_type_lookup (uda_image_t * image,
		 const char *type_name, uda_type_t ** type)
{
  uda_tword_t type_id;
  int status;
  if (!(image && image->mark == UDA_IMAGE_MARK))
    return uda_bad_assistant;
  type_id = lookup_type_by_name (type_name);
  if (!type_id != uda_ok)
    return uda_no_information;
  *type = uda_type_id_to_type (type_id);
  return uda_ok;
}

/* Calls on a type */
/* Get the length of the type in bytes */
static int
uda_type_length (uda_type_t * type, uda_tword_t * length)
{
  int status;
  struct type *gdbtype;
  if (!(type && type->mark == UDA_TYPE_MARK))
    return uda_bad_assistant;
  gdbtype = (struct type *) lookup_type_by_id (type->type_id);
  if (!type)
    return uda_no_information;
  CHECK_TYPEDEF (gdbtype);
  *length = TYPE_LENGTH (gdbtype);
  return uda_ok;
}

/* Lookup a field within an aggregate type by name, and return
 * its _bit_ offset within the aggregate, its bit length and its type.
 */
static int
uda_type_get_member_info (uda_type_t * type,
			  const char * member_name,
			  uda_tword_t * bit_offset,
			  uda_tword_t * bit_length,
                          uda_type_t ** member_type)
{
  const struct type *struct_type;
  const struct type *new_member_type;
  uda_tword_t member_type_id;
  int status;
  if (!(type && type->mark == UDA_TYPE_MARK))
    return uda_bad_assistant;

  struct_type = lookup_type_by_id (type->type_id);
  if (!struct_type)
    return uda_no_information;
  new_member_type = lookup_type_member (struct_type, member_name,
                                     bit_offset, bit_length);
  if (!new_member_type)
    return uda_no_information;
  member_type_id = get_type_id (new_member_type);

  *member_type = uda_type_id_to_type (member_type_id);
  return uda_ok;
}

/* Calls on a uda_thread_t */
/* Relocate a relocatable address for use in a specific UPC thread */
static int
uda_relocate_address (uda_thread_t * thread,
		      const uda_taddr_t * reloc_addr,
		      uda_taddr_t * addr)
{
  int status;
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  *addr = *reloc_addr;
  return uda_ok;
}

/* Look up a variable in a thread.
 *This is more general than looking it up in an image, since it also
 *looks in any shared libraries which are linked into the thread.
 *Since this is thread specific the result is an absolute (rather than
 *relocatable) address. The address is therefore _only_ valid within
 *the specific thread.
 */
static int
uda_thread_variable_lookup (uda_thread_t * thread,
			    const char *symbol, uda_taddr_t * addr)
{
  int status;
  /* For pthreads we need to be on the correct thread
     for GDB to find address of the symbol */
  if (upc_pthread_active)
    {
      int old_thread_num;
      old_thread_num = upc_thread_set (thread->id);
      *addr = lookup_symbol_address (symbol);
      upc_thread_restore (old_thread_num);
    }
  else
    {
      *addr = lookup_symbol_address (symbol);
    } 
  if (!*addr)
    return uda_no_symbol;
  return uda_ok;
}

/* Look up a type in a thread.
 *This is more general than looking it up in an image, since it also
 *looks in any shared libraries which are linked into the thread.
 */
static int
uda_thread_type_lookup (uda_thread_t * thread,
			const char *type_id, uda_type_t ** type)
{
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  /* not yet implemented */
  return uda_no_information;
}

/* Read store from a specific UPC thread */
static int
uda_read_store (uda_thread_t * thread,
		uda_taddr_t addr, uda_tword_t length, void *bytes)
{
  int status;
  int old_thread_num;
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  if (upcsingle && thread->id != uda_job->current_thread->id)
    return uda_no_information;
  old_thread_num = upc_thread_set (thread->id);
  status = target_read_memory (addr, bytes, length);
  upc_thread_restore (old_thread_num);
  return status;
}

/* Write store in a specific UPC thread */
static int
uda_write_store (uda_thread_t * thread,
		 uda_taddr_t addr,
		 uda_tword_t length,
		 uda_tword_t * length_written, void *bytes)
{
  int status;
  int old_thread_num;
  if (!(thread && thread->mark == UDA_THREAD_MARK))
    return uda_bad_assistant;
  if (upcsingle && thread->id != uda_job->current_thread->id)
    return uda_no_information;
  old_thread_num = upc_thread_set (thread->id);
  status = target_write_memory (addr, bytes, length);
  upc_thread_restore (old_thread_num);
  *length_written = length;
  return status;
}

/* Convert data from target byte order to big endian byte order */
static int
uda_target_to_big_end (uda_thread_t * thread,
		       uda_tword_t length, const void *t_bytes, void *bytes)
{
  int targ_is_big_end = thread->job->image->target_is_big_end;
  if (targ_is_big_end)
    memcpy (bytes, t_bytes, (size_t) length);
  else
    uda_rmt_swap_bytes (bytes, t_bytes, (size_t) length);
  return uda_ok;
}

/* Convert data from big endian byte order to target byte order */
static int
uda_big_end_to_target (uda_thread_t * thread,
		       uda_tword_t length, const void *bytes, void *t_bytes)
{
  int targ_is_big_end = thread->job->image->target_is_big_end;
  if (targ_is_big_end)
    memcpy (t_bytes, bytes, (size_t) length);
  else
    uda_rmt_swap_bytes (t_bytes, bytes, (size_t) length);
  return uda_ok;
}

static uda_type_t *
uda_type_id_to_type (uda_tword_t type_id)
{
  uda_type_t *type;
  for (type = uda_job->types; type; type = type->next)
    {
      if (type->type_id == type_id)
	return type;
    }
  type = (uda_type_t *) calloc (1, sizeof (uda_type_t));
  if (!type)
    {
      perror ("UDA cannot allocate type node");
      abort ();
    }
  type->mark = UDA_TYPE_MARK;
  type->type_id = type_id;
  type->next = uda_job->types;
  uda_job->types = type;
  return type;
}

/* Initialize plugin shared library.
   Callouts structure is being set for shared library plugin. */

void
init_uda_plugin (uda_callouts_p calls, char *dl_path)
{
  uda_job = (uda_job_t *) calloc (1, sizeof (uda_job_t));
  if (!uda_job)
    {
      perror ("UDA job allocation failed");
      abort ();
    }
  uda_job->mark = UDA_JOB_MARK;
  uda_job->image = (uda_image_t *) calloc (1, sizeof (uda_image_t));
  if (!uda_job->image)
    {
      perror ("UDA image allocation failed");
      abort ();
    }
  uda_job->image->mark = UDA_IMAGE_MARK;
  load_uda_plugin (dl_path);
  uda_setup_basic_callbacks (&uda_callbacks);

  /* setup callouts for UPC language */
  calls->uda_set_num_threads = &uda_iface_set_num_threads;
  calls->uda_set_thread_num = &uda_iface_set_thread_num;
  calls->uda_get_num_threads = &uda_iface_get_num_threads;
  calls->uda_get_thread_num = &uda_iface_get_thread_num;  
  calls->uda_set_type_sizes_and_byte_order = &uda_iface_set_type_sizes_and_byte_order;
  calls->uda_symbol_to_pts = &uda_iface_symbol_to_pts;
  calls->uda_length_of_pts = &uda_iface_length_of_pts;
  calls->uda_unpack_pts = &uda_iface_unpack_pts;
  calls->uda_pack_pts = &uda_iface_pack_pts;
  calls->uda_calc_pts_index_add = &uda_iface_calc_pts_index_add;
  calls->uda_calc_pts_diff = &uda_iface_calc_pts_diff;
  calls->uda_pts_to_addr = &uda_iface_pts_to_addr;
  calls->uda_read_shared_mem = &uda_iface_read_shared_mem;
  calls->uda_write_shared_mem = &uda_iface_write_shared_mem;
}

