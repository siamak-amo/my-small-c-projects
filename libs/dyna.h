/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  Dyna.h is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Dyna.h is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: dyna.h
    created on: 8 Oct 2024
  
    Dynamic Array (generic) implementation
    Based on `templates/slice.c` within this repository
  
    Usage:
    ```c
    #include <stdio.h>
    #include <stdlib.h>
  
    // Optionally, you can define these macros to determine
    // how the dynamic array grows.
    // In this example, capacity increases by DA_GFACT at each overflow
    // By default, it grows exponentially by a factor of 2
    #define DA_GFACT 8
    #define DA_DO_GROW(cap) ((cap) += DA_GFACT)
  
    #define DYNA_IMPLEMENTATION
    #include "dyna.h"
  
    int
    main (void)
    {
      // Character Array, with initial capacity 10
      // Using da_new or da_newn is not mandatory
      // and can be replaced with NULL initializing
      char *carr = da_newn (char, 10);
  
      for (char c ='a'; c <= 'z'; ++c)
        da_appd (carr, c);
      da_appd (carr, '\0');
  
      puts (carr); // must print ab...z
      da_free (carr);
  
  
      // C string array
      char **cstr = NULL;
  
      da_appd (cstr, "string0");
      da_appd (cstr, "string1");
      da_appd (cstr, "string2");


      // Generic struct array
      struct data {
        int index;
      };
      struct data *arr = da_new (struct data);
      // Copying local structs to @arr
      for (int i=0; i<7; ++i)
        {
          struct data tmp = {.index=i};
          da_appd (arr, tmp);
        }
  
      // Print & Free
      da_foreach (arr, i)
        printf ("struct arr[%i] - index: %d\n", i, arr[i].index);
      da_free (arr);


      // Pointer Array
      struct data **ptr_arr = da_new (struct data *);
      // Allocate data and append
      for (int i=0; i<7; ++i)
        {
          struct data *tmp = malloc (sizeof (struct data));
          *tmp = (struct data){.index=i};
          da_appd (ptr_arr, tmp);
        }
  

      // Using many append
      const char *source[] = {"a", "b", "c"};
      da_appd_arr (cstr, source, 3);

      // Append a dynamic array
      char **cstr2 = NULL;
      da_appd_da (cstr2, cstr);

      // Using advanced many append
      int i = da_many_appd (cstr2, 3);
      memcpy (cstr2 + i, source, sizeof source);

      return 0;
    }
    ```
  
    Options:
      `_DA_DEBUG`:  to print some debugging information
      `DA_INICAP`:  the default initial capacity of arrays
      `DA_DO_GRO`:  to define how arrays grow
      `DA_GFACT`:   growth factor (see the source code)
  
    WARNING:
      If an instance of this dynamic array, is stored outside
      the scope of a function (e.g F), you *MUST NOT* use
      `da_appd` inside F, because if an overflow occurs,
      `da_appd` will need to reallocate it's entire memory
      which frees the original pointer and potentially leads
      to undefined behavior (SEGFAULT or use-after-free)

      A simple solution is to store the array reference inside
      a struct and pass a pointer of it to the function F,
      so `da_appd` will update the reference properly

      Another solution is to use `da_aappd` macro,
      and F needs to accept pointer to dynamic array:
      ```c
        // scope 1
        {
          T val = {0};
          T *arr = da_new (T);
          my_function (&arr, val);
        }
  
        void
        my_function (void *array, T data)
        {
          // Append data to array
          // Do NOT pass use like C strings for data
          // only use pointers with known type like `char *`
          da_aappd (array, data);
        }
      ```

    Test program compilation:
      cc -x c -ggdb -Wall -Wextra -Werror \
        -D DYNA_IMPLEMENTATION -D DYNA_TEST -D_DA_DEBUG \
        -o dyna.out dyna.h
 **/
#ifndef DYNAMIC_ARRAY__H__
#define DYNAMIC_ARRAY__H__

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef DYNADEF
# define DYNADEF static inline
#endif

