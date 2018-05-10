#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define THREADCOUNT 4

pthread_barrier_t barrier;
pthread_t threads[THREADCOUNT];
int thread_ids[THREADCOUNT];

/* Returns the argument back, within range [0..THREADCOUNT).  */
int
get_value (int index)
{
  return thread_ids[index];
}

unsigned long
fast_fib (unsigned int n)
{
  int a = 0;
  int b = 1;
  int t;

  for (unsigned int i = 0; i < n; ++i)
    {
      t = b;
      b = a + b;
      a = t;
    }
  return a;
}

void *
thread_function (void *args)
{
  unsigned long result;
  int tid = get_value (*((int *) args));
  int status;

  status = pthread_barrier_wait (&barrier);

  if (status == PTHREAD_BARRIER_SERIAL_THREAD)
    printf("All threads entering compute region\n");

  result = fast_fib (100); /* testmarker01 */

  status = pthread_barrier_wait (&barrier);

  if (status == PTHREAD_BARRIER_SERIAL_THREAD)
    printf("All threads outputting results\n");

  pthread_barrier_wait (&barrier);
  printf ("Thread %d Result: %lu\n", tid, result);
  pthread_exit (NULL);
}

int main(void)
{
  int err = pthread_barrier_init (&barrier, NULL, THREADCOUNT);

  /* Create worker threads (main).  */
  printf ("Spawining worker threads\n");
  for (int tid = 0; tid < THREADCOUNT; ++tid)
    {
      thread_ids[tid] = tid;
      err = pthread_create (&threads[tid], NULL,
			    thread_function,
			    (void *) &thread_ids[tid]);
      if (err)
	{
	  fprintf (stderr, "Thread creation failed\n");
	  return EXIT_FAILURE;
	}
    }

  /* Wait for threads to complete then exit.  */
  for (int tid = 0; tid < THREADCOUNT; ++tid)
    pthread_join (threads[tid], NULL);

  pthread_exit (NULL);
  return EXIT_SUCCESS;
}
