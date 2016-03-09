/* UPC language thread support for GDB, the GNU debugger.

   Contributed by: Nenad Vukicevic <nenad@intrepid.com>

   Copyright (C) 2008, 2009 Free Software Foundation, Inc.

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

#ifndef UPC_THREAD_H
#define UPC_THREAD_H

/* UPC monitor  thread */
#define UPC_MONITOR_THREAD -1
#define UPC_THR_MON(x)  (x->unum == UPC_MONITOR_THREAD)
#define UPC_THR_0(x)    (x->unum == 0)
#define GDB_THR_NUM(x)  x->num
#define UPC_THR_NUM(x)  x->unum

extern int upc_thread_set (int num);
extern void upc_thread_restore (int num);
extern int upc_thread_count (void);

extern int upc_exit_code;
extern int upc_pthread_active;
extern struct thread_info *upc_thread0;
extern int is_upc_thread(struct thread_info *);
extern int upc_thread_num (struct thread_info *);
extern int upc_thread_of_inferior (struct inferior *);
extern int valid_upc_thread_id (int);
extern int valid_input_thread_id (int);
extern int show_thread_id (int);
extern void upc_thread_kill_cleanup (void);

#endif
