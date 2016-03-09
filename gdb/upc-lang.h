/* UPC language support definitions for GDB, the GNU debugger.

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

#if !defined (UPC_LANG_H)
#define UPC_LANG_H 1

extern char *upc_main_name (void);
extern int upc_shared_type_p (struct type *);
extern ULONGEST upc_blocksizeof (struct type *);
extern ULONGEST upc_elemsizeof (struct type *);
extern unsigned upc_pts_len (struct type *);
extern gdb_upc_pts_t upc_shared_var_address (struct symbol *var);
extern struct value *upc_value_from_pts (struct type *, gdb_upc_pts_t);
extern gdb_upc_pts_t upc_value_as_pts (struct value *);
struct value *upc_value_at_lazy (struct type *, gdb_upc_pts_t);
extern void upc_value_fetch_lazy (struct value *);
extern void upc_print_pts (struct ui_file *stream, int format,
                           struct type *, const gdb_byte *valaddr);
extern struct value *upc_pts_index_add (struct type *, struct value *,
                                        struct value *, LONGEST);
extern struct value *upc_pts_diff (struct value *, struct value *);
extern int upc_read_shared_mem (const gdb_upc_pts_t pts,
                                ULONGEST block_size,
                                ULONGEST element_size,
                                gdb_byte *data, int length);
extern int upc_thread_count ();
extern struct value *upc_read_var_value (struct symbol *var, struct frame_info *frame);
extern void upc_lang_init (char *cmd, int from_tty);
extern void upc_expand_threads_factor (struct type *type);
extern struct value *upc_value_subscript (struct value *, LONGEST index);
extern char *upc_demangle (const char *mangled, int options);

#endif /* !defined (UPC_LANG_H) */
