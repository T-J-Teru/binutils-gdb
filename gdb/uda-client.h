/* UPC Debugger Assistant client services definitions
   for GDB, the GNU debugger.

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

#ifndef _UDA_CLIENT_H_
#define _UDA_CLIENT_H_ 1

extern void uda_client_connect (const char *service_name);

/* defined in the client's command parser. */
extern int uda_client_cmd_exec (const char *cmd);

extern int uda_client_get_global_var_addr_cmd (const char *, uda_taddr_t *);
extern int uda_client_lookup_type_cmd (const char *, uda_tword_t *);
extern int uda_client_get_type_length_cmd (const uda_tword_t,
                                           uda_tword_t *);
extern int uda_client_get_type_member_descr_cmd (const uda_tword_t,
                                                 const char *,
                                                 uda_tword_t *bit_offset,
				                 uda_tword_t *bit_lenght,
						 uda_tword_t *member_type_id);
extern int uda_client_get_thread_local_addr_cmd (const uda_taddr_t addr,
                                                 const uda_tword_t thread,
				                 uda_taddr_t *);
extern int uda_client_read_local_mem_cmd (const uda_taddr_t addr,
                                          const uda_tword_t thread_num,
			                  const uda_tword_t length,
			                  uda_binary_data_t *);
extern int uda_client_write_local_mem_cmd (const uda_taddr_t addr,
                                           const uda_tword_t thread_num,
                                           uda_tword_t *actual_length,
				           const uda_binary_data_t *);
extern int uda_set_num_threads (const uda_tword_t);
extern int uda_set_thread_num (const uda_tword_t);
extern int uda_get_num_threads (uda_tword_t *num_threads);
extern int uda_get_thread_num (uda_tword_t *thread_num);
extern int uda_set_type_sizes_and_byte_order (const uda_target_type_sizes_t,
                                              const uda_tword_t byte_order);
extern int uda_symbol_to_pts (const uda_tword_t elem_size,
                              const uda_tword_t block_size,
		              const uda_taddr_t, const char *,
		              uda_debugger_pts_t *);
extern int uda_length_of_pts (uda_tword_t block_size,
		              uda_tword_t *pts_len);
extern int uda_unpack_pts (const size_t packed_pts_len,
                           const uda_target_pts_t *,
	                   const uda_tword_t block_size,
	                   uda_debugger_pts_t *);
extern int uda_pack_pts (const uda_taddr_t,
                         const uda_tword_t thread,
                         const uda_tword_t phase,
                         const uda_tword_t block_size,
	                 size_t *packed_pts_len,
                         uda_target_pts_t *);
extern int uda_calc_pts_index_add (const uda_debugger_pts_t *,
		                   const uda_tint_t index,
		                   const uda_tword_t elem_size,
		                   const uda_tword_t block_size,
		                   uda_debugger_pts_t *);
extern int uda_calc_pts_diff (const uda_debugger_pts_t *pts_1,
                              const uda_debugger_pts_t *pts_2,
		              const uda_tword_t elem_size,
		              const uda_tword_t block_size,
		              uda_tint_t *result);
extern int uda_pts_to_addr  (const uda_debugger_pts_t *,
			     const uda_tword_t block_size,
			     const uda_tword_t elem_size,
			     uda_taddr_t *addr);
extern int uda_read_shared_mem (const uda_taddr_t addrfield,
				const uda_tword_t thread_num,
				const uda_tword_t phase,
				uda_tword_t block_size,
				uda_tword_t element_size,
				const uda_tword_t length,
				uda_binary_data_t *data);
extern int uda_write_shared_mem (const uda_taddr_t addrfield,
				 const uda_tword_t thread_num,
				 const uda_tword_t phase,
				 uda_tword_t block_size,
				 uda_tword_t element_size,
				 const uda_tword_t length,
				 uda_tword_t *bytes_written,
				 const uda_binary_data_t *bytes);

#endif /* !_UDA_CLIENT_H_ */
