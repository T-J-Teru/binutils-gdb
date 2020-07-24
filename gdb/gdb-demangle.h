/* Basic C++ demangling support for GDB.
   Copyright (C) 2011-2020 Free Software Foundation, Inc.

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

#ifndef GDB_DEMANGLE_H
#define GDB_DEMANGLE_H

/* True means that encoded C++/ObjC names should be printed out in their
   C++/ObjC form rather than raw.  */
extern bool demangle;

/* Return true if symbols in assembler listing should be demangled.  The
   return value depends on the value of both 'set print asm-demangle' and
   'set print demangle'.  If 'set print demangle' is off then this will
   always return true.  If 'set print asm-demangle' is set to auto then
   this will mirror the value in 'set print demangle'.  */

extern bool asm_demangle_p ();

/* Check if a character is one of the commonly used C++ marker characters.  */
extern bool is_cplus_marker (int);

#endif /* GDB_DEMANGLE_H */
