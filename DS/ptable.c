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
#define pt_alloc(ptable, funcall) do {          \
    idx_t cap = ptmem_sizeof ((ptable)->cap);   \
    (ptable)->mem = funcall;                    \
  } while (0)
#define pt_realloc(ptable, funcall) do {        \
    idx_t cap = ptmem_sizeof ((ptable)->cap);   \
    void *mem = (ptable)->mem;                  \
    if (mem) {(ptable)->mem = funcall;}         \
  } while (0)
#define pt_free(ptable, funcall) do {           \
    idx_t cap = ptmem_sizeof ((ptable)->cap);   \
    void *mem = (ptable)->mem;                  \
    if (mem && cap > 0) {funcall;}              \
    (ptable)->mem = NULL;                       \
  } while (0)

#define pt_addrof(ptable, index) ((ptable)->mem + index)
#define pt_GET(pt, idx, T) ((T *)pt_addrof (pt, idx))

PTDEFF idx_t pt_prev_free_idx (PTable *pt, size_t idx);

/**
 *  append to the table
 *  @return: pointer to the @value on success
 *           NULL on table overflow
 */
PTDEFF int pt_append (PTable *pt, void *value);

/* delete by index, @return: same as pt_delete */
PTDEFF int pt_delete_byidx (PTable *pt, idx_t idx);
/* delete by value, @return: same as pt_delete */

PTDEFF const char *pt_strerr (int errnum);

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

    default:
      return "Unknown Error";
    }
  return NULL;
}

PTDEFF int
pt_append (PTable *pt, void *value)
{
  assert (pt->__lastocc <= pt->cap && pt->__freeidx <= pt->cap);

  if (pt->__freeidx >= pt->__lastocc)
    {
      if (pt->__freeidx == pt->cap)
        return PT_OVERFLOW;
      /* write on unused indices */
      pt->__lastocc = pt->__freeidx;
      pt->mem[pt->__freeidx] = value;
      pt->__freeidx++;
      return 0;
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

  return 0;
}


PTDEFF int
pt_delete_byidx (PTable *pt, idx_t idx)
{
  if (idx > pt->__lastocc)
    return PT_IDX_OUTOF_BOUND;

#ifdef HAVE_DFREE_PROTECTION
  /* have double free protection */
  ptr_t value = pt->__freeidx - idx;
  value = MEMPROTO_TO (value);

  if (MEMPROTO_FLAG ((ptr_t)pt->mem[idx]) == SLOT_GUARD_H)
    {
      /* double free detected */
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
  if (idx == (idx_t)-1)
    return -1;
  if (pt->__freeidx >= pt->__lastocc)
    return -1;

  off_t _offset = (off_t)pt->mem[idx];

#ifdef HAVE_DFREE_PROTECTION
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
