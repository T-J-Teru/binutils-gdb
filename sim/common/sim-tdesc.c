/* Copyright (C) 2018 Free Software Foundation, Inc.

This file is part of GDB, the GNU debugger.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "sim-main.h"
#include "sim-assert.h"

/* Return a string that is a GDB XML target description, or NULL if no
   target description is available.  Only return the part of the target
   description found in ANNEX, which will be "target.xml" for the
   top-level of the target description.  */

const char *
sim_read_target_description (SIM_DESC sd, const char *annex)
{
  SIM_CPU *cpu = STATE_CPU (sd, 0);

  SIM_ASSERT (STATE_MAGIC (sd) == SIM_MAGIC_NUMBER);
  SIM_ASSERT (annex != NULL);
  if (CPU_READ_TARGET_DESC (cpu) != NULL)
    return (* CPU_READ_TARGET_DESC (cpu)) (cpu, annex);
  else
    return NULL;
}

