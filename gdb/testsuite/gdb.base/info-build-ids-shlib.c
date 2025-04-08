#include <stdlib.h>

extern int foo (int);

int
foo (int dump_core_p)
{
  if (dump_core_p)
    abort ();

  return 0;
}
