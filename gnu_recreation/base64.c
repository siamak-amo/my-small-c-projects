/** file: base64.c
    created on: 4 Nov 2024
  
    Base64 utility
  
    Usage:
      base64 [OPTIONS]... [FILE]
    Options:
      -d, --decode       to decode
          --version      print the version
  
    Compilation:
      cc -ggdb -O3 -Wall -Wextra -Werror \
         -I../libs -o base64 base64.c
 **/
#include <stdio.h>
#include <string.h>
#include <errno.h>

// libs/libbase64.h
#define B64_IMPLEMENTATION
#include "libbase64.h"

#define Version "1"
#define CLI_IMPLEMENTATION
#include "clistd.h"

enum Mode
  {
    DECODE_MODE,
    ENCODE_MODE
  };
static enum Mode mode = ENCODE_MODE;
static int infd = STDIN_FILENO; /* input file */
static int ofd = STDOUT_FILENO; /* output file */

#define OPTCMP(s1, s2) \
  (s1 != NULL && s2 != NULL && 0 == strcmp (s1, s2))

int
main (int argc, char **argv)
{
  int EoO = 0;
  set_program_name (*argv);

  for (--argc, ++argv; argc != 0; --argc, ++argv)
    {
      if (**argv == '-' && !EoO)
        {
          switch (argv[0][1])
            {
            case '-':
              if (argv[0][2] == 0)
                EoO = 1;
              else if (OPTCMP (*argv, "--decode"))
                mode = DECODE_MODE;
              else if (OPTCMP (*argv, "--version"))
                goto VERSION_OPT;
              else
                goto INVALID_OPT;
              break;

            case 'd':
              mode = DECODE_MODE;
              break;

            case 'v':
            VERSION_OPT:
              warnln ("non-standard base64 program v%s", Version);
              goto End_of_Main;

            default:
            INVALID_OPT:
              warnln ("invalid option '%c' was ignored", argv[0][1]);
            }
        }
      else
        {
          if (infd == STDIN_FILENO)
            {
              FILE *f = fopen (*argv, "r");
              if (!f)
                warnln ("%s: %s", *argv, strerror (errno));
              else
                infd = fileno (f);
            }
          else
            {
              warnln ("extra operand '%s'", *argv);
            }
        }
    }

  int err = 0;
  { /* main logic */  
    switch (mode)
      {
      case ENCODE_MODE:
        b64_stream_encode (infd, ofd, &err);
        write (ofd, "\n", 1);
        break;

      case DECODE_MODE:
        b64_stream_decode (infd, ofd, &err);
        break;
      }

    switch (err)
      {
      case 0:
        break;

      case INVALID_B64:
        warnln ("invalid input");
        break;

      case EOBUFFER_B64:
      default:
        warnln ("internal error");
        break;
      }
  }
  
 End_of_Main:
  if (infd != STDIN_FILENO)
    {
      close (infd);
    }
  return 0;
}
