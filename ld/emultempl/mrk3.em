# This shell script emits a C file. -*- C -*-
#   Copyright 2015 Embecosm Ltd
#
# This file is part of the GNU Binutils.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
# MA 02110-1301, USA.
#

# This file is sourced from elf64.em, and defines extra mrk3
# specific routines.
#
fragment <<EOF

#include "elf/mrk3.h"
#include "elf64-mrk3.h"

static void
gld${EMULATION_NAME}_after_parse (void)
{
  link_info.relax_pass = 2;
  after_parse_default ();
}

static void
gld${EMULATION_NAME}_finish (void)
{
  finish_default ();

  /* This loop takes care of copying the non-relax flag from input sections
     to the corresponding output section.  This might be inefficient, not
     many sections have the non-relax flag, but there might be many input
     sections.  However, for now this is the easiest solution, so until
     this turns into a real performance problem I'll stick with this.  */
  LANG_FOR_EACH_INPUT_STATEMENT (is)
    {
      bfd *abfd = is->the_bfd;
      asection *sec;

      /* Propagate the NON_RELAX flag from each input section to the output
         section.  If any single input section is marked non-relax then the
         whole output section will be marked as non-relax.  */
      for (sec = abfd->sections; sec != NULL; sec = sec->next)
        {
          asection *out_sec = sec->output_section;

          if (out_sec
              && elf_section_flags (sec) & SHF_MRK3_NON_RELAX)
            elf_section_flags (out_sec) |= SHF_MRK3_NON_RELAX;
        }
    }
}

/* Called after input sections have been assigned to output sections.  We
   perform the default action, but we also fix up the .mrk3.records
   sections, which are going to be merged into a single output
   .mrk3.records section.

   The fix-up must be performed after the default action, as the default
   action is where linker relaxation occurs, it is linker relaxation that
   loads the contents of the .mrk3.records sections, and this must be done
   before the fix-up as the fix-up actually causes the records sections to
   become unloadable, here's why....

   .... each records section has a version number at the front, when the
   sections are merged we only want a single version number at the very
   front of the merged output section.  So, here, we remove the version
   number from the front of every input record section except the first, in
   this way when the sections are all merged they are correctly formatted,
   with a single version number at the front.  However, if, after this
   fix-up we tried to load the records from anything but the first records
   section, then the section contents would be invalid.   */

static void
mrk3_after_allocation (void)
{
  asection *o;
  bfd *output_bfd = link_info.output_bfd;
  asection *first = NULL;
  bfd_boolean need_layout = FALSE;

  /* Do this now, this will perform linker relaxation.  It's important that
     this is done before the code below which will modify the contents of
     the .mrk3.records sections.  Once these modifications have been
     performed then the records sections will be un-loadable.  */
  gld${EMULATION_NAME}_after_allocation ();

  /* Now update the contents of the .mrk3.records sections on all the input
     sections (other than the first one) so that when they are merged into
     a single output section, the new sections contents will still be
     loadable.  */
  o = bfd_get_section_by_name (output_bfd, MRK3_PROPERTY_RECORD_SECTION_NAME);
  if (o != NULL)
    {
      asection *i;

       for (i = o->map_head.s; i != NULL; i = i->map_head.s)
	{
          if (first == NULL)
            {
              first = i;

              if (first->output_offset > 0)
                {
                  einfo ("%X%P: first %s section is at offset 0x%"BFD_VMA_FMT"x not 0x0 in output section\n",
                         MRK3_PROPERTY_RECORD_SECTION_NAME, first->output_offset);
                  break;
                }
            }
          else
            {
              if (!elf64_mrk3_update_records_section (first->owner, first,
                                                      i->owner, i,
                                                      &link_info))
                {
                  einfo ("%X%P: Failed to update %s section from '%s'\n",
                         MRK3_PROPERTY_RECORD_SECTION_NAME, i->owner->filename);
                  break;
                }
              need_layout = TRUE;
            }
        }

       /* Re-layout the sections to account for changes to the records
          sections.  */
       if (need_layout)
         gld${EMULATION_NAME}_map_segments (need_layout);
    }
}

EOF

LDEMUL_FINISH=gld${EMULATION_NAME}_finish
LDEMUL_AFTER_PARSE=gld${EMULATION_NAME}_after_parse
LDEMUL_AFTER_ALLOCATION=mrk3_after_allocation
