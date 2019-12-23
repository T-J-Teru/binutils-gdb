/* Copyright (C) 2019 Free Software Foundation, Inc.

   This file is part of GDB.

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

#include <cstdio>

#include "overlay.h"
#include "gdbsupport/errors.h"

/* The one registered overlay manager.  */

std::unique_ptr<gdb_overlay_manager> registered_overlay_manager = nullptr;

/* See overlay.h.  */

std::string
overlay_manager_event_symbol_name ()
{
  if (registered_overlay_manager != nullptr)
    return registered_overlay_manager->event_symbol_name ();

  /* The symbol name we return here is the historical default.  Maybe in
     the future this should return an empty string meaning no overlay
     debugging supported, and we should force all users to provide an
     overlay manager extension - and possibly GDB should ship with a
     default that closely matches the existing default behaviour.  */
  return "_ovly_debug_event";
}

/* See overlay.h.  */

void
overlay_manager_register (std::unique_ptr <gdb_overlay_manager> mgr)
{
  if (registered_overlay_manager != nullptr)
    {
      /* Remove all overlay event breakpoints.  The new overlay manager
	 might place them in a different location.  The overlay event
	 breakpoints will be created automatically for us the next time we
	 try to resume the inferior.  */
      delete_overlay_event_breakpoint ();
    }

  registered_overlay_manager = std::move (mgr);

  /* Discard all cached overlay state.  */

  /* Finally, ask the new overlay manager to read its internal state and
     populate our internal data structure.  */
}

/* See overlay.h.  */

void
overlay_manager_hit_event_breakpoint (void)
{
  gdb_assert (registered_overlay_manager != nullptr);

  /* If the overlay manager doesn't want us to reload the overlay state
     when we hit the event breakpoint, then we're done.  */
  if (!registered_overlay_manager->reload_at_event_breakpoint ())
    return;

  std::unique_ptr<std::vector<gdb_overlay_manager::mapping>> mappings
    = registered_overlay_manager->read_mappings ();
}

void
_initialize_overlay (void)
{
}