/**
 *  Ensure `da_idx` is a type with the size of
 *  your machine's word (default is `intptr_t`),
 *  to guarantee memory alignment of `dyna_t -> arr`.
 *  Otherwise, storing structs in `arr` may not be safe.
 */
#ifndef da_idx
# define da_idx intptr_t
# define da_sidx intptr_t
#endif

/**
 *  Internal struct
 *  Users do not need to interact with it directly
 */
typedef struct
{
  da_idx cap; /* capacity of array */
  da_idx size; /* length of array */
  da_idx cell_bytes; /* size of each cell */

  /* Actual bytes of array */
  char arr[];
} dyna_t;

#ifdef _DA_DEBUG
# include <stdio.h>
# define da_fprintd(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
# define da_dprintf(format, ...) \
  da_fprintd ("%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
# define da_fprintd(format, ...)
# define da_dprintf(format, ...)
#endif /* _DA_DEBUG */

/* Initial capacity */
#ifndef DA_INICAP
# define DA_INICAP 2
#endif

/* Growth factor */
#ifndef DA_GFACT
# define DA_GFACT 2
#endif
#ifndef DA_DO_GROW
/**
 *  By default, it doubles the capacity;
 *  you may want to increase the capacity
 *  linearly like: `cap += DA_GFACT`
 */
# define DA_DO_GROW(cap) ((cap) *= DA_GFACT)
// #define DA_DO_GROW(cap) ((cap) += DA_GFACT)
// #define DA_DO_GROW(cap) ((cap) = 1 + (cap) * 3 / 2)
#endif

#ifndef dyna_alloc
# define dyna_alloc(s) malloc (s)
# define dyna_realloc(p, news) realloc (p, news)
# define dyna_free(p) free (p)
#endif

/* The internal DA_OFFSETOF macro, gives offset of @member in dyna_t */
#ifndef DYNA_NO_STDDEF
# include <stddef.h>
# define DA_OFFSETOF(member) offsetof (dyna_t, member)
#else
# define DA_OFFSETOF(member) ((size_t)((dyna_t *)(0))->member)
#endif /* DYNA_NO_STDDEF */

/** The internal container_of macro
 *  Users don't use this macro directly
 *  This macro, gives a pointer to the 'parent'
 *  struct of @arr the array pointer
 */
# define DA_CONTAINEROF(ptr) \
  ((dyna_t *)((char *)(ptr) - DA_OFFSETOF (arr)))

/**
 *  Users normally don't use these functions
 *  and instead, they use provided macros for
 *  generic type purposes and safety
 */
DYNADEF dyna_t * __mk_da (da_sidx, da_sidx);
DYNADEF da_sidx __da_appd (void **);
DYNADEF void * __da_aappd (void **, da_sidx);
DYNADEF void * __da_dup (void **);

#define DA_NNULL(arr) (NULL != arr)


/**
 **  External macros
 **  To be used by the users of dyna.h
 **  All in O(1) (amortized)
 **/

// To free dynamic array @arr
#define da_free(arr) do {                           \
    if (DA_NNULL (arr)) {                           \
      dyna_t *__da__ = DA_CONTAINEROF (arr);        \
      da_dprintf ("Destroying dyna @%p\n", __da__); \
      dyna_free (__da__);                           \
    }} while (0)

// To get length and capacity of @arr
#define da_sizeof(arr) \
  (DA_NNULL (arr) ? DA_CONTAINEROF (arr)->size : 0)
#define da_capof(arr) \
  (DA_NNULL (arr) ? DA_CONTAINEROF (arr)->cap : 0)

// Gives how many cells left until the next reallocation (at overflow)
#define da_leftof(arr) \
  (DA_NNULL (arr) ? (da_sidx)da_capof (arr) - (da_sidx)da_sizeof (arr) : 0)

/**
 *  Create a new dynamic array
 *  Only create dyna arrays with these macros
 *  @T: type of array, for example (char) or (char *)
 *  @return: pointer to @T array which you can read
 *    from it as a normal `T array[n]`
 *    only use `da_xxx` macros to append to it or free it
 */
#define da_new(T) da_newn (T, DA_INICAP)
#define da_newn(T, n) \
  ((T *)( ( (dyna_t *) __mk_da (sizeof (T), n) )->arr ))

