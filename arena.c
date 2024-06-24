/* This file is part of my-small-c-projects

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
#include <string.h>
#include <stdint.h>
#include <assert.h>

#ifdef _ARENA_DEBUG
#  include <stdio.h>
#  define fprintd(file, format, ...) \
  fprintf (file, "[debug %s:%d] "format, __func__, __LINE__, ##__VA_ARGS__)
#  define fprintdln(file, format, ...) fprintd (file, format"\n", ##__VA_ARGS__)
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
#  define ALIGNMENT_FACTOR 16
#  undef  ALIGNMENT /* must be a power of 2 */
#  define ALIGNMENT (1 << ALIGNMENT_FACTOR)
#endif

/* larger than 2G is huge */
#ifndef HUGE_MEM
#  define HUGE_MEM (2*1024*1024)
#endif

/* minimum capacity (1M) */
#ifndef ARENA_MIN_CAP
# define ARENA_MIN_CAP (1*1024)
#endif

/* flags must fit in 32 bits and be a power of 2 */
#define EXP2(n) (1 << (n))
#define FL2(val, flag) (((flag) & (val)) != 0)

/* flags              */
/* memory types       */
#define AFLAG_MALLOCED       EXP2 (10) /* allocated with malloc */
#define AFLAG_MAPPED         EXP2 (11) /* allocated with mmap */
/* allocation methods */
#define AUSE_MALLOC          EXP2 (1)
#define AUSE_ALIGNEDALLOC    EXP2 (2)
#define AUSE_MMAP            EXP2 (3)

/**
 *  converts flag of a region to it's memory type
 *  the first 10 bits of a flag
 */
#define memtypeof(flag) ((flag) & 0x3FF)

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
#    define __arena_alloc(size) aligned_alloc (ALIGNMENT, size)
#  else
#    define __arena_alloc(size) malloc (size)
#  endif
#endif
#ifdef AHAS_MMAP
#  define __arena_mmap(size)                                            \
  mmap (NULL, size,                                                     \
        PROT_READ | PROT_WRITE,                                         \
        MAP_ANONYMOUS | MAP_PRIVATE,                                    \
        -1, 0);
#endif

struct region_t {
  struct region_t *next; /* next region in linked list */
  uint len, cap; /* occupied length and capacity */
  uint flag; /* indicates how the memory was allocated */
  char mem[];
};
/**
 *  regions must be allocated of the size below
 *  the @cap only indicates the size of the @mem member
 */
#define size_of_region(size) (size + sizeof (struct region_t))
#define region_sizeof(r) ((r)->cap + sizeof (struct region_t))
#define region_leftof(r) ((r)->cap - (r)->len)
typedef struct region_t Region;

struct Arena_t {
  Region *head; /* head of the regions linked list */
  Region *end;
};
typedef struct Arena_t Arena;
#define new_arena() (Arena){0}
#define for_regions(a)                                                  \
  for ( (a)->cursor = (a)->head;                                        \
        NULL != (a)->cursor;                                            \
        (a)->cursor = ((a)->cursor)->next )
#define for_regions2(a)                                                 \
  for ( Region *r = (a)->head; NULL != r; r = r->next )


/* function definitions */
/**
 *  to allocate new arena in linked-list of capacity @cap
 *  initializes the list, if needed
 *  @flags:  see allocation method flags (AUSE flags)
 *  this function guarantees that the allocated memory
 *  within some region, has the right flags
 *  if you provide some new flag, that never has been used in
 *  the arena @A, it will allocate a whole new region
 */
char *arena_alloc (Arena *A, uint cap, uint flags);
/**
 *  the same as the arena_alloc, but the allocated memory
 *  might have different flags (see AUSE flags)
 *  in fact, it allocates the memory in the first region
 *  that has enough free space withing the arena @A
 */
char *arena_alloc2 (Arena *A, uint cap, uint flags);

/**
 *  manual realloc, preferably use arena_realloc
 *  this function always succeeds
 *  the new @flags, does not need to be the same as the old flags
 *  and the new memory will have the new @flags
 */
char *arena_realloc (Arena *A, char *old, uint old_size, uint new_size, int flags);

/**
 *  gives the corresponding region within the arena @A
 *  which contains the allocated memory at @ptr
 */
Region *regionof (Arena *A, char *ptr);

/**
 *  set's occupied length to zero for all the regions
 *  within the arena @A
 *  this function will not overwrite the memories, so
 *  your pointers will be valid until some new allocations occur
 */
void arena_reset (Arena *A);
/**
 *  frees all regions withing the arena @A
 *  your pointers are not valid anymore after calling this function
 */
void arena_free (Arena *A);

#endif /* ARENA_H__ */
#ifdef ARENA_IMPLEMENTATION

/* internal functions */
Region *__new_region_malloc (uint cap);
Region *__new_region_mmap (uint cap);
Region *__new_huge_region (uint cap);
/**
 *  @return:
 *    0  success
 *   -1  failur
 *   -2  when malloc or mmap not included respectively
 */
