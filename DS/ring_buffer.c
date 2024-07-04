/**
 *  file: ring_buffer.c
 *  created on: 4 Jul 2024
 *
 *  Ring Buffer Implementation
 *
 *  We not handle memory, allocating and freeing memory is up to users
 *  also you can use a mmaped file, see the test program
 *
 **/
#ifndef RIBG_BUFFER__H__
#define RIBG_BUFFER__H__
#include <stdbool.h>
#include <string.h>

#ifndef RB_NO_STDIO
#  define HAVE_FILEIO
#  include <stdio.h>
#endif

#ifndef RINGDEF
#  define RINGDEF static inline
#endif

#ifndef UNUSED
#  define UNUSED(x) (void)(x)
#endif

#ifndef MIN
#  define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#  define MAX(a, b) (((a) < (b)) ? (b) : (a))
#endif

/* safe ring access macros */
#define rb_safe_idx(ring, idx) ((idx) % (ring->cap))

struct ring_buffer {
  char *mem;
  size_t cap;
  size_t head, idx;
  bool full;
};
typedef struct ring_buffer RBuffer;

#define rb_new(buf, len) (RBuffer) {                            \
      .mem = buf,                                               \
      .cap = len,                                               \
      .head = 0, .idx = 0, .full = false                        \
   }

/* write null-terminated string to ring */
#define rb_writes (ring, src) rb_writen (ring, src, strlen (src))
/* write to ring */
RINGDEF int rb_writec (RBuffer *r, char c);
RINGDEF int rb_writen (RBuffer *r, const char *src, size_t len);

#ifdef HAVE_FILEIO
/* write from file to ring */
RINGDEF int rb_fwrite (RBuffer *r, FILE *f, size_t len);
#endif

/* only read from ring */
RINGDEF int rb_readc (RBuffer *r, char *dest);
RINGDEF int rb_readn (RBuffer *r, char *dest, size_t n);

/* read and move the head forward (flush) */
#define rb_flushc(ring, dest) ({                                \
      ring->head_idx = (ring->head_idx + 1) % (ring->cap);      \
      rb_readc (ring, dest); })
#define rb_flushn(ring, dest, n) ({                             \
      ring->head_idx = (ring->head_idx + n) % (ring->cap);      \
      rb_readn (ring, dest, n); })

#endif /* RIBG_BUFFER__H__ */


#ifdef RB_IMPLEMENTATION
RINGDEF int
rb_writec (RBuffer *r, char c)
{
  UNUSED (r);
  UNUSED (c);
  return 0;
}

RINGDEF int
rb_writen (RBuffer *r, const char *src, size_t len)
{
  UNUSED (r);
  UNUSED (src);
  UNUSED (len);
  return 0;
}

RINGDEF int
rb_readc (RBuffer *r, char *dest)
{
  UNUSED (r);
  UNUSED (dest);
  return 0;
}

RINGDEF int
rb_readn (RBuffer *r, char *dest, size_t n)
{
  UNUSED (r);
  UNUSED (n);
  UNUSED (dest);
  return 0;
}

#ifdef HAVE_FILEIO
RINGDEF int
rb_fwrite (struct ring_buffer *r, FILE *f, size_t len)
{
  UNUSED (r);
  UNUSED (f);
  UNUSED (len);
  return 0;
}
#endif /* HAVE_FILEIO */

#endif /* RB_IMPLEMENTATION */


#ifdef RB_TEST
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define rbassert(con, msg, exit) do { if (!(con)) {                     \
      fprintf (stderr, "%s:%d: [%s] Assertion `%s` failed",             \
               __FILE__, __LINE__, (msg) ? msg : "--", #con);           \
      if (exit) abort ();                                               \
    }} while (0)

/* prints function name @fun and runs `fun(...)` */
#define TESTFUN(fun, ...) do {                     \
    fprintf (stdout, " * Running %s:\n", #fun);    \
    if (0 == fun (__VA_ARGS__))                    \
      fprintf (stdout, "   PASS\n\n");             \
    else                                           \
      fprintf (stdout, "   FAIL\n\n");             \
  } while (0)

#define RETFAIL(n) return n
#define RETPASS() return 0

void
ringset (RBuffer *r)
{
  rbassert (NULL != r, "broken test", true);
  memset (r->mem, 0, r->cap);
}

void
assert__H (RBuffer *r, const char exp[r->cap], const char *msg)
{
  size_t i = 0;
  if (exp != 0)
    {
      for (; i < r->cap; ++i)
        rbassert (exp[i] == r->mem[i], msg, true);
    }
  else
    {
      for (; i < r->cap; ++i)
        rbassert (0 == r->mem[i], msg, true);
    }
}

int
map_file2ring (FILE *f, RBuffer *r)
{
  int ffd = fileno (f);
  if (NULL == f || 0 > ffd)
    return 1;

  ftruncate (ffd, r->cap);

  if (MAP_FAILED == (r->mem = mmap (
       NULL, r->cap,
       PROT_READ | PROT_WRITE,
       MAP_SHARED,
       ffd, 0
     )))
    {
      fclose (f);
      return 1;
    }
  return 0;
}

int
TEST_1 (RBuffer *r)
{
  UNUSED (r);
  RETPASS ();
}

int
TEST_2 (RBuffer *r)
{
  UNUSED (r);
  RETFAIL (-1);
}

int
main (void)
{
  FILE *f = tmpfile ();
  RBuffer ring = rb_new (NULL, 32); /* 32B */

  if (0 != map_file2ring (f, &ring))
    {
      return 1;
    }

  /* only for testing, it's not mandatory */
  ringset (&ring);
  assert__H (&ring, 0, "memset 0 failed");

  /* running tests */
  TESTFUN (TEST_1, &ring);
  TESTFUN (TEST_2, &ring);
  
  rbassert (0 == munmap (ring.mem, ring.cap),
             "munmap failed, broken ring logic!", true);
  if (f)
    fclose (f);
  return 0;
}
#endif /* RB_TEST */
