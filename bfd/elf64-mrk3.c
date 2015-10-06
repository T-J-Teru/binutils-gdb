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
#include "bfd_stdint.h"
#include "elf64-mrk3.h"

/* PLT design:
  [..]   sub.w r7, #2
  [....] mov.w @r7, #FOO@LO
  [..]   sub.w r7, #2
  [....] mov.w @r7, #FOO@HI
  [..]   eret
  Spaces where this PLT is used are noted with NOTE: PLTENC */

#define PLT_ENTRY_SIZE 14


#define BASEADDR(SEC)	((SEC)->output_section->vma + (SEC)->output_offset)

#define MRK3_GET_MEMORY_SPACE_ID(ADDR) (((ADDR) >> (32)) & 0xffffffff)
#define MRK3_GET_ADDRESS_LOCATION(ADDR) ((ADDR) & 0xffffffff)
#define MRK3_BUILD_ADDRESS(ID,LOC) (((ID & 0xffffffff) << 32) | (LOC & 0xffffffff))

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
	 0,                     /* Src_mask.  */
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
	 0,                     /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_32,             /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_32",           /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffffffff,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_64,             /* Type.  */
         0,                     /* Rightshift.  */
         4,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         64,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_64",           /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffffffffffffffff,    /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  /* This relocation is for the target for a CALL instruction. */
  HOWTO (R_MRK3_CALL16,         /* Type.  */
	 0,                     /* Rightshift.  */
	 2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 16,                    /* Bitpos.  */
	 complain_overflow_dont,/* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_MRK3_CALL16",       /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0,                     /* Src_mask.  */
	 0xffff0000,            /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_CALL14,         /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         14,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_CALL14",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x3fff,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_HIGH16,         /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_HIGH16",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_HI,         /* Type.  */
         16,                    /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_HI",       /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_LO,         /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_LO",       /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PCREL16,        /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         16,                    /* Bitpos.  */
         complain_overflow_signed, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_PCREL16",      /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PCREL8,         /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_signed, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_PCREL8",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_FORCEPCREL16,   /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         16,                    /* Bitpos.  */
         complain_overflow_signed, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_FORCEPCREL16", /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_FORCEPCREL8,    /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_signed, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_FORCEPCREL8",  /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_CONST4,         /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         4,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         3,                     /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_CONST4",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x78,                  /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_TBEQ_ADDR9,     /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         9,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         8,                     /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_TBEQ_ADDR9",   /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0x0,                   /* Src_mask.  */
         0xff000100,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_TBEQ_TGT,       /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         0,                     /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_TBEQ_TGT",     /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0x0,                   /* Src_mask.  */
         0xff,                  /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_TBEQ_IMM8,      /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_TBEQ_IMM8",    /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0x0,                   /* Src_mask.  */
         0xff0000,              /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_DIRECT9,        /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         9,                     /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         4,                     /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_DIRECT9",      /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x3bf0,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ECALL20,        /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         20,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ECALL20",      /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff000f,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS12,          /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         12,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS12",        /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xff00000f,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_JT16,             /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_dont, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_JT16",           /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_BYTE1,      /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_BYTE1",    /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_BYTE2,      /* Type.  */
         8,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_BYTE2",   /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_BYTE3,      /* Type.  */
         16,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_BYTE3",    /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_BYTE4,      /* Type.  */
         24,                    /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         32,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont,/* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_ABS_BYTE4",    /* Name.  */
         FALSE,                 /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0x00ff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PIC,            /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         16,                    /* Bitpos. */
         complain_overflow_dont, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_PIC",          /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0,                     /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
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
  { BFD_RELOC_NONE, R_MRK3_NONE },
  { BFD_RELOC_8,    R_MRK3_8 },
  { BFD_RELOC_16,   R_MRK3_16 },
  { BFD_RELOC_32,   R_MRK3_32 },
  { BFD_RELOC_64,   R_MRK3_64 }
};

static reloc_howto_type *
bfd_elf64_bfd_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
				 bfd_reloc_code_real_type code)
{
  unsigned int i;

  for (i = ARRAY_SIZE (mrk3_reloc_map); i--;)
    if (mrk3_reloc_map[i].bfd_reloc_val == code)
      return elf_mrk3_howto_table + mrk3_reloc_map[i].elf_reloc_val;

  return NULL;
}

static reloc_howto_type *
bfd_elf64_bfd_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
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

  r_type = ELF64_R_TYPE (dst->r_info);
  BFD_ASSERT (r_type < (unsigned int) R_MRK3_max);
  cache_ptr->howto = &elf_mrk3_howto_table[r_type];
}

/* Per section relaxation information.  */

struct mrk3_relax_info
{
  /* This flag is set true if any relaxation was performed on this
     section.  */
  bfd_boolean was_relaxed;

  /* The original size of the section before any relaxation took place.  */
  bfd_size_type original_size;
};

struct elf_mrk3_section_data
{
  struct bfd_elf_section_data elf;
  struct mrk3_relax_info relax_info;
};

static struct mrk3_relax_info *
get_mrk3_relax_info (asection *sec)
{
  struct elf_mrk3_section_data *section_data;

  /* No info available if no section or if it is an output section.  */
  if (!sec || sec == sec->output_section)
    return NULL;

  section_data = (struct elf_mrk3_section_data *) elf_section_data (sec);
  return &section_data->relax_info;
}

static void
init_mrk3_relax_info (asection *sec)
{
  struct mrk3_relax_info *relax_info = get_mrk3_relax_info (sec);

  if (relax_info)
    relax_info->was_relaxed = FALSE;
}

static bfd_boolean
elf_mrk3_new_section_hook (bfd *abfd, asection *sec)
{
  if (!sec->used_by_bfd)
    {
      struct elf_mrk3_section_data *sdata;
      bfd_size_type amt = sizeof (*sdata);

      sdata = bfd_zalloc (abfd, amt);
      if (sdata == NULL)
	return FALSE;
      sec->used_by_bfd = sdata;

      init_mrk3_relax_info (sec);
    }

  return _bfd_elf_new_section_hook (abfd, sec);
}

/* Set the right machine number for an MRK3 ELF file.  */
static bfd_boolean
mrk3_elf_object_p (bfd *abfd)
{
  /* The MRK3 compiler does not place the machine type into the elf
     headers.  This doesn't really matter right now, we assume that there's
     only one machine type for now.  In the future this might change.  */
  return bfd_default_set_arch_mach (abfd, bfd_arch_mrk3, bfd_mach_mrk3);
}

/* Helper function for MRK3_FINAL_LINK_RELOCATE.  Called to adjust the
   value being patched into a R_MRK3_CONST4 relocation.  The const4
   relocations are a 4-bit symbol value being patched into an instruction.
   The 4-bit value space is used to encode a set of common values rather
   than just a sequential set of values.  This function takes the symbol
   value in parameter SYMBOL_VALUE, and returns the actual value within the
   4-bit value space that encodes the given SYMBOL_VALUE.

   If the SYMBOL_VALUE is not one that is represented within the 4-bit
   value space then an error will be reported, and the value 0 will be
   returned.

   The parameters INPUT_BFD, INPUT_SECTION, and RELOC_OFFSET are only used
   when reporting the invalid value error, and are the same as the
   parameters being passed to MRK3_FINAL_LINK_RELOCATE.  */

static bfd_vma
mrk3_final_link_relocate_const4 (bfd *input_bfd,
                                 asection *input_section,
                                 bfd_vma reloc_offset,
                                 bfd_vma symbol_value)
{
  bfd_signed_vma val = symbol_value;

  /* Extracting the address part of relocation gives a 24-bit value.
     To allow comparison to -1 below, this code sign extends the 24-bit
     value.  */
  BFD_ASSERT (sizeof (val) == 8);
  BFD_ASSERT ((val >> 24) == 0);
  val = ((val << 40) >> 40);

  switch (val)
    {
      /* These values are represented 1:1 within CONST4.  */
    case 0: case 1: case 2: case 3: case 4:
    case 5: case 6: case 7: case 8: case 9: case 10:
      break;

      /* These values are also mapping into the CONST4 space.  */
    case -1: val = 11; break;
    case 16: val = 12; break;
    case 32: val = 13; break;
    case 64: val = 14; break;
    case 128: val = 15; break;

      /* No other value can be represented within CONST4.  */
    default:
      /* Error.  */
      _bfd_error_handler
        (_("warning: %B relocation at %s + 0x%"BFD_VMA_FMT
           "x is const4, containing invalid value %lld"),
         input_bfd, input_section->name, reloc_offset, val);
      /* Setting to zero prevents further (overflow) errors occurring later
         on; we've already registered an error about this issue, we don't
         need more.  */
      val = 0;
    }

  return ((bfd_vma) val);
}

/* Helper function for MRK3_FINAL_LINK_RELOCATE. Called to adjust the
   value being patched into a R_MRK3_PIC relocation. This relocation points
   to the functions entry in the per-address-mode procedure linkage table.

   This function also allocates the symbols location in the procedure
   linkage table, according to the address passed to it in RELOCATION. */

static bfd_vma
mrk3_final_link_relocate_pic (bfd *   output_bfd,
                              bfd_vma relocation)
{
  unsigned int i;
  bfd_vma contents;
  asection *s;

  s = bfd_get_section_by_name (output_bfd, ".plt");
  BFD_ASSERT (s != 0);

  /* Find the entry corresponding to the function, or fill in next entry.
     Note, i is a memory offset, also assumes relocation cannot be zero.
     For speedy comparisons, we store RELOCATION in the first 4 bytes of
     the PLT entry. This will be moved into the final location by
     MRK3_ELF_FINISH_DYNAMIC_SECTIONS. */
  for (i = 0; i < s->size; i += PLT_ENTRY_SIZE)
    {
      contents = bfd_get_32 (output_bfd, s->contents + i);
      if (contents == 0)
        {
          bfd_put_32 (output_bfd, relocation, s->contents + i);
          return s->vma + i;
        }
      else if (contents == relocation)
        {
          return s->vma + i;
        }
    }

  /* If we have reached this point, we ran out of PLT */
  _bfd_error_handler
    (_("warning: Ran out of PLT space!"));
  return ((bfd_vma) 0);
}

/* Perform a single relocation.

   The bulk of this function is a direct copy of the standard bfd routine
   used in these case _BFD_FINAL_LINK_RELOCATE, however, there is one MRK3
   specific change required (see comment inline) relating to byte vs code
   addresses.

   The SYMBOL_NAME is passed only for a debugging aid.  */

