#include "tinystd.h"

int
main (int, char **argv)
{
  char c;
  printf ("%s: getc / printf test.\n", __FILE__);
  printf ("%s: Reading until EOF:\n", *argv);

  while ((c = getc ()) != 0)
    {
      if (c >= ' ')
        printf ("`%c` ", c);
      else
        println ("EOF");
    }

  return EXIT_SUCCESS;
}
