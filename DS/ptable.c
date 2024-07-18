/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  ptable is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  ptable is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: ptable.c
 *  created on: 16 Jul 2024
 *
 *  Pointer Table, unordered pointer array
 *  This data structure allows for appending and deleting
 *  void pointers in O(1) time and memory complexity
 *  It does not allocate or free memory; instead, keeps track of
 *  indices, making it remap and realloc safe
 *  It can be used as a dynamic table (see the CLI program)
 *  with O(1) amortized time and memory complexity
 *  It also can be used as an stack (see pt_push and pt_pop)
 *
 *  In 64bit machines, always has memory protection feature
 *  In 32bit machines, with memory protection, the maximum length
 *  of table <= 0xffff=65535  ~512Mb
 *  you can define _NO_DFREE_PROTECTION to disable
 *  memory protection and get full table size
 *  In other platforms, there is no memory protection,
 *  and double free can happen with undefined behavior
 *
 *  Compilation:
 *    to compile the CLI program:
 *    cc -ggdb -Wall -Wextea -Werror ptable.c \
 *       $(pkg-config --cflags readline)
 *       -D PTABLE_IMPLEMENTATION \
 *       -D PTABLE_CLI -D PTABLE_TEST \
 *       -o test.out pkg-config --libs readline
 *
 *    to compile the test program:
 *    cc -ggdb -Wall -Wextea -Werror ptable.c \
 *       -D PTABLE_IMPLEMENTATION \
 *       -D PTABLE_TEST \
 *       -o test.out
 *
 *    to include in c files:
 *    ```{c}
 *    #include <stdio.h>
 *    #include <stdlib.h>
 *
 *    #define PTABLE_IMPLEMENTATION
 *    #include "ptable.c"
 *
 *    int
 *    main (void)
 *    {
 *      PTable pt = new_ptable (32); // length 32
 *      // using malloc, you can use mmap instead
 *      pt_alloc (&pt, malloc (cap));
 *
 *      {
 *        // do something here
 *        // call pa_append or pa_delete_by_idx
 *      }
 *
 *      pt_free (&pt, free (mem));
 *      return 0;
 *    }
 *    ```
 **/
#ifndef PTABLE__H__
#define PTABLE__H__
#include <string.h>
#include <sys/types.h>
#include <assert.h>

#ifndef PTDEFF
#  define PTDEFF static inline
#endif

#undef UNUSED
#define UNUSED(x) ((void)(x))

/* this value must exist in mem[__lastocc + 1] */
#if   __SIZEOF_POINTER__ == 8
#  define ptr_t size_t
#  define idx_t size_t
#  define off_t ssize_t
#  define HAVE_DFREE_PROTECTION
#  define SLOT_GUARD (0xdeadbeefcafebabe)
#  define SLOT_GUARD_H (0xdeadbeef)
   // idx ->  0x[flag]...[idx]
#  define MEMPROTO_TO(idx) (((ptr_t)SLOT_GUARD_H << 32) | ((idx) & 0xFFFFFFFF))
   // 0x[flag]...[idx] ->  idx
   // cast to int to save the sign of the underlying offset
#  define MEMPROTO_OF(addr) (int)((addr) & 0xFFFFFFFF)
#  define MEMPROTO_FLAG(addr) (((addr) & 0xFFFFFFFF00000000) >> 32)

#elif __SIZEOF_POINTER__ == 4
#  define ptr_t unsigned long
#  define idx_t unsigned long
#  define off_t long
#  define SLOT_GUARD (0xdeadbeef)
#  define SLOT_GUARD_H (0xcafe)
#  ifndef _NO_DFREE_PROTECTION
#    define HAVE_DFREE_PROTECTION
#    define MEMPROTO_TO(idx) (((ptr_t)SLOT_GUARD_H << 16) | ((idx) & 0x0000FFFF))
#    define MEMPROTO_OF(addr) (short)((addr) & 0x0000FFFF)
#    define MEMPROTO_FLAG(addr) (((addr) & 0xFFFF0000) >> 16)
#  endif

#elif __SIZEOF_POINTER__ == 2
#  include <stdint.h>
#  define ptr_t uint16_t
#  define idx_t uint16_t
#  define off_t int16_t
#  define SLOT_GUARD (0x4242)

#elif __SIZEOF_POINTER__ == 1
#  include <stdint.h>
#  define ptr_t uint8_t
#  define idx_t uint8_t
#  define off_t int8_t
#  define SLOT_GUARD (0x42)