static bfd_reloc_status_type
mrk3_final_link_relocate (bfd * output_bfd,
        reloc_howto_type *  howto,
			  bfd *               input_bfd,
			  asection *          input_section,
			  bfd_byte *          contents,
			  Elf_Internal_Rela * rel,
			  bfd_vma             relocation,
			  asection *          symbol_section,
			  const char *        symbol_name ATTRIBUTE_UNUSED,
                          struct elf_link_hash_entry * h)
{
  bfd_vma offset = rel->r_offset;
  bfd_vma addend = rel->r_addend;
  bfd_vma address = (input_section->output_section->vma
                     + input_section->output_offset
                     + offset);
  bfd_vma relocation_memory_id, address_location;

  /* Sanity check the address.  */
  if (offset > bfd_get_section_limit (input_bfd, input_section))
    return bfd_reloc_outofrange;

  /* This function assumes that we are dealing with a basic relocation
     against a symbol.  We want to compute the value of the symbol to
     relocate to.  This is just RELOCATION, the value of the symbol, plus
     ADDEND, any addend associated with the reloc.  */
  relocation += addend;

  /* For mrk3 we use address space identifier bits merged in to the VMA in
     order to track which address space an address is in.
     The real location within the address space is held in bits 0 to 23,
     while the address space identifier is held in bits 24 to 31.

     Now RELOCATION will hold the address that we wish to generate a
     relocation to.  In most well behaved cases this will include the
     address space identifier.  However, if the destination symbol was
     undefined then the relocation address might have no address space
     identification bits present.

     Another complication is that code addresses are 16-bit word values,
     not 8-bit byte values, and so the address must be scaled, however, we
     must take care that we don't scale the address space identifier bits
     otherwise they will become corrupted.

     The approach that we take is to mask the address space identification
     bits from the value in RELOCATION.  We then perform any pc-relative,
     or scaling adjustments to RELOCATION.

     Now, if the value is being patched into a debug section then the
     address space bits must be merged back into the value of RELOCATION,
     otherwise, it is fine to pass th value through without the address
     space bits being present.

     As an extra check, for pc-relative relocations, if the address space
     identifier in RELOCATION does not match the address space identifier
     on the relocation ADDRESS then we give an error (this would imply a
     pc-relative relocation into a different memory space, something that
     is not supported).  The only exception to this is if the memory space
     identifier on RELOCATION is zero, this usually implies that we are
     relocating against an undefined (weak) symbol.  */

  relocation_memory_id = MRK3_GET_MEMORY_SPACE_ID (relocation);

  if (howto->pc_relative
      && relocation_memory_id != 0
      && relocation_memory_id != MRK3_GET_MEMORY_SPACE_ID (address))
    _bfd_error_handler
      (_("warning: %B relocation at %s + 0x%"BFD_VMA_FMT
         "x is pc-relative across address spaces, %#08lx to %#08lx"),
       input_bfd, input_section->name, offset, address, relocation);

  relocation = MRK3_GET_ADDRESS_LOCATION (relocation);

  /* The special R_MRK3_CONST4 relocation uses a mapping table to encode it's
     value.  Only some values can be handled by a R_MRK3_CONST4
     relocation.  */
  if (howto->type == R_MRK3_CONST4)
    relocation = mrk3_final_link_relocate_const4 (input_bfd, input_section,
                                                  offset, relocation);

  /* The special R_MRK3_PIC relocation requires a mapping to it's location in
     the address space PLT. The call of this function also allocates the
     functions real address to the PLT. */
  if (howto->type == R_MRK3_PIC)
    relocation = mrk3_final_link_relocate_pic (output_bfd, relocation);

  /* If the relocation is PC relative, we want to set RELOCATION to
     the distance between the symbol (currently in RELOCATION) and the
     location we are relocating.  Some targets (e.g., i386-aout)
     arrange for the contents of the section to be the negative of the
     offset of the location within the section; for such targets
     pcrel_offset is FALSE.  Other targets (e.g., m88kbcs or ELF)
     simply leave the contents of the section as zero; for such
     targets pcrel_offset is TRUE.  If pcrel_offset is FALSE we do not
     need to subtract out the offset of the location within the
     section (which is just ADDRESS).  */
  address_location = MRK3_GET_ADDRESS_LOCATION (address);
  if (howto->pc_relative)
    relocation -= address_location;

  /* If the symbol being targeted is a code symbol, and the relocation is
     NOT located inside debugging information then we should scale the
     value to make it into a word addressed value.  */
  if (symbol_section
      && ((symbol_section->flags & SEC_CODE) != 0)
      && ((input_section->flags & SEC_DEBUGGING) == 0))
    {
      if (relocation & 1)
        _bfd_error_handler
          (_("warning: %B relocation at %s + 0x%"BFD_VMA_FMT"x "
             "references code, but has least significant bit "
             "set (0x%"BFD_VMA_FMT"x)"),
           input_bfd, input_section->name, offset, relocation);
      if (address_location & 1)
        _bfd_error_handler
          (_("warning: %B relocation at %s + 0x%"BFD_VMA_FMT"x "
             "references code, but is at an unaligned address "
             " 0x%"BFD_VMA_FMT"x"),
           input_bfd, input_section->name, offset, address_location);

      /* Scale the byte addresses into a 16-bit word address.  */
      relocation = ((bfd_signed_vma) relocation) >> 1;
      address_location = ((bfd_signed_vma) address_location) >> 1;
    }

  /* It is important that this overflow check is performed after we have
     changed addresses from byte addresses to word addresses where
     appropriate, otherwise the BITWSIZE below would be wrong.  */
  if (howto->type == R_MRK3_CALL16
      || howto->type == R_MRK3_CALL14)
    {
      unsigned int bitsize = howto->bitsize;

      /* Call instructions to undefined weak symbols are patched to jump
         address zero within the current call sized page.  */
      if ((h && h->root.type == bfd_link_hash_undefweak)
          || bfd_is_und_section (symbol_section))
        relocation = address_location & ~((1 << bitsize) - 1);

      /* Compare the address of the relocation with the address of the
         destination.  Only BITSIZE least significant bits are allowed to
         vary between the two addresses, the remainder must match.  */
      if ((address_location >> bitsize) != (relocation >> bitsize))
        return bfd_reloc_overflow;

      /* The generic overflow check in common code is disabled for call
         style relocations, so nothing more is required here.  */
    }

  if ((input_section->flags & SEC_DEBUGGING) != 0
      && !howto->pc_relative
      && (howto->bitsize == 32 || howto->bitsize == 64))
    relocation = MRK3_BUILD_ADDRESS (relocation_memory_id, relocation);

  /* Handle relocations for which patching the value in is non trivial.
     These can't be resolved using the standard function as that will only
     cope with code where the value to be patched in is a continuous series
     of bits.  */
  if (howto->type == R_MRK3_DIRECT9)
    {
      bfd_vma x;
      bfd_byte *location = contents + offset;

      /* Overflow check.  Would be nice if this could be shared from the
         common bfd code, however, currently the overflow check is tied
         into the patching in code.  */
      if ((relocation >> 9) != 0)
        return bfd_reloc_overflow;

      x = bfd_get_16 (input_bfd, location);
      x &= ~howto->dst_mask;

      switch (howto->type)
        {
        case R_MRK3_DIRECT9:
          x |= ((((relocation >> 6) & 0x7) << 11)
                | (((relocation >> 0) & 0x3f) << 4));
          break;

        default:
          /* This is really an error in the tools.  */
          return bfd_reloc_notsupported;
          break;
        }

      bfd_put_16 (input_bfd, x, location);

      return bfd_reloc_ok;
    }

  if (howto->type == R_MRK3_TBEQ_ADDR9)
    {
      bfd_vma x;
      bfd_byte *location = contents + offset;

      /* Overflow check.  Would be nice if this could be shared from the
         common bfd code, however, currently the overflow check is tied
         into the patching in code.  */
      if ((relocation >> 9) != 0)
        return bfd_reloc_overflow;

      x = bfd_get_32 (input_bfd, location);
      x &= ~howto->dst_mask;

      switch (howto->type)
        {
        case R_MRK3_TBEQ_ADDR9:
          /* Mask out bits to be patched, and merge in relocation.  */
          x |= ((relocation & 0xff) << 24) | (relocation & 0x100);
          break;

        default:
          /* This is really an error in the tools.  */
          return bfd_reloc_notsupported;
          break;
        }

      bfd_put_32 (input_bfd, x, location);

      return bfd_reloc_ok;
    }

  if (howto->type == R_MRK3_ECALL20)
    {
      bfd_vma x;
      bfd_byte *location = contents + offset;

      /* Overflow check.  Would be nice if this could be shared from the
         common bfd code, however, currently the overflow check is tied
         into the patching in code.  */
      if ((relocation >> 20) != 0)
        return bfd_reloc_overflow;

      x = bfd_get_32 (input_bfd, location);
      x &= ~howto->dst_mask;

      switch (howto->type)
        {
        case R_MRK3_ECALL20:
          /* Mask out bits to be patched, and merge in relocation.  */
          x |= ((relocation & 0xffff) << 16) | (relocation >> 16);
          break;

        default:
          /* This is really an error in the tools.  */
          return bfd_reloc_notsupported;
          break;
        }

      bfd_put_32 (input_bfd, x, location);

      return bfd_reloc_ok;
    }

  if (howto->type == R_MRK3_ABS12)
    {
      bfd_vma x;
      bfd_byte *location = contents + offset;

      /* Overflow check.  Would be nice if this could be shared from the
         common bfd code, however, currently the overflow check is tied
         into the patching in code.  */
      if ((relocation >> 12) != 0)
        return bfd_reloc_overflow;

      x = bfd_get_32 (input_bfd, location);
      x &= ~howto->dst_mask;

      switch (howto->type)
        {
        case R_MRK3_ABS12:
          /* Mask out bits to be patched, and merge in relocation.  */
          x |= ((relocation & 0xff) << 24) | (relocation >> 8);
          break;

        default:
          /* This is really an error in the tools.  */
          return bfd_reloc_notsupported;
          break;
        }

      bfd_put_32 (input_bfd, x, location);

      return bfd_reloc_ok;
    }
  /* Now call the standard bfd routine to handle a single relocation.  */
  return _bfd_relocate_contents (howto, input_bfd, relocation,
				 contents + offset);
}

