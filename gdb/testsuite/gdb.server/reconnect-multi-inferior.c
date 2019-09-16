#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_COUNT 10

void
breakpt (void)
{
  asm ("" ::: "memory");
}

void
do_child (void)
{
  breakpt ();
}

int
main ()
{
  int i;
  pid_t children[CHILD_COUNT];

  for (i = 0; i < CHILD_COUNT; ++i)
    {
      pid_t pid;

      children[i] = 0;
      pid = fork ();

      if (pid == -1)
        abort ();

      if (pid == 0)
        {
          do_child ();
          break;
        }

      children[i] = pid;
      breakpt ();
    }

  for (i = 0; i < CHILD_COUNT; ++i)
    {
      pid_t pid;
      int wstatus;

      pid = waitpid (children[i], &wstatus, 0);
      if (pid == -1 || pid != children[i])
        abort ();
      children[i] = 0;
    }

  return 0;
}
