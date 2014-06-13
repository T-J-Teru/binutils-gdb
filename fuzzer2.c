#include <stdio.h>
#include <stdlib.h>
#include <demangle.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

#define __USE_GNU
#include <dlfcn.h>

#define MAXLEN 253
#define ALPMIN 33
#define ALPMAX 127

int
main (int argc, char *argv[])
{
  char *result;
  const char *symbol = "_Z`E9yJ]}hkg5:68I}UZs5fCfOmc%oH(o)fD=2\"<y(>J[S{%QR^F\"`$hJ,fN6/p'QWdnh!E}Bbb|Qx##Kzbg[eP'qQn(__H18G:;ax9$v6\"H0$Jt}H\\t.G6:w%\\qd%>{fW7HkjLb;g+d&nZ$7Q3_4H4FMo920VHuHxX4";
  //const char *symbol = "_Z3XXXDFyuVb";
  
  result = cplus_demangle (symbol, DMGL_AUTO | DMGL_ANSI | DMGL_PARAMS);

  printf ("Result: %s\n", result);
  free (result);

  return 0;
}
