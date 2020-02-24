#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

volatile int loop_count = 10;
volatile int thread_count = 3;

static void
thread_level_5 (int id, int count)
{
  printf ("Thread %d reached %s, #%d\n",
	  id, __PRETTY_FUNCTION__, count);
}

static void
thread_level_4 (int id, int count)
{
  thread_level_5 (id, count);
}

static void
thread_level_3 (int id, int count)
{
  thread_level_4 (id, count);
}

static void
thread_level_2 (int id, int count)
{
  thread_level_3 (id, count);
}

static void
thread_level_1 (int id, int count)
{
  thread_level_2 (id, count);
}

static void *
thread_worker (void *arg)
{
  int i, max, id;

  id = *((int *) arg);
  max = loop_count;
  for (i = 0; i < max; ++i)
    thread_level_1 (id, (i + 1));

  return NULL;
}

struct thread_info
{
  pthread_t thread;
  int id;
};

int
main ()
{
  int i, max = thread_count;

  struct thread_info *info = malloc (sizeof (struct thread_info) * max);
  if (info == NULL)
    abort ();

  for (i = 0; i < max; ++i)
    {
      struct thread_info *thr = &info[i];
      thr->id = i + 1;
      if (pthread_create (&thr->thread, NULL, thread_worker, &thr->id) != 0)
	abort ();
    }

  for (i = 0; i < max; ++i)
    {
      struct thread_info *thr = &info[i];
      if (pthread_join (thr->thread, NULL) != 0)
	abort ();
    }

  free (info);
}
