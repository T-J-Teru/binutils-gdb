#include <stdio.h>
#include <stdlib.h>
#include <demangle.h>
#include <signal.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#define __USE_GNU
#include <dlfcn.h>

#define MAXLEN 253
#define ALPMIN 33
#define ALPMAX 127

/* The last symbol passed to the demangle function.  */
char symbol[2 + MAXLEN + 1] = "_Z";

/* The current active random seed.  */
unsigned int seed = 0;

/* Pointers to the real memory management routines.  */
void *(*real_malloc) (size_t) = NULL;
void (*real_free) (void *) = NULL;
void *(*real_realloc)(void *, size_t) = NULL;
void *(*real_calloc) (size_t, size_t) = NULL;

#define ALLOC_ARRAY_SIZE 100

struct allocation_record
{
  struct
  {
    void *p;
    size_t size;
    int in_use;
  } allocations [ALLOC_ARRAY_SIZE];

  unsigned int released_count;
  unsigned int used_count;
  struct allocation_record *next;
};

struct allocation_record *head = NULL;
unsigned int active_allocations = 0;

void report_failure (const char *reason)
{
  printf ("Failed: %s\n", reason);
  printf ("  Seed: %u\n", seed);
  printf ("Symbol: %s\n", symbol);
  exit (EXIT_FAILURE);
}

void
handle_sigsegv ()
{
  report_failure ("SIGSEGV");
}

void
init_funcs ()
{
  assert (real_malloc == NULL);
  assert (real_free == NULL);
  assert (real_realloc == NULL);
  assert (real_calloc == NULL);

  real_malloc = dlsym (RTLD_NEXT, "malloc");
  real_free = dlsym (RTLD_NEXT, "free");
  real_realloc = dlsym (RTLD_NEXT, "realloc");
  real_calloc = dlsym (RTLD_NEXT, "calloc");

  assert (real_malloc != NULL);
  assert (real_free != NULL);
  assert (real_realloc != NULL);
  assert (real_calloc != NULL);
}

void
record_allocation (void *ptr, size_t size)
{
  /* Find an allocation_record in which to place the result.  */
  struct allocation_record **tmp;

  tmp = &head;
  while (*tmp != NULL)
    {
      if ((*tmp)->used_count < ALLOC_ARRAY_SIZE)
        break;

      tmp = &((*tmp)->next);
    }

  if (*tmp == NULL)
    {
      /* Reached end of chain, allocate a new entry.  */
      (*tmp) = real_malloc (sizeof (struct allocation_record));
      memset (*tmp, 0, sizeof (struct allocation_record));
    }

  (*tmp)->allocations [(*tmp)->used_count].p = ptr;
  (*tmp)->allocations [(*tmp)->used_count].size = size;
  (*tmp)->allocations [(*tmp)->used_count].in_use = 1;
  (*tmp)->used_count++;
  active_allocations++;
}

void
release_allocation (void *ptr)
{
  struct allocation_record *tmp, **prev;

  prev = &head;
  tmp = head;
  while (tmp != NULL)
    {
      unsigned int i;

      for (i = 0; i < ALLOC_ARRAY_SIZE; ++i)
        {
          if (tmp->allocations [i].in_use
              && tmp->allocations [i].p == ptr)
            {
              active_allocations--;
              tmp->allocations [i].in_use = 0;
              ++tmp->released_count;
              goto clean_up;
            }
        }

      prev = &tmp->next;
      tmp = tmp->next;
    }

  return;

 clean_up:
  if (tmp->released_count == ALLOC_ARRAY_SIZE)
    {
      (*prev) = tmp->next;
      real_free (tmp);
    }
}

int
main (int argc, char *argv[])
{
  unsigned long long counter = 0;
  struct sigaction act;
  int reseed = 0;

  if (argc == 2)
    {
      seed = atoi (argv [1]);
      srand (seed);
    }
  else
    reseed = 1;

  init_funcs ();

  act.sa_handler = handle_sigsegv;
  sigemptyset (&act.sa_mask);
  act.sa_flags = 0;

  if (sigaction (SIGSEGV, &act, NULL) < 0)
    abort ();

  while (1)
    {
      char *buffer = symbol + 2;
      int length, i;
      char *result;
      unsigned int allocs;

      if (reseed && (counter % 1000000) == 0)
        {
          seed = time (NULL) + counter;
          srand (seed);
          fprintf (stderr, "Checked %8lld symbols, new seed = %8d\n",
                   counter, seed);
        }

      length = rand () % MAXLEN;
      for (i = 0; i < length; i++)
	*(buffer++) = (rand () % (ALPMAX - ALPMIN)) + ALPMIN;

      *(buffer++) = '\0';

      /* Set watermark on memory allocation.  */
      allocs = active_allocations;

      result = cplus_demangle (symbol, DMGL_AUTO | DMGL_ANSI | DMGL_PARAMS);
      free (result);

      /* Check that memory has been released.  */
      if (active_allocations != allocs)
        {
          fprintf (stderr, "active allocations = %d (not %d)\n",
                   active_allocations, allocs);
          report_failure ("Memory Leak");
        }

      counter++;
    }
}

void *
malloc (size_t size)
{
  void *res;
  res = real_malloc (size);
  record_allocation (res, size);
#ifdef DEBUG_MEM
  fprintf (stderr, "malloc (%d) = %p (aa = %d)\n",
           size, res, active_allocations);
#endif
  return res;
}

void
free (void *ptr)
{
  release_allocation (ptr);
#ifdef DEBUG_MEM
  if (ptr != NULL)
    fprintf (stderr, "free (%p) (aa = %d)\n", ptr, active_allocations);
#endif
  return real_free (ptr);
}

void *
realloc (void *ptr, size_t size)
{
  void *res;
  res = real_realloc (ptr, size);
  release_allocation (ptr);
  record_allocation (res, size);
#ifdef DEBUG_MEM
  fprintf (stderr, "realloc (%p, %d) = %p (aa = %d)\n",
           ptr, size, res, active_allocations);
#endif
  return res;
}

void *
calloc (size_t nmemb, size_t size)
{
  void *res;
  res = real_calloc (nmemb, size);
  record_allocation (res, size);
#ifdef DEBUG_MEM
  fprintf (stderr, "calloc (%d, %d) = %p (aa = %d)\n",
           nmemb, size, res, active_allocations);
#endif
  return res;
}
