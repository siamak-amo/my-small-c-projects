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

/* function definitions */
/* read one byte from the ring @r */
RINGDEF int rb_readc (RBuffer *r, char *dest);
/**
 *  to read @n bytes from the ring @r
 *  to the destination @dest
 */
RINGDEF int rb_readn (RBuffer *r, size_t n, char dest[n]);
/**
 *  like rb_readn, but makes the @dest null-terminated
 *  length of the @dest buffer must be at least n+1
 *  otherwise it has undefined behavior
 */
RINGDEF int rb_sreadn (RBuffer *r, size_t n, char dest[n+1]);
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
  r->mem[r->idx] = c;
  r->idx = (r->idx + 1) % r->cap;

  if (r->full)
    r->head = (r->head + 1) % r->cap;
  if (0 == r->idx)
    r->full = true;
  return 0;
}

RINGDEF int
rb_writen (RBuffer *r, const char *src, size_t len)
{
  size_t rw = 0;
  size_t __rest;

  if (len < r->cap)
    {
      // TODO
      __rest = MIN (len, r->cap - r->idx);
      len -= __rest;
      rw += __rest;
      memcpy (r->mem + r->idx, src, __rest);
      src += __rest;
      r->idx = (r->idx + __rest) % r->cap;

      if (r->full)
        r->head = (r->head + __rest) % r->cap;

      if (0 != len)
        r->full = true;
      else
        return rw;

      memcpy (r->mem, src, len);
      rw += len;
      r->head = (r->head + len) % r->cap;
      r->idx = (r->idx + len) % r->cap;;
      return rw;
    }
  else
    {
      src += len - r->cap - 1;
      memcpy (r->mem, src, r->cap);

      r->full = true;
      r->head = 0;
      r->idx = 0;
      return r->cap;
    }

  return rw;
}

RINGDEF int
rb_readc (RBuffer *r, char *dest)
{
  if (NULL == r->mem)
    return 0;
  *dest = r->mem[r->idx];
  return 0;
}

RINGDEF int
rb_sreadn (RBuffer *r, size_t n, char dest[n+1])
{
  int __ret = rb_readn (r, n, dest);
  dest[n] = '\0';
  return __ret;
}

RINGDEF int
rb_readn (RBuffer *r, size_t n, char dest[n])
{
  size_t rw = 0;
  size_t __rest;

  __rest = MIN (n, r->cap - r->idx);
  memcpy (dest, r->mem + r->head, __rest);
  n -= __rest;
  rw += __rest;
  dest += __rest;

  while (n != 0)
    {
      __rest = MIN (n, r->cap);
      memcpy (dest, r->mem, __rest);
      n -= __rest;
      dest += __rest;
      rw += __rest;
    }

  return rw;
}

#ifdef HAVE_FILEIO
RINGDEF int
rb_fwrite (struct ring_buffer *r, FILE *f, size_t len)
{
  size_t rw = 0;
  size_t __rest;
  size_t freads;

  if (len < r->cap)
    {
      // TODO
      __rest = MIN (len, r->cap - r->idx);
      len -= __rest;
      freads = fread (r->mem + r->idx, 1, __rest, f);
      rw += freads;
      r->idx = (r->idx + freads) % r->cap;

      if (r->full)
        r->head = (r->head + freads) % r->cap;
      if (0 != len)
        r->full = true;
      else
        return rw;

      if (freads < __rest)
        {
          return rw;
        }

      freads = fread (r->mem, 1, len, f);
      rw += freads;
      r->head = (r->head + freads) % r->cap;
      r->idx = (r->idx + freads) % r->cap;
      return rw;
    }
  else
    {
      fseek (f, len - r->cap, SEEK_CUR);
      freads = fread (r->mem, 1, r->cap, f);
      if (freads == r->cap)
        {
          r->full = true;
          r->head = 0;
          r->idx = 0;
        }
      else
        {
          /* unexpected end of file */
          r->idx = (r->idx + freads) % r->cap;
          /* unexpected end of file */
          if (r->full || (r->idx + freads > r->cap))
            {
              r->full = true;
              r->head = (r->head + freads) % r->cap;
            }
        }
      return r->cap;
    }

  return rw;
}
#endif /* HAVE_FILEIO */

#endif /* RB_IMPLEMENTATION */


#ifdef RB_TEST
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>

#define rbassert(con, msg, exit) do { if (!(con)) {                     \
      fprintf (stderr, "%s:%d: [%s%s] Assertion `%s` failed",           \
               __FILE__, __LINE__, (msg) ? msg : "",                    \
               (msg) ? " FAILED" : "FAILED", #con);                     \
      if (exit) abort ();                                               \
    }} while (0)

