#include "tinystd.h"

int
assert (bool condition)
{
  printf ("%s.\n",
          (condition) ? "passed" : "failed");
  return condition;
}

int
main (void)
{
  int passed = 1;
  const char *TEST;
  static char tmp[128];
  printf ("%s: Mem set/cmp/cpy test.\n", __FILE__);

#define LOG(ptr) printf ("%s = '%s' \t ", #ptr, ptr)
#define ASSERT(condition) do {                  \
    LOG (tmp);                                  \
    passed &= assert (condition);               \
  } while (0)
  
  
  /* Test 1 */
  printf ("test 1: ");
  {
    memset (tmp, '*', 64);
    memset (tmp + 2, '-', 16);
    tmp[18]=0;

    TEST = "**----------------";
    ASSERT (memcmp (tmp, TEST, 19) == 0);
  }

  /* Test 2 */
  printf ("test 2: ");
  {
    const char *hex = "0123456789abcdef";
    memset (tmp, 0, 64);
    char *p = mempcpy (tmp, hex + 10, 6);
    memcpy (p, hex, 10);

    TEST = "abcdef0123456789";
    ASSERT (memcmp (tmp, TEST, 17) == 0);
  }

  if (passed)
    {
      puts ("All tests passed.");
      return EXIT_SUCCESS;
    }
  return EXIT_FAILURE;
}
