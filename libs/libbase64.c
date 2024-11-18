/**
 *  file: libbase64.d
 *  created on: 3 Nov 2024
 *
 *  Base64 implementation
 *  self test included
 *
 *
 *  Usage:
 *  ```c
 *    #define B64_IMPLEMENTATION
 *    #include "libbase64.c"
 *
 *    int
 *    main (void)
 *    {
 *      int err;
 *
 *      // encode
 *      char tmp[16];
 *      b64_encode ("test\n", 5, tmp, 16, &err);
 *      printf ("encoder: %s\n", tmp);
 *
 *      // decode
 *      char buf[16];
 *      b64_decode (tmp, strlen (tmp), buf, 16, &err);
 *      printf ("decoder: %s\n", buf);
 *
 *      return 0;
 *    }
 *  ```
 *
 *  Options:
 *    define `B64_NO_STREAM`: to not include stream
 *      encoder and decoder functions
 *    `EOBUFFER_B64` and `INVALID_B64`: to check errors
 *
 *  Compilation (self test program):
 *    cc -ggdb -Wall -Wextra -Werror \
 *       -D B64_IMPLEMENTATION \
 *       -D B64_TEST \
 *       -o test.out libbase64.c
 **/
#ifndef B64__H__
#define B64__H__

#ifndef B64_NO_STREAM
# include <unistd.h>
#endif

#ifndef B64DEFF
# define B64DEFF static inline
#endif

#define EOBUFFER_B64 -1 // end of buffer
#define INVALID_B64 -2 // invalid base64

/* encode table */
#define MAX_B64 64
static const char b64[MAX_B64] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";
#define B64_MASK 0x3F // max b64 index 63

/* decode table */
static const char _dec64[] = {
  /* [indexes: 43,...,47] `+` and `/` */
  62, 0xFF,0xFF,0xFF, 63,
  /* [indexes: 48,...,57] 0-9 */
  52,53,54,55,56,57,58,59,60,61,
  /* [indexes: 58,...,64] invalid zone */
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  /* [indexes: 65,...,90] A-Z */
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,
  13,14,15,16,17,18,19,20,21,22,23,24,25,
  /* [indexes: 91,...,96] invalid zone */
  0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
  /* [indexes: 97,...,122] */
  26,27,28,29,30,31,32,33,34,35,36,37,38,
  39,40,41,42,43,44,45,46,47,48,49,50,51,
};
#define dec64(n) \
  ((n < 43 || n > 122) ? 0xFF : _dec64[n-43])

/**
 *  base64 decoder and encoder
 *  returns number of bytes written on @dest (<= dstlen)
 */
B64DEFF int b64_encode (const void *restrict src_v, int srclen,
                void *restrict dst_v, int dstlen, int *error);
B64DEFF int b64_encode (const void *restrict src_v, int srclen,
                void *restrict dst_v, int dstlen, int *error);

/**
 *  stream decoder and encoder
 *  @ifd, @ofd: input and output file descriptors
 *  returns number of bytes written on @ofd
 */
#ifndef B64_NO_STREAM
B64DEFF int b64_stream_decode (int ifd, int ofd, int *error);
B64DEFF int b64_stream_encode (int ifd, int ofd, int *error);
#endif


#ifdef B64_IMPLEMENTATION

B64DEFF int
b64_encode (const void *restrict src_v, int srclen,
                  void *restrict dst_v, int dstlen,
            int *error)
{
  unsigned int n;
#define b64off(off) b64[ (n >> (6*off)) & B64_MASK ]
  int rw = 0;
  const unsigned char *src = src_v;
  unsigned char *dst = dst_v;

  *error = 0;
  while (srclen >= 3)
    {
      srclen -= 3;
      /* encode 3 bytes => result is 4 bytes */
      if (dstlen < 4)
        {
          *error = EOBUFFER_B64;
          return rw;
        }
      
      n = src[0]<<16 | src[1]<<8 | src[2]<<0;
      src += 3;
      *(dst++) = b64off (3);
      *(dst++) = b64off (2);
      *(dst++) = b64off (1);
      *(dst++) = b64off (0);
      dstlen -= 4;
      rw += 4;
    }

  /* handling =,== at the end */
  if (srclen >= 2)
    {
      if (dstlen < 4)
        {
          *error = EOBUFFER_B64;
          return rw;
        }
      
      n = src[0]<<16 | src[1]<<8;
      *(dst++) = b64off (3);
      *(dst++) = b64off (2);
      *(dst++) = b64off (1);
      *(dst++) = '=';
      rw += 4;
    }
  else if (srclen >= 1)
    {
      if (dstlen < 4)
        {
          *error = EOBUFFER_B64;
          return rw;
        }

      n = src[0]<<16;
      *(dst++) = b64off (3);
      *(dst++) = b64off (2);
      *(dst++) = '=';
      *(dst++) = '=';
      rw += 4;
    }

  *dst = 0;
  return rw;
}

