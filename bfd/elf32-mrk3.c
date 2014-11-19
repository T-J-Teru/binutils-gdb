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
   0xffffffff,                /* Src_mask.  */
   0xffffffff,                /* Dst_mask.  */
   FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_HIGH16,         /* Type.  */
   0,                     /* Rightshift.  */
   2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
   32,                    /* Bitsize.  */
   FALSE,                 /* PC_relative.  */
   16,                    /* Bitpos. */
   complain_overflow_bitfield, /* Complain_on_overflow.  */
   bfd_elf_generic_reloc, /* Special_function.  */
   "R_MRK3_HIGH16",       /* Name.  */
   TRUE,                  /* Partial_inplace.  */
   0xffff0000,            /* Src_mask.  */
   0xffff0000,            /* Dst_mask.  */
   FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_WORD16,             /* Type.  */
   1,                     /* Rightshift.  */
   1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
   16,                    /* Bitsize.  */
   FALSE,                 /* PC_relative.  */
   0,                     /* Bitpos.  */
   complain_overflow_bitfield, /* Complain_on_overflow.  */
   bfd_elf_generic_reloc, /* Special_function.  */
   "R_MRK3_WORD16",       /* Name.  */
   TRUE,                  /* Partial_inplace.  */
   0x1ffff,                /* Src_mask.  */
   0xffff,                /* Dst_mask.  */
   FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_HI,         /* Type.  */
   16,                     /* Rightshift.  */
   2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
   32,                    /* Bitsize.  */
   FALSE,                 /* PC_relative.  */
   16,                    /* Bitpos. */
   complain_overflow_dont, /* Complain_on_overflow.  */
   bfd_elf_generic_reloc, /* Special_function.  */
   "R_MRK3_ABS_HI",       /* Name.  */
   FALSE,                  /* Partial_inplace.  */
   0xffff0000,            /* Src_mask.  */
   0xffff0000,            /* Dst_mask.  */
   FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_ABS_LO,         /* Type.  */
   0,                     /* Rightshift.  */
   2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
   32,                    /* Bitsize.  */
   FALSE,                 /* PC_relative.  */
   16,                    /* Bitpos. */
   complain_overflow_dont, /* Complain_on_overflow.  */
   bfd_elf_generic_reloc, /* Special_function.  */
   "R_MRK3_ABS_LO",       /* Name.  */
   FALSE,                  /* Partial_inplace.  */
   0xffff0000,            /* Src_mask.  */
   0xffff0000,            /* Dst_mask.  */
   FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_AUTO16,             /* Type.  */
	 0,                     /* Rightshift.  */
	 1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
	 16,                    /* Bitsize.  */
	 FALSE,                 /* PC_relative.  */
	 0,                     /* Bitpos.  */
	 complain_overflow_bitfield, /* Complain_on_overflow.  */
	 bfd_elf_generic_reloc, /* Special_function.  */
	 "R_MRK3_AUTO16",       /* Name.  */
	 TRUE,                  /* Partial_inplace.  */
	 0x1ffff,                /* Src_mask.  */
	 0xffff,                /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PCREL16,             /* Type.  */
   1,                     /* Rightshift.  */
   1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
   16,                    /* Bitsize.  */
   FALSE,                 /* PC_relative.  */
   0,                     /* Bitpos.  */
   complain_overflow_bitfield, /* Complain_on_overflow.  */
   bfd_elf_generic_reloc, /* Special_function.  */
   "R_MRK3_PCREL16",       /* Name.  */
   TRUE,                  /* Partial_inplace.  */
   0x1ffff,                /* Src_mask.  */
   0xffff,                /* Dst_mask.  */
   TRUE),                /* PCrel_offset.  */

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
  { BFD_RELOC_32,   R_MRK3_32 }
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

/* Perform a single relocation.
   By default we use the standard BFD routines.
   The SYMBOL_NAME is passed only for a debugging aid.  */

static bfd_reloc_status_type
mrk3_final_link_relocate (reloc_howto_type *  howto,
			  bfd *               input_bfd,
			  asection *          input_section,
			  bfd_byte *          contents,
			  Elf_Internal_Rela * rel,
			  bfd_vma             relocation,
			  asection *          symbol_section,
			  const char *        symbol_name ATTRIBUTE_UNUSED)
{
  /* If this is a relocation to a code symbol, and the relocation is NOT
     located inside debugging information then we should scale the value to
     make it into a word addressed value.  */
  if (symbol_section
      && ((symbol_section->flags & SEC_CODE) != 0)
      && ((input_section->flags & SEC_DEBUGGING) == 0))
    {
      /* TODO: Should give a warning if this scaling is going to drop off a
         set bit as this may indicate that either something has gone wrong,
         or that the user is not getting what they expect.  */
      relocation >>= 1;
      rel->r_addend >>= 1;
    }

  /* Only install relocation if above tests did not disqualify it.  */
  return _bfd_final_link_relocate (howto, input_bfd, input_section,
				   contents, rel->r_offset,
				   relocation, rel->r_addend);
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
mrk3_elf_relocate_section (bfd *output_bfd ATTRIBUTE_UNUSED,
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

      r_type = ELF32_R_TYPE (rel->r_info);
      r_symndx = ELF32_R_SYM (rel->r_info);
      howto  = elf_mrk3_howto_table + ELF32_R_TYPE (rel->r_info);
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

      if (info->relocatable)
	continue;

      /* Finally, the sole MRK3-specific part.  */
      r = mrk3_final_link_relocate (howto, input_bfd, input_section,
                                    contents, rel, relocation, sec, name);

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

  return TRUE;
}

#define TARGET_LITTLE_SYM   bfd_elf32_mrk3_vec
#define TARGET_LITTLE_NAME  "elf32-mrk3"
/*#define TARGET_BIG_SYM      bfd_elf32_mrk3_vec*/
/*#define TARGET_BIG_NAME     "elf32-mrk3"*/
#define ELF_ARCH            bfd_arch_mrk3
#define ELF_MACHINE_CODE    0
#define ELF_MAXPAGESIZE     0x1000

#define elf_info_to_howto                   0
#define elf_info_to_howto_rel               mrk3_info_to_howto_rel
#define elf_backend_object_p                mrk3_elf_object_p
#define elf_backend_relocate_section		mrk3_elf_relocate_section

#include "elf32-target.h"