/**
 *  Duplicate a dynamic array
 *  returns a pointer to a new dynamic array
 *  which is a duplicate of @arr
 *  this also must be freed via `da_free`
 */
#define da_dup(arr) (DA_NNULL (arr) ? __da_dup ((void **)&arr) : NULL)

/**
 *  For loop macro
 *  usage:
 *    `da_for (arr, index, 0, index++) { arr[index]; }`
 */
#define da_for(__DA__, __IDX_NAME__, __IDX_START__, __IDX_NEXT__)   \
  for (da_idx __IDX_NAME__ = __IDX_START__,                         \
         __max_idx__ = da_sizeof (__DA__);                          \
       __IDX_NAME__ < __max_idx__;                                  \
       __IDX_NEXT__)

/**
 *  For each macro
 *  To iterate over all elements of a dynamic array
 *  It translates to a simple `for` statement
 */
#define da_foreach(__DA__, __IDX_NAME__) \
  da_for (__DA__, __IDX_NAME__, 0, ++(__IDX_NAME__))

/**
 *  Append to array macro
 *  @arr: the pointer that da_new has provided
 *  @val: value of type T, type of the array
 */
#define da_appd(arr, val) do {                          \
    if (NULL == arr) arr = da_new (typeof (*(arr)));    \
    _da_appd (arr, val);                                \
  } while (0)

/* _da_appd is not NULL safe */
#define _da_appd(arr, val) do {                             \
    da_sidx __da_idx__;                                     \
    if ((__da_idx__ = __da_appd ((void **)&(arr))) != -1) { \
      arr[__da_idx__] = (val);                              \
    }} while (0)

/**
 *  Appends a C array to a dynamic array
 *
 *  @dst_arr: destination dynamic array
 *  @src_arr: source array (normal C array)
 *  @len: length of @src_arr
 *    It will not be evaluated at each iteration
 */
#define da_appd_arr(dst_arr, src_arr, len) do {     \
    for (da_idx __idx = 0, __max_idx = len;         \
         __idx < __max_idx; __idx++) {              \
      da_appd (dst_arr, src_arr[__idx]);            \
    }} while (0)

/**
 *  Appenda one dynamic array to another
 *
 *  @dst_arr: destination  -  @src_arr: source
 *  It is safe to append an array to itself
 */
#define da_appd_da(dst_arr, src_arr) do {           \
    da_foreach (src_arr, __idx)                     \
      da_appd (dst_arr, src_arr[__idx]);            \
  } while (0)

/**
 *  Advanced many append
 *  (NOT safe to call from different scope)
 *
 *  This allocates enough space in @arr, and
 *  returns the appropriate index which can be used
 *  to copy the data to the array
 *
 *  @n: to allocate n entries in @arr
 */
#define da_many_appd(arr, n) ({ da_sidx __i = 0;    \
      if (NULL == arr)                              \
        arr = da_newn (typeof (*(arr)), n);         \
      __i = __da_many_appd ((void **)&(arr), n);    \
      __i; })

/**
 *  Drops contents of a dynamic array
 *  It only sets the size of @arr to zero,
 *  but will not free it's memory
 */
#define da_drop(arr) do {                           \
    if (DA_NNULL (arr)) {                           \
      dyna_t *__da__ = DA_CONTAINEROF (arr);        \
      __da__->size = 0;                             \
    }} while (0)

/**
 *  Appends @val to @arr from a different scope
 *  such as another function
 *
 *  @arr: pointer to the reference of the
 *        dynamic array (void *)
 */
#define da_aappd(arr, val) do {                     \
    if (NULL == *(void **)(arr))                    \
      *(void **)(arr) = da_new (typeof (val) *);    \
    _da_aappd (arr, val);                           \
  } while (0)

#define _da_aappd(arr, val) do {                                \
    typeof (val) *__arr__;                                      \
    if ((__arr__ = __da_aappd ((void **)(arr), sizeof (val))))  \
      *__arr__ = (val);                                         \
  } while (0)


#ifdef DYNA_IMPLEMENTATION

