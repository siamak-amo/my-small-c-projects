/**
 *  file: ptable.c
 *  created on: 16 Jul 2024
 *
 *  Pointer Table
 *  remap / realloc safe
 *
 *
 *
 *
 *
 *
 *
 **/
#ifndef PTABLE__H__
#define PTABLE__H__
#include <string.h>
#include <assert.h>

#ifndef PTDEFF
#  define PTDEFF static inline
#endif

#undef UNUSED
#define UNUSED(x) ((void)(x))

struct ptable_t {
  void **mem;
  size_t cap; /* capacity (entry count) of mem */
  /* internal fields */
  size_t __freeidx; /* first free to write index */
  size_t __lastocc; /* last occupied index */
};
typedef struct ptable_t PTable;

/* this value must exist in mem[__lastocc + 1] */
#if   __SIZEOF_POINTER__ == 8
#  define HAVE_DFREE_PROTECTION
#  define SLOT_GUARD (0xdeadbeefcafebabe)
#  define SLOT_GUARD_H (0xdeadbeef)
   // idx ->  0x[flag]...[idx]
#  define MEMPROTO_TO(idx) (((size_t)SLOT_GUARD_H << 32) | ((idx) & 0xFFFFFFFF))
   // 0x[flag]...[idx] ->  idx
   // cast to int to save the sign of the underlying offset
#  define MEMPROTO_OF(addr) (int)((addr) & 0xFFFFFFFF)
#  define MEMPROTO_FLAG(addr) ((addr) >> 32)
#elif __SIZEOF_POINTER__ == 4
#  define SLOT_GUARD (0xdeadbeef)
#  define SLOT_GUARD_H (0xcafe)
#  ifndef _NO_DFREE_PROTECTION
#    define HAVE_DFREE_PROTECTION
#    define MEMPROTO_TO(idx) (((unsigned int)SLOT_GUARD_H << 16) | ((idx) & 0x0000FFFF))
#    define MEMPROTO_OF(addr) (short)((addr) & 0x0000FFFF)
#    define MEMPROTO_FLAG(addr) ((addr) >> 16)
#  endif
#elif __SIZEOF_POINTER__ == 2
#  define SLOT_GUARD (0x4242)
#elif __SIZEOF_POINTER__ == 1
#  define SLOT_GUARD (0x42)
#else
#  define FLOT_GUARD (0)
#endif

/* sizeof mem of capacity @cap (in bytes) */
#define ptmem_sizeof(cap) ((cap) * sizeof (void *))
#define new_ptable(c) (PTable){.cap = c,                \
      .__freeidx = 0, .__lastocc = 0,                   \
      .mem = NULL                                       \
    }
/* alloc, free, realloc */
#define pt_alloc(ptable, funcall) do {          \
    size_t cap = ptmem_sizeof ((ptable)->cap);  \
    (ptable)->mem = funcall;                    \
  } while (0)
#define pt_realloc(ptable, funcall) do {        \
    size_t cap = ptmem_sizeof ((ptable)->cap);  \
    void *mem = (ptable)->mem;                  \
    if (mem) {(ptable)->mem = funcall;}         \
  } while (0)
#define pt_free(ptable, funcall) do {           \
    size_t cap = ptmem_sizeof ((ptable)->cap);  \
    void *mem = (ptable)->mem;                  \
    if (mem && cap > 0) {funcall;}              \
  } while (0)

#define pt_addrof(ptable, index) ((ptable)->mem + index)
#define pt_GET(pt, idx, T) ((T *)pt_addrof (pt, idx))

/**
 *  finds index of @addr (pointer to the mem)
 *  @return: max value of size_t on failure
 */
PTDEFF size_t pt_indexof (PTable *pt, void *addr);

/**
 *  linear search
 *  @return: index on success
 *           max value of size_t on failure
 */
PTDEFF size_t pt_search (PTable *pt, void *value);

/**
 *  append to the table
 *  @return: pointer to the @value on success
 *           NULL on table overflow
 */
PTDEFF void *pt_append (PTable *pt, void *value);
/**
 *  delete arbitrary item from table by address
 *  @return: NULL on failure, @value on success
 */
PTDEFF void *pt_delete (PTable *pt, void *value);
/* delete by index, @return: same as pt_delete */
PTDEFF void *pt_delete_byidx (PTable *pt, size_t idx);
/* delete by value, @return: same as pt_delete */
#define pt_delete_byvalue(pt, val) \
  pt_delete_byidx (pt, pt_search (pt, val))

#endif /* PTABLE__H__ */

#ifdef PTABLE_IMPLEMENTATION
PTDEFF size_t
pt_indexof (PTable *pt, void *addr)
{
  UNUSED(pt);
  UNUSED(addr);
  return 0;
}

PTDEFF size_t
pt_search (PTable *pt, void *value)
{
  UNUSED(pt);
  UNUSED(value);
  return 0;
}

PTDEFF void *
pt_append (PTable *pt, void *value)
{
  UNUSED(pt);
  UNUSED(value);
  return NULL;
}

PTDEFF void *
pt_delete (PTable *pt, void *value)
{
  UNUSED(pt);
  UNUSED(value);
  return NULL;
}

PTDEFF void *
pt_delete_byidx (PTable *pt, size_t idx)
{
  UNUSED(pt);
  UNUSED(idx);
  return NULL;
}
#endif /* PTABLE_IMPLEMENTATION */

#ifdef PTABLE_TEST
#include <stdio.h>
#include <stdlib.h>

int
main (void)
{
  PTable pt = new_ptable (64); /* 64 entries */
  pt_alloc (&pt, malloc (cap));

  {
    // main
  }
  
  pt_free (&pt, free (mem));
  return 0;
}

#endif