#else
#  define FLOT_GUARD (0)
#endif

enum pt_errnum_t {
  PT_NONE = 0,
  PT_OVERFLOW,
  PT_ALREADY_FREED,
  PT_DOUBLEFREE,
  PT_BROKEN_LOGIC,
  PT_IDX_OUTOF_BOUND,
  PT_NULLPTR,
  PT_MEM_SMASHING,
};

struct ptable_t {
  void **mem;
  idx_t cap; /* capacity (entry count) of mem */
  /* internal fields */
  idx_t __freeidx; /* first free to write index */
  idx_t __lastocc; /* last occupied index */
};
typedef struct ptable_t PTable;
#define pt_last_idx(pt) ((pt)->__lastocc) /* last occupied index */
#define pt_ffree_idx(pt) ((pt)->__freeidx) /* first free index */

/* sizeof mem of capacity @cap (in bytes) */
#define ptmem_sizeof(cap) ((cap) * sizeof (void *))
#define pt_sizeof(pt) ptmem_sizeof ((pt)->cap)
#define new_ptable(c) (PTable){.cap = c,                \
      .__freeidx = 0, .__lastocc = 0,                   \
      .mem = NULL                                       \
      }

/**
 *  alloc, realloc, free macros
 *  within @funcall of these macros, you have access
 *  to mem, and cap, for instance:
 *    with malloc:   pt_realloc (&pt, realloc (cap));
 *                   pt_alloc (&pt, malloc (cap));
 *    with mmap:     pt_free (&pt, munmap (mem, cap));
 */

#define pt_mem(ptable, funcall) do {            \
    idx_t cap = ptmem_sizeof ((ptable)->cap);   \
    void *mem = (ptable)->mem;                  \
    funcall;                                    \
  } while (0)
#define pt_alloc(ptable, funcall) do {          \
    idx_t cap = pt_sizeof (ptable);             \
    (ptable)->mem = funcall;                    \
  } while (0)
#define pt_realloc(ptable, funcall) do {        \
    idx_t cap = pt_sizeof (ptable);             \
    void *mem = (ptable)->mem;                  \
    if (mem) {(ptable)->mem = funcall;}         \
  } while (0)
#define pt_free(ptable, funcall) do {           \
    idx_t cap = pt_sizeof (ptable);             \
    void *mem = (ptable)->mem;                  \
    if (mem && cap > 0) {funcall;}              \
    (ptable)->mem = NULL;                       \
  } while (0)

#define pt_addrof(ptable, index) ((ptable)->mem + index)
#define pt_GET(pt, idx, T) ((T *)pt_addrof (pt, idx))

/**
 *  find the previous freed index, starting from @idx
 *  not necessarily < @idx
 *  @return: on success    -> previous index
 *           on error/end  -> max value of idx_t (-1)
 */
PTDEFF idx_t pt_prev_free_idx (PTable *pt, idx_t idx);

/**
 *  append to the table
 *  @return: on success   -> 0
 *           on failure   -> error codes in pt_errornum_t
 */
PTDEFF int pt_append (PTable *pt, void *value);

/**
 *  delete an element by index
 *  @return: on success  -> 0
 *           on failure  -> -1
 */
PTDEFF int pt_delete_byidx (PTable *pt, idx_t idx);

/* @return: error message */
PTDEFF const char *pt_strerr (int errnum);

/**
 *  stack version
 *  you can use this library also as stack
 */
/* @return: same as the pt_append function */
#define pt_push(pt, val) pt_append (pt, val)
/* only get the top of the stack, type: void pointer */
#define pt_top(pt) (pt->mem[pt->__lastocc])
/**
 *  removes the top of stack and return it
 *  returns NULL when the stack is empty
 */
PTDEFF void *pt_pop (PTable *pt);

#endif /* PTABLE__H__ */

#ifdef PTABLE_IMPLEMENTATION

PTDEFF const char *
pt_strerr (int errnum)
{
  switch (errnum)
    {
    case PT_NONE:
      return NULL;
    case PT_OVERFLOW:
      return "Table Overflow";
    case PT_ALREADY_FREED:
      return "Slot is already free";
    case PT_DOUBLEFREE:
      return "Double free detected";
    case PT_BROKEN_LOGIC:
      return "Broken Logic";
    case PT_IDX_OUTOF_BOUND:
      return "Index out of range";
    case PT_NULLPTR:
      return "Null Pointer";
    case PT_MEM_SMASHING:
      return "Memory Smashing detected";

    default:
      return "Unknown Error";
    }
  return NULL;
}

