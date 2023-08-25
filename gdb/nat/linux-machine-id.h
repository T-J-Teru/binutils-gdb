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

#ifndef NAT_LINUX_MACHINE_ID_H
#define NAT_LINUX_MACHINE_ID_H

#include <string>

/* Return a string that contains the Linux boot-id, formatted for use in
   the qMachineId packet.  If anything goes wrong then an empty string is
   returned, otherwise a non-empty string is returned.

   This is used by gdbserver when sending the reply to a qMachineId packet,
   and used by GDB to check the value returned in for a qMachineId
   packet.  */

extern std::string gdb_linux_machine_id_linux_boot_id ();

/* Return a string that contains the result of calling cuserid, that is, a
   username associated with the effective user-id of the current process.
   If anything goes wrong then an empty string is returned, otherwise a
   non-empty string is returned.

   This is used by gdbserver when sending the reply to a qMachineId packet,
   and used by GDB to check the value returned in for a qMachineId
   packet.  */

extern std::string gdb_linux_machine_id_cuserid ();

/* ... */

extern std::string gdb_linux_machine_id_namespaces ();

#endif /* NAT_LINUX_MACHINE_ID_H */