/* Relocate an MRK3 ELF section.

   The RELOCATE_SECTION function is called by the new ELF backend linker
   to handle the relocations for a section.

   The relocs are always passed as Rela structures; if the section
   actually uses Rel structures, the r_addend field will always be
   zero.

   This function is responsible for adjusting the section contents as
   necessary, and (if using Rela relocs and generating a relocatable
   output file) adjusting the reloc addend as necessary.

   This function does not have to worry about setting the reloc
   address or the reloc symbol index.

   LOCAL_SYMS is a pointer to the swapped in local symbols.

   LOCAL_SECTIONS is an array giving the section in the input file
   corresponding to the st_shndx field of each local symbol.

   The global hash table entry for the global symbols can be found
   via elf_sym_hashes (input_bfd).

   When generating relocatable output, this function must handle
   STB_LOCAL/STT_SECTION symbols specially.  The output symbol is
   going to be the section symbol corresponding to the output
   section, which means that the addend must be adjusted
   accordingly.  */

static bfd_boolean
mrk3_elf_relocate_section (bfd *output_bfd,
			       struct bfd_link_info *info,
			       bfd *input_bfd,
			       asection *input_section,
			       bfd_byte *contents,
			       Elf_Internal_Rela *relocs,
			       Elf_Internal_Sym *local_syms,
			       asection **local_sections)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  Elf_Internal_Rela *rel;
  Elf_Internal_Rela *relend;

  symtab_hdr = & elf_tdata (input_bfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (input_bfd);
  relend     = relocs + input_section->reloc_count;

  for (rel = relocs; rel < relend; rel ++)
    {
      reloc_howto_type *           howto;
      unsigned long                r_symndx;
      Elf_Internal_Sym *           sym;
      asection *                   sec;
      struct elf_link_hash_entry * h;
      bfd_vma                      relocation;
      bfd_reloc_status_type        r;
      const char *                 name = NULL;
      int                          r_type ATTRIBUTE_UNUSED;

      r_type = ELF64_R_TYPE (rel->r_info);
      r_symndx = ELF64_R_SYM (rel->r_info);
      howto  = elf_mrk3_howto_table + ELF64_R_TYPE (rel->r_info);
      h      = NULL;
      sym    = NULL;
      sec    = NULL;

      if (r_symndx < symtab_hdr->sh_info)
	{
	  asection *osec;

	  sym = local_syms + r_symndx;
	  sec = local_sections [r_symndx];
	  osec = sec;

	  if ((sec->flags & SEC_MERGE)
	      && ELF_ST_TYPE (sym->st_info) == STT_SECTION)
	    {
	      /* This relocation is relative to a section symbol that is going
		 to be merged.  Change it so that it is relative to the merged
		 section symbol.  */
	      rel->r_addend = _bfd_elf_rel_local_sym (output_bfd, sym, &sec,
						      rel->r_addend);
	    }

          /* APB 20-Aug-2015: The following has been adjusted in an attempt
             to better handle reprocessing of relocations, in the case
             where relocations are preserved using --emit-relocs.  After a
             non-relocatable link, the symbol value for a section symbol
             becomes the VMA of the section.  As a result, adding the
             symbol value to the section base address results in an
             incorrect (double) value for the address being patched in.  */
          if (ELF_ST_TYPE (sym->st_info) == STT_SECTION)
            relocation = BASEADDR (sec);
          else
            relocation = BASEADDR (sec) + sym->st_value;

	  name = bfd_elf_string_from_elf_section
	    (input_bfd, symtab_hdr->sh_link, sym->st_name);
	  name = (name == NULL) ? bfd_section_name (input_bfd, osec) : name;
	}
      else
	{
	  bfd_boolean warned ATTRIBUTE_UNUSED;
	  bfd_boolean unresolved_reloc ATTRIBUTE_UNUSED;
	  bfd_boolean ignored ATTRIBUTE_UNUSED;

	  RELOC_FOR_GLOBAL_SYMBOL (info, input_bfd, input_section, rel,
				   r_symndx, symtab_hdr, sym_hashes,
				   h, sec, relocation,
				   unresolved_reloc, warned, ignored);

	  name = h->root.root.string;
	}

      if (sec != NULL && discarded_section (sec))
	RELOC_AGAINST_DISCARDED_SECTION (info, input_bfd, input_section,
					 rel, 1, relend, howto, 0, contents);

      if (!info->relocatable)
        {
          /* Patch in the relocation.  This is not needed if we are
	     performing a relocatable link.  */
          r = mrk3_final_link_relocate (output_bfd, howto, input_bfd,
					input_section, contents, rel,
					relocation, sec, name, h);

	  /* Handle ant errors.  */
	  if (r != bfd_reloc_ok)
	    {
	      const char * msg = NULL;

	      switch (r)
		{
		case bfd_reloc_overflow:
		  r = info->callbacks->reloc_overflow
		    (info, (h ? &h->root : NULL), name, howto->name,
		     (bfd_vma) 0, input_bfd, input_section, rel->r_offset);
		  break;

		case bfd_reloc_undefined:
		  r = info->callbacks->undefined_symbol
		    (info, name, input_bfd, input_section, rel->r_offset, TRUE);
		  break;

		case bfd_reloc_outofrange:
		  msg = _("internal error: out of range error");
		  break;

		  /* This is how mrk3_final_link_relocate tells us of a
		     non-kosher reference between insn & data address spaces.  */
		case bfd_reloc_notsupported:
		  if (sym != NULL) /* Only if it's not an unresolved symbol.  */
		    msg = _("unsupported relocation between data/insn address spaces");
		  break;

		case bfd_reloc_dangerous:
		  msg = _("internal error: dangerous relocation");
		  break;

		default:
		  msg = _("internal error: unknown error");
		  break;
		}

	      if (msg)
		r = info->callbacks->warning
		  (info, msg, name, input_bfd, input_section, rel->r_offset);

	      if (! r)
		return FALSE;
	    }
	}

      /* If we plan to emit the relocations then we should adjust the
         addend here if the relocation is against a section symbol.
         However, if it's safe to adjust the relocation in the case of
         emit relocations, then it should also be safe to adjust the
         relocation in all cases.  Doing this in all cases should mean
         bugs are revealed earlier.  */
      if (sym != NULL && ELF_ST_TYPE (sym->st_info) == STT_SECTION)
        rel->r_addend += sec->output_offset;
    }

  return TRUE;
}

/* Access to internal relocations, section contents and symbols.  */

/* During relaxation, we need to modify relocations, section contents,
   and symbol definitions, and we need to keep the original values from
   being reloaded from the input files, i.e., we need to "pin" the
   modified values in memory.  We also want to continue to observe the
   setting of the "keep-memory" flag.  The following functions wrap the
   standard BFD functions to take care of this for us.  */

static Elf_Internal_Rela *
retrieve_internal_relocs (bfd *abfd, asection *sec, bfd_boolean keep_memory)
{
  Elf_Internal_Rela *internal_relocs;

  if ((sec->flags & SEC_LINKER_CREATED) != 0)
    return NULL;

  internal_relocs = elf_section_data (sec)->relocs;
  if (internal_relocs == NULL)
    internal_relocs = (_bfd_elf_link_read_relocs
		       (abfd, sec, NULL, NULL, keep_memory));
  return internal_relocs;
}

static void
pin_internal_relocs (asection *sec, Elf_Internal_Rela *internal_relocs)
{
  elf_section_data (sec)->relocs = internal_relocs;
}

static void
release_internal_relocs (asection *sec, Elf_Internal_Rela *internal_relocs)
{
  if (internal_relocs
      && elf_section_data (sec)->relocs != internal_relocs)
    free (internal_relocs);
}

static bfd_byte *
retrieve_contents (bfd *abfd, asection *sec, bfd_boolean keep_memory)
{
  bfd_byte *contents;
  bfd_size_type sec_size;

  sec_size = bfd_get_section_limit (abfd, sec);
  contents = elf_section_data (sec)->this_hdr.contents;

  if (contents == NULL && sec_size != 0)
    {
      if (!bfd_malloc_and_get_section (abfd, sec, &contents))
	{
	  if (contents)
	    free (contents);
	  return NULL;
	}
      if (keep_memory)
	elf_section_data (sec)->this_hdr.contents = contents;
    }
  return contents;
}

static void
pin_contents (asection *sec, bfd_byte *contents)
{
  /* If this assert triggers then we're about to leak memory.  */
  BFD_ASSERT (elf_section_data (sec)->this_hdr.contents == contents
              || elf_section_data (sec)->this_hdr.contents == NULL);
  elf_section_data (sec)->this_hdr.contents = contents;
}

static void
release_contents (asection *sec, bfd_byte *contents)
{
  if (contents && elf_section_data (sec)->this_hdr.contents != contents)
    free (contents);
}

/* Fetch the local symbols from INPUT_BFD and cache them.  */

static Elf_Internal_Sym *
retrieve_local_syms (bfd *input_bfd)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Sym *isymbuf;
  size_t locsymcount;

  symtab_hdr = &elf_tdata (input_bfd)->symtab_hdr;
  locsymcount = symtab_hdr->sh_info;

  isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
  if (isymbuf == NULL && locsymcount != 0)
    isymbuf = bfd_elf_get_elf_syms (input_bfd, symtab_hdr, locsymcount, 0,
				    NULL, NULL, NULL);

  /* Save the symbols for this input file so they won't be read again.  */
  if (isymbuf && isymbuf != (Elf_Internal_Sym *) symtab_hdr->contents)
    symtab_hdr->contents = (unsigned char *) isymbuf;

  return isymbuf;
}

static void
relax_log (const char *fmt, ...)
{
  static enum
  {
    UNINITIALISED, LOGGING_OFF, LOGGING_ON
  } relaxation_logging;
  va_list ap;

  if (relaxation_logging == UNINITIALISED)
    {
      const char *var = getenv ("MRK3_RELAXATION_LOGGING");
      if (var && (*var == 'y' || *var == 'Y'))
        relaxation_logging = LOGGING_ON;
      else
        relaxation_logging = LOGGING_OFF;
    }

  if (relaxation_logging == LOGGING_OFF)
    return;

  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
}

/* Delete some bytes from a section while changing the size of an instruction.
   The parameter "addr" denotes the section-relative offset pointing just
   behind the shrinked instruction. "addr+count" point at the first
   byte just behind the original unshrinked instruction.  */