dyna_t *
__mk_da(da_sidx cell_size, da_sidx n)
{
  if (0 == n)
    n = 1; /* prevent 0 capacity initialization */
  size_t ptrlen = sizeof (dyna_t) + cell_size * n;
  dyna_t *da = (dyna_t *) dyna_alloc (ptrlen);
  da->cap = n;
  da->size = 0;
  da->cell_bytes = cell_size;

  da_dprintf ("Allocated dyna, cell_size: %luB, capacity: %lu, "
              "size: %luB (%luB metadata + %luB array)  @%p\n",
              (size_t) cell_size,
              (size_t) da->cap,
              (size_t) ptrlen,
              (size_t) sizeof (dyna_t),
              (size_t) (cell_size * n),
              da);
  return da;
}

DYNADEF da_sidx
__da_many_appd (void **arr, int n)
{
  dyna_t *da;
  size_t old_size, new_size;

  if (!(da = DA_CONTAINEROF (*arr)))
    return -1;

  old_size = da->size;
  da->size += n;
  if (da->size > da->cap)
    {
      da_dprintf ("Not enough space, size: %lu, needed: %lu\n",
                  (size_t) da->size,
                  (size_t) da->size + (size_t) n);
      {
        da->cap = da->size;
        new_size = sizeof (dyna_t) + da->cap * da->cell_bytes;
        da = dyna_realloc (da, new_size);
        if (!da)
          return -1;
        *arr = da->arr;
      }
      da_dprintf ("Reallocated, new capacity: %lu\n",
                  (size_t) da->cap);
    }

  return old_size++;
}

DYNADEF da_sidx
__da_appd (void **arr)
{
  dyna_t *da;
  size_t new_size;

  if (!(da = DA_CONTAINEROF (*arr)))
    return -1;

  if (da->size >= da->cap)
    {
      da_dprintf ("Overflow, size=cap: %lu, cell_size: %luB\n",
                  (size_t) da->cap,
                  (size_t) da->cell_bytes);
      {
        DA_DO_GROW (da->cap);
        new_size = sizeof (dyna_t) + da->cap * da->cell_bytes;
        da = dyna_realloc (da, new_size);
        if (!da)
          return -1;
        *arr = da->arr;
      }
      da_dprintf ("Reallocated, new capacity: %lu\n",
                  (size_t) da->cap);
    }

  return da->size++;
}

DYNADEF void *
__da_aappd (void **arr, da_sidx cell_bytes)
{
  da_sidx idx2append;
  if ((idx2append = __da_appd (arr)) == -1)
    return NULL;

  return *(char **)arr + cell_bytes * (idx2append);
}

DYNADEF void *
__da_dup (void **arr)
{
  dyna_t *da = DA_CONTAINEROF (*arr);
  size_t lenof_da = da->size * da->cell_bytes + sizeof (dyna_t);
  dyna_t *new_da = malloc (lenof_da);
  memcpy (new_da, da, lenof_da);
  return &new_da->arr;
}

#endif /* DYNA_IMPLEMENTATION */

#ifdef DYNA_TEST
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  char **inputs;
} Opt;


void
get_user_input (Opt *opt)
{
  char *p = NULL;
  size_t len;
  puts ("- Reading until EOF");
  while (1)
    {
      printf (">>> ");
      ssize_t n = getline (&p, &len, stdin);
      if (n < 0 || !p)
        break;
      if (n > 1)
        {
          p[n-1] = '\0';
          da_appd (opt->inputs, strdup (p));
        }
    }
  puts ("EOF.");
  puts ("------------------------------");
}

int
main (void)
{
  /* As inputs is NULL, we don't need da_new */
  Opt opt = {0};

  get_user_input (&opt);

  da_foreach (opt.inputs, idx)
    {
      printf ("input[%lu] -> `%s`\n", idx, opt.inputs[idx]);
      free (opt.inputs[idx]);
    }

  puts ("------------------------------");
  da_free (opt.inputs);
  return 0;
}
#endif /* DYNA_TEST */

#endif /* DYNAMIC_ARRAY__H__ */
