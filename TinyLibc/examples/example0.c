#include "tinystd.h"

#ifdef NO_MAIN_TEST
# define MAIN dummy
#else
# define MAIN main
#endif

int
MAIN (int argc, char **argv)
{
  int i = 0;
  printf ("%s: Arg test.\n", __FILE__);

  if (argc < 2)
    {
      printf ("No argument!\n");
      return EXIT_SUCCESS;
    }

  argc--;
  argv++;
  printf ("Got %d argument%s",
          argc,
          (argc == 1) ? ":\n": "s:\n");

  for (; argc > 0; --argc, ++argv)
    {
      printf (" [%d]-> `%s`\n", ++i, *argv);
    }

  return EXIT_SUCCESS;
}
