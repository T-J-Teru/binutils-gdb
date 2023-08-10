/* Copyright (C) 2023 Free Software Foundation, Inc.

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

#include "gdbsupport/common-defs.h"

#include "nat/linux-machine-id.h"
#include "safe-ctype.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/* See nat/linux-machine-id.h.  */

std::string
gdb_linux_machine_id_linux_boot_id ()
{
  int fd = open ("/proc/sys/kernel/random/boot_id", O_RDONLY);
  if (fd < 0)
    return "";

  std::string boot_id;
  char buf;
  while (read (fd, &buf, sizeof (buf)) == sizeof (buf))
    {
      if (ISXDIGIT (buf))
	boot_id += buf;
    }

  close (fd);

  return boot_id;
}

/* See nat/linux-machine-id.h.  */

std::string
gdb_linux_machine_cuserid ()
{
  char cuserid_str[L_cuserid];
  char *res = cuserid (cuserid_str);
  if (res == nullptr)
    return "";

  return std::string (cuserid_str);
}