PTDEFF void *
pt_pop (PTable *pt)
{
  if (pt->__lastocc == 0 && pt->__freeidx == 0)
    return NULL;
  void *ret = pt->mem[pt->__lastocc];
  pt_delete_byidx (pt, pt->__lastocc);
  return ret;
}

PTDEFF int
pt_append (PTable *pt, void *value)
{
  if (!pt || !pt->mem)
    return PT_NULLPTR;
  assert (pt->__lastocc <= pt->cap && pt->__freeidx <= pt->cap);

  if (pt->__freeidx >= pt->__lastocc)
    {
      if (pt->__lastocc > 0 && pt->__lastocc > pt->__freeidx)
        {
          if (pt->__lastocc + 1 < pt->cap &&
              pt->mem[pt->__lastocc + 1] != (void *)SLOT_GUARD)
            return PT_MEM_SMASHING;
        }
      /* write on unused indices */
      pt->__lastocc = pt->__freeidx;
      pt->mem[pt->__freeidx] = value;
      pt->__freeidx++;
      if (pt->__lastocc + 1 >= pt->cap)
        return PT_OVERFLOW;
    }
  else
    {
      /* write on freed indices */
      off_t _offset = (off_t)pt->mem[pt->__freeidx];
#ifdef HAVE_DFREE_PROTECTION
      if (MEMPROTO_FLAG (_offset) != SLOT_GUARD_H)
        {
          /* double free detected */
          return PT_DOUBLEFREE;
        }
      _offset = MEMPROTO_OF (_offset);
#endif
      if (_offset == 0)
        {
          /* double free detected! */
          return PT_DOUBLEFREE;
        }
      assert (pt->__freeidx < pt->__lastocc && "Broken Logic");
      pt->mem[pt->__freeidx] = value;
      if (pt->__freeidx >= pt->__lastocc)
        pt->__lastocc = pt->__freeidx;
      pt->__freeidx += _offset;
    }
  /* write the gaurd */
  if (pt->__lastocc < pt->cap)
    pt->mem[pt->__lastocc + 1] = (void *)SLOT_GUARD;
  if (pt->__lastocc + 1 >= pt->cap)
    return PT_OVERFLOW;

  return 0;
}


PTDEFF int
pt_delete_byidx (PTable *pt, idx_t idx)
{
  if (!pt || !pt->mem)
    return PT_NULLPTR;
  if (idx > pt->__lastocc)
    return PT_IDX_OUTOF_BOUND;

#ifdef HAVE_DFREE_PROTECTION
  /**
   *  we cannot detect doable free or memory corruption here
   *  only we can prevent it from happening
   */
  ptr_t value = pt->__freeidx - idx;
  value = MEMPROTO_TO (value);

  if (MEMPROTO_FLAG ((ptr_t)pt->mem[idx]) == SLOT_GUARD_H)
    {
      /* prevent double free */
      return PT_ALREADY_FREED;
    }
  pt->mem[idx] = (void *)value;
#else
  pt->mem[idx] = (void*)(pt->__freeidx - idx);
#endif

  pt->__freeidx = idx;
  if (pt->__freeidx == pt->__lastocc && pt->__lastocc > 0)
    pt->__lastocc--;

  return 0;
}

PTDEFF idx_t
pt_prev_free_idx (PTable *pt, idx_t idx)
{
  if (!pt || !pt->mem)
    return PT_NULLPTR;
  if (idx == (idx_t)-1)
    return -1;
  if (pt->__freeidx >= pt->__lastocc)
    return -1;

  off_t _offset = (off_t)pt->mem[idx];

#ifdef HAVE_DFREE_PROTECTION
  if (SLOT_GUARD_H != MEMPROTO_FLAG (_offset))
    {
      /* double free or memory smashing */
      return -1;
    }
  _offset = MEMPROTO_OF (_offset);
#endif

  if (_offset == 0)
    {
      if (pt->__lastocc <= pt->__freeidx)
        return -1;
      else
        {
          /* double free detected! */
          return -1;
        }
    }
  idx += _offset;
  if (idx > pt->__lastocc)
    return -1;
  return idx;
}
#endif /* PTABLE_IMPLEMENTATION */

