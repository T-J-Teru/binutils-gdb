/* Copyright (C) 2006-2015 Free Software Foundation, Inc.

   Written by Bjoern Haase <bjoern.m.haase@web.de>

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
   Foundation, Inc., 51 Franklin Street - Fifth Floor,
   Boston, MA 02110-1301, USA.  */

#ifndef __ELF64_MRK3__
#define __ELF64_MRK3__

/* The name of the section into which the property records are stored.  */
#define MRK3_PROPERTY_RECORD_SECTION_NAME ".mrk3.records"

/* The current version number for the format of the property records.  */
#define MRK3_PROPERTY_RECORDS_VERSION 1

/* The size of the header that is written to the property record section
   before the property records are written out.  */
#define MRK3_PROPERTY_SECTION_HEADER_SIZE 2

/* This holds a single property record in memory, the structure of this
   data when written out to the ELF section is more compressed.  */

struct mrk3_property_record
{
  /* The section and offset for this record.  */
  asection *section;
  bfd_vma offset;

  /* The type of this record.  */
  enum {
    RECORD_ORG = 1,
    RECORD_ALIGN = 2,
  } type;

  /* Type specific data.  */
  union
  {
    /* RECORD_ORG.  */
    struct
    {
      unsigned long fill;
    } org;

    /* RECORD_ALIGN.  */
    struct
    {
      unsigned long bytes;
      unsigned long fill;

      /* This field is used during linker relaxation to track the number of
         bytes that have been opened up before this alignment directive.
         When we have enough bytes available it is possible to move the
         re-align this directive backwards while still maintaining the
         alignment requirement.  */
      unsigned long preceding_deleted;
    } align;
  } data;
};

struct mrk3_property_record_list
{
  /* The version number tells us the structure of the property record data
     within the section.  See MRK3_PROPERTY_RECORDS_VERSION.  */
  bfd_byte version;

  /* The number of property records.  This is stored as a 2-byte value in
     the section contents.  */
  unsigned long record_count;

  /* The section from which the property records were loaded.  This is the
     actual section containing the records, not the section(s) to which the
     records apply.  */
  asection *section;

  /* The actual property records.  */
  struct mrk3_property_record *records;
};

/* Load the property records from ABFD, return NULL if there are non
   found, otherwise return pointer to dynamically allocated memory.  The
   memory for the header and all of the records are allocated in a single
   block, as such only the header needs to be freed.  */

extern struct mrk3_property_record_list *elf64_mrk3_load_property_records (bfd *abfd);

/* Return a string that is the name of the property record pointed to by REC.  */
extern const char *elf64_mrk3_property_record_name (struct mrk3_property_record *rec);

/* Update the contents of a records section, used during linking.  */
extern bfd_boolean elf64_mrk3_update_records_section (bfd *, asection *,
                                                      bfd *, asection *,
                                                      struct bfd_link_info *);

#endif /* __ELF64_MRK3__ */
