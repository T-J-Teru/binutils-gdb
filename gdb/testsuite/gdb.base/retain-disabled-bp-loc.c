/* Copyright 2024 Free Software Foundation, Inc.

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

#include <dlfcn.h>
#include <stdlib.h>

static void
foo ()
{
  /* Nothing.  */
}

void
breakpt ()
{
  /* Nothing.  */
}

int
main (void)
{
  void *handle;
  void (*func)(int);

  breakpt ();
  breakpt ();		/* Breakpoint 1.  */

  /* Load the first shared library.  */
  handle = dlopen (SHLIB_1_NAME, RTLD_LAZY);
  if (handle == NULL)
    abort ();

  breakpt ();		/* Breakpoint 2.  */

  /* Unload the shared library.  */
  if (dlclose (handle) != 0)
    abort ();

  breakpt ();		/* Breakpoint 3.  */

  /* Load the second shared library.  */
  handle = dlopen (SHLIB_2_NAME, RTLD_LAZY);
  if (handle == NULL)
    abort ();

  breakpt ();		/* Breakpoint 4.  */

  /* Unload the shared library.  */
  if (dlclose (handle) != 0)
    abort ();

  breakpt ();		/* Breakpoint 5.  */

  /* Load the first shared library for a second time.  */
  handle = dlopen (SHLIB_1_NAME, RTLD_LAZY);
  if (handle == NULL)
    abort ();

  breakpt ();		/* Breakpoint 6.  */

  /* Unload the shared library.  */
  if (dlclose (handle) != 0)
    abort ();

  breakpt ();		/* Breakpoint 7.  */

  foo ();

  return 0;
}