static bfd_boolean
mrk3_elf_relax_delete_bytes (bfd *abfd,
                             asection *sec,
                             bfd_vma addr,
                             int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend, *internal_relocs;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_vma toaddr;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;
  struct bfd_section *isec;
  struct mrk3_relax_info *relax_info;

  /* Mark that the section was relaxed, and record the original size.  */
  relax_info = get_mrk3_relax_info (sec);
  if (!relax_info->was_relaxed)
    {
      relax_info->original_size = sec->size;
      relax_info->was_relaxed = TRUE;
    }

  /* Actually delete the bytes.  The contents will have already been
     cached by the control logic of linker relaxation, so no need to pin
     the contents here.  */
  contents = retrieve_contents (abfd, sec, TRUE);
  toaddr = sec->size;
  if (toaddr - addr - count > 0)
    memmove (contents + addr, contents + addr + count,
             (size_t) (toaddr - addr - count));
  sec->size -= count;

  /* Adjust all the reloc addresses in SEC.  The relocations of SEC are
     already cached, so there's no need to call pin_internal_relocs.  */
  internal_relocs = retrieve_internal_relocs (abfd, sec, TRUE);
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if (irel->r_offset > addr && irel->r_offset < toaddr)
        irel->r_offset -= count;
    }

   /* The reloc's own addresses are now ok. However, we need to readjust
      the reloc's addend, i.e. the reloc's value if two conditions are met:
      1.) the reloc is relative to a symbol in this section that
          is located in front of the shrinked instruction
      2.) symbol plus addend end up behind the shrinked instruction.

      The most common case where this happens are relocs relative to
      the section-start symbol.

      This step needs to be done for all of the sections of the bfd.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  for (isec = abfd->sections; isec; isec = isec->next)
    {
      bfd_vma symval;
      bfd_vma shrinked_insn_address;

      if (isec->reloc_count == 0)
        continue;

      shrinked_insn_address = (sec->output_section->vma
                               + sec->output_offset + addr - count);

      irel = retrieve_internal_relocs (abfd, isec, TRUE);
      for (irelend = irel + isec->reloc_count;
           irel < irelend;
           irel++)
        {
          /* Read this BFD's local symbols if we haven't done
             so already.  */
          if (isymbuf == NULL)
            isymbuf = retrieve_local_syms (abfd);

          /* Get the value of the symbol referred to by the reloc.  */
          if (ELF64_R_SYM (irel->r_info) < symtab_hdr->sh_info)
            {
              /* A local symbol.  */
              asection *sym_sec;

              isym = isymbuf + ELF64_R_SYM (irel->r_info);
              sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
              symval = isym->st_value;
              /* If the reloc is absolute, it will not have
                 a symbol or section associated with it.  */
              if (sym_sec == sec)
                {
                  symval += sym_sec->output_section->vma
                    + sym_sec->output_offset;

                  if (symval <= shrinked_insn_address
                      && (symval + irel->r_addend) > shrinked_insn_address)
                    irel->r_addend -= count;
                }
              /* else...Reference symbol is absolute.  No adjustment needed.  */
            }
          /* else...Reference symbol is extern.  No need for adjusting
             the addend.  */
        }
    }

  /* Adjust the local symbols defined in this section.  */
  isym = retrieve_local_syms (abfd);
  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);
  if (isym != NULL)
    {
      Elf_Internal_Sym *isymend;

      isymend = isym + symtab_hdr->sh_info;
      for (; isym < isymend; isym++)
	{
	  if (isym->st_shndx == sec_shndx
	      && isym->st_value > addr
	      && isym->st_value < toaddr)
	    isym->st_value -= count;
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf64_External_Sym)
              - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
           || sym_hash->root.type == bfd_link_hash_defweak)
          && sym_hash->root.u.def.section == sec
          && sym_hash->root.u.def.value > addr
          && sym_hash->root.u.def.value < toaddr)
        {
          sym_hash->root.u.def.value -= count;
        }
    }

  return TRUE;
}

/* Insert COUNT bytes into the contents of SEC (from ABFD) at section
   offset ADDR.  Adjust symbols and relocations as appropriate.  This
   function can only be used on a section that has been reduced in size
   by mrk3_elf_relax_delete_bytes, as we don't allocate new memory for
   the section contents, instead we rely on the region of memory already
   allocated being big enough.  We no that we will never grow this section
   beyond its original size.  Information to support this assertion is
   carried around on the per-section relaxation data, and we assert that
   this is true.  */

static bfd_boolean
mrk3_elf_relax_insert_bytes (bfd *abfd,
                             asection *sec,
                             bfd_vma addr,
                             int count)
{
  Elf_Internal_Shdr *symtab_hdr;
  unsigned int sec_shndx;
  bfd_byte *contents;
  Elf_Internal_Rela *irel, *irelend, *internal_relocs;
  Elf_Internal_Sym *isym;
  Elf_Internal_Sym *isymbuf = NULL;
  bfd_vma toaddr;
  struct elf_link_hash_entry **sym_hashes;
  struct elf_link_hash_entry **end_hashes;
  unsigned int symcount;
  struct bfd_section *isec;
  struct mrk3_relax_info *relax_info;

  relax_info = get_mrk3_relax_info (sec);
  BFD_ASSERT (relax_info->was_relaxed);
  BFD_ASSERT (sec->size + count <= relax_info->original_size);

  /* Actually create some space in the section contents.  */
  toaddr = sec->size;
  contents = retrieve_contents (abfd, sec, TRUE);
  memmove (contents + addr + count, contents + addr,
           (size_t) (toaddr - addr));
  memset (contents + addr, 0, count);
  sec->size += count;

  /* Adjust all the reloc addresses.  */
  internal_relocs = retrieve_internal_relocs (abfd, sec, TRUE);
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      /* Get the new reloc address.  */
      if (irel->r_offset >= addr && irel->r_offset < toaddr)
        irel->r_offset += count;
    }

   /* The reloc's own addresses are now ok. However, we need to readjust
      the reloc's addend, i.e. the reloc's value if two conditions are met:
      1.) the reloc is relative to a symbol in this section that
          is located in front of the shrinked instruction
      2.) symbol plus addend end up behind the shrinked instruction.

      The most common case where this happens are relocs relative to
      the section-start symbol.

      This step needs to be done for all of the sections of the bfd.  */
  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  for (isec = abfd->sections; isec; isec = isec->next)
    {
      bfd_vma symval;
      bfd_vma shrinked_insn_address;

      if (isec->reloc_count == 0)
        continue;

      shrinked_insn_address = (sec->output_section->vma
                               + sec->output_offset + addr);

      irel = retrieve_internal_relocs (abfd, isec, TRUE);
      for (irelend = irel + isec->reloc_count;
           irel < irelend;
           irel++)
        {
          /* Read this BFD's local symbols if we haven't done
             so already.  */
          if (isymbuf == NULL)
            isymbuf = retrieve_local_syms (abfd);

          /* Get the value of the symbol referred to by the reloc.  */
          if (ELF64_R_SYM (irel->r_info) < symtab_hdr->sh_info)
            {
              /* A local symbol.  */
              asection *sym_sec;

              isym = isymbuf + ELF64_R_SYM (irel->r_info);
              sym_sec = bfd_section_from_elf_index (abfd, isym->st_shndx);
              symval = isym->st_value;
              /* If the reloc is absolute, it will not have
                 a symbol or section associated with it.  */
              if (sym_sec == sec)
                {
                  symval += sym_sec->output_section->vma
                    + sym_sec->output_offset;

                  if (symval < shrinked_insn_address
                      && (symval + irel->r_addend) >= shrinked_insn_address)
                    irel->r_addend += count;
                }
              /* else...Reference symbol is absolute.  No adjustment needed.  */
            }
          /* else...Reference symbol is extern.  No need for adjusting
             the addend.  */
        }
    }

  /* Adjust the local symbols defined in this section.  */
  isym = retrieve_local_syms (abfd);
  sec_shndx = _bfd_elf_section_from_bfd_section (abfd, sec);
  if (isym != NULL)
    {
      Elf_Internal_Sym *isymend;

      isymend = isym + symtab_hdr->sh_info;
      for (; isym < isymend; isym++)
	{
	  if (isym->st_shndx == sec_shndx
	      && isym->st_value >= addr
	      && isym->st_value < toaddr)
	    isym->st_value += count;
	}
    }

  /* Now adjust the global symbols defined in this section.  */
  symcount = (symtab_hdr->sh_size / sizeof (Elf64_External_Sym)
              - symtab_hdr->sh_info);
  sym_hashes = elf_sym_hashes (abfd);
  end_hashes = sym_hashes + symcount;
  for (; sym_hashes < end_hashes; sym_hashes++)
    {
      struct elf_link_hash_entry *sym_hash = *sym_hashes;
      if ((sym_hash->root.type == bfd_link_hash_defined
           || sym_hash->root.type == bfd_link_hash_defweak)
          && sym_hash->root.u.def.section == sec
          && sym_hash->root.u.def.value >= addr
          && sym_hash->root.u.def.value < toaddr)
        {
          sym_hash->root.u.def.value += count;
        }
    }

  return TRUE;
}

/* Take the most significant 16 bits of INSN and return true if the
   instruction is a 16-bit call instruction, otherwise return false.  */

static bfd_boolean
is_16bit_call_instruction (const uint16_t insn)
{
  return (insn == 0x0fc0);
}

/* Take the most significant 16 bits of INSN and return true if the
   instruction is a 16-bit call instruction, otherwise return false.  */

static bfd_boolean
is_14bit_call_instruction (const uint16_t insn)
{
  return ((insn & 0xc000) == 0x8000);
}

/* Take the most significant 16 bits of INSN and return true if the
   instruction is one that takes a 16-bit immediate, which also has a 4-bit
   immediate version available, otherwise, return false.  */

static bfd_boolean
is_relaxable_16bit_immediate_instruction (const uint16_t insn)
{
  bfd_boolean is_match = FALSE;

  if ((insn & 0x03f8) == 0)
    {
      switch ((insn >> 11) & 0x1f)
        {
        case 0:  /* SUB */
        case 1:  /* SUBB */
        case 2:  /* ADD */
        case 3:  /* ADDC */
        case 4:  /* AND */
        case 5:  /* OR */
        case 6:  /* XOR */
        case 8:  /* CMP */
        case 12: /* TST */
        case 13: /* MOV */
          is_match = TRUE;
          break;
        }
    }
  return is_match;
}

/* Return true if the 16 bits of INSN are for a mov instruction taking an
   abs16 parameter.  */

static bfd_boolean
is_relaxable_abs16_mov_instruction (const uint16_t insn)
{
  return ((insn & 0xfbf0) == 0x6810);
}

/* Is SEC from ABFD one that should be processed during the relax phase of
   linker relaxation.  Return true if it is, otherwise return false.  The
   majority of the checks we require are actually done in common code, so
   it turns out all we need to do here is report a log message.  */

static bfd_boolean
mrk3_relax_section_filter (bfd *abfd, asection *sec)
{
  if (elf_section_flags (sec) & SHF_MRK3_NON_RELAX)
    return FALSE;

  relax_log ("Relaxing section `%s' from `%s'\n",
             sec->name, abfd->filename);
  return TRUE;
}

/* Is a relocation of TYPE one that can be relaxed?  Return true if it is,
   otherwise return false.  */

