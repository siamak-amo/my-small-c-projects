/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  unescape.h is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  unescape.h is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: unescape.h
    created on: 26 Sep 2024
  
    Interpretation of backslash escapes
    shell `echo` command compatible
    see `ascii` and `echo` manual pages for more information

    Also provides a minimal unescape URL encoding functionality

    Features :
      Backslash:
        '\\'
      Elementary escapes:
        '\a', '\b', '\e', '\t', '\n', '\v', '\f', '\r'
      Hex:
        '\xHH'  ->  0xHH   where: HH is 1 or 2 hexadecimal value
                                  from 00 to FF (or ff)
      Octal:
        '\0NNN' ->  0oNNN  where: NNN is 1 or 2 or 3 octal value
                                  from 000 to 777

      URL decoder:
        '%HH'  ->  0xHH   in URL encoded strings, like: '\xHH'


    Usage:
    ```c
      #define UNESCAPE_IMPLEMENTATION
      #include "unescape.h"
  
      // when you need to interpret backslash:
      {
        // in-place with no extra memory
        ssize_t new_len = unescape (buffer);
        // new_len is always <= length of buffer
        if (new_len < 0)
          {
            // error, invalid input
          }
        else
          {
            // using the buffer
          }
  
        // with duplicate or malloc
        char *dest = malloc (strlen (buffer));
        unescape2 (dest, buffer);
        // using dest buffer
        ...
        free (dest);
      }
    ```
 **/
#ifndef UNESCAPE__H__
#define UNESCAPE__H__

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
/**
 *  Equivalent to unescape, but in a different buffer
 *  Allocating @dest same size as @src is enough
 */
ssize_t unescape2 (char *restrict dest, const char *restrict src);

/**
 *  URL decoder, decodes C string at @buff in-place
 *  @return:  @buff
 **/
char *url_unescape (char *buff);
/* separate buffer */
char *url_unescape2 (char *restrict dst, const char *restrict src);


#ifdef UNESCAPE_IMPLEMENTATION

/* Internal helper functions */
// @return:  length of hex \xHH in @ptr after x
static inline int __scanhex (const char *restrict ptr,
                          unsigned char *restrict result);
// @return:  length of octal \0NNN in @ptr after 0
static inline int __scanoct (const char *restrict ptr,
                          unsigned char *restrict result);

/* internal macros */
#define __next_eq(ptr, val) *((ptr)++) = val
#define __next_eqn(ptr, val, n) *((ptr) += n) = val

#define nextw_b(val) {                                          \
    if (val) __next_eq (w_ptr, val);                            \
    else *w_ptr = val;                                          \
    break;                                                      \
  }

/* Hex */
#define HEXCHAR2NUM(HH)                                         \
  (  (HH >= '0' && HH <= '9') ? (HH - '0' + 0x00)               \
     : (HH >= 'A' && HH <= 'F') ? (HH - 'A' + 0x0A)             \
     : (HH >= 'a' && HH <= 'f') ? (HH - 'a' + 0x0a)             \
     : -1  )
/* Octal */
#define OCTCHAR2NUM(NNN)                                        \
  (  (NNN >= '0' && NNN <= '7') ? (NNN - '0')                   \
     : -1  )