int __region_free (Region *r);
int __region_unmap (Region *r);

#ifdef AHAS_MALLOC
Region *
__new_region_malloc (uint cap)
{
  Region *r = __arena_alloc (size_of_region (cap));
  assert (r && "end of memory");
  r->flag = AFLAG_MALLOCED | AUSE_MALLOC;
  r->cap = cap;
  r->next = NULL;
  return r;
}

int
__region_free (Region *r)
{
  if (NULL == r)
    return -1;
  if (FL2 (AFLAG_MALLOCED, r->flag))
    {
      free (r);
      return 0;
    }
  else
    return -1;
}
#else
Region *
__new_region_malloc (uint cap)
{
  UNUSED (cap);
  return NULL;
}

int
__region_free (Region *r)
{
  UNUSED (r);
  return -2;
}
#endif
#ifdef AHAS_MMAP
Region *
__new_region_mmap (uint cap)
{
  Region *r = __arena_mmap (size_of_region (cap));
  assert (r && "end of memory");
  r->flag = AFLAG_MAPPED | AUSE_MMAP;
  r->cap = cap;
  r->next = NULL;
  return r;
}

int
__region_unmap (Region *r)
{
  if (NULL == r)
    return -1;
  if (FL2 (AFLAG_MAPPED, r->flag))
    {
      int ret = munmap (r, region_sizeof (r));
      if (0 == ret)
        return 0;
      else
        return -1;
    }
  else
    return -1;
}
#else
Region *
__new_region_mmap (uint cap)
{
  UNUSED (cap);
  return NULL;
}

int
__region_unmap (Region *r)
{
  UNUSED (r);
  return -2;
}
#endif


Region *
__new_huge_region (uint cap)
{
#if defined (AHAS_MMAP)
  return __new_region_mmap (cap);
#else
  return NULL; /* prevent allocating huge memory with malloc */
#endif
  return NULL; /* unreachable */
}