static bfd_boolean
mrk3_relax_relocation_filter (unsigned int type)
{
  return (type == R_MRK3_PCREL16
          || type == R_MRK3_HIGH16
          || type == R_MRK3_CALL16);
}

/* Process a single relocation IREL in SEC from ABFD during the relax phase
   of linker relaxation.  */

static bfd_boolean
mrk3_relax_handle_relocation (bfd *abfd, asection *sec,
                              struct bfd_link_info *link_info,
                              Elf_Internal_Rela *irel,
                              bfd_vma symval,
                              Elf_Internal_Rela *internal_relocs,
                              bfd_boolean *again)
{
  switch (ELF64_R_TYPE (irel->r_info))
    {
    case R_MRK3_PCREL16:
      {
        bfd_byte *contents;
        bfd_vma reloc_addr, dest_addr;
        bfd_signed_vma offset;
        uint16_t insn;

        /* Compute the from and to addresses.  */
        reloc_addr = (sec->output_section->vma
                      + sec->output_offset
                      + irel->r_offset);
        dest_addr = (symval + irel->r_addend);

        /* A pc-relative relocation across address spaces is not going
           to work, this should be detected, and give an error later in
           the process.  For now, just don't try to relax.  */
        if (MRK3_GET_MEMORY_SPACE_ID (reloc_addr)
            != MRK3_GET_MEMORY_SPACE_ID (dest_addr))
          return TRUE;

        /* Let's not worry about address space ID any more.  */
        reloc_addr = MRK3_GET_ADDRESS_LOCATION (reloc_addr);
        dest_addr = MRK3_GET_ADDRESS_LOCATION (dest_addr);

        offset = ((bfd_signed_vma) (dest_addr - reloc_addr)) >> 1;
        if (offset < -127 || offset > 127)
          return TRUE;

        /* Get the encoding of the instruction we're relaxing, and
           convert to a 8-bit branch encoding.  */
        relax_log ("    Convert to 8-bit branch instruction.\n");
        contents = retrieve_contents (abfd, sec, link_info->keep_memory);
        insn = bfd_get_16 (abfd, contents + irel->r_offset);
        BFD_ASSERT ((insn & 0xff) == 0x80);
        insn = insn & 0xff00;
        bfd_put_16 (abfd, insn, contents + irel->r_offset);

        /* Note that we've changed the relocs, section contents,
           etc.  */
        pin_internal_relocs (sec, internal_relocs);
        pin_contents (sec, contents);

        /* Fix the relocation's type.  */
        irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
                                     R_MRK3_PCREL8);

        /* Actually delete the bytes.  */
        if (!mrk3_elf_relax_delete_bytes (abfd, sec,
                                          irel->r_offset + 2, 2))
          return FALSE;

        /* That will change things, so, we should relax again.
           Note that this is not required, and it may be slow.  */
        *again = TRUE;
      }
      break;

    case R_MRK3_CALL16:
      {
        bfd_byte *contents;
        uint16_t insn;
        bfd_vma reloc_addr, dest_addr;

        /* Compute the from and to addresses.  */
        reloc_addr = (sec->output_section->vma
                      + sec->output_offset
                      + irel->r_offset);
        dest_addr = (symval + irel->r_addend);

        /* A CALL instruction across address spaces does not make
           sense, and probably indicates an error.  To avoid
           confusion such cases are not modified here.  */
        if (MRK3_GET_MEMORY_SPACE_ID (reloc_addr)
            != MRK3_GET_MEMORY_SPACE_ID (dest_addr))
          return TRUE;

        /* Let's not worry about address space ID any more.  */
        reloc_addr = MRK3_GET_ADDRESS_LOCATION (reloc_addr);
        dest_addr = MRK3_GET_ADDRESS_LOCATION (dest_addr);

        /* The 14-bit call instruction places the 14-bits of the
           word address into the lower 14-bits of the current pc to
           compute the call destination.  To check that a call from
           RELOC_ADDR to DEST_ADDR (both of which are byte
           addresses) will fit we check that everything other than
           the lower 15-bits match.  */
        if ((reloc_addr & ~0x7fff) != (dest_addr & ~0x7fff))
          return TRUE;

        relax_log ("    Relocation at: %#08lx\n", reloc_addr);
        relax_log ("    Destination at %#08lx\n", dest_addr);

        /* Convert to a 14-bit CALL instruction.  */
        contents = retrieve_contents (abfd, sec, link_info->keep_memory);
        insn = bfd_get_16 (abfd, contents + irel->r_offset);
        relax_log ("    Instruction encoding is %#08x\n", insn);
        relax_log ("    Convert to 14-bit call instruction.\n");
        BFD_ASSERT (is_16bit_call_instruction (insn));
        insn = 0x8000;
        bfd_put_16 (abfd, insn, contents + irel->r_offset);

        /* Note that we've changed the relocs, section contents,
           etc.  */
        pin_internal_relocs (sec, internal_relocs);
        pin_contents (sec, contents);

        /* Fix the relocation's type.  */
        irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
                                     R_MRK3_CALL14);

        /* Actually delete the bytes.  */
        if (!mrk3_elf_relax_delete_bytes (abfd, sec,
                                          irel->r_offset + 2, 2))
          return FALSE;

        /* That will change things, so, we should relax again.
           Note that this is not required, and it may be slow.  */
        *again = TRUE;
      }
      break;

    case R_MRK3_HIGH16:
      {
        bfd_byte *contents;
        uint16_t insn;
        bfd_signed_vma imm_value;
        unsigned int new_reloc_type;

        /* Get the instruction code for relaxing.  Only some of the
           16-bit immediate instructions have 4-bit immediate versions,
           skip those that don't.  */
        contents = retrieve_contents (abfd, sec, link_info->keep_memory);
        insn = bfd_get_16 (abfd, contents + irel->r_offset);
        relax_log ("    Instruction encoding is %#08x\n", insn);

        imm_value = (symval + irel->r_addend);
        relax_log ("    Immediate value is %#08lx\n", imm_value);

        if (is_relaxable_abs16_mov_instruction (insn))
          {
            /* Will this fit in a direct9 encoding?  */
            imm_value = (symval + irel->r_addend);
            if ((MRK3_GET_ADDRESS_LOCATION (imm_value) >> 9) != 0)
              {
                release_contents (sec, contents);
                return TRUE;
              }

            /* Clear some bits.  */
            relax_log ("    Convert to direct9 instruction.\n");
            insn = ((insn & 0x040f) | 0xc000);
            new_reloc_type = R_MRK3_DIRECT9;
          }
        else if (is_relaxable_16bit_immediate_instruction (insn))
          {
            /* Only select values can be encoded in a 4-bit immediate.  */
            if (imm_value < -1
                || (imm_value > 10
                    && imm_value != 16
                    && imm_value != 32
                    && imm_value != 64
                    && imm_value != 128))
              {
                release_contents (sec, contents);
                return TRUE;
              }

            /* Set bit 7 to convert to the 4-bit constant version of
               the instruction.  */
            relax_log ("    Convert to 4-bit constant instruction.\n");
            insn |= (1 << 7);
            new_reloc_type = R_MRK3_CONST4;
          }
        else
          {
            release_contents (sec, contents);
            return TRUE;
          }

        /* Write out updated instruction.  */
        bfd_put_16 (abfd, insn, contents + irel->r_offset);

        /* Note that we've changed the relocs, section contents,
           etc.  */
        pin_internal_relocs (sec, internal_relocs);
        pin_contents (sec, contents);

        /* Fix the relocation's type.  */
        irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
                                     new_reloc_type);

        /* Actually delete the bytes.  */
        if (!mrk3_elf_relax_delete_bytes (abfd, sec,
                                          irel->r_offset + 2, 2))
          return FALSE;

        /* That will change things, so, we should relax again.
           Note that this is not required, and it may be slow.  */
        *again = TRUE;
      }
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

/* Return true if SEC from ABFD needs to be checked during the check phase
   of linker relaxation,  otherwise return false.  */

static bfd_boolean
mrk3_check_section_filter (bfd *abfd, asection *sec)
{
  struct mrk3_relax_info *relax_info;

  /* If this section was not relaxed then no checking required.  */
  relax_info = get_mrk3_relax_info (sec);
  if (!relax_info->was_relaxed)
    return FALSE;

  relax_log ("Checking section `%s' from `%s'\n",
             sec->name, abfd->filename);
  return TRUE;
}

/* Which relocations need to be processed during the check phase of linker
   relaxation?  Return true if TYPE needs to be checked, otherwise return
   false.  */

static bfd_boolean
mrk3_check_relocation_filter (unsigned int type)
{
  return (type == R_MRK3_PCREL8 || type == R_MRK3_CALL14);
}

/* Process a single relocation IREL in SEC from ABFD during the check
   phase of linker relaxation.  */

