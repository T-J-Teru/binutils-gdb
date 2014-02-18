/* MRK3-specific support for 32-bit ELF
   Copyright 1994, 1995, 1997, 1999, 2001, 2002, 2005, 2007
   Free Software Foundation, Inc.
   Contributed by Doug Evans (dje@cygnus.com).

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "elf-bfd.h"
#include "libiberty.h"
#include "elf/mrk3.h"

#define BASEADDR(SEC)	((SEC)->output_section->vma + (SEC)->output_offset)

static reloc_howto_type elf_mrk3_howto_table[] =
{
  /* This reloc does nothing.  */
  HOWTO (R_MRK3_NONE,		/* Type.  */
	 0,			/* Rightshift.  */
	 2,			/* Size (0 = byte, 1 = short, 2 = long).  */
	 32,			/* Bitsize.  */
	 FALSE,			/* PC_relative.  */
	 0,			/* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc,	/* Special_function.  */
	 "R_MRK3_NONE",		/* Name.  */
	 TRUE,			/* Partial_inplace.  */
	 0,			/* Src_mask.  */
	 0,			/* Dst_mask.  */
	 FALSE),		/* PCrel_offset.  */
  /* This relocation is for the target for a CALL instruction. */
  HOWTO (R_MRK3_CALL16,         /* Type.  */
	 0,                     /* Rightshift.  */
	 2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 32,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 16,                    /* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_MRK3_CALL16",       /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff0000,            /* Src_mask.  */
	 0xffff0000,            /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_8,              /* Type.  */
	 0,                     /* Rightshift.  */
	 0,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 8,                     /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 16,                    /* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_MRK3_8",            /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xff,                  /* Src_mask.  */
	 0xff,                  /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_16,             /* Type.  */
	 0,                     /* Rightshift.  */
	 1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_MRK3_16",           /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0xffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
};

/* Map BFD reloc types to MRK3 ELF reloc types.  */

struct mrk3_reloc_map
{
  bfd_reloc_code_real_type bfd_reloc_val;
  unsigned char elf_reloc_val;
};

static const struct mrk3_reloc_map mrk3_reloc_map[] =
{
  { BFD_RELOC_NONE, R_MRK3_NONE, }
};

static reloc_howto_type *
bfd_elf32_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = ARRAY_SIZE (mrk3_reloc_map); i--;)
    if (mrk3_reloc_map[i].bfd_reloc_val == code)
      return elf_mrk3_howto_table + mrk3_reloc_map[i].elf_reloc_val;

  return NULL;
}

static reloc_howto_type *
bfd_elf32_bfd_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 const char *r_name)
{
  unsigned int i;

  for (i = 0;
       i < sizeof (elf_mrk3_howto_table) / sizeof (elf_mrk3_howto_table[0]);
       i++)
    if (elf_mrk3_howto_table[i].name != NULL
	&& strcasecmp (elf_mrk3_howto_table[i].name, r_name) == 0)
      return &elf_mrk3_howto_table[i];

  return NULL;
}

/* Set the howto pointer for an MRK3 ELF reloc.  */

static void
mrk3_info_to_howto_rel (bfd *abfd ATTRIBUTE_UNUSED,
		       arelent *cache_ptr,
		       Elf_Internal_Rela *dst)
{
  unsigned int r_type;

  r_type = ELF32_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MRK3_max);
  cache_ptr->howto = &elf_mrk3_howto_table[r_type];
}

/* Set the right machine number for an MRK3 ELF file.  */
static bfd_boolean
mrk3_elf_object_p (bfd *abfd)
{
  // Set the machine to 0. (we have no officially assigned machine number.)
  unsigned int mach = 0;
  return bfd_default_set_arch_mach (abfd, bfd_arch_mrk3, mach);
}


/* There is a rather horrible situation that the target compiler generates a
   big-endian format ELF, even though it really is a little endian machine. So
   we need to do this as big endian, and fix up elsewhere. */
/* #define TARGET_LITTLE_SYM   bfd_elf32_mrk3_vec */
/* #define TARGET_LITTLE_NAME  "elf32-mrk3" */
#define TARGET_BIG_SYM      bfd_elf32_mrk3_vec
#define TARGET_BIG_NAME     "elf32-mrk3"
#define ELF_ARCH            bfd_arch_mrk3
#define ELF_MACHINE_CODE    0
#define ELF_MAXPAGESIZE     0x1000

#define elf_info_to_howto                   0
#define elf_info_to_howto_rel               mrk3_info_to_howto_rel
#define elf_backend_object_p                mrk3_elf_object_p

#include "elf32-target.h"
