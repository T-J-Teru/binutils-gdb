/* This testcase is part of GDB, the GNU debugger.

   Copyright 2026 Free Software Foundation, Inc.

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

#include "symcat.h"
#include "gdb/section-scripts.h"

/* Ensure SCRIPT_SUFFIX is defined.  */
#ifndef SCRIPT_SUFFIX
#error missing SCRIPT_SUFFIX definition
#endif

/* Create a Python script in a section.  */
#define DEFINE_GDB_SCRIPT_TEXT		\
asm( \
".pushsection \".debug_gdb_scripts\", \"S\",%progbits\n" \
".byte " XSTRING (SECTION_SCRIPT_ID_PYTHON_TEXT) "\n" \
".ascii \"gdb.inlined-script." XSTRING(SCRIPT_SUFFIX) "\\n\"\n"    \
".ascii \"filename = gdb.current_objfile().filename\\n\"\n" \
".ascii \"if not filename in global_auto_load_tracker:\\n\"\n" \
".ascii \"  global_auto_load_tracker[filename] = 0\\n\"\n" \
".ascii \"global_auto_load_tracker[filename] += 1\\n\"\n" \
 ".ascii \"print('deferred-auto-load: script loaded: " XSTRING(SCRIPT_SUFFIX) "')\\n\"\n" \
".byte 0\n" \
".popsection\n" \
);

DEFINE_GDB_SCRIPT_TEXT