static bfd_boolean
mrk3_check_handle_relocation (bfd *abfd, asection *sec,
                              struct bfd_link_info *link_info,
                              Elf_Internal_Rela *irel,
                              bfd_vma symval,
                              Elf_Internal_Rela *internal_relocs,
                              bfd_boolean *again)
{
  switch (ELF64_R_TYPE (irel->r_info))
    {
    case R_MRK3_PCREL8:
      {
        bfd_byte *contents;
        bfd_vma reloc_addr, dest_addr;
        bfd_signed_vma offset;
        uint16_t insn;

        /* Compute the from and to addresses.  */
        reloc_addr = (sec->output_section->vma
                      + sec->output_offset
                      + irel->r_offset);
        dest_addr = (symval + irel->r_addend);

        /* A pc-relative relocation across address spaces would not
           have been created by linker relaxation.  If this is spotted
           here then the user has manually created an 8-bit
           pc-relative relocation across address spaces.  We can't fix
           this, and an error will be generated later on.  Just ignore
           this for now.  */
        if (MRK3_GET_MEMORY_SPACE_ID (reloc_addr)
            != MRK3_GET_MEMORY_SPACE_ID (dest_addr))
          return TRUE;

        /* Let's not worry about address space ID any more.  */
        reloc_addr = MRK3_GET_ADDRESS_LOCATION (reloc_addr);
        dest_addr = MRK3_GET_ADDRESS_LOCATION (dest_addr);

        offset = ((bfd_signed_vma) (dest_addr - reloc_addr)) >> 1;
        if (offset >= -127 && offset <= 127)
          {
            relax_log ("    Does not need reverting.\n");
            return TRUE;
          }

        /* This 8-bit branch is out of range and needs to
           be reverted.  */
        relax_log ("    Reverting to 16-bit branch instruction.\n");
        contents = retrieve_contents (abfd, sec, link_info->keep_memory);
        insn = bfd_get_16 (abfd, contents + irel->r_offset);
        insn = (insn & 0xff00) | 0x80;
        bfd_put_16 (abfd, insn, contents + irel->r_offset);

        /* Note that we've changed the relocs, section contents,
           etc.  */
        pin_internal_relocs (sec, internal_relocs);
        pin_contents (sec, contents);

        /* Fix the relocation's type.  */
        irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
                                     R_MRK3_PCREL16);

        /* Actually insert the bytes.  */
        if (!mrk3_elf_relax_insert_bytes (abfd, sec,
                                          irel->r_offset + 2, 2))
          return FALSE;

        /* That will change things, so, we should relax again.
           Note that this is not required, and it may be slow.  */
        *again = TRUE;
      }
      break;

    case R_MRK3_CALL14:
      {
        bfd_byte *contents;
        uint16_t insn;
        bfd_vma reloc_addr, dest_addr;

        /* Compute the from and to addresses.  */
        reloc_addr = (sec->output_section->vma
                      + sec->output_offset
                      + irel->r_offset);
        dest_addr = (symval + irel->r_addend);

        /* A CALL instruction across address spaces does not make
           sense, and probably indicates an error.  To avoid
           confusion such cases are not modified here.  */
        if (MRK3_GET_MEMORY_SPACE_ID (reloc_addr)
            != MRK3_GET_MEMORY_SPACE_ID (dest_addr))
          return TRUE;

        /* Let's not worry about address space ID any more.  */
        reloc_addr = MRK3_GET_ADDRESS_LOCATION (reloc_addr);
        dest_addr = MRK3_GET_ADDRESS_LOCATION (dest_addr);

        /* The 14-bit call instruction places the 14-bits of the
           word address into the lower 14-bits of the current pc to
           compute the call destination.  To check that a call from
           RELOC_ADDR to DEST_ADDR (both of which are byte
           addresses) will fit we check that everything other than
           the lower 15-bits match.  */
        relax_log ("    Relocation at: %#08lx\n", reloc_addr);
        relax_log ("    Destination at %#08lx\n", dest_addr);
        if ((reloc_addr & ~0x7fff) == (dest_addr & ~0x7fff))
          {
            relax_log ("    Does not need reverting.\n");
            return TRUE;
          }

        /* Convert to a 14-bit CALL instruction.  */
        contents = retrieve_contents (abfd, sec, link_info->keep_memory);
        insn = bfd_get_16 (abfd, contents + irel->r_offset);
        relax_log ("    Instruction encoding is %#08x\n", insn);
        relax_log ("    Reverting to 16-bit call instruction.\n");
        BFD_ASSERT (is_14bit_call_instruction (insn));
        insn = 0x0fc0;
        bfd_put_16 (abfd, insn, contents + irel->r_offset);

        /* Note that we've changed the relocs, section contents,
           etc.  */
        pin_internal_relocs (sec, internal_relocs);
        pin_contents (sec, contents);

        /* Fix the relocation's type.  */
        irel->r_info = ELF64_R_INFO (ELF64_R_SYM (irel->r_info),
                                     R_MRK3_CALL16);

        /* Actually insert the bytes.  */
        if (!mrk3_elf_relax_insert_bytes (abfd, sec,
                                          irel->r_offset + 2, 2))
          return FALSE;

        /* That will change things, so, we should relax again.
           Note that this is not required, and it may be slow.  */
        *again = TRUE;
      }
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

/* Per-section filter called on each section (SEC from ABFD) that linker
   relaxation visits.  Return true if the section should be handled,
   otherwise return false.  */

typedef bfd_boolean (*section_filter_ftype) (bfd *abfd, asection *sec);

/* Are we interested in relocations of TYPE within this phase of linker
   relaxation?  Return true if we are, otherwise return false.  */

typedef bfd_boolean (*reloc_filter_ftype) (unsigned int type);

/* Function called to actually perform some action on the section.  IREL is
   the relocation being modified, SYMVAL is the value being patched in.
   CONTENTS are the section contents.  INTERNAL_RELOCS are the relocations
   for this section.  AGAIN should have its contents set to true if the
   section contents are modified.  */

typedef bfd_boolean (*handle_reloc_ftype) (bfd *abfd, asection *sec,
                                           struct bfd_link_info *link_info,
                                           Elf_Internal_Rela *irel,
                                           bfd_vma symval,
                                           Elf_Internal_Rela *internal_relocs,
                                           bfd_boolean *again);

/* Collection of functions that are used to specialise the linker
   relaxation process for either relaxing, or checking that previous
   relaxations are valid.  */

struct mrk3_relaxation_hooks
{
  section_filter_ftype section_filter;
  reloc_filter_ftype reloc_filter;
  handle_reloc_ftype handle_reloc;
};

/* The functions for performing linker relaxation.  */

static struct mrk3_relaxation_hooks mrk3_relax_hooks =
  {
    mrk3_relax_section_filter,
    mrk3_relax_relocation_filter,
    mrk3_relax_handle_relocation
  };

/* The functions to check that previous relaxation are still valid.  */

static struct mrk3_relaxation_hooks mrk3_check_hooks =
  {
    mrk3_check_section_filter,
    mrk3_check_relocation_filter,
    mrk3_check_handle_relocation
  };

/* Worker core for linker relaxation.  This function performs common task
   of iterating over the relocations in section SEC from ABFD.  The
   functions within HOOKS are used to specialise for either performing
   relaxation, or checking that previously applied relaxations are still
   valid.

   The contents of AGAIN will have already been set to false.  The contents
   should be changed to true if the section contents are modified, this
   will trigger another iteration of either relaxation or checking to
   ensure that everything is still valid with the new contents.

   The LINK_INFO is the general purpose control data structure.

   The return value should be true if no errors are encountered, otherwise,
   return false.  */

static bfd_boolean
mrk3_elf_relax_section_worker (bfd *abfd,
                               asection *sec,
                               struct bfd_link_info *link_info,
                               bfd_boolean *again,
                               struct mrk3_relaxation_hooks *hooks)
{
  Elf_Internal_Shdr *symtab_hdr;
  Elf_Internal_Rela *internal_relocs;
  Elf_Internal_Rela *irel, *irelend;
  Elf_Internal_Sym *isymbuf = NULL;

  if (!hooks->section_filter (abfd, sec))
    return TRUE;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;

  /* Get a copy of the native relocations.  */
  internal_relocs = retrieve_internal_relocs (abfd, sec,
                                              link_info->keep_memory);
  if (internal_relocs == NULL || sec->reloc_count == 0)
    return TRUE;

  /* Walk through the relocs looking for relaxing opportunities.  */
  irelend = internal_relocs + sec->reloc_count;
  for (irel = internal_relocs; irel < irelend; irel++)
    {
      bfd_vma symval;
      reloc_howto_type *howto;
      unsigned char sym_type;
      bfd_vma toff;
      asection *tsec;

      /* Filter out all relocation types that we know can't be handled.  */
      if (!hooks->reloc_filter (ELF64_R_TYPE (irel->r_info)))
        continue;

      BFD_ASSERT (ELF64_R_TYPE (irel->r_info) < (unsigned int) R_MRK3_max);
      howto = &elf_mrk3_howto_table[ELF64_R_TYPE (irel->r_info)];
      relax_log ("  Relocation type: %s at section Offset %#08lx\n",
                 howto->name, irel->r_offset);

      /* Read this BFD's local symbols if we haven't done so already.  */
      if (isymbuf == NULL)
        isymbuf = retrieve_local_syms (abfd);

      /* Get the value of the symbol referred to by the reloc.  */
      /* Get the value of the symbol referred to by the reloc.  */
      if (ELF64_R_SYM (irel->r_info) < symtab_hdr->sh_info)
	{
	  /* A local symbol.  */
	  Elf_Internal_Sym *isym;

	  /* Read this BFD's local symbols.  */
	  if (isymbuf == NULL)
	    {
	      isymbuf = (Elf_Internal_Sym *) symtab_hdr->contents;
	      if (isymbuf == NULL)
		isymbuf = bfd_elf_get_elf_syms (abfd, symtab_hdr,
						symtab_hdr->sh_info, 0,
						NULL, NULL, NULL);
	      if (isymbuf == 0)
		goto error_return;
	    }
	  isym = isymbuf + ELF64_R_SYM (irel->r_info);
	  if (isym->st_shndx == SHN_UNDEF)
	    tsec = bfd_und_section_ptr;
	  else if (isym->st_shndx == SHN_ABS)
	    tsec = bfd_abs_section_ptr;
	  else if (isym->st_shndx == SHN_COMMON)
	    tsec = bfd_com_section_ptr;
	  else
	    tsec = bfd_section_from_elf_index (abfd, isym->st_shndx);

	  toff = isym->st_value;
	  sym_type = ELF_ST_TYPE (isym->st_info);
	}
      else
	{
	  /* Global symbol handling.  */
	  unsigned long indx;
          struct elf_link_hash_entry *h;

	  indx = ELF64_R_SYM (irel->r_info) - symtab_hdr->sh_info;
	  h = elf_sym_hashes (abfd)[indx];

	  while (h->root.type == bfd_link_hash_indirect
		 || h->root.type == bfd_link_hash_warning)
	    h = (struct elf_link_hash_entry *) h->root.u.i.link;

	  if (h->root.type == bfd_link_hash_defined
	      || h->root.type == bfd_link_hash_defweak)
	    {
	      tsec = h->root.u.def.section;
	      toff = h->root.u.def.value;
	    }
	  else if (h->root.type == bfd_link_hash_undefined
		   || h->root.type == bfd_link_hash_undefweak)
	    {
	      tsec = bfd_und_section_ptr;
	      toff = link_info->relocatable ? indx : 0;
	    }
	  else
	    continue;

	  sym_type = h->type;
	}

      if (tsec->sec_info_type == SEC_INFO_TYPE_MERGE)
	{
	  /* At this stage in linking, no SEC_MERGE symbol has been
	     adjusted, so all references to such symbols need to be
	     passed through _bfd_merged_section_offset.  (Later, in
	     relocate_section, all SEC_MERGE symbols *except* for
	     section symbols have been adjusted.)

	     gas may reduce relocations against symbols in SEC_MERGE
	     sections to a relocation against the section symbol when
	     the original addend was zero.  When the reloc is against
	     a section symbol we should include the addend in the
	     offset passed to _bfd_merged_section_offset, since the
	     location of interest is the original symbol.  On the
	     other hand, an access to "sym+addend" where "sym" is not
	     a section symbol should not include the addend;  Such an
	     access is presumed to be an offset from "sym";  The
	     location of interest is just "sym".  */
	  if (sym_type == STT_SECTION)
	    toff += irel->r_addend;

	  toff =
            _bfd_merged_section_offset (abfd, &tsec,
                                        elf_section_data (tsec)->sec_info,
                                        toff);

	  if (sym_type != STT_SECTION)
	    toff += irel->r_addend;
	}

      symval = toff + tsec->output_section->vma + tsec->output_offset;

      if (!hooks->handle_reloc (abfd, sec, link_info, irel, symval,
                                internal_relocs, again))
        goto error_return;
    }

  /* These release calls will only free the resources if they have not been
     pinned to the section or bfd.  */
  release_internal_relocs (sec, internal_relocs);
  return TRUE;

error_return:
  release_internal_relocs (sec, internal_relocs);
  return FALSE;
}

/* The entry point for linker relaxation.  Decide which phase of linker
   relaxation we're in and call the correct worker function.  */

static bfd_boolean
mrk3_elf_relax_section (bfd *abfd,
                        asection *sec,
                        struct bfd_link_info *link_info,
                        bfd_boolean *again)
{
  bfd_boolean answer = FALSE;
  static int highest_pass = 0;

  /* Set the contents of AGAIN to false.  The worker functions will set
     this to true if and of the section contents are changed, and another
     trip around is required.  */
  *again = FALSE;

  /* Due to the very strange way in which linker relaxation is triggered on
     ELF files from `gldelf64mrk3_map_segments` the whole linker relaxation
     process is run multiple times.  This can cause problems if we perform
     a relax phase after a check phase.  To work around this I use this
     highest pass mechanism to ensure that once relaxation is finished we
     don't return to it.  */
  if (link_info->relax_pass < highest_pass)
    return TRUE;
  highest_pass = link_info->relax_pass;

  /* We don't have to do anything for a relocatable link, if this section
     does not have relocs, or if this is not a code section.  */
  if (link_info->relocatable
      || (sec->flags & SEC_RELOC) == 0
      || sec->reloc_count == 0
      || (sec->flags & SEC_CODE) == 0)
    return TRUE;

  relax_log ("\n\n----- Pass = %d ----- \n", link_info->relax_pass);
  BFD_ASSERT (link_info->relax_pass < 2);
  switch (link_info->relax_pass)
    {
    case 0:
      answer =
        mrk3_elf_relax_section_worker (abfd, sec, link_info, again,
                                       &mrk3_relax_hooks);
      break;

    case 1:
      answer =
        mrk3_elf_relax_section_worker (abfd, sec, link_info, again,
                                       &mrk3_check_hooks);
      break;
    }

  return answer;
}

/* Based on _bfd_elf_create_dynamic_sections */
static bfd_boolean
mrk3_elf_create_plt_section (bfd *dynobj, struct bfd_link_info *info)
{
   flagword flags;
   struct elf_link_hash_entry *h;
   asection *s;
   const struct elf_backend_data *bed = get_elf_backend_data (dynobj);
   struct elf_link_hash_table *htab = elf_hash_table (info);
   bfd *output_bfd = info->output_bfd;

   /* If .plt already exists, we don't need to recreate it */
   if (htab->splt != NULL)
     return TRUE;

   flags  = bed->dynamic_sec_flags;
   flags |= SEC_ALLOC | SEC_CODE | SEC_LOAD;
   s = bfd_make_section_anyway_with_flags (dynobj, ".plt", flags);
   if (s == NULL
       || ! bfd_set_section_alignment (dynobj, s, bed->plt_alignment))
     return FALSE;
   htab->splt = s;

   htab->dynobj = output_bfd;

   /* Define PLT symbol */
   h = _bfd_elf_define_linkage_sym (dynobj, info, s,
                                    "_PROCEDURE_LINKAGE_TABLE");
   htab->hplt = h;
   if (h == NULL)
     return FALSE;

   return TRUE;
 }

/* Check through relocations in a section, and assign space in PLT where
   required */
static bfd_boolean
mrk3_elf_check_relocs (bfd *abfd,
                       struct bfd_link_info *info ATTRIBUTE_UNUSED,
                       asection *sec ATTRIBUTE_UNUSED,
                       const Elf_Internal_Rela *relocs)
{
  Elf_Internal_Shdr *symtab_hdr;
  struct elf_link_hash_entry **sym_hashes;
  const Elf_Internal_Rela *rel;

  const Elf_Internal_Rela *rel_end;

  symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  sym_hashes = elf_sym_hashes (abfd);

  rel_end = relocs + sec->reloc_count;
  for (rel = relocs; rel < rel_end; rel++)
    {
      struct elf_link_hash_entry *h;
      unsigned long r_symndx;

      r_symndx = ELF64_R_SYM (rel->r_info);
      if (r_symndx < symtab_hdr->sh_info)
        h = NULL;
      else
        {
          h = sym_hashes[r_symndx - symtab_hdr->sh_info];
          while (h->root.type == bfd_link_hash_indirect
                 || h->root.type == bfd_link_hash_warning)
            h = (struct elf_link_hash_entry *) h->root.u.i.link;
        }

      switch (ELF64_R_TYPE (rel->r_info))
        {
          /* These relocs require a plt entry */
          case R_MRK3_PIC:
            if (h != NULL)
              {
                /* Create PLT section if it doesn't already exist and define
                   the PLT symbol */
                mrk3_elf_create_plt_section (abfd, info);
                h->needs_plt = 1;
                /* Enables analysis of dynamic sections */
                h->plt.refcount += 1;
                info->dynamic = 1;
                /* If we have not seen this symbol before space needs
                   allocating in the PLT */
                if (h->plt.refcount == 0)
                  elf_hash_table (info)->splt->size += PLT_ENTRY_SIZE;
              }
            break;
          /* Do we need anything else here? */
          default:
            break;
        }
    }

  return TRUE;
}

/* Allocate memory for PLT and place this new section into the final object. */
static bfd_boolean
mrk3_elf_size_dynamic_sections (bfd *output_bfd,
                                struct bfd_link_info *info)
{
  asection *tmpplt = elf_hash_table(info)->splt;
  asection *plt = bfd_get_section_by_name (output_bfd, ".plt");
  plt->contents = bfd_zalloc (output_bfd, tmpplt->size);
  plt->size = tmpplt->size;
  elf_hash_table(info)->splt = plt;

  return TRUE;
}

/* Finish PLT section */
static bfd_boolean
mrk3_elf_finish_dynamic_sections (bfd * output_bfd,
                                  struct bfd_link_info * info ATTRIBUTE_UNUSED)
{
  asection *plt;
  bfd_vma i;
  bfd_vma address;
  bfd_vma address_lo;
  bfd_vma address_hi;

  plt = bfd_get_section_by_name (output_bfd, ".plt");
  if (plt == NULL)
    return TRUE;

  for (i = 0; i < plt->size; i += PLT_ENTRY_SIZE)
    {
      /* NOTE: PLTENC
      Additionally, mrk3_final_link_relocate_pic used the first four bytes of
      the PLT entry to store the address of the function, so this must be moved
      into its correct place within the entry. */
      address = bfd_get_32 (output_bfd, plt->contents + i);
      address = address / 2;
      address_lo = address & 0xffff;
      address_hi = (address >> 16) & 0xffff;
      /* sub r7, #2   - 2b (0)  */
      bfd_put_16 (output_bfd, 0x0497,     plt->contents + i);
      /* mov @r7, #LO - 4b (2)  */
      bfd_put_16 (output_bfd, 0x6c0b,     plt->contents + i + 2);
      bfd_put_16 (output_bfd, address_lo, plt->contents + i + 4);
      /* sub r7, #2   - 2b (6)  */
      bfd_put_16 (output_bfd, 0x0497,     plt->contents + i + 6);
      /* mov @r7, #HI - 4b (8) */
      bfd_put_16 (output_bfd, 0x6c0b,     plt->contents + i + 8);
      bfd_put_16 (output_bfd, address_hi, plt->contents + i + 10);
      /* eret         - 2b (12) */
      bfd_put_16 (output_bfd, 0x1bc7,     plt->contents + i + 12);
    }
  return TRUE;
}

/* Get the input section for a given symbol index.
   If the symbol is:
   . a section symbol, return the section;
   . a common symbol, return the common section;
   . an undefined symbol, return the undefined section;
   . an indirect symbol, follow the links;
   . an absolute value, return the absolute section.  */

static asection *
get_elf_r_symndx_section (bfd *abfd, unsigned long r_symndx)
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  asection *target_sec = NULL;
  if (r_symndx < symtab_hdr->sh_info)
    {
      Elf_Internal_Sym *isymbuf;
      unsigned int section_index;

      isymbuf = retrieve_local_syms (abfd);
      section_index = isymbuf[r_symndx].st_shndx;

      if (section_index == SHN_UNDEF)
	target_sec = bfd_und_section_ptr;
      else if (section_index == SHN_ABS)
	target_sec = bfd_abs_section_ptr;
      else if (section_index == SHN_COMMON)
	target_sec = bfd_com_section_ptr;
      else
	target_sec = bfd_section_from_elf_index (abfd, section_index);
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h = elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
        h = (struct elf_link_hash_entry *) h->root.u.i.link;

      switch (h->root.type)
	{
	case bfd_link_hash_defined:
	case  bfd_link_hash_defweak:
	  target_sec = h->root.u.def.section;
	  break;
	case bfd_link_hash_common:
	  target_sec = bfd_com_section_ptr;
	  break;
	case bfd_link_hash_undefined:
	case bfd_link_hash_undefweak:
	  target_sec = bfd_und_section_ptr;
	  break;
	default: /* New indirect warning.  */
	  target_sec = bfd_und_section_ptr;
	  break;
	}
    }
  return target_sec;
}

