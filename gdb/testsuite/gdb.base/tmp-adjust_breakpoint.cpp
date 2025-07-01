#include<stdio.h>

int foo () {
  return 0;
}

int foo (int var) {
    int i = 8;

    var += i;

    return var;
}

int
main (void)
{
  foo ();
  return foo(2);
}