#define strnassert(str1, str2, n, msg, exit) \
  rbassert (0 == strncmp (str1, str2, n), msg, exit)

/* prints function name @fun and runs `fun(...)` */
#define TESTFUN(fun, ...) do {                     \
    fprintf (stdout, "Running %s:\n", #fun);       \
    if (0 == fun (__VA_ARGS__))                    \
      fprintf (stdout, "PASS\n\n");                \
    else                                           \
      fprintf (stdout, "FAIL\n\n");                \
  } while (0)

#define RETFAIL(n) return n
#define RETPASS() return 0

void
ringset (RBuffer *r)
{
  rbassert (NULL != r, "broken test", true);
  memset (r->mem, 0, r->cap);
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

struct test_case_t {
  char *expected;
  char *description;
};
typedef struct test_case_t TestCase;

#define Tcase(exp, msg) (TestCase) {              \
    .expected=exp, .description=msg               \
  }
#define do_test(r, tcase, action) do {                          \
    printf (" - testing %s:\n", (tcase)->description);          \
    action;                                                     \
    int __explen = strlen ((tcase)->expected);                  \
    char *tmp = malloc (__explen + 1);                          \
    rb_sreadn (r, __explen, tmp);                               \
    strnassert (tmp, (tcase)->expected, __explen,               \
                (tcase)->description, true);                    \
    printf ("    [%-32s]\n", tmp);                              \
    free (tmp);                                                 \
  } while (0)


int
TEST_1 (RBuffer *r)
{
  const TestCase tests[] = {
    Tcase("0123456789",
          "simple writec"),
    Tcase("0123456789abcdefghijklmnopqrstuv",
          "simple rb_readn"),
    Tcase("3456789abcdefghijklmnopqrstuvABC",
          "rb_readn after overflow"),
  };

  /* test 1: before overflow */
  do_test (r, tests, {
      for (int i=0; i<10; ++i)
        rb_writec (r, '0' + i);
    });

  /* test 2: on overflow */
  do_test (r, tests + 1, {
      for (int i=0; i<22; ++i)
        rb_writec (r, 'a' + i);
    });

  /* test 3: after overflow */
  do_test (r, tests + 2, {
      for (int i=0; i<3; ++i)
        rb_writec (r, 'A' + i);
    });

  RETPASS ();
}

int
TEST_2 (RBuffer *r)
{
  rbassert (r->mem != NULL, "NULL", true);

  const TestCase tests[] = {
    Tcase("defghijklmnopqrstuvABC0123456789",
          "simple writen"),
    Tcase("6789**********************ABCDEF",
          "writen on overflow"),
    Tcase("**************************abcdef",
          "longer than capacity writen")
  };

  do_test (r, tests, {
      rb_writen (r, "0123456789", 10);
    });

  do_test (r, tests + 1, {
      rb_writen (r, "**********************ABCDEF", 28);
    });

  do_test (r, tests + 2, {
      rb_writen (r, "******************************abcdef", 37);
    });

  RETPASS ();
}

int
TEST_3 (RBuffer *r)
{
  rbassert (r->mem != NULL, "NULL", true);
  FILE *tmp_file = tmpfile ();
  ftruncate (fileno (tmp_file), 50);
  fwrite ("ABCDEFGHIJ012345678901234567890123456789abcdefghij",
          1, 50, tmp_file);
  fseek (tmp_file, 0, SEEK_SET);

  const TestCase tests[] = {
    Tcase("****************abcdefABCDEFGHIJ",
          "simple fwrite"),
    Tcase("345678901234567890123456789abcde",
          "longer than capacity fwrite"),
    Tcase("8901234567890123456789abcdefghij",
          "longer than file fwrite")
  };

  do_test (r, tests, {
      rb_fwrite (r, tmp_file, 10);
    });

  do_test (r, tests + 1, {
      rb_fwrite (r, tmp_file, 35);
    });

  do_test (r, tests + 2, {
      rb_fwrite (r, tmp_file, 10);
    });

  RETPASS ();
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

  /* running tests */
  TESTFUN (TEST_1, &ring);
  TESTFUN (TEST_2, &ring);
  TESTFUN (TEST_3, &ring);
  
  rbassert (0 == munmap (ring.mem, ring.cap),
             "munmap failed, broken ring logic!", true);
  if (f)
    fclose (f);
  return 0;
}
#endif /* RB_TEST */
