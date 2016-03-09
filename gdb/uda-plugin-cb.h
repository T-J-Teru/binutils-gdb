/* UDA plugin callback interface definition.

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

#ifndef _UDA_PLUGIN_CB_H_
#define _UDA_PLUGIN_CB_H_ 1

/***********************************************************************
 * Callbacks from the UDA plugin to the UDA server.
 *
 * All calls from the UDA plugin back into the UDA server are made via
 * a callback table, this means that the plugin DLL does not need
 * to be linked against the server to satisfy external symbols, and
 * makes it easier to extend the interface while being able to continue
 * to use old assistant plugin libraries.
 *
 * Here we describe the type signatures of the callbacks so that we
 * can then define a callback table.
 *
 * All non memory allocation callbacks which can fail return a status
 * value as the function result. The interesting result is returned
 * via a pointer argument.
 */

#include "uda-types.h"

/* Allocate store */
typedef void *(*uda_malloc_cb_t) (size_t);
/* Free it again */
typedef void (*uda_free_cb_t) (void *);

/* Print a message (intended for debugging use *ONLY*). */
typedef void (*uda_prints_cb_t) (const char *);

/* Convert an error code from the debugger into an error message 
 * (this cannot fail since it returns a string including the error
 * number if it is unknown */
typedef const char *(*uda_error_string_cb_t) (int);

/* Given a job return the number of UPC threads in it. */
typedef int (*uda_job_thread_count_cb_t) (uda_job_t *, size_t *);

/* Given a job return the requested UPC thread within it. */
typedef int (*uda_job_get_thread_cb_t) (uda_job_t *, uda_tword_t,
					uda_thread_t **);

/* Given a job return the image associated with it. */
typedef int (*uda_job_get_image_cb_t) (uda_job_t *, uda_image_t **);

/* Given a thread return the job it belongs to. */
typedef int (*uda_thread_get_job_cb_t) (uda_thread_t *, uda_job_t **);

/* These functions let the assistant library associate information
 * with specific jobs and threads and images. The definition of
 * uda_*_info is up to the assistant; the debugger only ever has a
 * pointer to it and doesn't look inside, so the assistant can store
 * whatever it wants.
 *
 * If a uda_get_*_info function is called before the corresponding
 * uda_set_*_info function has been called (or after the info has been
 * reset to (void *)0), then the get function will return
 * uda_no_information and nullify the result pointer.
 */
/* Associate information with a UPC job object */
typedef int (*uda_job_set_info_cb_t) (uda_job_t *, uda_job_info_t *);
typedef int (*uda_job_get_info_cb_t) (uda_job_t *, uda_job_info_t **);

/* Associate information with a UPC thread object */
typedef int (*uda_thread_set_info_cb_t) (uda_thread_t *, uda_thread_info_t *);
typedef int (*uda_thread_get_info_cb_t) (uda_thread_t *,
					 uda_thread_info_t **);

/* Associate information with an image object */
typedef int (*uda_image_set_info_cb_t) (uda_image_t *, uda_image_info_t *);
typedef int (*uda_image_get_info_cb_t) (uda_image_t *, uda_image_info_t **);

/* Calls on an image */
/* Fill in the sizes of target types for this image */
typedef int (*uda_get_type_sizes_cb_t) (uda_image_t *,
					uda_target_type_sizes_t *);
/* Lookup a global variable and return its relocatable address */
typedef int (*uda_variable_lookup_cb_t) (uda_image_t *, const char *,
					 uda_taddr_t *);
/* Lookup a type and return it */
typedef int (*uda_type_lookup_cb_t) (uda_image_t *, const char *,
				     uda_type_t **);

/* Calls on a type */
/* Get the length of the type in bytes */
typedef int (*uda_type_length_cb_t) (uda_type_t *, uda_tword_t *);
/* Lookup a field within an aggregate type by name, and return
 * its _bit_ offset within the aggregate, its bit length and its type.
 */
typedef int (*uda_type_get_member_info_cb_t) (uda_type_t *, const char *,
					      uda_tword_t *bit_offset,
					      uda_tword_t *bit_length,
					      uda_type_t **);

/* Calls on a uda_thread_t */
/* Relocate a relocatable address for use in a specific UPC thread */
typedef int (*uda_relocate_address_cb_t) (uda_thread_t *,
					  const uda_taddr_t *,
					  uda_taddr_t *);

/* Look up a variable in a thread.
 * This is more general than looking it up in an image, since it also
 * looks in any shared libraries which are linked into the thread.
 * Since this is thread specific the result is an absolute (rather than
 * relocatable) address. The address is therefore _only_ valid within
 * the specific thread.
 */
typedef int (*uda_thread_variable_lookup_cb_t) (uda_thread_t *, const char *,
						uda_taddr_t *);

/* Look up a type in a thread.
 * This is more general than looking it up in an image, since it also
 * looks in any shared libraries which are linked into the thread.
 */
typedef int (*uda_thread_type_lookup_cb_t) (uda_thread_t *, const char *,
					    uda_type_t **);

/* Read store from a specific UPC thread */
typedef int (*uda_read_store_cb_t) (uda_thread_t *, uda_taddr_t, uda_tword_t,
				    void *);
/* Write store in a specific UPC thread */
typedef int (*uda_write_store_cb_t) (uda_thread_t *, uda_taddr_t, uda_tword_t,
                                     uda_tword_t *, void *);
/* Convert data from target byte order to big endian order */
typedef int (*uda_target_to_big_end_cb_t) (uda_thread_t *, uda_tword_t,
					   const void *, void *);
/* Convert data from big endian byte order to target byte order */
typedef int (*uda_big_end_to_target_cb_t) (uda_thread_t *, uda_tword_t,
					   const void *, void *);

struct uda_basic_callbacks_struct
{
  uda_malloc_cb_t malloc_cb;
  uda_free_cb_t free_cb;
  uda_prints_cb_t prints_cb;
  uda_error_string_cb_t error_string_cb;
  uda_get_type_sizes_cb_t get_type_sizes_cb;
  uda_variable_lookup_cb_t variable_lookup_cb;
  uda_type_lookup_cb_t type_lookup_cb;
  uda_relocate_address_cb_t relocate_address_cb;
  uda_job_thread_count_cb_t job_thread_count_cb;
  uda_job_get_thread_cb_t job_get_thread_cb;
  uda_job_get_image_cb_t job_get_image_cb;
  uda_thread_get_job_cb_t thread_get_job_cb;
  uda_job_set_info_cb_t job_set_info_cb;
  uda_job_get_info_cb_t job_get_info_cb;
  uda_thread_set_info_cb_t thread_set_info_cb;
  uda_thread_get_info_cb_t thread_get_info_cb;
  uda_image_set_info_cb_t image_set_info_cb;
  uda_image_get_info_cb_t image_get_info_cb;
  uda_type_length_cb_t type_length_cb;
  uda_type_get_member_info_cb_t type_get_member_info_cb;
  uda_read_store_cb_t read_store_cb;
  uda_write_store_cb_t write_store_cb;
  uda_target_to_big_end_cb_t target_to_big_end_cb;
  uda_big_end_to_target_cb_t big_end_to_target_cb;
  uda_thread_type_lookup_cb_t thread_type_lookup_cb;
  uda_thread_variable_lookup_cb_t thread_variable_lookup_cb;
};

typedef struct uda_basic_callbacks_struct uda_basic_callbacks_t;

#endif /* !UDA_PLUGIN_CB_H_ */
