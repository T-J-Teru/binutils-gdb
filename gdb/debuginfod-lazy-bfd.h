/* debuginfod utilities for GDB.
   Copyright (C) 2026 Free Software Foundation, Inc.

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

#ifndef GDB_DEBUGINFOD_LAZY_BFD_H
#define GDB_DEBUGINFOD_LAZY_BFD_H

#include "gdb_bfd.h"
#include "gdbsupport/scoped_fd.h"
#include "objfiles.h"

extern gdb_bfd_ref_ptr debuginfod_open_deferred_download_skeleton
  (objfile *objfile, scoped_fd &&fd, const char *filename);

#endif /* GDB_DEBUGINFOD_LAZY_BFD_H */
