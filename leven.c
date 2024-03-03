/**
 *  file: leven.c
 *  created on: 3 Mar 2024
 *
 *  Implements Levenshtein Distance
 *
 *  algorithm details: leven(s1[.n], s2[.m]):
 *    time: O(n*m)
 *    memory: O(Min(n,m))
 *     we simply use n for memory allocation, so provide
 *     the smaller string first
 *
 *  usage:
 *    `
 *    #define LEVEN_IMPLEMENTATION
 *    #include "leven.c"
 *
 *    // immediate call
 *    size_t d = leven_imm (s1, s2, 1);
 *
 *    // use stack for buffer - for small s1
 *    size_t d = leven_stk (s1, s2, 1);
 *
 *    // when distance of s1 ans s is needed
 *    // for fixed s1 and some s in a list
 *    LARR_t *_tmp = leven_alloc (s1, 1);
 *    for (s in list)
 *      leven_H (s1, s, _tmp, 1);
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
#define leven_alloc(s, cl) malloc (strlen (s) / cl)

/**
 *  functions to calculate levenshtein distance @s1 and @s2
 *  @cl:   character length, pass 2 for non-ascii strings
 *         or sizeof(wchar_t) for wchar strings, otherwise 1
 *  @tmp:  temporary buffer
 *         use leven_alloc and leven_free macros
 */
LEVENDEF size_t
leven_imm (const char *s1, const char *s2, size_t cl);
/* use stack */
LEVENDEF size_t
leven_stk (const char *s1, const char *s2, size_t cl);
/* manually handle the memory */
LEVENDEF size_t
leven_H (const char *s1, const char *s2, LARR_t* tmp, size_t cl);
#endif

/* the implementation */
#ifdef LEVEN_IMPLEMENTATION

/**
 *  internal function
 */
static inline size_t
calculate_leven__H (const char *s1, const char *s2,
                   size_t n, LARR_t *tmp, size_t cl)
{
  const char *p1, *p2;
  /* last diagonal value in the `imaginary` matrix */
  size_t diag, prev_diag;
  size_t x,y;

  for (y = 1; y <= n; ++y)
    tmp[y] = y;
  for (x = 1; x <= strlen (s2); ++x)
    {
      tmp[0] = x;
      p1 = s1;
      p2 = s2;
      for (y = 1, diag = x - 1; y <= n; ++y)
        {
          prev_diag = tmp[y];
          tmp[y] = MIN3(tmp[y] + 1, tmp[y - 1] + 1,
                        diag + ((strncmp (p1, p2, cl) == 0) ? 0 : 1));
          diag = prev_diag;
          p1 += cl;
          p2 += cl;
        }
    }
  return tmp[n];
}

LEVENDEF size_t
leven_H (const char *s1, const char *s2, LARR_t *tmp);
{
}

LEVENDEF size_t
leven_imm (const char *s1, const char *s2)
{
}

LEVENDEF size_t
leven_stk (const char *s1, const char *s2)
{
}

#endif
