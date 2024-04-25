/* ELF specific corefile related functionality.

   Copyright (C) 2024 Free Software Foundation, Inc.

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

#if !defined (ELF_CORELOW_H)
#define ELF_CORELOW_H 1

#include "arch-utils.h"

struct gdbarch;
struct bfd;

/* Parse and return execution context details from CBFD.  */

extern core_file_exec_context elf_corefile_parse_exec_context
  (struct gdbarch *gdbarch, bfd *cbfd);

#endif /* !ELF_CORELOW_H */
