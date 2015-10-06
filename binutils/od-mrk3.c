/* Copyright (C) 2011-2015 Free Software Foundation, Inc.
   Written by Senthil Kumar Selvaraj, Atmel.

   This file is part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include <stddef.h>
#include <time.h>
#include <stdint.h>
#include "safe-ctype.h"
#include "bfd.h"
#include "objdump.h"
#include "bucomm.h"
#include "bfdlink.h"
#include "bfd.h"
#include "elf/external.h"
#include "elf/internal.h"
#include "elf64-mrk3.h"

/* Index of the options in the options[] array.  */
#define OPT_MRK3_RECORDS 0

/* List of actions.  */
static struct objdump_private_option options [] =
  {
    { "mrk3-records",  0 },
    { NULL, 0 }
  };

/* Display help.  */

static void
elf64_mrk3_help (FILE *stream)
{
  fprintf (stream, _("\
For MRK3 ELF files:\n\
  mrk3-records    Display contents of .mrk3.records section\n\
"));
}

/* Return TRUE if ABFD is handled.  */

static int
elf64_mrk3_filter (bfd *abfd)
{
  return bfd_get_flavour (abfd) == bfd_target_elf_flavour;
}

static void
elf64_mrk3_dump_records (bfd *abfd)
{
  struct mrk3_property_record_list *r_list;
  unsigned int i;

  r_list = elf64_mrk3_load_property_records (abfd);
  if (r_list == NULL)
    return;

  printf ("\nContents of `%s' section:\n\n", r_list->section->name);

  printf ("  Version: %d\n", r_list->version);

  for (i = 0; i < r_list->record_count; ++i)
    {
      printf ("   %d %s @ %s + %#08lx (%#08lx)\n",
              i,
              elf64_mrk3_property_record_name (&r_list->records [i]),
              r_list->records [i].section->name,
              r_list->records [i].offset,
              (bfd_get_section_vma (abfd, r_list->records [i].section)
               + r_list->records [i].offset));
      switch (r_list->records [i].type)
        {
        case RECORD_ORG:
          printf ("     Fill: 0x%04lx\n",
                  r_list->records [i].data.org.fill);
          break;
        case RECORD_ALIGN:
          printf ("     Align: 0x%04lx, Fill: 0x%04lx\n",
                  r_list->records [i].data.align.bytes,
                  r_list->records [i].data.align.fill);
          break;
        }
    }

  free (r_list);
}

static void
elf64_mrk3_dump (bfd *abfd)
{
  if (options[OPT_MRK3_RECORDS].selected)
    elf64_mrk3_dump_records (abfd);
}

const struct objdump_private_desc objdump_private_desc_elf64_mrk3 =
  {
    elf64_mrk3_help,
    elf64_mrk3_filter,
    elf64_mrk3_dump,
    options
  };
