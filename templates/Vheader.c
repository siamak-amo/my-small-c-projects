/**
 *  file: Vheader.c
 *  created on: 3 Aug 2024
 *
 *  This is Virtual Header template
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

  /* end of _Vheader_arr */
  DATA_PTR_T data[]; /* actual data */
} _Vheader_arr;


/* generic version */
typedef struct {
  void *header; /* generic header */

  /* end of _Vheader_t */
  DATA_PTR_T data[];
} _Vheader_t;

const _Vheader_t *
v_headerof (const void *ptr)
{
  const _Vheader_t *__ptr = NULL;
  if (!ptr)
    return NULL;

  __ptr = (_Vheader_t *)ptr;
  return __ptr - 1;
}
#define headerof(ptr, T) ((T *) v_headerof (ptr))
#define arr_headerof(ptr) headerof (ptr, _Vheader_arr)

/* only for generic version */
#define g_headerof(ptr, header_T) \
  ((header_T *) (headerof (ptr, _Vheader_t)->header))

/* only for _Vheader_arr */
uint
v_sizeof (const void *ptr)
{
  _Vheader_arr *header = arr_headerof (ptr);
  return header->len;
}
uint
v_cap (const void *ptr)
{
  _Vheader_arr *header = arr_headerof (ptr);
  return header->cap;
}

void *
v_gnew (uint cap, void *header)
{
  _Vheader_t *__result = VALLOC (cap + sizeof (_Vheader_t));
  __result->header = header;

  return __result->data;
}

void *
v_new (uint cap)
{
  _Vheader_arr *__result = VALLOC (cap + sizeof (_Vheader_arr));

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
  _Vheader_arr *__ptr = arr_headerof (ptr);
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
  char *str = v_new (13);
  strncpy (str, "Hello World\n", v_cap (str));
  printf ("str: %scapacity: %u\n", str, v_cap (str));

  
  puts ("\n* dynamic array example *");
  int *arr = v_new (2);

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
