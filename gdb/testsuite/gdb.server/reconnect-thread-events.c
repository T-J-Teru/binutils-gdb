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

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "gdb_watchdog.h"

volatile int ready = 0;

void
breakpt_before_thread (void)
{
}

void
breakpt_after_thread (void)
{
}

static void *
worker (void *arg)
{
  while (!ready)
    usleep (1000);
  return NULL;
}

int
main (void)
{
  pthread_t thr;

  gdb_watchdog (120);

  breakpt_before_thread ();

  if (pthread_create (&thr, NULL, worker, NULL) != 0)
    abort ();

  breakpt_after_thread ();

  ready = 1;
  pthread_join (thr, NULL);

  return 0;
}
