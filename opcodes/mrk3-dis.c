/* Disassemble MRK3 instructions.
	 Copyright 1999, 2000, 2002, 2004, 2005, 2006, 2007, 2008
	 Free Software Foundation, Inc.

	 Contributed by Denis Chertykov <denisc@overta.ru>

	 This file is part of libopcodes.

	 This library is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation; either version 3, or (at your option)
	 any later version.

	 It is distributed in the hope that it will be useful, but WITHOUT
	 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
	 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
	 License for more details.

	 You should have received a copy of the GNU General Public License
	 along with this program; if not, write to the Free Software
	 Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
	 MA 02110-1301, USA.  */


#include <assert.h>
#include "sysdep.h"
#include "dis-asm.h"
#include "opintl.h"
#include "libiberty.h"
#include "p40/gdb_types.h"
#include "p40/gdb_mem_map.h"
#include "p40/P40_DLL.h"

int print_insn_mrk3 (bfd_vma addr, disassemble_info *info) {
  uint32_t insn = 0;
  uint32_t insn_len = 0;
  char text[512] = {0};

  /* Temporary kludge to deal with lack of client disassembler. */
#if 0
  insn_len  = Dll_PrintInsn(addr, text, sizeof(text) - 1);
  (*info->fprintf_func) (info->stream, "%s", text);
#else
  insn_len  = 1;
  (*info->fprintf_func) (info->stream, "dissassemble of address %08x\n",
			       addr);
#endif

  return insn_len ;
}