/* Get the section-relative offset for a symbol number.  */

static bfd_vma
get_elf_r_symndx_offset (bfd *abfd, unsigned long r_symndx)
{
  Elf_Internal_Shdr *symtab_hdr = &elf_tdata (abfd)->symtab_hdr;
  bfd_vma offset = 0;

  if (r_symndx < symtab_hdr->sh_info)
    {
      Elf_Internal_Sym *isymbuf;
      isymbuf = retrieve_local_syms (abfd);
      offset = isymbuf[r_symndx].st_value;
    }
  else
    {
      unsigned long indx = r_symndx - symtab_hdr->sh_info;
      struct elf_link_hash_entry *h =
	elf_sym_hashes (abfd)[indx];

      while (h->root.type == bfd_link_hash_indirect
             || h->root.type == bfd_link_hash_warning)
	h = (struct elf_link_hash_entry *) h->root.u.i.link;
      if (h->root.type == bfd_link_hash_defined
          || h->root.type == bfd_link_hash_defweak)
	offset = h->root.u.def.value;
    }
  return offset;
}

/* Callback used by QSORT to order relocations AP and BP.  */

static int
internal_reloc_compare (const void *ap, const void *bp)
{
  const Elf_Internal_Rela *a = (const Elf_Internal_Rela *) ap;
  const Elf_Internal_Rela *b = (const Elf_Internal_Rela *) bp;

  if (a->r_offset != b->r_offset)
    return (a->r_offset - b->r_offset);

  /* We don't need to sort on these criteria for correctness,
     but enforcing a more strict ordering prevents unstable qsort
     from behaving differently with different implementations.
     Without the code below we get correct but different results
     on Solaris 2.7 and 2.8.  We would like to always produce the
     same results no matter the host.  */

  if (a->r_info != b->r_info)
    return (a->r_info - b->r_info);

  return (a->r_addend - b->r_addend);
}