B64DEFF int
b64_decode (const void *restrict src_v, int srclen,
                  void *restrict dst_v, int dstlen,
            int *error)
{
  int rw = 0;
#define Break(n) {*error = n; break;}
  unsigned int n, b;
  const unsigned char *src = src_v;
  unsigned char *dst = dst_v;

  for (; srclen > 0; srclen -= 4)
    {
      /* decode byte #1 */
      if (dstlen <= 0)
        Break (EOBUFFER_B64);
      if (*src == '=' || (n = dec64 (src[0])) == 0xFF)
        Break (INVALID_B64);
      b = (n << 2) & 0xFC; // first 6 bits
      src++;
      if (*src == '=' || (n = dec64 (src[0])) == 0xFF)
        Break (INVALID_B64);
      src++;
      b |= (n >> 4) & 0x03; // last 2 bits
      *(dst++) = b;
      rw++;
      dstlen--;

      /* decode byte #2 */
      if (dstlen <= 0)
        Break (EOBUFFER_B64);
      b = (n << 4) & 0xF0;
      if (*src == '=')
        Break (0);
      if ((n = dec64 (src[0])) == 0xFF)
        Break (INVALID_B64);
      src++;
      b |= (n >> 2) & 0x0F;
      *(dst++) = b;
      rw++;
      dstlen--;

      /* decode byte #3 */
      if (dstlen <= 0)
        Break (EOBUFFER_B64);
      b = (n << 6) & 0xC0; // 0b11000000
      if (*src == '=')
        Break (0);
      if ((n = dec64 (src[0])) == 0xFF)
        Break (INVALID_B64);
      src++;
      b |= n;
      *(dst++) = b;
      rw++;
      dstlen--;
    }

  *dst = 0;
  return rw;
}

#ifndef B64_NO_STREAM
int
b64_stream_decode (int ifd, int ofd, int *error)
{
  int r = 0, w = 0; // read and write bytes
  char in[4], tmp[3];

  while (read (ifd, in + r, 1) == 1)
    {
      if (r == 3 || in[r] == '\n')
        {
          int n = b64_decode (in, r, tmp, 3, error);
          if (*error)
            break;
          w += n;
          write (ofd, tmp, n);
          r = 0;
          continue;
        }
      else
        r++;
    }

  return w;
}

B64DEFF int
b64_stream_encode (int ifd, int ofd, int *error)
{
  int r, w = 0; // read & write bytes
  char in[3], tmp[4];

  while ((r = read (ifd, in, 3)) > 0)
    {
      int n = b64_encode (in, r, tmp, 4, error);
      if (*error)
        break;
      w += n;
      write (ofd, tmp, n);
    }
  return w;
}
#endif /* B64_NO_STREAM */

#endif /* B64_IMPLEMENTATION */
#endif /* B64__H__ */


/*
**  The self test program
**  Tests b64_encode and b64_decode functions
*/
#ifdef B64_TEST
#include <stdio.h>
#include <string.h>

struct TCASE
{
  const char *test;
  const char *exp;
};
#define Tcase(t, e) (struct TCASE){t, e}


static struct TCASE tests[] = {
  Tcase ("", ""),
  Tcase ("\n", "Cg=="),
  Tcase ("a", "YQ=="),
  Tcase ("aa", "YWE="),
  Tcase ("aaa", "YWFh"),
  Tcase ("abcd", "YWJjZA=="),
  Tcase ("wxyzt", "d3h5enQ="),

  {NULL, NULL}
};

#define DO_TEST(exp, got)                       \
  if (DO_CMP) {                                 \
    printf ("pass\n");                          \
  } else {                                      \
    printf ("failed\n"                          \
            "  Expected: `%s` != Got: `%s`\n",  \
            exp, got);                          \
  }

int
main (void)
{
  char tmp[256];
  int err;
  
  for (struct TCASE *t = tests; t->test != NULL; ++t)
    {
      /* testing encoder */
      printf ("testing: encode(`%s`)... ", t->test);
      b64_encode (t->test, strlen (t->test),
                  tmp, 256, &err);

#define DO_CMP (err == 0 && strcmp (t->exp, tmp) == 0)
      DO_TEST (t->exp, tmp);
#undef DO_CMP

      /* testing decoder */
      printf ("testing: decode(`%s`)... ", t->exp);
      b64_decode (t->exp, strlen (t->exp),
                  tmp, 256, &err);    

#define DO_CMP (err == 0 && strcmp (t->test, tmp) == 0)
      DO_TEST (t->test, tmp);
#undef DO_CMP

    }
  
  return 0;
}
#endif /* B64_TEST */
