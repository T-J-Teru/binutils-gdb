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

EOF

LDEMUL_FINISH=gld${EMULATION_NAME}_finish