/* Return true if ADDRESS is within the vma range of SECTION from ABFD.  */

static bfd_boolean
mrk3_is_section_for_address (bfd *abfd, asection *section, bfd_vma address)
{
  bfd_vma vma;
  bfd_size_type size;

  vma = bfd_get_section_vma (abfd, section);
  if (address < vma)
    return FALSE;

  size = section->size;
  if (address >= vma + size)
    return FALSE;

  return TRUE;
}

/* Data structure used by MRK3_FIND_SECTION_FOR_ADDRESS.  */

struct mrk3_find_section_data
{
  /* The address we're looking for.  */
  bfd_vma address;

  /* The section we've found.  */
  asection *section;
};

/* Helper function to locate the section holding a certain virtual memory
   address.  This is called via bfd_map_over_sections.  The DATA is an
   instance of STRUCT MRK3_FIND_SECTION_DATA, the address field of which
   has been set to the address to search for, and the section field has
   been set to NULL.  If SECTION from ABFD contains ADDRESS then the
   section field in DATA will be set to SECTION.  As an optimisation, if
   the section field is already non-null then this function does not
   perform any checks, and just returns.  */

static void
mrk3_find_section_for_address (bfd *abfd,
                              asection *section, void *data)
{
  struct mrk3_find_section_data *fs_data
    = (struct mrk3_find_section_data *) data;

  /* Return if already found.  */
  if (fs_data->section != NULL)
    return;

  /* If this section isn't part of the addressable code content, skip it.  */
  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0
      && (bfd_get_section_flags (abfd, section) & SEC_CODE) == 0)
    return;

  if (mrk3_is_section_for_address (abfd, section, fs_data->address))
    fs_data->section = section;
}

/* Load all of the property records from SEC, a section from ABFD.  Return
   a STRUCT MRK3_PROPERTY_RECORD_LIST containing all the records.  The
   memory for the returned structure, and all of the records pointed too by
   the structure are allocated with a single call to malloc, so, only the
   pointer returned needs to be free'd.  */

static struct mrk3_property_record_list *
elf64_mrk3_load_records_from_section (bfd *abfd, asection *sec)
{
  bfd_byte *contents = NULL, *ptr, *tmp;
  bfd_size_type size, mem_size;
  bfd_byte version;
  uint16_t record_count, i;
  struct mrk3_property_record_list *r_list = NULL;
  Elf_Internal_Rela *internal_relocs = NULL, *rel, *rel_end;
  struct mrk3_find_section_data fs_data;

  fs_data.section = NULL;

  size = bfd_get_section_size (sec);
  contents = retrieve_contents (abfd, sec, FALSE);
  ptr = contents;

  /* Load the relocations for the '.mrk3.records' section if there are any, and
     sort them.  */
  internal_relocs = retrieve_internal_relocs (abfd, sec, FALSE);
  if (internal_relocs)
    qsort (internal_relocs, sec->reloc_count,
           sizeof (Elf_Internal_Rela), internal_reloc_compare);

  /* There is a header at the start of the property record section SEC, the
     format of this header is:
       uint16_t : version number
  */

  /* Check we have at least got a headers worth of bytes.  */
  if (size < MRK3_PROPERTY_SECTION_HEADER_SIZE)
    goto load_failed;

  version = *((uint16_t *) ptr);
  ptr+=2;
  BFD_ASSERT (ptr - contents == MRK3_PROPERTY_SECTION_HEADER_SIZE);

  /* Now we need to calculate the number of entries in the section so that
     we can allocate memory to hold them all.  */
  record_count = 0;
  for (tmp = ptr; ((bfd_size_type) (tmp - contents)) < size; )
    {
      bfd_byte entry_type = *((bfd_byte *) tmp);
      switch (entry_type)
        {
        case RECORD_ORG:
          tmp += 11;
          break;
        case RECORD_ALIGN:
          tmp += 13;
          break;
        }
      ++record_count;
    }

  /* Now allocate space for the list structure, and all of the list
     elements in a single block.  */
  mem_size = sizeof (struct mrk3_property_record_list)
    + sizeof (struct mrk3_property_record) * record_count;
  r_list = bfd_malloc (mem_size);
  if (r_list == NULL)
    goto load_failed;

  r_list->version = version;
  r_list->section = sec;
  r_list->record_count = record_count;
  r_list->records = (struct mrk3_property_record *) (&r_list [1]);
  size -= MRK3_PROPERTY_SECTION_HEADER_SIZE;

  /* Check that we understand the version number.  There is only one
     version number right now, anything else is an error.  */
  if (r_list->version != MRK3_PROPERTY_RECORDS_VERSION)
    goto load_failed;

  rel = internal_relocs;
  rel_end = rel + sec->reloc_count;
  for (i = 0; i < record_count; ++i)
    {
      bfd_vma address;

      /* Each entry is a single byte type, followed by an 8-byte address.
         After that is the type specific data.  */
      if (size < 9)
        goto load_failed;

      r_list->records [i].type = *((bfd_byte *) ptr);
      ptr += 1;
      size -= 1;

      r_list->records [i].section = NULL;
      r_list->records [i].offset = 0;

      if (rel)
        {
          /* The offset of the address within the .mrk3.records section.  */
          size_t offset = ptr - contents;

          while (rel < rel_end && rel->r_offset < offset)
            ++rel;

          if (rel == rel_end)
            rel = NULL;
          else if (rel->r_offset == offset)
            {
              /* Find section and section offset.  */
              unsigned long r_symndx;

              asection * rel_sec;
              bfd_vma sec_offset;

              r_symndx = ELF64_R_SYM (rel->r_info);
              rel_sec = get_elf_r_symndx_section (abfd, r_symndx);
              sec_offset = get_elf_r_symndx_offset (abfd, r_symndx)
                + rel->r_addend;

              r_list->records [i].section = rel_sec;
              r_list->records [i].offset = sec_offset;
            }
        }

      address = *((uint64_t *) ptr);
      ptr += 8;
      size -= 8;

      if (r_list->records [i].section == NULL)
        {
          /* Try to find section and offset from address.  */
          if (fs_data.section != NULL
              && !mrk3_is_section_for_address (abfd, fs_data.section,
                                              address))
            fs_data.section = NULL;

          if (fs_data.section == NULL)
            {
              fs_data.address = address;
              bfd_map_over_sections (abfd, mrk3_find_section_for_address,
                                     &fs_data);
            }

          if (fs_data.section == NULL)
            {
              fprintf (stderr, "Failed to find matching section.\n");
              goto load_failed;
            }

          r_list->records [i].section = fs_data.section;
          r_list->records [i].offset
            = address - bfd_get_section_vma (abfd, fs_data.section);
        }

      switch (r_list->records [i].type)
        {
        case RECORD_ORG:
          /* A 2-byte fill to load.  */
          if (size < 2)
            goto load_failed;
          r_list->records [i].data.org.fill = *((uint16_t *) ptr);
          ptr += 2;
          break;
        case RECORD_ALIGN:
          /* A 2-byte alignment, and a 2-byte fill to load.  */
          if (size < 4)
            goto load_failed;
          r_list->records [i].data.align.bytes = *((uint16_t *) ptr);
          ptr += 2;
          r_list->records [i].data.align.fill = *((uint16_t *) ptr);
          ptr += 2;
          size -= 4;
          /* Just initialise PRECEDING_DELETED field, this field is
             used during linker relaxation.  */
          r_list->records [i].data.align.preceding_deleted = 0;
          break;
        default:
          goto load_failed;
        }
    }

  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  return r_list;

 load_failed:
  release_internal_relocs (sec, internal_relocs);
  release_contents (sec, contents);
  free (r_list);
  return NULL;
}

/* Load all of the property records from ABFD.  See
   ELF64_MRK3_LOAD_RECORDS_FROM_SECTION for details of the return value.  */

struct mrk3_property_record_list *
elf64_mrk3_load_property_records (bfd *abfd)
{
  asection *sec;

  /* Find the '.mrk3.records' section and load the contents into memory.  */
  sec = bfd_get_section_by_name (abfd, MRK3_PROPERTY_RECORD_SECTION_NAME);
  if (sec == NULL)
    return NULL;
  return elf64_mrk3_load_records_from_section (abfd, sec);
}

const char *
elf64_mrk3_property_record_name (struct mrk3_property_record *rec)
{
  const char *str;

  switch (rec->type)
    {
    case RECORD_ORG:
      str = "ORG";
      break;
    case RECORD_ALIGN:
      str = "ALIGN";
      break;
    default:
      str = "unknown";
    }

  return str;
}

#define TARGET_LITTLE_SYM   mrk3_elf64_vec
#define TARGET_LITTLE_NAME  "elf64-mrk3"
/*#define TARGET_BIG_SYM      mrk3_elf64_vec*/
/*#define TARGET_BIG_NAME     "elf64-mrk3"*/

#define ELF_ARCH            bfd_arch_mrk3
#define ELF_MACHINE_CODE    EM_MRK3
#define ELF_MAXPAGESIZE     0x1000

#define elf_info_to_howto                   0
#define elf_info_to_howto_rel               mrk3_info_to_howto_rel
#define elf_backend_object_p                mrk3_elf_object_p
#define elf_backend_relocate_section		mrk3_elf_relocate_section

#define elf_backend_can_gc_sections         1
#define bfd_elf64_bfd_relax_section         mrk3_elf_relax_section
#define bfd_elf64_new_section_hook	    elf_mrk3_new_section_hook

/* PLT */
#define elf_backend_check_relocs               mrk3_elf_check_relocs
#define elf_backend_plt_alignment              2
#define elf_backend_size_dynamic_sections      mrk3_elf_size_dynamic_sections
#define elf_backend_finish_dynamic_sections    mrk3_elf_finish_dynamic_sections


#include "elf64-target.h"
