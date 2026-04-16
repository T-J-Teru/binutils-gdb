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

#include <unistd.h>
#include <pthread.h>

/* Set to 1 by the secondary thread once it has started.  */
volatile int thread_started = 0;

#ifdef IS_INFERIOR_2

/* Only present in the inferior 2 binary.  The secondary thread calls
   this in a loop, providing a breakpoint target that will only be hit
   by the secondary thread of inferior 2.  */

void
breakpt_func (void)
{
  /* Nothing.  */
}

#endif

void *
thread_func (void *arg)
{
  thread_started = 1;

  while (1)
    {
#ifdef IS_INFERIOR_2
      breakpt_func ();
#endif
      usleep (1000);
    }

  return NULL;
}

void
all_threads_started (void)
{
  /* GDB sets a breakpoint here to synchronize once both
     threads are running.  */
}

int
main (void)
{
  pthread_t thr;

  alarm (300);
  pthread_create (&thr, NULL, thread_func, NULL);

  while (!thread_started)
    usleep (1000);

  all_threads_started ();

  pthread_join (thr, NULL);
  return 0;
}
