extern int foo (int);

#ifdef DUMP_CORE
static int dump_core_flag = 1;
#else
static int dump_core_flag = 0;
#endif

int
main (void)
{
  int result = foo (dump_core_flag);

  return result;
}
