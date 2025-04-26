#include "tinystd.h"

#ifndef ENV_NAME
# define ENV_NAME "EX1"
#endif

int
main (void)
{
  printf ("%s: Env test.\n", __FILE__);
  char *env = getenv (ENV_NAME);

  if (env)
    {
      printf ("%s --> `%s`\n", ENV_NAME, env);
    }
  else
    {
      printf ("%s is not provided\n", ENV_NAME);
    }

  return EXIT_SUCCESS;
}
