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
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

   Test that GDB doesn't lose an event for a thread it didn't know
   about, until an event is reported for it.  */

#define _GNU_SOURCE
#include <sched.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/futex.h>
#include <sys/syscall.h>

/* Global synchronization variables.  */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int ready = 0;

#define STACK_SIZE 0x1000

/* Wrap futex syscall.  See 'man 2 futex'.  */

static int
futex (int *uaddr, int futex_op, int val, const struct timespec *timeout,
       int *uaddr2, int val3)
{
  return syscall (SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

/* Function to run in the raw clone thread.  This just spins until the
   integer flag pointed to by ARG is set to non-zero.  */

static int
clone_fn (void *arg)
{
  int *thread_exit = (int *) arg;	/* Break inside raw thread.  */
  *thread_exit = 0;

  while (*thread_exit == 0)
    ;
}

/* A variable into which the kernel will write the child thread-id.  This
   will be cleared when the thread exits.  This only gets used when
   USE_FUTEX is defined.  */

static int thread_tid;

/* Create a thread using a raw clone call.  */

void *
do_raw_clone ()
{
  unsigned char *stack;
  int res;
  int clone_pid;

  /* This flag is shared with the raw thread.  It is initially -1, then
     set to 0 in the raw thread to show that the thread has started up.
     This flag is then set to 1 by the main thread to indicate that the
     raw thread should exit.  Using a volatile for thread synchronisation
     is not great, but this avoids having to make library calls from the
     raw thread, which might trigger a need for TLS to be setup correctly
     on some targets.  */
  volatile int thread_exit = -1;

  stack = malloc (STACK_SIZE);
  assert (stack != NULL);	/* Break before raw thread created.  */

#ifdef USE_FUTEX
#define TID_FLAGS (CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID)
#else
#define TID_FLAGS (0)
#endif

#define CLONE_FLAGS (CLONE_THREAD | CLONE_SIGHAND | CLONE_VM	\
		     | CLONE_SETTLS | TID_FLAGS)

  clone_pid = clone (clone_fn, stack + STACK_SIZE, CLONE_FLAGS,
		     (void *) &thread_exit, &thread_tid,
		     NULL, &thread_tid);

  assert (clone_pid > 0);	/* Immediately after the clone.  */

  while (thread_exit != 0)
    usleep (1000);

  /* Trigger the clone thread to exit.  */
  thread_exit = 1;	/* Break after raw thread created.  */

#ifdef USE_FUTEX
  /* In this mode we rely on the futex to notify us when the thread has
     exited.  In a broken GDB we used to deadlock at this point as GDB
     would fail to restart the raw thread, and so the kernel would never
     wake this futex.  */
  while (thread_tid != 0)
    futex (&thread_tid, FUTEX_WAIT, clone_pid, NULL, NULL, 0);

  /* We do know that the raw thread has completed at this point, so we
     could free its stack.  We don't though, just to be consistent with the
     no futex path.  This doesn't really matter, this is just a small
     test.  */
#else
  /* There's no attempt to synchronise with the raw clone thread on this
     path.  In a broken GDB the raw thread might not be resumed, in which
     case this timeout will expire.  */
  sleep (2);

  /* We cannot be sure that the raw thread has finished by this point, so
     we cannot free the stack.  This isn't critical, this is just a small
     test case.  */
#endif

  return NULL;			/* Break after raw thread exits.  */
}

/* Something for our pthread thread to do.  This just blocks until the main
   thread releases it, at which point this thread will exit.  */

void *
worker_thread (void *arg)
{
  pthread_mutex_lock(&mutex);

  /* Let the main thread know we are live by clearing the ready flag.  */
  ready = 0;
  pthread_cond_signal(&cond);

  /* Now spin until the main thread sets this flag back to non-zero.  */
  while (ready == 0)
    {
      /* The thread will block here until main signals 'cond' */
      pthread_cond_wait (&cond, &mutex);
    }

  pthread_mutex_unlock (&mutex);

  return NULL;
}

int
main (void)
{
  pthread_t thread_id;
  volatile int flag = 0;
  int res;

  /* The pthread will set the READY flag back to zero.  */
  ready = 1;

  alarm (300);

  res = pthread_create (&thread_id, NULL, worker_thread, (void *)&flag);
  assert (res == 0);

  /* Wait for the pthread to set READY to zero.  */
  pthread_mutex_lock(&mutex);
  while (ready == 1)
    pthread_cond_wait (&cond, &mutex);
  pthread_mutex_unlock (&mutex);

  /* Create a thread using a raw clone call.  */
  do_raw_clone ();		/* Break before do_raw_clone.  */

  /* Unblock the pthread.  */
  pthread_mutex_lock (&mutex);
  ready = 1;
  pthread_cond_signal (&cond);
  pthread_mutex_unlock (&mutex);

  /* Wait for the worker to finish.  */
  pthread_join (thread_id, NULL);

  return 0;
}
