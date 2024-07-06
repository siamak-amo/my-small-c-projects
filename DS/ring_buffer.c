/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  ring_buffer is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  ring_buffer is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: ring_buffer.c
 *  created on: 4 Jul 2024
 *
 *  Ring Buffer Implementation
 *
 *  This program is impartial to memory allocation
 *  and as it uses indexes to access its memory,
 *  remapping the memory is safe
 *  It's possible to use mapped files as memory, see the test program
 *
 *  time and memory complexity:
 *    readc, writec, reset:
 *      time O(1), mem O(1)
 *    readn, writen, swriten:
 *      time: O(n), n = number of requested bytes to read/write
 *      mem:  O(1)
 *
 *  Compilation:
 *    to compile the test program:
 *      cc -ggdb -Wall -Wextra -Werror \
 *        -D RB_IMPLEMENTATION \
 *        -D RB_TEST \
 *        -o test.out ring_buffer.c
 *
 *    to compile the example program:
 *      cc -ggdb -Wall -Wextra -Werror \
 *        -D RB_IMPLEMENTATION \
 *        -D RB_EXAMPLE \
 *        -o ring.out ring_buffer.c \
 *        $(pkg-config --libs readline)
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

/* reset the ring */
#define rb_reset(r) do {                        \
    (r)->head = 0;                              \
    (r)->idx = 0;                               \
    (r)->full = false;                          \
  } while (0)

/* reset and memset the memory */
#define rb_rememset(r) {                        \
    rb_reset (r);                               \
    memset ((r)->mem, 0, (r)->cap);             \
  }

/**
 *  like rb_readn, but makes the @dest null-terminated
 *  length of the @dest buffer must be at least n+1
 *  otherwise it has undefined behavior
 */
#define rb_sreadn(r, n, dest)     \
  do {                            \
    rb_readn (r, n, dest);        \
    dest[n] = 0;                  \
  } while (0)

/* write null-terminated string to ring */
#define rb_writes(ring, src) rb_writen (ring, src, strlen (src))
/* write to ring */
RINGDEF void rb_writec (RBuffer *r, char c);
RINGDEF void rb_writen (RBuffer *r, const char *src, size_t len);

#ifdef HAVE_FILEIO
/* write from file to ring */
RINGDEF void rb_fwrite (RBuffer *r, FILE *f, size_t len);
#endif

/* function definitions */

/* read one byte from the ring @r */
RINGDEF void rb_readc (RBuffer *r, char *dest);

/**
 *  to read @n bytes from the ring @r
 *  to the destination @dest
 */
RINGDEF void rb_readn (RBuffer *r, size_t n, char dest[n]);

#endif /* RIBG_BUFFER__H__ */


#ifdef RB_IMPLEMENTATION
RINGDEF void
rb_writec (RBuffer *r, char c)
{
  r->mem[r->idx] = c;
  r->idx = (r->idx + 1) % r->cap;

  if (r->full)
    r->head = (r->head + 1) % r->cap;
  if (0 == r->idx)
    {
      r->full = true;
    }
}

RINGDEF void
rb_writen (RBuffer *r, const char *src, size_t len)
{
  size_t __rest;

  if (len <= r->cap)
    {
      __rest = MIN (len, r->cap - r->idx);
      len -= __rest;
      memcpy (r->mem + r->idx, src, __rest);
      src += __rest;

      if (r->full)
        r->head = (r->head + __rest) % r->cap;
      if (r->idx + __rest >= r->cap)
        r->full = true;
      r->idx = (r->idx + __rest) % r->cap;

      if (0 != len)
        r->full = true;
      else
        return;

      memcpy (r->mem, src, len);
      r->head = (r->head + len) % r->cap;
      r->idx = (r->idx + len) % r->cap;;
    }
  else
    {
      src += len - r->cap;
      memcpy (r->mem, src, r->cap);

      r->full = true;
      r->head = 0;
      r->idx = 0;
    }
}

RINGDEF void
rb_readc (RBuffer *r, char *dest)
{
  if (NULL != r->mem)
    *dest = r->mem[r->idx];
}

RINGDEF void
rb_readn (RBuffer *r, size_t n, char dest[n])
{
  size_t __rest;

  if (!r->full)
    {
      __rest = MIN (n, r->idx);
      memcpy (dest, r->mem, __rest);
      return;
    }

  __rest = MIN (n, r->cap - r->idx);
  memcpy (dest, r->mem + r->head, __rest);
  n -= __rest;
  dest += __rest;

  while (n != 0)
    {
      __rest = MIN (n, r->cap);
      memcpy (dest, r->mem, __rest);
      n -= __rest;
      dest += __rest;
    }
}

