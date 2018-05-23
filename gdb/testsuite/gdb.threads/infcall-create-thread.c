/* This testcase is part of GDB, the GNU debugger.

   Copyright 2018 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

/* This defines the maximum number of threads to spawn.  */
#define THREADCOUNT 4

/* Define global thread identifiers.  */
static pthread_t threads[THREADCOUNT];

/* Next thread ID to fill in.  */
static int next_thread = 0;

/* Encapsulate the synchronization of the threads. Perform a barrier before
   and after the computation.  */
static void *
thread_function (void *args)
{
   while (1)
    sleep (5);
}

/* Create a new thread.  */
static void
spawn_thread ()
{
  int err;

  if (next_thread == THREADCOUNT)
    {
      fprintf (stderr, "Attempt to create too many threads.\n");
      exit (EXIT_FAILURE);
    }

  err = pthread_create (&threads[next_thread], NULL, thread_function, NULL);
  if (err != 0)
    {
      fprintf (stderr, "Thread creation failed\n");
      exit (EXIT_FAILURE);
    }

  ++next_thread;
}

/* Place a breakpoint on this function.  */
void
breakpt ()
{
  asm ("" ::: "memory");
}

int
main ()
{
  /* Create the worker threads.  */
  breakpt ();
  printf ("Spawning worker threads\n");
  for (int tid = 0; tid < 2 /*THREADCOUNT*/; ++tid)
    {
      spawn_thread ();
      breakpt ();
    }

  /* Wait for the threads to complete then exit.  Currently threads block
     for ever and this will never complete, but it serves to block the
     main thread.  */
  for (int tid = 0; tid < THREADCOUNT; ++tid)
    pthread_join (threads[tid], NULL);

  return EXIT_SUCCESS;
}
