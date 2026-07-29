/* Copyright (C) 2026 Free Software Foundation, Inc.

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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   This linker script is used to produce an executable that starts with a
   non-allocatable section and does not contain a `.text` section.  */

MEMORY
{
  DATA (rw) : ORIGIN = 0x8000000, LENGTH = 0x10000
  TEXT (rx) : ORIGIN = LENGTH (DATA), LENGTH = 0x10000
}

SECTIONS
{
  .my_non_alloc_sec (INFO) : { . = . + 0x10; }
  .text.all : { *(.text) } > TEXT
  .data : { *(.data) } > DATA
  _edata = .;
  .bss : { *(.bss) } > DATA
  _end = .;
}
