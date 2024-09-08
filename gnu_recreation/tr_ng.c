/**
 *  file: tr_ng.c
 *  created on: 7 Sep 2024
 *
 *  Tr command (new generation!)
 *  Translate or delete characters
 *
 *  Usage:
 *    ./tr char1 char2 ...
 *
 *    Example:
 *      # to map a to b  and  A to B
 *      ./tr 'a' 'b' 'A' 'B'
 *
 *      # to map '\n' to ','  and  remove any tab '\t' character
 *      ./tr '\n' ',' '\t' '\0'
 *
 *      # using hex values to map B to C
 *      ./tr '\x42' 'C'
 *      ./tr '\x42' '\x43'
 *
 *    Interpreted sequences:
 *      \a, \b, \t, \n, \v, \f, \r
 *              see ASCII table for more details
 *
 *      \xNN    character with hex value 0xNN (1 to 2 digits)
 *              in range 0 to 255=0xFF
 *
 *  Compilation:
 *    replace `-I../` with path to `buffered_io.h` file:
 *      cc -ggdb -O3 -Wall -Wextra -Werror \
 *         -D_USE_BIO -I../ \
 *         -o tr tr_ng.c
 *    or remove `-D_USE_BIO` to compile without buffered_io
 *
 **/
#include <unistd.h>

/**
 *  using buffered_io.h for better performance
 *  this file is available in my_small_c_projects repository
 */
#ifdef _USE_BIO
/* we define max buffer size = page_size / 4 */
#  ifndef _BMAX
#    define _BMAX (sysconf (_SC_PAGESIZE) / 4)
#  endif
#  include <stdlib.h>
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif

#define CHAR_S 256
static char ASCII_table[CHAR_S] = {
  '\0', '\x01', '\02', '\03', '\x04', '\x05', '\x06',
  '\a', '\b', '\t', '\n', '\v', '\f', '\r',
  '\x0E', '\x0F', '\x10', '\x11', '\x12', '\x13', '\x14', '\x15', '\x16',
  '\x17', '\x18', '\x19', '\x1A', '\x1B', '\x1C', '\x1D', '\x1E', '\x1F',
  ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  ':', ';', '<', '=', '>', '?', '@',
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
  '[', '\\', ']', '^', '_', '`',
  'a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',
  '{', '|', '}', '~',
  '\x7F' /* End of ASCII table */
};

#define IS_HEX(x) (('0' <= (x) && '9' >= (x)) ||        \
                   ('a' <= (x) && 'f' >= (x)) ||        \
                   ('A' <= (x) && 'F' >= (x)))
/* @x MUST be in one of: [0-9] [a-f] [A-F] */
#define HEX2NUM(x) ((x <= '9') ? (x - '0') :            \
                    (x <= 'F') ? (x - 'A' + 0x0A) :     \
                    (x <= 'f') ? (x - 'a' + 0x0a) : 0)

int
norm_scape (const char *str)
{  
  if (*str != '\\' && str[1] == '\0')
    return *str;

  switch (str[1])
    {
    case '0': return '\0';
    case 'a': return '\a';
    case 'b': return '\b';
    case 't': return '\t';
    case 'n': return '\n';
    case 'v': return '\v';
    case 'f': return '\f';
    case 'r': return '\r';

    case 'x':
      /* \xYY where YY in {00, ..., FF} */
      if (str[2] == 0 || str[3] == 0)
        return 0;
      if (IS_HEX (str[2]) && IS_HEX (str[3]))
        return HEX2NUM (str[2]) * 16 + HEX2NUM (str[3]);
    }

  return 0;
}

int
parse_param (int argc, char **argv)
{
#define __next(n) argc -= n, argv += n

  for (__next(1); argc > 0; __next(2))
    {
      if (argc <= 1)
        break;

      int from = norm_scape (argv[0]);
      int to = norm_scape (argv[1]);

      if (from < 0x7F)
        {
          ASCII_table[from] = to;
        }
    }
  return 0;
}

int
main (int argc, char **argv)
{
#ifdef _USE_BIO
  int cap = _BMAX;
  BIO_t bio = bio_new (cap, malloc (cap), 1);
#  define putc_H(c) bio_putc (&bio, c)
#else
#  define putc_H(c) write (1, &c, 1)
#endif

#define tr_putc(c) if (c > 0x7F) {              \
    putc_H (c);                                 \
  } else {                                      \
    char to_write = ASCII_table[c];             \
    if (c != 0 && to_write == 0) continue;      \
    putc_H (to_write);                          \
  }

  int c = 1;
  /* updates the ASCII_table */
  parse_param (argc, argv);

  if (!isatty (0))
    {
      while (read (0, &c, 1) > 0)
        {
          tr_putc (c);
        }
    }
  else
    {
      while (c != 0 && read (0, &c, 1) == 1)
        {
          tr_putc (c);
        }
    }


#ifdef _USE_BIO
  bio_flush (&bio);
  free (bio.buffer);
#endif
  
  return 0;
}
