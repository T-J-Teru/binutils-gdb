/* Copyright (C) 2023 Free Software Foundation, Inc.

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

#include "defs.h"
#include "sim/callback.h"
#include "sim/sim.h"

/* TODO: Will need to do something with this.  */
static host_callback gdb_callback;

/* TODO: This needs to be populated from actual command line arguments.  */
static char *sim_argv[] = { "-E", "little", NULL };

/* TODO: Take/process actual command line arguments.  */
int
main ()
{
  SIM_DESC sim_desc = NULL;

  sim_desc = sim_open (SIM_OPEN_DEBUG, &gdb_callback,
		       NULL, sim_argv);

  return 0;
}
