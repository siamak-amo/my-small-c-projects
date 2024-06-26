/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: leven.c
 *  created on: 3 Mar 2024
 *
 *  Levenshtein Distance Algorithm Implementation
 *
 *  implementation details:
 *    leven(s1[.n], s2[.m]):
 *      time: O(n*m)
 *      memory: O(Min(n,m))
 *  this implementation simply uses n for all memory allocations,
 *  so provide the smaller string first or use LEVEN_SMALLERx macros
 *
 *  compile the test program:
 *    cc -ggdb -Wall -Wextra -Werror \
 *       -D LEVEN_IMPLEMENTATION \
 *       -D LEVEN_TEST -o test leven.c
 *
 *  to include in c files:
 *    `
 *    #define LEVEN_IMPLEMENTATION
 *    #include "leven.c"
 *
 *    // immediate call
 *    size_t d = leven_imm (s1, s2);
 *
 *    // for small s1, use stack
 *    size_t d = leven_stk (s1, s2);
 *
 *    // when distance of s1 and s is needed
 *    // for fixed s1 and some s in a list
 *    LARR_t *_tmp = leven_alloc (s1);
 *    for (s in list)
 *      leven_H (s1, s, _tmp);
 *    leven_free (_tmp);
 *    `
 **/

#ifndef LEVEN__H__
#define LEVEN__H__

#include <stdlib.h>
#include <strings.h>

/* leven array type */
#define LARR_t unsigned int
#define MIN(a, b) (((a)<(b)) ? (a) : (b))
#define MIN3(a, b, c) MIN(MIN((a), (b)), (c))

#ifndef LEVENDEF
#define LEVENDEF static inline
#endif

/* macros to allocate and free temporary LARR_t buffer */
#define leven_free(ptr) free (ptr)
#define leven_alloc(s)                                  \
  (LARR_t*) malloc (leven_strlen (s) * sizeof(LARR_t*))

/* leven_charlen function equivalent macro */
#define leven_chrlen(chr)                       \
  ((((chr) & 0xF8) == 0xF0) ? 4 :               \
   (((chr) & 0xF0) == 0xE0) ? 3 :               \
   (((chr) & 0xE0) == 0xC0) ? 2 :               \
   (((chr) & 0x80) == 0x00) ? 1 : 0)

/**
 *  smaller string first wrapper macros
 *  example:
 *    LEVEN_SMALLER (leven_imm, str1, str2);
 *    LEVEN_SMALLER (leven_stk, str1, str2);
 *    LEVEN_SMALLER (leven_H, str1, str2, tmp_buffer);
 *
 *  SMALLER2 uses leven_strlen instead of strlen function
 *  use SMALLERF to pass string length calculator function
 **/
#define LEVEN_SMALLER(fun, s1, s2, ...)         \
  SMALLERF (fun, strlen, s1, s2, __VA_ARGS__)
#define LEVEN_SMALLER2(fun, s1, s2, ...)        \
  SMALLERF (fun, leven_strlen, s1, s2, __VA_ARGS__)

