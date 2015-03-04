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

#define MRK3_GET_MEMORY_SPACE_ID(ADDR) (((ADDR) >> (64 - 8)) & 0xff)
#define MRK3_GET_ADDRESS_LOCATION(ADDR) ((ADDR) & 0xffffffff)
#define MRK3_BUILD_ADDRESS(ID,LOC) (((ID & 0xff) << (64 - 8)) | (LOC & 0xffffffff))

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
         0xffffffff,            /* Src_mask.  */
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
         0xffffffffffffffff,    /* Src_mask.  */
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
	 0xffff0000,            /* Src_mask.  */
	 0xffff0000,            /* Dst_mask.  */
	 FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_CALL14,         /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         14,                    /* Bitsize.  */
         FALSE,                 /* PC_relative.  */
         0,                     /* Bitpos. */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_CALL14",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0x3fff,                /* Src_mask.  */
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
         0xffff0000,            /* Src_mask.  */
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
         0xffff0000,            /* Src_mask.  */
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
         0xffff0000,            /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PCREL16,        /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         16,                    /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_PCREL16",      /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0xffff,                /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_PCREL8,         /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_PCREL8",       /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0xff,                  /* Src_mask.  */
         0x00ff,                /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_FORCEPCREL16,   /* Type.  */
         0,                     /* Rightshift.  */
         2,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         16,                    /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         16,                    /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_FORCEPCREL16", /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0xffff,                /* Src_mask.  */
         0xffff0000,            /* Dst_mask.  */
         FALSE),                /* PCrel_offset.  */
  HOWTO (R_MRK3_FORCEPCREL8,    /* Type.  */
         0,                     /* Rightshift.  */
         1,                     /* Size (0 = byte, 1 = short, 2 = long).  */
         8,                     /* Bitsize.  */
         TRUE,                  /* PC_relative.  */
         0,                     /* Bitpos.  */
         complain_overflow_bitfield, /* Complain_on_overflow.  */
         bfd_elf_generic_reloc, /* Special_function.  */
         "R_MRK3_FORCEPCREL8",  /* Name.  */
         TRUE,                  /* Partial_inplace.  */
         0xff,                  /* Src_mask.  */
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
         0xf,                   /* Src_mask.  */
         0x78,                  /* Dst_mask.  */
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

/* Perform a single relocation.

   The bulk of this function is a direct copy of the standard bfd routine
   used in these case _BFD_FINAL_LINK_RELOCATE, however, there is one MRK3
   specific change required (see comment inline) relating to byte vs code
   addresses.

   The SYMBOL_NAME is passed only for a debugging aid.  */

static bfd_reloc_status_type
mrk3_final_link_relocate (reloc_howto_type *  howto,
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
                                    contents, rel, relocation, sec, name, h);

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

#define TARGET_LITTLE_SYM   bfd_elf64_mrk3_vec
#define TARGET_LITTLE_NAME  "elf64-mrk3"
/*#define TARGET_BIG_SYM      bfd_elf64_mrk3_vec*/
/*#define TARGET_BIG_NAME     "elf64-mrk3"*/
#define ELF_ARCH            bfd_arch_mrk3
#define ELF_MACHINE_CODE    EM_MRK3
#define ELF_MAXPAGESIZE     0x1000

#define elf_info_to_howto                   0
#define elf_info_to_howto_rel               mrk3_info_to_howto_rel
#define elf_backend_object_p                mrk3_elf_object_p
#define elf_backend_relocate_section		mrk3_elf_relocate_section

#define elf_backend_can_gc_sections         1

#include "elf64-target.h"
