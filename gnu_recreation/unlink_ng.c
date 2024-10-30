/**
 *  file: unlink_ng.c
 *  created on: 30 Oct 2024
 *
 *  unlink new generation
 *  call the unlink function to remove the specified file(s)
 *
 *  Usage:
 *    unlink [FILE]...
 *
 *  Compilation:
 *    cc -ggdb -O3 -Wall -Wextra -Werror \
 *       -o unlink unlink_ng.c
 **/
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define PROGNAME "unlink"

#define OPTCMP(s1, s2) \
  (s1 != NULL && s2 != NULL && 0 == strcmp (s1, s2))

#undef errorf
#define errorf(format, ...) \
  fprintf (stderr, PROGNAME": "format"\n", ##__VA_ARGS__)

#define EXIT_FAILURE 1

int
main (int argc, char **argv)
{
  int EoO = 0; /* end of options */
  int ARGC = argc;
  char **ARGV = argv;
  /* parsing options */
  for (--argc, ++argv; argc != 0; --argc, ++argv)
    {
      if (EoO)
        break;
      switch (**argv)
        {
        case '-':
          if (OPTCMP (*argv, "--"))
            {
              EoO = 1;
              break;
            }
          /* options */
          else if (OPTCMP (*argv, "--version"))
            {
              errorf ("non-standard unlink program");
              return 0;
            }
          else if (OPTCMP (*argv, "--help"))
            {
              errorf ("call the unlink function to remove the specified file\n"
                      "usage: unlink [FILE]...");
              return 0;
            }
          else
            {
              errorf ("unrecognized option '%s'", *argv);
              return EXIT_FAILURE;
            }
          break;
        }
    }

  argc = ARGC;
  argv = ARGV;
  EoO = 0;
  /* actual unlink */
  for (--argc, ++argv; argc != 0; --argc, ++argv)
    {
      if (OPTCMP (*argv, "--"))
        {
          EoO = 1;
          continue;
        }

      if (!EoO && **argv == '-')
        continue;

      {
        if (unlink (*argv))
          {
            errorf ("cannot unlink '%s': %s", *argv, strerror (errno));
          }
      }
    }

  return 0;
}
