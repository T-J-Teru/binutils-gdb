/* UPC Debugger Assistant remote protocol utility definitions
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

#ifndef _UDA_RMT_UTILS_H_
#define _UDA_RMT_UTILS_H_ 1

extern void *uda_malloc(size_t size);
extern void uda_free(void *ptr);
typedef int (*uda_rmt_cmd_fp_t) (const char *cmd);
extern void uda_rmt_init (FILE *rmt_in, FILE *rmt_out,
			  const uda_rmt_cmd_fp_t);
extern void uda_rmt_set_target_info (const uda_target_type_sizes_t *,
                                     const int target_big_endian,
                                     const int target_pts_has_opaque);
extern void uda_rmt_swap_bytes (gdb_byte *, const gdb_byte *, const size_t);
extern size_t uda_decode_binary_data (char *, const char *, const size_t);
extern size_t uda_decode_hex_bytes (char *, const char *, const size_t);
extern ULONGEST uda_decode_hex_word (const char *, const size_t n_chars);
extern void uda_encode_binary_data (char *, const gdb_byte *, const size_t);
extern void uda_encode_hex_bytes (char *, const gdb_byte *,
                                  const size_t n_bytes,
                                  const int skip_leading_zeros);
extern int uda_errno;
extern void uda_error (const char *);
extern const char *uda_db_error_string (int);
extern void uda_rmt_format_msg (char *msg, const char *fmt, ...);
extern int uda_rmt_recv_reply (const char *, ...);
#define uda_rmt_recv_status() (uda_rmt_recv_reply (""))
extern int uda_rmt_scan_msg (const char *msg, const char *fmt, ...);
extern void uda_rmt_send_cmd (const char *, ...);
extern void uda_rmt_send_reply (const char *, ...);
extern void uda_rmt_send_status (const int);
extern size_t uda_scan_binary_data (const char **);
extern size_t uda_scan_hex_bytes (const char **);
#include <stdarg.h>
extern void uda_rmt_vformat_msg (char *msg, const char *fmt, va_list);
extern int uda_rmt_vscan_msg (const char *msg, const char *fmt, va_list);

#endif /* !_UDA_RMT_UTILS_H_ */
