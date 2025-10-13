/* Copyright (C) 2025 Free Software Foundation, Inc.

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

#include "gdbthread.h"
#include "target.h"

/* Calls finish_thread_state on scope exit, unless release() is called
   to disengage.  */

struct scoped_finish_thread_state
{
  scoped_finish_thread_state (process_stratum_target *targ, ptid_t ptid)
    : m_ptid (ptid)
  {
    if (targ != nullptr)
      m_target_ref = target_ops_ref::new_reference (targ);
  }

  ~scoped_finish_thread_state ()
  {
    if (!m_released)
      {
	target_ops *t = m_target_ref.get ();
	process_stratum_target *p_target
	  = gdb::checked_static_cast<process_stratum_target *> (t);
	finish_thread_state (p_target, m_ptid);
      }
  }

  void release () noexcept
  {
    m_released = true;
  }

private:

  ptid_t m_ptid;
  target_ops_ref m_target_ref;
  bool m_released = false;
};
