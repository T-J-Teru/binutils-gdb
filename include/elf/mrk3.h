/* MRK3 ELF support for BFD.
   Copyright 1999, 2000, 2004, 2006 Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street - Fifth Floor, Boston, MA 02110-1301, USA.  */

#ifndef _ELF_MRK3_H
#define _ELF_MRK3_H

#include "elf/reloc-macros.h"

/* Processor specific flags for the ELF header e_flags field.  */
#define EF_MRK3_MACH 0x7F

/* If bit #7 is set, it is assumed that the elf file uses local symbols
   as reference for the relocations so that linker relaxation is possible.  */
#define EF_MRK3_LINKRELAX_PREPARED 0x80

#define E_MRK3_MACH_MRK31 1
#define E_MRK3_MACH_MRK32 2
#define E_MRK3_MACH_MRK325 25
#define E_MRK3_MACH_MRK33 3
#define E_MRK3_MACH_MRK331 31
#define E_MRK3_MACH_MRK335 35
#define E_MRK3_MACH_MRK34 4
#define E_MRK3_MACH_MRK35 5
#define E_MRK3_MACH_MRK351 51
#define E_MRK3_MACH_MRK36 6

/* Relocations.  */
START_RELOC_NUMBERS (elf_mrk3_reloc_type)
     RELOC_NUMBER (R_MRK3_NONE,			0)
     RELOC_NUMBER (R_MRK3_32,			1)
     RELOC_NUMBER (R_MRK3_7_PCREL,		2)
     RELOC_NUMBER (R_MRK3_13_PCREL,		3)
     RELOC_NUMBER (R_MRK3_16, 			4)
     RELOC_NUMBER (R_MRK3_16_PM, 		5)
     RELOC_NUMBER (R_MRK3_LO8_LDI,		6)
     RELOC_NUMBER (R_MRK3_HI8_LDI,		7)
     RELOC_NUMBER (R_MRK3_HH8_LDI,		8)
     RELOC_NUMBER (R_MRK3_LO8_LDI_NEG,		9)
     RELOC_NUMBER (R_MRK3_HI8_LDI_NEG,	       10)
     RELOC_NUMBER (R_MRK3_HH8_LDI_NEG,	       11)
     RELOC_NUMBER (R_MRK3_LO8_LDI_PM,	       12)
     RELOC_NUMBER (R_MRK3_HI8_LDI_PM,	       13)
     RELOC_NUMBER (R_MRK3_HH8_LDI_PM,	       14)
     RELOC_NUMBER (R_MRK3_LO8_LDI_PM_NEG,       15)
     RELOC_NUMBER (R_MRK3_HI8_LDI_PM_NEG,       16)
     RELOC_NUMBER (R_MRK3_HH8_LDI_PM_NEG,       17)
     RELOC_NUMBER (R_MRK3_CALL,		       18)
     RELOC_NUMBER (R_MRK3_LDI,                  19)
     RELOC_NUMBER (R_MRK3_6,                    20)
     RELOC_NUMBER (R_MRK3_6_ADIW,               21)
     RELOC_NUMBER (R_MRK3_MS8_LDI,              22)
     RELOC_NUMBER (R_MRK3_MS8_LDI_NEG,          23)
     RELOC_NUMBER (R_MRK3_LO8_LDI_GS,	       24)
     RELOC_NUMBER (R_MRK3_HI8_LDI_GS,	       25)
END_RELOC_NUMBERS (R_MRK3_max)

#endif /* _ELF_MRK3_H */
