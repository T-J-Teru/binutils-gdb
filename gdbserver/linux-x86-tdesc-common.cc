/* Common target description related support code for x86 (i386 and x86-64).

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

#include "server.h"
#include "tdesc.h"
#include "x86-tdesc.h"
#include "arch/x86-linux-tdesc.h"

/* See arch/x86-linux-tdesc.h.  */

void
x86_linux_post_init_tdesc (target_desc *tdesc, bool is_64bit)
{
#ifdef __x86_64__
  if (is_64bit)
    init_target_desc (tdesc, amd64_expedite_regs);
  else
#endif
    init_target_desc (tdesc, i386_expedite_regs);
}

