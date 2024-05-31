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
 *  this implementation simply uses n for memory allocations,
 *  so provide the smaller string first or use SMALLERx macros
 *
 *  compile the test program:
 *    cc -Wall -Wextra -D LEVEN_IMPLEMENTATION
 *                     -D LEVEN_TEST -o test leven.c
 *
 *
 *  to include in other files:
 *    `
 *    #define LEVEN_IMPLEMENTATION
 *    #include "leven.c"
 *
 *    // immediate call
 *    size_t d = leven_imm (s1, s2);
 *
 *    // use stack for buffer - for small s1
 *    size_t d = leven_stk (s1, s2);
 *
 *    // when distance of s1 ans s is needed
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

/* smaller string first wrapper macros */
/* use these macros for the leven imm, stk and H function */
#define SMALLER(fun, s1, s2, ...)               \
  SMALLERF (fun, strlen, s1, s2, __VA_ARGS__)
#define SMALLER2(fun, s1, s2, ...)              \
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
      lc = leven_charlen (*s);
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

const char *s1 = "compatible";

struct test_case {
  char *s;
  size_t res;
};
typedef struct test_case tc_t;
#define TC(a, b) &(tc_t){.s=a, .res=b}
#define lenof(arr) (sizeof(arr) / sizeof(*(arr)))

/* charlen test cases */
const tc_t *charlen_tests[] = {
  TC("A",  1),
  TC(" ",  1),
  TC("И",  2),
  TC("€",  3),
  TC("𐍈",  4)
};
/* strlen test cases */
const tc_t *strlen_tests[] = {
  TC(NULL,       0),
  TC("",         0),
  TC("012345",   6),
  TC("€𐍈И",      3),
  TC("©®",       2),
  TC("A©,®01",   6)
};
/* levenshtein distance test cases */
const tc_t *ld_tests[] = {
  TC("compatible",   0),
  TC("compateble",   1),
  TC("compatable",   1),
  TC("compatble",    4),
  TC("compatibel",   2),
  TC("xxxxxx",       10)
};

int
main (void)
{
  size_t LD = 0;
  size_t len = 0;
  const tc_t *tc;
  LARR_t *tmp = leven_alloc (s1);

  puts ("- Testing strlen ---------------------------------------");
  tc = *charlen_tests;
  for (size_t i=0; i < lenof(charlen_tests); tc = charlen_tests[++i])
    {
      len = leven_charlen (*tc->s);
      printf ("* charlen(\"%s\")=%lu   \t...", tc->s, len);
      assert ((len == tc->res) && "test failure");
      printf ("PASS %lu\n", tc->res);

    }

  puts ("- Testing strlen ---------------------------------------");
  tc = *strlen_tests;
  for (size_t i=0; i < lenof(strlen_tests); tc = strlen_tests[++i])
    {
      len = leven_strlen (tc->s);
      printf ("* strlen(\"%s\")=%lu   \t...", tc->s, len);
      assert ((len == tc->res) && "test failure");
      printf ("PASS %lu\n", tc->res);
    }

  puts ("");
  puts ("- Testing Levenshtein Distance -------------------------");
  tc = *ld_tests;
  for (size_t i=0; i < lenof(ld_tests); tc = ld_tests[++i])
    {
      LD = leven_H (s1, tc->s, tmp);
      printf ("* LD(\"%s\", \"%s\")=%lu \t...", s1, tc->s, LD);
      assert ((LD == tc->res) && "test failure");
      printf ("PASS %lu\n", tc->res);
    }

  leven_free(tmp);
  return 0;
}

#endif
