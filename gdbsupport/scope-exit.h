/* Copyright (C) 2019-2025 Free Software Foundation, Inc.

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

#ifndef GDBSUPPORT_SCOPE_EXIT_H
#define GDBSUPPORT_SCOPE_EXIT_H

#include <functional>
#include <type_traits>
#include "gdbsupport/preprocessor.h"

/* scope_exit is a general-purpose scope guard that calls its exit
   function at the end of the current scope.  A scope_exit may be
   canceled by calling the "release" method.  The API is modeled on
   P0052R5 - Generic Scope Guard and RAII Wrapper for the Standard
   Library, which is itself based on Andrej Alexandrescu's
   ScopeGuard/SCOPE_EXIT.

   There are two forms available:

   - The "make_scope_exit" form allows canceling the scope guard.  Use
     it like this:

     auto cleanup = make_scope_exit ( <function, function object, lambda> );
     ...
     cleanup.release (); // cancel

   - If you don't need to cancel the guard, you can use the SCOPE_EXIT
     macro, like this:

     SCOPE_EXIT
       {
	 // any code you like here.
       }

   See also forward_scope_exit.  */

template<typename EF>
class scope_exit
{
public:

  template<typename EFP,
	   typename = gdb::Requires<std::is_constructible<EF, EFP>>>
  scope_exit (EFP &&f)
    try : m_exit_function ((!std::is_lvalue_reference<EFP>::value
			    && std::is_nothrow_constructible<EF, EFP>::value)
			   ? std::move (f)
			   : f)
  {
  }
  catch (...)
    {
      /* "If the initialization of exit_function throws an exception,
	 calls f()."  */
      f ();
      /* "throws: Nothing, unless the initialization of exit_function
	 throws."  */
      throw;
    }

  template<typename EFP,
	   typename = gdb::Requires<std::is_constructible<EF, EFP>>>
  scope_exit (scope_exit &&rhs)
    noexcept (std::is_nothrow_move_constructible<EF>::value
	      || std::is_nothrow_copy_constructible<EF>::value)
    : m_exit_function (std::is_nothrow_constructible<EFP>::value
		       ? std::move (rhs)
		       : rhs)
  {
    rhs.release ();
  }

  ~scope_exit ()
  {
    if (!m_released)
      m_exit_function ();
  }

  DISABLE_COPY_AND_ASSIGN (scope_exit);
  void operator= (scope_exit &&) = delete;

  /* If this is called, then the wrapped function will not be called
     on destruction.  */
  void release () noexcept
  {
    m_released = true;
  }

private:
  /* The function to call on scope exit.  */
  EF m_exit_function;

  /* Only when false will m_exit_function be called from the class
     dtor.  */
  bool m_released = false;
};

template <typename EF>
scope_exit<typename std::decay<EF>::type>
make_scope_exit (EF &&f)
{
  return scope_exit<typename std::decay<EF>::type> (std::forward<EF> (f));
}

namespace detail
{

enum class scope_exit_lhs {};

template<typename EF>
scope_exit<typename std::decay<EF>::type>
operator+ (scope_exit_lhs, EF &&rhs)
{
  return scope_exit<typename std::decay<EF>::type> (std::forward<EF> (rhs));
}

}

/* Register a block of code to run on scope exit.  Note that the local
   context is captured by reference, which means you should be careful
   to avoid inadvertently changing a captured local's value before the
   scope exit runs.  */

#define SCOPE_EXIT \
  auto CONCAT(scope_exit_, __LINE__) = ::detail::scope_exit_lhs () + [&] ()

#endif /* GDBSUPPORT_SCOPE_EXIT_H */
