/* UDA definitions.

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

#ifndef __UDA_DEFS_H_
#define __UDA_DEFS_H_ 1

#ifndef NULL
#define NULL ((void *)0)
#endif

/* callout structure */

struct uda_callouts {
  int (*uda_set_num_threads)(
      const uda_tword_t);
  int (*uda_set_thread_num)(
      const uda_tword_t);
  int (*uda_get_num_threads)(
      uda_tword_t *);
  int (*uda_get_thread_num)(
      uda_tword_t *);
  int (*uda_set_type_sizes_and_byte_order)(
      const uda_target_type_sizes_t,
      const uda_tword_t byte_order);
  int (*uda_symbol_to_pts)(
      const uda_tword_t elem_size,
      const uda_tword_t block_size,
      const uda_taddr_t, const char *,
      uda_debugger_pts_t *);
  int (*uda_length_of_pts)(
      uda_tword_t block_size,
      uda_tword_t *pts_len);
  int (*uda_unpack_pts)(
      const size_t packed_pts_len,
      const uda_target_pts_t *,
      const uda_tword_t block_size,
      const uda_tword_t elem_size,      
      uda_debugger_pts_t *);
  int (*uda_pack_pts)(
      const uda_taddr_t addrfield,
      const uda_tword_t thread,
      const uda_tword_t phase,
      const uda_tword_t block_size,
      const uda_tword_t elem_size,      
      size_t *packed_pts_len,
      uda_target_pts_t *);
  int (*uda_calc_pts_index_add)(
      const uda_debugger_pts_t *,
      const uda_tint_t index,
      const uda_tword_t elem_size,
      const uda_tword_t block_size,
      uda_debugger_pts_t *);
  int (*uda_calc_pts_diff)(
      const uda_debugger_pts_t *pts_1,
      const uda_debugger_pts_t *pts_2,
      const uda_tword_t elem_size,
      const uda_tword_t block_size,
      uda_tint_t *result);
  int (*uda_pts_to_addr)(
      const uda_debugger_pts_t *,
      const uda_tword_t block_size,
      const uda_tword_t elem_size,
      uda_taddr_t *addr);
  int (*uda_read_shared_mem)(
      const uda_taddr_t addrfield,
      const uda_tword_t thread_num,
      const uda_tword_t phase,
      uda_tword_t block_size,
      uda_tword_t element_size,
      const uda_tword_t length,
      uda_binary_data_t *data);
  int (*uda_write_shared_mem)(
      const uda_taddr_t addrfield,
      const uda_tword_t thread_num,
      const uda_tword_t phase,
      uda_tword_t block_size,
      uda_tword_t element_size,
      const uda_tword_t length,
      uda_tword_t *bytes_written,
      const uda_binary_data_t *bytes);
};
typedef struct uda_callouts uda_callouts_t;
typedef struct uda_callouts *uda_callouts_p;

void init_uda_client(uda_callouts_p callouts);
void init_uda_plugin(uda_callouts_p callouts, char *dl_path);
void load_uda_plugin(char *dl_path);

#endif /* !__UDA_DEFS_H_ */
