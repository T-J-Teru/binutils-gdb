/* This testcase is part of GDB, the GNU debugger.

   Copyright 2022-2023 Free Software Foundation, Inc.

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
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

/* Number of threads doing forks.  */
#define N_FORKERS 2

static void
sleep_a_bit (void)
{
  usleep (1000 * 50);
}

static void
delay_loop (int limit)
{
  int i;

  for (i = 0; i < limit; ++i) /* for loop */
    {
      sleep_a_bit ();  /* break here */
      sleep_a_bit ();  /* other line */
    }
}


static void *
forker (void *arg)
{
  for (;;)
    {
      pid_t pid = FORK_FUNC ();

      if (pid == 0)
	{
	  //usleep (1000 * 20);
	  delay_loop (1);
	  _exit (11);
	}

      assert (pid > 0);

      /* Wait for children to exit.  */
      int ret;
      int stat;
      do
	{
	  ret = waitpid (pid, &stat, 0);
	} while (ret == -1 && errno == EINTR);

      assert (ret == pid);
      assert (WIFEXITED (stat));
      assert (WEXITSTATUS (stat) == 11);

      /* We need a sleep, otherwise the forking threads spam events and the
	 stepping thread doesn't make progress.  Sleep for a bit less than
	 `sleep_a_bit` does, so that forks are likely to interrupt a "next".  */
      usleep (40 * 1000);
    }

  return NULL;
}

int
main (void)
{
  int i;

  alarm (600);

  pthread_t thread[N_FORKERS];
  for (i = 0; i < N_FORKERS; ++i)
    {
      int ret = pthread_create (&thread[i], NULL, forker, NULL);
      assert (ret == 0);
    }

  delay_loop (INT_MAX);

  for (i = 0; i < N_FORKERS; ++i)
    pthread_join (thread[i], NULL);

  return 0;
}
