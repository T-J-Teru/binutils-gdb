/* UDA server interface definition.

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

#ifndef __UDA_PLUGIN_IFACE_H_
#define __UDA_PLUGIN_IFACE_H_ 1

/* Job object, managed by the UDA server.  */
extern uda_job_t *uda_job;

struct uda_job_
  {
    uda_tword_t mark;
    uda_job_info_t *info;
    uda_image_t *image;
    uda_type_t *types;
    uda_tword_t num_threads;
    uda_thread_t *threads;
    uda_thread_t *current_thread;
  };

struct uda_thread_
  {
    uda_tword_t mark;
    uda_tword_t id;
    uda_thread_info_t *info;
    uda_job_t *job;
  };

struct uda_image_
  {
    uda_tword_t mark;
    uda_image_info_t *info;
    uda_job_t *job;
    uda_target_type_sizes_t target_sizes;
    uda_tword_t target_is_big_end;
    uda_tword_t target_pts_has_opaque;
  };

struct uda_type_
  {
    uda_tword_t mark;
    uda_type_t *next;
    uda_tword_t type_id;
  };

/* Arbitrary values used to validate the various record
   types managed by the UDA server.  */
#define UDA_JOB_MARK	0x4a4f42
#define UDA_IMAGE_MARK	0x494d47
#define UDA_THREAD_MARK 0x544852
#define UDA_TYPE_MARK	0x545950

#endif /* !__UDA_PLUGIN_IFACE_H_ */
