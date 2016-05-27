#include <cstdio>

class MyClass
{
public:
  MyClass()
  {
    printf("this: %p\n", this); /* BP-1 */
  }

  void method()
  {
    printf("this: %p\n", this); /* BP-3 */
  }
};

int
main (void)
{
  MyClass myClass; 
  printf("&myClass: %p\n", &myClass); /* BP-2 */
  myClass.method();
  return 0;
}
