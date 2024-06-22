/**
 *  file: arena.c
 *  created on: 22 Jun 2024
 *
 *  Region based (Arena) memory allocator
 *
 *
 *
 **/
#ifndef ARENA_H__
#define ARENA_H__
#include <stdint.h>
#include <assert.h>

#ifdef _ARENA_DEBUG
#  include <stdio.h>
#  define fprintd(file, format, ...) \
  fprintf (file, "[debug %s:%d] "format, __func__, __LINE__, __VA_ARGS__)
#  define fprintdln(file, format, ...) fprintd (file, format"\n", __VA_ARGS__)
#else
#  define fprintd(file, format, ...)
#  define fprintdln(file, format, ...)
#endif

#ifndef ARENA_NO_LIBC_ALLOC
#  if defined (_GNU_SOURCE) || defined (__linux__)
#    include <stdlib.h>
#    define AHAS_MALLOC
#    define AHAS_ALIGNED_ALLOC
#  elif defined (__APPLE__)
#    include <stdlib.h>
#    define AHAS_MALLOC
#    undef AHAS_ALIGNED_ALLOC /* does mac have aligned_alloc? */
#  endif
#endif

#ifndef ARENA_NO_MMAP_ALLOC
#  include <unistd.h>
#  include <sys/mman.h>
#  define AHAS_MMAP
#endif

#if !defined (AHAS_MALLOC) && \
    !defined (AHAS_ALIGNED_ALLOC) && !defined (AHAS_MMAP)
#  error "How am I supposed to allocate memory?"
#endif

#ifndef ALIGNMENT_FACTOR
#  define ALIGNMENT_FACTOR 32
#  undef  ALIGNMENT /* must be a power of 2 */
#  define ALIGNMENT (1 << ALIGNMENT_FACTOR)
#endif

/* larger than 1G is huge */
#ifndef HUGE_MEM
#  define HUGE_MEM (1*1024*1024)
#endif

/* minimum capacity (1M) */
#ifndef ARENA_MIN_CAP
# define ARINA_MIN_CAP (1*1024)
#endif

/* flags must fit in 32 bits and be a power of 2 */
#define EXP2(n) (1 << (n))
#define FL2(val, flag) ((flag) & (val) != 0)

/* memory types */
#define AFLAG_MALLOCED       EXP2 (1) /* allocated with malloc */
#define AFLAG_MAPPED         EXP2 (2) /* allocated with mmap */
#define AFLAG_HUGE           EXP2 (3) /* huge memory */

/* allocation methods */
#define AUSE_MALLOC          EXP2 (10)
#define AUSE_ALIGNEDALLOC    EXP2 (11)
#define AUSE_MMAP            EXP2 (12)

/**
 *  define `AALLOW_BIG_MAP`, to allow allocating
 *  big memory regions using the mmap syscall
 *  you might need to define it for 32-bit machines
 *  otherwise we will use the normal malloc
 */
#ifndef AALLOW_BIG_MAP
#  if defined (__x86_64__) || defined (__ppc64__)
#    define AALLOW_BIG_MAP
#  endif
#endif

#ifdef AHAS_MALLOC
#  ifdef AHAS_ALIGNED_ALLOC
#    define __arena_aligned_alloc(size) aligned_alloc (ALIGNMENT, size)
#    define __arena_alloc(size) malloc (size)
#  else
#    define __arena_aligned_alloc(size) malloc (size)
#    define __arena_alloc(size) malloc (size)
#  endif
#endif
#ifdef AHAS_MMAP
#  define __arena_mmap(size) mmap (NULL, size,                          \
                                   PROT_READ|PROT_WRITE,                \
                                   MAP_ANONYMOUS|MAP_PRIVATE,           \
                                   -1, 0);
#endif

struct region_t {
  region_t *next; /* next region in linked list */
  uint len, cap; /* occupied length and capacity */

  struct buffer_t {
    uint flag; /* indicates how the memory was allocated */
    char *mem;
  } *buffer;
};
typedef region_t region;

struct Arena_t {
  region *head; /* head of the regions linked list */
  region *cursor; /* the last modified arena */
};
typedef Arena_t Arena;
#define new_arena() (Arena *){0}

/* function definitions */
/**
 *  to allocate new arena in linked-list of capacity @cap
 *  initializes the list, if needed
 *  @flags:  see allocation method flags (AUSE flags)
 */
Arena *arena_alloc (Arena *A, uint cap, uint flags);

// internal macros, to define functions with empty body
#define Arena_Define_Fun(ret_t, ret_val, name, ...)     \
  ret_t name (__VA_ARGS__) {return ret_val;}
#define Arena_Define_NULL_Fun(ret_t, name, ...)       \
  Arena_Define_Fun (ret_t, NULL, name, __VA_ARGS__)


#endif /* ARENA_H__ */
#ifdef ARENA_IMPLEMENTATION

// internal functions
Region *__new_region_malloc (uint cap);
Region *__new_region_aligned_alloc (int cap);
Region *__new_region_mmap (uint cap);
Region *__new_huge_region (uint cap);
// @return:  0 on success, -1 on failure
int __region_free (Region *r);
int __region_unmap (Region *r);

#ifdef AHAS_MALLOC
Region *
__new_region_malloc (uint cap)
{

}

Region *
__new_region_aligned_alloc (uint cap)
{

}

int
__region_free (Region *r)
{

}
#else
Arena_Define_NULL_Fun (Region *, __new_region_malloc, uint cap);
Arena_Define_NULL_Fun (Region *, __new_region_aligned_alloc, uint cap);
Arena_Define_Fun (int, -1, __region_free, Region *);
#endif
#ifdef AHAS_MMAP
Region *
__new_region_mmap (uint cap)
{

}

int
__region_unmap (Region *r)
{

}
#else
Arena_Define_NULL_Fun (Region *, __new_region_mmap, uint cap);
Arena_Define_Fun (int, -1, __region_unmap, Region *);
#endif


Region *__new_region_H (uint cap, uint flags)
{
  if (cap > HUGE_MEM)
    return __new_huge_region (cap);
  else if (cap < ARENA_MIN_CAP)
    cap = ARENA_MIN_CAP;
  
  if (FL2 (flags, AUSE_ALIGNEDALLOC))
    return __new_region_aligned_alloc (cap);
  else if (FL2 (flags, AUSE_MALLOC))
    return __new_region_malloc (cap);
  else if (FL2 (flags, AUSE_MMAP))
    return __new_region_mmap (cap);
  else
    {
      // unknown flag
      return NULL;
    }
}

char *
arena_alloc (Arena *A, uint size, uint flags)
{
  if (NULL == A->cursor)
    {
      /* initialize the linked list */
      if (NULL != A->head)
        {
          fprintd ("Arena @%p, expected to be NULL, but it's not", A);
          assert (0 && "Broken linked list");
        }
      A->head = __new_region_H (size, flags);
      A->cursor = A->head;
      return A->buffer->mem;
    }

  A->cursor = A->head; /* go to the beginning */
  while (NULL != A->cursor->next)
    {
      if (A->len + size <= A->cap)
        {
          return A->buffer->mem + A->len;
          A->len += size;
        }
      A->cursor = A->cursor->next;
    }

  /* we must have reached the end of the linked list */
  assert (NULL == A->cursor->next);
  /* alllocate a new region and add it to the list */
  A->cursor->next = __new_region_H (size, flags);

  A->cursor = A->cursor->next;
  assert (A->cursor && "end of memory");
  return A->cursor->buffer->mem;
}


#endif