#ifdef HAVE_FILEIO
RINGDEF void
rb_fwrite (RBuffer *r, FILE *f, size_t len)
{
  size_t __rest;
  size_t freads;

  if (len < r->cap)
    {
      __rest = MIN (len, r->cap - r->idx);
      len -= __rest;
      freads = fread (r->mem + r->idx, 1, __rest, f);
      r->idx = (r->idx + freads) % r->cap;

      if (r->full)
        r->head = (r->head + freads) % r->cap;
      if (0 != len)
        r->full = true;
      else
        return;

      if (freads < __rest)
        {
          return;
        }

      freads = fread (r->mem, 1, len, f);
      r->head = (r->head + freads) % r->cap;
      r->idx = (r->idx + freads) % r->cap;
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
          if (r->full || (r->idx + freads >= r->cap))
            {
              r->full = true;
              r->head = (r->head + freads) % r->cap;
            }
          r->idx = (r->idx + freads) % r->cap;
        }
    }
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
    printf ("    [%-32s]\n", tmp);                              \
    strnassert (tmp, (tcase)->expected, __explen,               \
                (tcase)->description, true);                    \
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
    Tcase("123456789abcdefghijklmnopqrstuvW",
          "simple rb_readn"),
    Tcase("456789abcdefghijklmnopqrstuvWABC",
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

  do_test (r, tests + 2, {
      rb_writec (r, 'W');
    });

  /* test 3: after overflow */
  do_test (r, tests + 3, {
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
    Tcase("efghijklmnopqrstuvWABC0123456789",
          "simple writen"),
    Tcase("6789**********************ABCDEF",
          "writen on overflow"),
    Tcase("**************************abcdef",
          "longer than capacity writen"),
    Tcase("1234", "use after reset")
  };

  do_test (r, tests, {
      rb_writen (r, "0123456789", 10);
    });

  do_test (r, tests + 1, {
      rb_writen (r, "**********************ABCDEF", 28);
    });

  do_test (r, tests + 2, {
      rb_writen (r, "******************************abcdef", 36);
    });

  do_test (r, tests + 3, {
      rb_reset (r);
      rb_writen (r, "1234", 4);
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

  /* reset the ring */
  rb_reset (r);

  const TestCase tests[] = {
    Tcase("ABCDEFGHIJ",
          "simple fwrite"),
    Tcase("345678901234567890123456789abcde",
          "longer than capacity fwrite"),
    Tcase("8901234567890123456789abcdefghij",
          "longer than file fwrite"),
    Tcase("34567890123456789abcdefghij",
          "after reset"),
    Tcase("ABCDEFGHIJ0123456789012345678901",
          "after rest, frwite on boundary"),
    Tcase("DEFGHIJ0123456789012345678901234",
          "use after fwrite on boundary")
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

  do_test (r, tests + 3, {
      rb_reset (r);
      fseek (tmp_file, 0, SEEK_SET);
      rb_fwrite (r, tmp_file, 55);
    });

  do_test (r, tests + 4, {
      rb_reset (r);
      fseek (tmp_file, 0, SEEK_SET);
      rb_fwrite (r, tmp_file, 32);
    });

  do_test (r, tests + 5, {
      rb_fwrite (r, tmp_file, 3);
    });

  fclose (tmp_file);
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
  rb_rememset (&ring);

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


#ifdef RB_EXAMPLE
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

static RBuffer r;
static char *__tmp;

void
help ()
{
  puts ("type:\n"
        "  \\ring          ~~> to see the ring structure\n"
        "  \\help          ~~> to print help\n"
        "  \\              ~~> to read from the ring\n"
        "  anything else  ~~> to write on the ring");
}

void
cmd (char *c)
{
  if ('r' == *c || 0 == strncmp (c, "ring", 4))
    {
      printf ("ring @%p\nhead: %lu, index:"
              " %lu, cap: %lu\nmem: [%s]\n",
              &r, r.head, r.idx, r.cap, r.mem);
    }
  if ('h' == *c || 0 == strncmp (c, "help", 4))
    {
      help ();
    }
  else if (0 == strlen (c))
    {
      rb_sreadn (&r, r.cap, __tmp);
      printf ("@r [%s]\n", __tmp);
    }
}

int
main ()
{
  char *__ln = NULL; /* for readline */
  r = rb_new (NULL, 32);
  r.mem = malloc (r.cap);
  rb_rememset (&r);

  __tmp = malloc (r.cap + 1);
  memset (__tmp, 0, r.cap + 1);

  printf ("Ring Buffer -- example program\n");
  help ();

  while (true)
    {
      __ln = readline (">>> ");
      if (NULL == __ln)
        break;

      if ('\\' == *__ln)
        {
          cmd (__ln + 1);
          goto EOLOOP;
        }

      if (strlen (__ln) > 0)
        rb_writes (&r, __ln);

    EOLOOP: /* end of loop */
      free (__ln);
    }

  free (__tmp);
  return 0;
}
#endif