#define SMALLERF(fun, meter, s1, s2, ...)       \
  ({ int __sm = (meter (s1) < meter (s2));      \
    (__sm) ? fun (s1, s2, ##__VA_ARGS__)        \
      : fun (s2, s1, ##__VA_ARGS__);})

/**
 *  functions to calculate levenshtein distance @s1 and @s2
 *  @s1, @s1 might include some non-ascii characters
 *  @tmp:  temporary buffer
 *         use leven_alloc and leven_free macros
 */
LEVENDEF size_t
leven_imm (const char *restrict s1, const char *restrict s2);
/* use stack */
LEVENDEF size_t
leven_stk (const char *restrict s1, const char *restrict s2);
/* manually handle the memory */
LEVENDEF size_t
leven_H (const char *restrict s1, const char *restrict s2, LARR_t* tmp);

/**
 *  charlen function,
 *  @return:
 *    on success -> length of a UTF-8 character {1,2,3,4}
 *    on error   -> 0
 *  @chr:
 *    the first byte of a UTF-8 character
 **/
LEVENDEF int
leven_charlen (const char chr);

/**
 *  strlen function, returns length of the @s
 *  counts each non-ascii char once
 */
LEVENDEF size_t
leven_strlen (const char *s);
#endif

/* the implementation */
#ifdef LEVEN_IMPLEMENTATION

LEVENDEF int
leven_charlen (const char chr)
{
  unsigned char c = chr;

  c >>= 3;
  if (c == 0x1E)
    return 4;

  c >>= 1;
  if (c == 0x0E)
    return 3;

  c >>= 1;
  if (c == 0x06)
    return 2;

  c >>= 2;
  if (c == 0)
    return 1; /* ASCII character */

  return 0; /* error, invalid UTF-8 character */
}

LEVENDEF size_t
leven_strlen (const char *s)
{
  size_t len = 0, lc = 0;

  if (s == NULL)
    return 0;

  while (*s != '\0')
    {
      lc = leven_chrlen (*s);
      /* consider invalid character is one byte */
      if (0 == lc)
        lc = 1;
      len++;
      s += lc;
    }
  return len;
}

/**
 *  internal function
 *  compare chars at @s1 and @s2
 *  this function can handle existence of non-ascii characters
 */
static inline int
leven_charcmp (const char *restrict s1, const char *restrict s2)
{
  int c1;
  if ((c1 = leven_charlen (*s1)) == leven_charlen(*s2))
    {
      while (c1-- != 0)
        {
          if (*s1 != *s2)
            return -1;
          s1++;
          s2++;
        }
      return 0; /* equal */
    }
  else
    return -1;
}

/**
 *  internal function, always use leven_xxx functions
 *  the implementation of the levenshtein algorithm
 */
static inline size_t
calculate_leven__H (const char *restrict s1,
                    const char *restrict s2, size_t n, LARR_t *tmp)
{
  const char *p1, *p2;
  /* character length of s1 and s2 */
  int cl1 = 0, cl2 = 0;
  /* last diagonal value in the `imaginary` matrix */
  size_t diag = 0, prev_diag = 0;
  size_t x,y;

  for (y = 1; y <= n; ++y)
    tmp[y] = y;
  for (x = 1; x <= leven_strlen (s2); ++x)
    {
      tmp[0] = x;
      p1 = s1;
      p2 = s2;
      for (y = 1, diag = x - 1; y <= n; ++y)
        {
          cl1 = leven_charlen (*p1);
          cl2 = leven_charlen (*p2);
          prev_diag = tmp[y];

          tmp[y] = MIN3(tmp[y] + 1, tmp[y - 1] + 1,
                        diag + ((leven_charcmp (p1, p2) == 0) ? 0 : 1));
          diag = prev_diag;
          p1 += cl1;
          p2 += cl2;
        }
    }
  return tmp[n];
}

LEVENDEF size_t
leven_H (const char *restrict s1, const char *restrict s2, LARR_t *tmp)
{
  return calculate_leven__H (s1, s2, leven_strlen (s1), tmp);
}

LEVENDEF size_t
leven_imm (const char *s1, const char *s2)
{
  size_t n = leven_strlen (s1);
  LARR_t *tmp = leven_alloc(s1);

  size_t res = calculate_leven__H (s1, s2, n, tmp);
  leven_free(tmp);

  return res;
}

LEVENDEF size_t
leven_stk (const char *restrict s1, const char *restrict s2)
{
  size_t n = leven_strlen (s1);
  /* memory usage: 4(n+1) bytes */
  LARR_t tmp[n + 1];

  return calculate_leven__H (s1, s2, n, tmp);
}

#endif

#ifdef LEVEN_TEST
/* a simple test program */
/* test distance from s1 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct test_case {
  char *s;
  size_t res;
};
typedef struct test_case tc_t;

/* macro to make test case */
#define T_CASE(test_str, answer) &(tc_t){.s=test_str, .res=answer}
/* length of array macro */
#define lenof(arr) (sizeof(arr) / sizeof(*(arr)))
/* macro to make for loop over tc_t array */
#define TEST_LOOP(test_cases, tc_ptr)                   \
  for (size_t __i=0; __i < lenof (test_cases) &&        \
         (tc_ptr=test_cases[__i]); __i++)

/* UTF-8 character length (in bytes) test cases */
const tc_t *charlen_tests[] = {
  T_CASE ("A",  1),
  T_CASE (" ",  1),
  T_CASE ("И",  2),
  T_CASE ("€",  3),
  T_CASE ("𐍈",  4)
};
/* UTF-8 string length (in characters) test cases */
const tc_t *strlen_tests[] = {
  T_CASE (NULL,       0),
  T_CASE ("",         0),
  T_CASE ("012345",   6),
  T_CASE ("€𐍈И",      3),
  T_CASE ("©®",       2),
  T_CASE ("A©,®01",   6)
};
/* levenshtein distance test cases */
const tc_t *ld_tests[] = {
  T_CASE ("compatible",   0),
  T_CASE ("compateble",   1),
  T_CASE ("compatable",   1),
  T_CASE ("compatble",    4),
  T_CASE ("compatibel",   2),
  T_CASE ("xxxxxx",       10)
};

#define TEST_H(ret, exp) do {                                   \
    if (ret == exp) {                                           \
      fprintf (stdout, "\t PASS\n");                            \
    } else {                                                    \
      fprintf (stdout, "\t FAILED expected %lu\n", exp);        \
      assert ((ret == exp) && "test failure");                  \
    }} while (0)


int
main (void)
{
  const tc_t *tc;
  /* to test leven distance from s1 */
  const char *s1 = "compatible";
  LARR_t *tmp = leven_alloc (s1);

  puts ("- Testing charlen --------------------------------------");
  TEST_LOOP (charlen_tests, tc)
    {
      size_t char_l = leven_charlen (*tc->s);
      printf ("* charlen(\"%s\") = %lu  ", tc->s, char_l);
      TEST_H (char_l, tc->res);
    }

  puts ("\n- Testing strlen ---------------------------------------");
  TEST_LOOP (strlen_tests, tc)
    {
      size_t str_l = leven_strlen (tc->s);
      printf ("* strlen(\"%s\") = %lu        ", tc->s, str_l);
      TEST_H (str_l, tc->res);
    }

  puts ("\n- Testing Levenshtein Distance -------------------------");
  TEST_LOOP (ld_tests, tc)
    {
      size_t LD = leven_H (s1, tc->s, tmp);
      printf ("* LD(\"%s\", \"%s\") = %lu  ", s1, tc->s, LD);
      TEST_H (LD, tc->res);
    }

  leven_free(tmp);
  return 0;
}

#endif
