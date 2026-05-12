/* This testcase is part of GDB, the GNU debugger.

   Copyright 2026 Free Software Foundation, Inc.

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

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

volatile int global_var = 0;

void
breakpt ()
{
  global_var = 42;	/* Break here.  */
}

int
main ()
{
  pid_t pid;

  alarm (300);

  /* Create the child process.  */
  pid = fork ();
  assert (pid >= 0);

  if (pid == 0)
    {
      /* Child process.  */
      exit (0);
    }
  else
    {
      /* Parent process.  */

      /* Wait for the child process to finish.  */
      wait (NULL);

      while (global_var == 0)
	sleep (1);

      breakpt ();
    }

  return 0;
}