#ifdef PTABLE_TEST
#include <stdio.h>
#include <stdlib.h>

#define BASE_TABLE_SIZE 16
#define ROW_CHUNK 13

#define pt_error(code) \
  printf ("[error:l%d]  %s\n", __LINE__, pt_strerr (code))

/* the cli program */
#ifdef PTABLE_CLI
#include <unistd.h>
#include <termios.h>
#include <readline/readline.h>

void
__toggle_ICANON ()
{
  struct termios term;
  tcgetattr (fileno (stdin), &term);
  term.c_lflag ^= (ICANON | ECHO);
  tcsetattr (fileno (stdin), TCSANOW, &term);
}

char
__do_more ()
{
  __toggle_ICANON ();
  printf ("--More-- press any key to see more, q to exit ");
  fflush (stdout);
  char c = getchar();
  printf ("\r\e[K");
  __toggle_ICANON ();
  return c;
}

ssize_t
readline_getnumber (const char *prompt)
{
  char *s = readline (prompt);
  if (!s)
    return -1;
  if ('\0' == *s)
    return 0;
  ssize_t ret = atoll (s);
  free (s);
  return ret;
}

void *
readline_gethex (const char *prompt)
{
  void *ret;
  char *s = readline (prompt);
  sscanf (s, "%p", &ret);
  free (s);
  return ret;
}

int
main_loop (PTable *pt)
{
  int __errno;
  ssize_t idx;
  void *ptr = NULL;
  char c = '\0';
  char *tmp, *tmp_H = NULL;

  while (1)
    {
      if (NULL == tmp_H)
        {
          tmp_H = readline (">>> ");
          if (NULL == tmp_H)
            break;
          tmp = tmp_H;
          c = '\n';
        }
      else
        {
          c = *(tmp++);
          if (c == '\0')
            {
              free (tmp_H);
              tmp_H = NULL;
            }
        }
      /* execute command c */
      switch (c)
        {
        case 'a':
        case 'A':
          {
          AppendCMD:
            if ((__errno = pt_append (pt, ptr += 0x1111)))
              pt_error (__errno);
            if (PT_OVERFLOW == __errno)
              {
                pt->cap += BASE_TABLE_SIZE;
                pt_realloc (pt, realloc (mem, cap));
                printf ("table has extended, capacity: %lu\n", pt->cap);
                ptr -= 0x1111;
                goto AppendCMD;
              }
          }
          break;
        case 'P':
          {
            printf("idx   type   pointer value \n"
                   "--------------------------------\n");
            goto PrintCommand;
          }
          break;
        case 'p':
          {
          PrintCommand:
            ptr_t i = 0;
            do {
              for (ptr_t __j = 0;
                   __j < ROW_CHUNK && i <= pt->cap;
                   __j++, ++i)
                {
                  char slot_type =
                    (i<= pt_last_idx(pt) && i != pt_ffree_idx (pt)) ? 'o' : ' ';

                  for (idx_t idx = pt_ffree_idx(pt);
                       idx != (idx_t)-1 && idx < pt_last_idx(pt);
                       idx = pt_prev_free_idx (pt, idx))
                    {
                      if (i == idx)
                        slot_type = 'f';
                    }

                  off_t val = (off_t)pt->mem[i];
#ifdef HAVE_DFREE_PROTECTION
                  /* we only have memory flag for freed slots */
                  if (slot_type == 'f')
                    {
                      if (SLOT_GUARD_H != MEMPROTO_FLAG (val))
                        {
                          /* memory smashing detected */
                          slot_type = '*';
                        }
                      val = MEMPROTO_OF (val);
                    }
#else
                  if (0 == val && 'f' == slot_type)
                    {
                      slot_type = '*'; /* double free! */
                    }
#endif

                  printf ("%-4lu  [%c]-> %c0x%.16lx", i, slot_type,
                          (val >= 0) ? ' ' : '-', val);

                  if ('*' == slot_type)
                    printf ("  \t<- memory corruption");
                  else
                    {
                      if (i == pt_last_idx(pt))
                        printf ("  \t<- last");
                      if (i == pt_ffree_idx(pt))
                        printf ("  \t<- free");
                    }
                  puts ("");
                }
              if (i <= pt_last_idx(pt))
                c = __do_more ();
            } while (i <= pt_last_idx(pt) && 'q' != c);
          }
          break;
        case 'd':
          {
            if ((idx = readline_getnumber ("index: ")) >= 0)
              if ((__errno = pt_delete_byidx (pt, idx)))
                pt_error (__errno);
          }
          break;

        case 'r':
          {
            if ((idx = readline_getnumber ("index: ")) >= 0)
              {
                printf (" table[%ld] = %p\n", idx, pt->mem[idx]);
              }
          }
          break;

        case 'w':
          if ((idx = readline_getnumber ("index: ")) >= 0)
            {
              pt->mem[idx] = readline_gethex ("value in hex: ");
            }
          break;

        case '<':
          pt_push (pt, readline_gethex ("value in hex: "));
          break;

        case '>':
          printf ("pop: %p\n", pt_pop (pt));
          break;

        case 'q':
          return 0;

        default:
          continue;
        }
    }
  return __errno;
}

