/** file: base64.c
    created on: 4 Nov 2024
  
    Base64 utility
  
    Usage:
      base64 [OPTIONS]... [FILE]
    Options:
      -d, --decode       to decode
      -l, --line         to read line by line, and
                         possess each line separately
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
enum IO_Mode
  {
    CIN_COUT = 0,  /* char in char out */
    LIN_LOUT       /* line in line out */
  };

static enum Mode mode = ENCODE_MODE;
static enum IO_Mode ioMode = CIN_COUT;

FILE *inf, *outf;

#define OPTCMP(s1, s2) \
  (s1 != NULL && s2 != NULL && 0 == strcmp (s1, s2))
#define MIN(a,b) ((a<b) ? a : b)
#define MAX(a,b) ((a>b) ? a : b)

int
main (int argc, char **argv)
{
  int EoO = 0;
  set_program_name (*argv);
  inf = stdin, outf = stdout;

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
              else if (OPTCMP (*argv, "--line"))
                ioMode = LIN_LOUT;
              else if (OPTCMP (*argv, "--version"))
                goto VERSION_OPT;
              else
                goto INVALID_OPT;
              break;

            case 'd':
              mode = DECODE_MODE;
              break;
            case 'l':
              ioMode = LIN_LOUT;
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
          if (inf == stdin)
            {
              FILE *f = fopen (*argv, "r");
              if (!f)
                warnln ("%s: %s", *argv, strerror (errno));
              else
                inf = f;
            }
          else
            {
              warnln ("extra operand '%s'", *argv);
            }
        }
    }

  int err = 0, rw=0;
  char *line = NULL; size_t line_cap = 0;
#define TMP_CAP (MAX (B64_ENCODE_B, B64_DECODE_B) + 1)
  char tmp[TMP_CAP];
  { /* main logic */  
    switch (mode)
      {
      case ENCODE_MODE:
        if (CIN_COUT == ioMode)
          {
            b64_stream_encode (fileno (inf), fileno (outf), &err);
            fprintf (outf, "\n");
          }
        if (LIN_LOUT == ioMode)
          {
            while ((rw = getline (&line, &line_cap, inf) - 1) >= 0)
              {
                for (char *line_ptr = line; rw > 0;
                     rw -= B64_DECODE_B, line_ptr += B64_DECODE_B)
                  {
                    int input_len = MIN(rw, B64_DECODE_B);
                    int n = b64_encode (line_ptr, input_len, tmp, TMP_CAP, &err);
                    if (n > 0)
                      fprintf (outf, "%.*s", n, tmp);
                    else
                      break;
                  }
                fprintf (outf, "\n");
              }
          }
        break;

      case DECODE_MODE:
        if (CIN_COUT == ioMode)
          {
            b64_stream_decode (fileno (inf), fileno (outf), &err);
          }
        if (LIN_LOUT == ioMode)
          {
             while ((rw = getline (&line, &line_cap, inf) - 1) >= 0)
              {
                for (char *line_ptr = line; rw > 0;
                     rw -= B64_ENCODE_B, line_ptr += B64_ENCODE_B)
                  {
                    int input_len = MIN(rw, B64_ENCODE_B);
                    int n = b64_decode (line_ptr, input_len, tmp, TMP_CAP, &err);
                    if (n > 0)
                      fprintf (outf, "%.*s", n, tmp);
                    else
                      break;
                  }
                fprintf (outf, "\n");
              }
          }
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
  if (inf && inf != stdin)
    fclose (inf);
  if (outf && outf != stdout)
    fclose (outf);
  return 0;
}
