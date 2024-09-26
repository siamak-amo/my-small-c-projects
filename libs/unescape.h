/**
 *  file: unescape.h
 *  created on: 26 Sep 2024
 *
 *  Interpretation of backslash escapes
 *  see `ascii` and `echo` man pages for more information
 *
 *  Features :
 *    backslash itself:
 *      '\\'
 *    elementary escapes:
 *      '\a', '\b', '\e', '\t', '\n', '\v', '\f', '\r'
 *    hex:
 *      '\xHH'  ->  0xHH   where: HH is a 2 hexadecimal value
 *                                from 00 to FF (or ff)
 *    octal:
 *      '\0NNN' ->  0oNNN  where: NNN is a 3 octal value
 *                                from 000 to 777
 *
 *     * invalid HH and NNN will be considered as 0 *
 *
 *
 *  Usage:
 *  ```{c}
 *    #define UNESCAPE_IMPLEMENTATION
 *    #include "unescape.h"
 *
 *    // when you need to interpret backslash:
 *    {
 *      // in-place with no extra memory
 *      unescape (buffer);
 *      // using the buffer
 *      ...
 *
 *      // with extra memory
 *      char dest[strlen (buffer)];
 *      unescape2 (dest, buffer);
 *
 *      // with duplicate or malloc
 *      char *dest = malloc (strlen (buffer));
 *      unescape2 (dest, buffer);
 *      // using dest buffer
 *      ...
 *      free (dest);
 *    }
 *  ```
 **/
#ifndef UNESCAPE__H__
#define UNESCAPE__H__

/* Hex */
#define hchr2num(HH)                         \
  (  (HH >= '0' && HH <= '9') ? (HH - '0')   \
   : (HH >= 'A' && HH <= 'F') ? (HH - 'A')   \
   : (HH >= 'a' && HH <= 'f') ? (HH - 'a')   \
   : 0  )
/* Octal */
#define nchr2num(NNN)                           \
  (  (NNN >= '0' && NNN <= '7') ? (NNN - '0')   \
     : 0  )

/**
 *  Macros to set value of @ptr equal to @val
 *  and move @ptr forward by 1 or @n
 *  *BE* careful with NULL character, you don't probably
 *  mean to increase @ptr when encounter '\0'
 */
#define __next_eq(ptr, val) *((ptr)++) = val
#define __next_eqn(ptr, val, n) *((ptr) += n) = val

/**
 *  Interprets the backslash `\` character(s)
 *  of the @buff in-place until Null character or error
 *
 *  @return:
 *    on success:  number of byte(s) written
 *    on failure:  -1 * number of bytes(s) written
 *                 ==> returns -1 * strlen of the output
 *
 *  Failure reasons:
 *    `\?` where `?` is not supported
 *    empty `\` with no following value
 *    invalid hex and octal like:
 *      `\xH` is invalid  --> we expect 2 characters \xHH
 *      `\0N` is invalid  --> we expect 3 characters \0NNN
 *    wrong hex and octal values like \xzz will be considered
 *    as `\0` character but NOT INVALID
 */
ssize_t unescape (char *buff);
/* The same as `unescape` */
ssize_t unescape2 (char *restrict dest, const char *restrict src);


/* implementation */
#ifdef UNESCAPE_IMPLEMENTATION
ssize_t
unescape2 (char *restrict dest, const char *restrict src)
{
  char *w_ptr = dest;
  const char *r_ptr = src;
  for ( ;; ++r_ptr)
    {
      if (*r_ptr == '\0')
        {
          *w_ptr = '\0';
          break;
        }

      if (*r_ptr != '\\')
        {
          __next_eq (w_ptr, *r_ptr);
          continue;
        }

      switch (*(++r_ptr))
        {
#define nextw_b(val) {                          \
            if (val) __next_eq (w_ptr, val);    \
            else *w_ptr = val;                  \
            break;                              \
          }
        case '\0':
          goto reterr; /* invalid string */

          /* case of `\\x` --> `\\` and `x` */
        case '\\': nextw_b ('\\');
          /* simple escapes */
        case 'a': nextw_b ('\a');
        case 'b': nextw_b ('\b');
        case 'e': nextw_b ('\e');
        case 't': nextw_b ('\t');
        case 'n': nextw_b ('\n');
        case 'v': nextw_b ('\v');
        case 'f': nextw_b ('\f');
        case 'r': nextw_b ('\r');
        case '\'': nextw_b ('\'');
        case '\"': nextw_b ('\"');
          /* advanced escapes */
        case 'x': /* HEX \xHH */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 16;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 1;

            nextw_b (x);
            break;
          }
        case '0': /* Octal \0NNN */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 8 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 1;

            nextw_b (x);
            break;
          }

        default:
          goto reterr;
        }
    }

  /* success return */
  return (ssize_t)(w_ptr - dest);
  /* failure return */
 reterr:
  *w_ptr = '\0';
  return (ssize_t)(dest - w_ptr);
}

ssize_t
unescape (char *buff)
{
  char *w_ptr = buff;
  char *r_ptr = buff;
  for ( ;; ++r_ptr)
    {
      if (*r_ptr == '\0')
        {
          *w_ptr = '\0';
          break;
        }

      if (*r_ptr != '\\')
        {
          __next_eq (w_ptr, *r_ptr);
          continue;
        }

      switch (*(++r_ptr))
        {
#define nextw_b(val) {                          \
            if (val) __next_eq (w_ptr, val);    \
            else *w_ptr = val;                  \
            break;                              \
          }
        case '\0':
          goto reterr; /* invalid string */

          /* case of `\\x` --> `\\` and `x` */
        case '\\': nextw_b ('\\');
          /* simple escapes */
        case 'a': nextw_b ('\a');
        case 'b': nextw_b ('\b');
        case 'e': nextw_b ('\e');
        case 't': nextw_b ('\t');
        case 'n': nextw_b ('\n');
        case 'v': nextw_b ('\v');
        case 'f': nextw_b ('\f');
        case 'r': nextw_b ('\r');
        case '\'': nextw_b ('\'');
        case '\"': nextw_b ('\"');
          /* advanced escapes */
        case 'x': /* HEX \xHH */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 16;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 1;

            nextw_b (x);
            break;
          }
        case '0': /* Octal \0NNN */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 8 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 1;

            nextw_b (x);
            break;

        default:
          goto reterr;
          }
        }
    }

  /* success return */
  return (ssize_t)(w_ptr - buff);
  /* failure return */
 reterr:
  *w_ptr = '\0';
  return (ssize_t)(buff - w_ptr);
}

#endif /* UNESCAPE_IMPLEMENTATION */
#endif /* UNESCAPE__H__ */