#define __UNSCAPE__(w_ptr, r_ptr)                               \
  for ( ;; ++(r_ptr)) {                                         \
    if (*(r_ptr) == '\0') {                                     \
      *(w_ptr) = '\0';                                          \
      break;                                                    \
    }                                                           \
    if (*(r_ptr) != '\\') {                                     \
      __next_eq ((w_ptr), *(r_ptr));                            \
      continue;                                                 \
    }                                                           \
    switch (*(++(r_ptr))) {                                     \
    case '\0':                                                  \
      goto reterr; /* invalid string */                         \
      /* case of `\\x` --> `\\` and `x` */                      \
    case '\\': nextw_b ('\\');                                  \
      /* simple escapes */                                      \
    case 'a': nextw_b ('\a');                                   \
    case 'b': nextw_b ('\b');                                   \
    case 'e': nextw_b (0x1B);                                   \
    case 't': nextw_b ('\t');                                   \
    case 'n': nextw_b ('\n');                                   \
    case 'v': nextw_b ('\v');                                   \
    case 'f': nextw_b ('\f');                                   \
    case 'r': nextw_b ('\r');                                   \
    case '\'': nextw_b ('\'');                                  \
    case '\"': nextw_b ('\"');                                  \
      /* advanced escapes */                                    \
    case 'x': /* HEX \xHH */                                    \
      {                                                         \
        int hex_len;                                            \
        unsigned char x = 0;                                    \
        if ((hex_len = __scanhex ((r_ptr) + 1, &x)) != 0)       \
          {                                                     \
            (r_ptr) += hex_len;                                 \
            nextw_b (x);                                        \
          }                                                     \
        break;                                                  \
      }                                                         \
    case '0': /* Octal \0NNN */                                 \
      {                                                         \
        int oct_len;                                            \
        unsigned char x = 0;                                    \
        if ((oct_len = __scanoct ((r_ptr) + 1, &x)) != 0)       \
          {                                                     \
            (r_ptr) += oct_len;                                 \
            nextw_b (x);                                        \
          }                                                     \
        break;                                                  \
      }                                                         \
    default:                                                    \
      nextw_b (*r_ptr);                                         \
      break;                                                    \
    }                                                           \
  }

#define __URL_DECODE__(w_ptr, r_ptr)                            \
  for ( ;; ++(r_ptr)) {                                         \
    if (*(r_ptr) == '\0') {                                     \
      *(w_ptr) = '\0';                                          \
      break;                                                    \
    }                                                           \
    if (*(r_ptr) == '%') {                                      \
      unsigned char x;                                          \
      int hex_len = __scanhex ((r_ptr) + 1, &x);                \
      r_ptr += hex_len;                                         \
      __next_eq ((w_ptr), x);                                   \
    } else {                                                    \
      __next_eq ((w_ptr), *(r_ptr));                            \
    }                                                           \
  }

/**
 *  Internal functions to read hex and octal
 *  values from @ptr and store it into @result
 *  these functions return the strlen of the read number
 *  for `\xab`  ->  @ptr must be pointing to `a`
 */
static inline int
__scanhex (const char *restrict ptr,
           unsigned char *restrict result)
{
  int h1 = 0, h2 = 0;
  if (*ptr == '\0' || (h1 = HEXCHAR2NUM (*ptr)) == -1)
    {
      return 0;
    }
  ptr++;
  if (*ptr == '\0' || (h2 = HEXCHAR2NUM (*ptr)) == -1)
    {
      *result = h1;
      return 1;
    }
  *result = h1 * 16 + h2 * 1;
  return 2;
}

static inline int
__scanoct (const char *restrict ptr,
           unsigned char *restrict result)
{
  int n1 = 0, n2 = 0, n3 = 0;
  if (*ptr == '\0' || (n1 = OCTCHAR2NUM (*ptr)) == -1)
    {
      return 0;
    }
  ptr++;
  if (*ptr == '\0' || (n2 = OCTCHAR2NUM (*ptr)) == -1)
    {
      *result = n1;
      return 1;
    }
  ptr++;
  if (*ptr == '\0' || (n3 = OCTCHAR2NUM (*ptr)) == -1)
    {
      *result = n1 * 8 + n2 * 1;
      return 2;
    }
  *result = n1 * 8*8 + n2 * 8*1 + n1;
  return 3;
}

ssize_t
unescape2 (char *restrict dest, const char *restrict src)
{
  char *w_ptr = dest;
  const char *r_ptr = src;

  __UNSCAPE__ (w_ptr, r_ptr);

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

  __UNSCAPE__ (w_ptr, r_ptr);
  
  /* success return */
  return (ssize_t)(w_ptr - buff);
  /* failure return */
 reterr:
  *w_ptr = '\0';
  return (ssize_t)(buff - w_ptr);
}


char *
url_unescape (char *buff)
{
  char *__dst, *__res;
  __dst = __res = buff;
  __URL_DECODE__ (__res, buff);
  return __dst;
}

char *
url_unescape2 (char *restrict dst, const char *restrict src)
{
  char *__dst = dst;
  __URL_DECODE__ (dst, src);
  return __dst;
}

#endif /* UNESCAPE_IMPLEMENTATION */
#endif /* UNESCAPE__H__ */