Region *
__new_region_H (uint cap, uint flags)
{
  if (cap > HUGE_MEM)
    return __new_huge_region (cap);
  else if (cap < ARENA_MIN_CAP)
    cap = ARENA_MIN_CAP;
  
  if (FL2 (flags, AUSE_MALLOC))
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
arena_alloc2 (Arena *A, uint size, uint flags)
{
  if (NULL == A->head)
    {
      /* initialize the linked list */
      if (NULL != A->end)
        {
          fprintdln (stderr, "Arena expected to be NULL, but it's not");
          assert (0 && "Broken linked list");
        }
      A->head->next = __new_region_H (size, flags);
      if (NULL == A->head)
        return NULL;
      A->head->len = size;
      A->end = A->head;
      return A->head->mem;
    }

  Region *r = A->head; /* go to the beginning */
  uint __len = 0;
  while (NULL != r)
    {
      if (r->len + size <= r->cap)
        {
          __len = r->len;
          r->len += size;
          fprintdln (stdout, "region allocated %u, left %u bytes",
                     size, region_leftof (r));
          return r->mem + __len;
        }
      else
        fprintdln (stderr, "region has cap: %u but wanted: %u",
                   region_leftof (r), size);
      r = r->next;
    }

  /* we must have reached the end of the linked list */
  assert (NULL == r && NULL != A->end);
  /* alllocate a new region and add it to the list */
  A->end->next = __new_region_H (size, flags);
  if (NULL == A->end->next)
    return NULL;
  A->end = A->end->next;
  A->end->len = size;
  return A->end->mem;
}

char *
arena_alloc (Arena *A, uint size, uint flags)
{
  if (NULL == A || 0 == size)
    return NULL;
  if (NULL == A->head)
    {
      /* initialize the linked list */
      if (NULL != A->end)
        {
          fprintdln (stderr, "Arena expected to be NULL, but it's not");
          assert (0 && "Broken linked list");
        }
      A->head = __new_region_H (size, flags);
      if (NULL == A->head)
        return NULL;
      A->head->len = size;
      A->end = A->head;
      return A->head->mem;
    }

  Region *r = A->head; /* go to the beginning */
  uint __len = 0;
  while (NULL != r)
    {
      if (r->len + size <= r->cap)
        {
          if (memtypeof (r->flag) == flags)
            {
              __len = r->len;
              r->len += size;
              fprintdln (stdout, "region allocated %u, left %u bytes",
                         size, region_leftof (r));
              return r->mem + __len;
            } else
            {
              fprintdln (stderr, "region has flag: %u but wanted: %u",
                         memtypeof (r->flag), flags);
            }
        }
      else
        fprintdln (stderr, "region has cap: %u but wanted: %u",
                   region_leftof (r), size);
      r = r->next;
    }

  /* we must have reached the end of the linked list */
  assert (NULL == r && NULL != A->end);
  /* alllocate a new region and add it to the list */
  A->end->next = __new_region_H (size, flags);
  if (NULL == A->end->next)
    return NULL;
  A->end = A->end->next;
  A->end->len = size;
  return A->end->mem;
}

Region *
regionof (Arena *A, char *ptr)
{
  for_regions2 (A)
    {
      size_t __mem_head = (size_t)&(r->mem);
      size_t __ptr = (size_t)ptr;
      if (__ptr >= __mem_head && __ptr <= __mem_head + r->len)
        return r;
    }
  return NULL;
}

char *
arena_realloc (Arena *A, char *old, uint old_size,
               uint new_size, int flags)
{
  if (new_size <= old_size)
    return old;

  char *res = arena_alloc (A, new_size, flags);
  memcpy (res, old, old_size);
  return res;
}

void
arena_reset (Arena *A)
{
  for_regions2 (A)
    {
      r->len = 0;
    }
}

void
arena_free (Arena *A)
{
  Region *tmp;
  for_regions2 (A)
    {
      tmp = r;
      __region_free (tmp);
    }
  A->head = NULL;
  A->end = NULL;
}
#endif /* ARENA_IMPLEMENTATION */

#ifdef ARENA_TEST
/* the test program */
#include <stdio.h>
#include <assert.h>

Arena
arenabak (Arena *A)
{
  return (Arena){
    .head = A->head,
    .end = A->end
  };
}

int
main (void)
{
  Region *r = NULL;
  // min size is 1M
  Arena A = new_arena ();

  printf ("start allocating...\n");
  /* p1 should be allocated in the first region, of capacity 1024 */
  char *p1 = arena_alloc (&A, 500, AUSE_MALLOC);
  assert (NULL != p1);

  /**
   *  p2 should be allocated in the second region, because
   *  the flags of p2 is different (allocated with mmap)
   */
  char *p2 = arena_alloc (&A, 600, AUSE_MMAP);
  assert (NULL != p2);

  /**
   *  p3 should be alocated in the first region, because
   *  the first region has enough capacity (1024 - 500)
   */
  char *p3 = arena_alloc (&A, 111, AUSE_MALLOC);
  assert (NULL != p3);

  /**
   *  although the first region has enough space
   *  for this allocation (1024 - 500 - 100),
   *  p4 should be allocated in the second region; because
   *  flags of this one is differet
   *  it uses mmap but the first region has allocated using mmap
   */
  char *p4 = arena_alloc (&A, 200, AUSE_MMAP);
  assert (NULL != p4);

  /**
   *  the `arena_alloc2` function does not care about the flags
   *  thus, this should be allocated in the first region
   */
  char *p5 = arena_alloc2 (&A, 150, AUSE_MMAP);
  assert (NULL != p5);
  printf ("done\n\n");

  /* test 1  --  expected regions */
  printf ("testing expected regions has created... ");
  r = A.head;
  assert (NULL != r && "empty list");
  r = r->next;
  assert (NULL != r && "second region is NULL");
  // assert (NULL == r->next && "more than 2 region");
  printf ("pass\n");

  printf ("testing length and capacity of regions... ");
  r = A.head;
  assert (r->cap == ARENA_MIN_CAP && "Wrong capacity");
  assert (r->len == (500+111+150) && "Wrong length");
  r = r->next;
  assert (r->cap == ARENA_MIN_CAP && "Wrong capacity");
  assert (r->len == (600+200) && "Wrong length");
  printf ("pass\n\n");
  

  /* test 2  --  region_alloc */
  printf ("testing p1, ..., p5 are in the expected rigions... ");
  Arena tmp = arenabak (&A);
  Region *first_r = A.head;
  Region *second_r = A.head->next;

  r = regionof (&tmp, p1);
  assert (r == first_r &&
          "p1 is in wrong region");
  r = regionof (&tmp, p2);
  assert (r == second_r &&
          "p2 is in wrong region"); 
  r = regionof (&tmp, p3);
  assert (r == first_r &&
          "p3 is in wrong region");
  r = regionof (&tmp, p4);
  assert (r == second_r &&
          "p4 is in wrong region");
  r = regionof (&tmp, p5);
  assert (r == first_r &&
          "p5 is in wrong region");
  printf ("pass\n");

  /* test 3  --  address */
  printf ("testing address of p1, ..., p5... ");
  char *_first_r_mem = first_r->mem;
  char *_second_r_mem = second_r->mem;

  assert (_first_r_mem == p1 &&
          "address of p1 is wrong");
  assert (_second_r_mem == p2 &&
          "address of p2 is wrong");
  assert (_first_r_mem + 500 == p3 &&
          "address of p3 is wrong");
  assert (_second_r_mem + 600 == p4 &&
          "address of p1 is wrong");
  assert (_first_r_mem + 500 + 111 == p5 &&
          "address of p1 is wrong");
  printf ("pass\n\n");

  /* test 3  --  freeing arena */
  printf ("resetting... ");
  arena_reset (&A);
  for_regions2 (&A)
    {
      assert (r->len == 0 && "region was missed");
    }
  printf ("pass\n");

  printf ("freeing... ");
  arena_free (&A);
  printf ("passed\n");
  
  return 0;
}

#endif /* ARENA_TEST */
