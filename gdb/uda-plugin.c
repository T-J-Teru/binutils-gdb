/* UDA plugin dynamic load support.

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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include "defs.h"
#include "uda-types.h"
#include "uda-plugin.h"
#include "uda-defs.h"

uda_plugin_t uda_plugin;

typedef struct plugin_symbol_def_struct
  {
    const char * const name;
    const void **plugin_field_ref;
  } plugin_symbol_def_t;

#define plugin_sym(SYM) { #SYM, (void *)&uda_plugin.SYM##_fp }

plugin_symbol_def_t uda_plugin_sym_tbl[] =
  {
    plugin_sym (uda_destroy_image_info),
    plugin_sym (uda_destroy_job_info),
    plugin_sym (uda_destroy_thread_info),
    plugin_sym (uda_error_string),
    plugin_sym (uda_get_threadno),
    plugin_sym (uda_index_pts),
    plugin_sym (uda_initialize_job),
    plugin_sym (uda_length_of_pts),
    plugin_sym (uda_pack_pts),
    plugin_sym (uda_pts_difference),
    plugin_sym (uda_pts_to_addr),
    plugin_sym (uda_setup_basic_callbacks),
    plugin_sym (uda_show_opaque),
    plugin_sym (uda_symbol_to_pts),
    plugin_sym (uda_unpack_pts),
    plugin_sym (uda_read_upc_shared_mem),
    plugin_sym (uda_write_upc_shared_mem),
    plugin_sym (uda_version_compatibility),
    plugin_sym (uda_version_string),
  };
const int n_uda_plugin_syms = sizeof (uda_plugin_sym_tbl)
                                 / sizeof (plugin_symbol_def_t);
#undef plugin_sym

void
load_uda_plugin (char *dl_path)
{
   int i;
   void *dl_handle;
   char *err_msg;

   if (!dl_path)
     {
       fprintf (stderr, "UDA plugin library not specified.\n");
       return;
     }

   dl_handle = dlopen (dl_path,  RTLD_GLOBAL|RTLD_NOW);
   err_msg = dlerror();
   if (!dl_handle || err_msg)
     {
       fprintf (stderr, "cannot open UDA plugin library: %s\n",
                dl_path);
       if (!err_msg)
         fprintf (stderr, "reason: %s\n", err_msg);
       abort ();
     }
   for (i = 0; i < n_uda_plugin_syms; ++i)
     {
        const plugin_symbol_def_t *p = &uda_plugin_sym_tbl[i];
        void *dl_addr;
	dl_addr = dlsym (dl_handle, p->name);
	err_msg = dlerror();
	if (err_msg)
	  {
	    fprintf (stderr,
	     "Cannot find entry point '%s' in UDA plugin library %s\n",
	     p->name, dl_path);
	  }
       *(p->plugin_field_ref) = dl_addr;
     }
}

