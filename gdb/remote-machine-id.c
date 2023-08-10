/* Copyright (C) 2023 Free Software Foundation, Inc.

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

#include "defs.h"
#include "remote-machine-id.h"

#include <string>
#include <vector>

/* List of all registered machine_id_validation objects.  */
static std::vector<std::unique_ptr<machine_id_validation>> validation_list;

/* See remote-machine-id.h.  */

void
register_machine_id_validation (std::unique_ptr<machine_id_validation> &&validation)
{
  validation_list.emplace_back (std::move (validation));
}

/* See remote-machine-id.  */

bool
validate_machine_id (const std::unordered_map<std::string, std::string> &kv_pairs)
{
  for (const auto &validator : validation_list)
    {
      const auto kv_master = kv_pairs.find (validator->master_key ());
      if (kv_master == kv_pairs.end ())
	continue;

      if (!validator->check_master_key (kv_master->second))
	continue;

      /* Check all the secondary keys in KV_PAIRS.  */
      bool match_failed = false;
      for (const auto &kv : kv_pairs)
	{
	  if (kv.first == validator->master_key ())
	    continue;

	  if (!validator->check_secondary_key (kv.first, kv.second))
	    {
	      match_failed = true;
	      break;
	    }
	}

      if (!match_failed)
	return true;
    }

  /* None of the machine_id_validation objects matched KV_PAIRS.  */
  return false;
}
