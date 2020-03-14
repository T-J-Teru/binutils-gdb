/* This testcase is part of GDB, the GNU debugger.

   Copyright 2020 Free Software Foundation, Inc.

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
#include <signal.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

/* The number of threads to create.  */
volatile int thread_count = 3;

/* This is initialised with our pid. GDB will read and print this value
   from the Dejagnu test script, the test script will then use the pid to
   send signals to this process.  */
pid_t global_pid;

/* Holds one end of two different pipes.  Things written to READ will not
   appear on WRITE.  */
struct pipe_fds
{
  int read;
  int write;
};

/* Information passed into each thread.  */
struct thread_info
{
  /* Just a numeric id for the thread.  */
  int id;

  /* File handles with which the worker thread can communicate with the
     master thread.  */
  struct pipe_fds fds;
};

/* The control information held by the master thread, one of these for each
   worker thread.  */
struct thread_ctrl
{
  /* The actual pthread handle, used to join the threads.  */
  pthread_t thread;

  /* File handles with which the master thread can communicate with the
     worker threads.  */
  struct pipe_fds fds;

  /* The information that is passed into the worker thread.  */
  struct thread_info info;
};

/* Wait for a single byte of the read file handle in FDS.  */
static void
wait_on_byte (struct pipe_fds *fds)
{
  ssize_t rtn;
  char c;

  while ((rtn = read (fds->read, &c, 1)) != 1)
    {
      if (rtn != -1 || errno != EINTR)
	abort ();
    }
}

/* Send a single byte to the write file handle in FDS.  */
static void
send_byte (struct pipe_fds *fds)
{
  ssize_t rtn;
  char c = 'x';
  while ((rtn = write (fds->write, &c, 1)) != 1)
    {
      if (rtn != -1 || errno != EINTR)
	abort ();
    }
}

/* Create a function used to mark a breakpoint location.  */
#define BREAKPOINT_FUNC(N)				\
  static void						\
  breakpt_ ## N ()					\
  {							\
    printf ("Hit breakpt_" #N "\n");			\
  }

BREAKPOINT_FUNC (0)	/* breakpt_0 */
BREAKPOINT_FUNC (1)	/* breakpt_1 */
BREAKPOINT_FUNC (2)	/* breakpt_2 */

/* The worker thread entry point.  */
static void *
thread_worker (void *arg)
{
  struct thread_info *info = (struct thread_info *) arg;
  int id = info->id;

  printf ("Thread %d created.\n", id);
  breakpt_0 ();

  /* Let the main thread know that this thread is now running.  */
  send_byte (&info->fds);

  /* The thread with id #2 is special, it waits here for a nudge from the
     main thread.  */
  if (id == 2)
    {
      wait_on_byte (&info->fds);
      breakpt_2 ();
      send_byte (&info->fds);
    }

  /* Now wait for an incoming message indicating that the thread should
     exit.  */
  wait_on_byte (&info->fds);
  printf ("In thread %d, exiting...\n", id);
  return NULL;
}

/* Initialise CTRL for thread ID, this includes setting up all of the pipe
   file handles.  */
static void
thread_ctrl_init (struct thread_ctrl *ctrl, int id)
{
  int fds[2];

  ctrl->info.id = id;
  if (pipe (fds))
    abort ();
  ctrl->info.fds.read = fds[0];
  ctrl->fds.write = fds[1];

  if (pipe (fds))
    abort ();
  ctrl->fds.read = fds[0];
  ctrl->info.fds.write = fds[1];
}

/* Wait for a SIGUSR1 to arrive.  Assumes that SIGUSR1 is blocked on entry
   to this function.  */
static void
wait_for_sigusr1 (void)
{
  int signo;
  sigset_t set;

  sigemptyset (&set);
  sigaddset (&set, SIGUSR1);

  /* Wait for a SIGUSR1.  */
  if (sigwait (&set, &signo) != 0)
    abort ();
  if (signo != SIGUSR1)
    abort ();
}

/* Main program.  */
int
main ()
{
  sigset_t set;
  int i, max = thread_count;

  /* Set an alarm in case the testsuite crashes, don't leave the test
     running forever.  */
  alarm (300);

  struct thread_ctrl *info = malloc (sizeof (struct thread_ctrl) * max);
  if (info == NULL)
    abort ();

  /* Put the pid somewhere easy for GDB to read, also print it.  */
  global_pid = getpid ();
  printf ("pid = %lld\n", ((long long) global_pid));

  /* Block SIGUSR1, all threads will inherit this sigmask. */
  sigemptyset (&set);
  sigaddset (&set, SIGUSR1);
  if (pthread_sigmask (SIG_BLOCK, &set, NULL))
    abort ();

  /* Create each thread.  */
  for (i = 0; i < max; ++i)
    {
      struct thread_ctrl *thr = &info[i];
      thread_ctrl_init (thr, i + 1);

      if (pthread_create (&thr->thread, NULL, thread_worker, &thr->info) != 0)
	abort ();

      /* Wait for an indication that the thread has started, and is ready
	 for action.  */
      wait_on_byte (&thr->fds);
    }

  printf ("All threads created.\n");

  /* Give thread thread #1 a little nudge.  */
  if (max >= 2)
    {
      send_byte (&info[1].fds);
      wait_on_byte (&info[1].fds);
    }

  breakpt_1 ();

  /* For each thread in turn wait for a SIGUSR1 to arrive, signal the
     thread so that it will exit (by sending it a byte down its pipe), then
     join the newly exited thread.  */
  for (i = 0; i < max; ++i)
    {
      struct thread_ctrl *thr = &info[i];

      wait_for_sigusr1 ();

      printf ("Telling thread %d to exit\n", thr->info.id);
      send_byte (&thr->fds);

      if (pthread_join (thr->thread, NULL) != 0)
	abort ();

      printf ("Thread %d exited\n", thr->info.id);
    }

  free (info);

  /* Final wait before exiting.  */
  wait_for_sigusr1 ();

  return 0;
}
