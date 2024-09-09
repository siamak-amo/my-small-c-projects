/**
 *  file: tokenizeIt.c
 *  created on: 8 Sep 2024
 *
 *  Text Tokenizer
 *
 *  Usage:
 *    echo 'input' | ./tokenizeIt
 *
 *    input:
 *      'token.token2-6=xyz====token__H\t\n\t  test'
 *    output:
 *      token, token2-6, xyz, token__H, test  (on each line)
 *
 *    input:
 *      'func abc(int param) {return 'xyz'  +  \"666\" AAA}'
 *    output:
 *      func, abc, int, param, return, AAA
 *
 *
 *  Options:
 *    -a        to grab all tokens including strings
 *              wrapped by `'` and `"`
 *    -o        output file path
 *
 *
 *  Compilation:
 *    cc -ggdb -O3 -Wall -Wextra -Werror \
 *       -D_USE_BIO -I /path/to/buffered_io.h \
 *       -o tokenizeIt tokenizeIt.c
 **/
#include <stdio.h>
#include <unistd.h>

#ifdef _USE_BIO
#  ifndef _BMAX
#    define _BMAX (sysconf (_SC_PAGESIZE) / 4)
#  endif
#  include <stdlib.h>
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif /* _USE_BIO */

struct Opt {
  int dont_skip; /* include stuff between `'` and `"` */
  int out; /* output file descriptor */
};

enum Grab_State {
  GS_Grabbing,  /* to include in the output */
  GS_EO_TOKEN,  /* end of token */
  GS_EOL,       /* end of line == no newline is needed */
  GS_Skipping,  /* between `"`, `'` */
};
enum Grab_State st = GS_Grabbing;
#define GOTO(state) st = state

/* is alphanumeric */
#define IS_ALPHANUM(x) (('a' <= x && 'z' >= x) || \
                        ('A' <= x && 'Z' >= x) || \
                        ('0' <= x && '9' >= x))

/* token: X-Y_Z for alphanumeric values X,Y,Z */
#define IS_TOKEN(x) (IS_ALPHANUM(x) || \
                     (x == '-') || (x == '_'))


int
mk_tokens (const struct Opt *opt)
{
#ifdef _USE_BIO
  int cap = _BMAX;
  BIO_t bio = bio_new (cap, malloc (cap), opt->out);
# define writec(c) bio_putc (&bio, c)
# define writeln() writec ('\n')
#else
# define writec(c) write (opt->out, &(c), 1)
# define writeln() write (opt->out, "\n", 1)
#endif

  int skipper_char = 0;
  char c;
  while (read (0, &c, 1) > 0)
    {
      if (IS_TOKEN (c))
        {
          if (st != GS_Skipping)
            GOTO (GS_Grabbing);
        }
      else
        {
          if ((c == '\'' || c == '"') && opt->dont_skip == 0)
            {
              if (st == GS_Skipping)
                {
                  if (skipper_char == c)
                    GOTO (GS_EOL);
                  continue;
                }
              else
                {
                  GOTO (GS_Skipping);
                  skipper_char = c;
                }
            }
          else
            {
              if (st == GS_Grabbing)
                GOTO (GS_EO_TOKEN);
            }
        }

      switch (st)
        {
        case GS_Grabbing:
          writec (c);
          break;

        case GS_EO_TOKEN:          
          writeln ();
          GOTO (GS_EOL);
          break;

        default:
          break;
        }
    }
  
#ifdef _USE_BIO
  bio_flush (&bio);
  free (bio.buffer);
#endif

  return 0;
}

int
main (int argc, char **argv)
{
  struct Opt opt = {
    .dont_skip = 0,
    .out = fileno (stdout),
  };

#define __next(n) {argc -= n; argv += n;}
  for (--argc, ++argv; argc > 0; --argc, ++argv)
    {
      if (argv[0][0] == '-') /* option */
        {
          switch (argv[0][1])
            {
            case 'o':
            case 'O':
              if (argc < 2)
                {
                  fprintf (stderr, "`-o` needs an argument\n");
                  return 1;
                }
              FILE *__f = fopen (argv[1], "w");
              puts(argv[1]);
              if (__f)
                opt.out = fileno (__f);
              else
                return 1;
              __next (1);
              break;

            case 'a':
            case 'A':
              opt.dont_skip = 1;
              break;

            default:
              fprintf (stderr, "unknown option `%s`\n", argv[0]);
              return 1;
            }
        }
    }

  /* main logic */
  mk_tokens (&opt);
  /* closing the output fd */
  if (0 != close (opt.out))
    return 1;
  
  return 0;
}
