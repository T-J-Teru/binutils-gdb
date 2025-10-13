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

/* Calls finish_thread_state on scope exit, unless release() is called to
   disengage.  Don't use scope_exit_base here as that class introduces
   undefined behaviour; it calls a member function on a sub-class from the
   parent class destructor.  In this specific case, the m_target_ref member
   variable will have already been deleted by the time the parent class
   destructor is called.  */

struct scoped_finish_thread_state
{
  /* At the end of the enclosing scope, call finish_thread_state passing in
     TARG and PTID.  If TARG is not NULL then a reference to TARG is
     retained in order to prevent it being deleted.  */
  scoped_finish_thread_state (process_stratum_target *targ, ptid_t ptid)
    : m_ptid (ptid)
  {
    if (targ != nullptr)
      m_target_ref = target_ops_ref::new_reference (targ);
  }

  /* Call finish_thread_state unless 'release ()' has already been called.  */
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

  DISABLE_COPY_AND_ASSIGN (scoped_finish_thread_state);

  /* Signal that finish_thread_state should not be called in this case.  */
  void release () noexcept
  {
    m_released = true;
  }

private:

  /* The thread-id and target on which to call finish_thread_state.  */
  ptid_t m_ptid;
  target_ops_ref m_target_ref;

  /* Only when this flag is false will finish_thread_state will be called
     from the class destructor.  */
  bool m_released = false;
};