#else

struct mem_test_case_t {
  idx_t index;
  void *exp_value; /* expected value */
};
typedef struct mem_test_case_t mt_case;
#define MT(i, val) (mt_case){.index=i, .exp_value=(void*)val}
#define lenof(carray) \
  ((sizeof (carray) != 0) ? (sizeof (carray) / sizeof ((carray)[0])) : 0)

int
__do_test__ (PTable *pt, const mt_case tests[], int len)
{
  void **mem = pt->mem;
  for (const mt_case *t = &tests[0]; len != 0; --len, ++t)
    {
      if (mem[t->index] != t->exp_value)
        {
          printf ("[test idx:%lu] failed! %p != %p\n",
                  t->index, mem[t->index], t->exp_value);
          return -1;
        }
    }
  return 0;
}
#define __do_test(pt, arr) __do_test__ (pt, arr, lenof (arr))

#define TEST(pt, msg, actions, ...) do {                \
    int __errno = 0;                                    \
    puts (" * "msg);                                    \
    actions;                                            \
    if (__errno) pt_error (__errno);                    \
    if (0 == sizeof ((mt_case[]){__VA_ARGS__}))         \
      { puts ("no test"); break; }                      \
    const mt_case tests[] = {__VA_ARGS__};              \
    if (0 == __do_test (pt, tests))                     \
      puts ("pass\n");                                  \
    else puts ("fail");                                 \
  } while (0)

void
run_tests (PTable *pt)
{
  /* memset to zero, for testing purposes */
  pt_mem (pt, memset (mem, 0, cap));

  TEST (pt, "test 1  --  append to table",
        {
          for (void *i = NULL; i < (void*)0x999; i += 0x111)
            pt_append (pt, i);
        },
        MT(0, 0), MT(1, 0x111), MT(2, 0x222), /* beginning */
        MT(9, 0), MT(10, 0)); /* boundary values */

  TEST (pt, "test 2 (64bit only)  --  delete by index",
        {
          /* it must succeed */
          __errno |= pt_delete_byidx (pt, 1);
          __errno |= pt_delete_byidx (pt, 2);

          /* double free, it must fail */
          __errno |= PT_ALREADY_FREED != pt_delete_byidx (pt, 1);
          __errno |= PT_ALREADY_FREED != pt_delete_byidx (pt, 2);

          __errno |= pt_delete_byidx (pt, 5);
        },
        /* before and after delete must be intact */
        MT(0, 0), MT(3, 0x333),
        /* we had 9 elements, if we delete the first one ==>
         *  offset to the previous free index = 8 */
        MT(1, MEMPROTO_TO(8)),
        /* last deleted index = 1  ==>  offset = -1 */
        MT(2, MEMPROTO_TO(-1)),
        MT(5, MEMPROTO_TO(-3)) /* offset to index 2 = -3 */
        );
}

#endif /* PTABLE_TEST */


int
main (void)
{
  PTable pt = new_ptable (BASE_TABLE_SIZE); /* 16 entries */
  pt_alloc (&pt, malloc (cap));

#ifdef PTABLE_CLI
  /* CLI program */
  puts ("Pointer Table!\n"
        "  a,A      to append to the table (0x111, 0x222, ...)\n"
        "  d        to delete by index\n"
        "  p,P      to print the table\n"
        "  r,w      to read / write on memory\n"
        "  <,>      to push / pop like an stack\n"
        "  q        to exit\n");
  main_loop (&pt);
#else
  /* normal test */
  run_tests (&pt);
#endif
  
  pt_free (&pt, free (mem));
  return 0;
}

#endif
