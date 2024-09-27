/**
 *  file: slice.c
 *  created on: 3 Aug 2024
 *
 *  This is slice template
 *  for having some headers (like size and capacity) before pointers
 *
 *  Adding header before pointers/arrays
 *  define DATA_PTR_T to specify the pointer type
 *
 *             you have pointer to this address,
 *             |  so you can use the pointer as normal
 *             v
 *  +----------+------------------------------
 *  |  header  |  beginning of array/pointer
 *  +----------+------------------------------
 *
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DATA_PTR_T
#  define DATA_PTR_T char
#endif

#ifndef VALLOC
#  define VALLOC(n_bytes) malloc (n_bytes)
#  define VFREE(ptr) free (ptr)
#  define VREALLOC(ptr, n_bytes) realloc (ptr, n_bytes)
#endif

#ifndef uint
#  define uint unsigned int
#endif

/* for (dynamic) arrays */
typedef struct {
  uint len; /* occupied length */
  uint cap; /* capacity */

  /* end of _slice */
  DATA_PTR_T data[]; /* actual data */
} _slice;


/* generic version */
typedef struct {
  void *header; /* generic header */

  /* end of _Slice_t */
  DATA_PTR_T data[];
} _Slice_t;

const _Slice_t *
slice_headerof (const void *ptr)
{
  const _Slice_t *__ptr = NULL;
  if (!ptr)
    return NULL;

  __ptr = (_Slice_t *)ptr;
  return __ptr - 1;
}
#define headerof(ptr, T) ((T *) slice_headerof (ptr))
#define arr_headerof(ptr) headerof (ptr, _slice)

/* only for generic version */
#define g_headerof(ptr, header_T) \
  ((header_T *) (headerof (ptr, _Slice_t)->header))

/* only for _slice */
uint
slice_sizeof (const void *ptr)
{
  _slice *header = arr_headerof (ptr);
  return header->len;
}
uint
slice_cap (const void *ptr)
{
  _slice *header = arr_headerof (ptr);
  return header->cap;
}

void *
slice_gnew (uint cap, void *header)
{
  _Slice_t *__result = VALLOC (cap + sizeof (_Slice_t));
  __result->header = header;

  return __result->data;
}

void *
slice_new (uint cap)
{
  _slice *__result = VALLOC (cap + sizeof (_slice));

  __result->len = 0;
  __result->cap = cap;

  return __result->data;
}


/**
 *  a simple dynamic array implementation
 */
#define arr_setlen(ptr, val) arr_headerof(ptr)->len = val
#define arr_lenof(ptr) (arr_headerof(ptr)->len)
#define arr_lenpp(ptr) (arr_lenof(ptr)++)
#define arr_pplen(ptr) (++arr_lenof(ptr))

#define arr_append(ptr, val) do {                               \
    typeof(ptr[0]) *__ptr = arr_expand (ptr, sizeof (ptr[0]));  \
    __ptr[arr_lenpp (__ptr)] = val;                             \
  } while (0)

void *
arr_expand (void *ptr, size_t type_size)
{
  _slice *__ptr = arr_headerof (ptr);
  if (!ptr || __ptr->len >= __ptr->cap)
    {
      __ptr->cap = __ptr->cap * 2 + 1;
      __ptr = VREALLOC (__ptr, __ptr->cap * type_size);
      return __ptr->data;
    }
  return ptr;
}


int
main (void)
{
  puts ("* pointer example *");
  char *str = slice_new (13);
  strncpy (str, "Hello World\n", slice_cap (str));
  printf ("str: %scapacity: %u\n", str, slice_cap (str));

  
  puts ("\n* dynamic array example *");
  int *arr = slice_new (2);

  arr_append (arr, 1);
  for (int i=0; i<5; ++i)
    {
      arr_append (arr, (i+2) * arr[i]);
    }

  for (int i=0; i<5; ++i)
    {
      printf ("%d! = %u\n", i+1, arr[i]);
    }
  
  
  return 0;
}
