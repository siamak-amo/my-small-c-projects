/** file: dyna.h
    created on: 8 Oct 2024
  
    Dynamic Arrya implementation
    based on `templates/slice.c` within this repository
  
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
      char *carr = da_newn (char, 10);
  
      for (char c ='a'; c <= 'z'; ++c)
        da_appd (carr, c);
      da_appd (carr, '\0');
  
      puts (carr); // must print ab...z
      da_free (carr);
  
  
      // C string array
      char **cstr = da_new (char *);
  
      da_appd (cstr, "string0");
      da_appd (cstr, "string1");
      da_appd (cstr, "string2");
  
      // Print & Free
      for (size_t i=0; i < da_sizeof (cstr); ++i)
        printf ("str%lu: {%s}\n", i, cstr[i]);
      da_free (cstr);
  
  
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
  
      // Print & Free
      da_foreach (ptr_arr, i)
        {
          printf ("ptr_arr[%i] - index: %d\n", i, ptr_arr[i]->index);
          free (ptr_arr[i]);
        }
      da_free (ptr_arr);
  
      return 0;
    }
    ```
  
    Options:
      `_DA_DEBUG`:  to print some debugging information
      `DA_INICAP`:  the default initial capacity of arrays
      `DA_DO_GRO`:  to define how arrays grow
      `DA_GFACT`:   growth factor (see the source code)
  
    WARNING:
      If an instance of this dynamic array, is being stored
      outside of scope of a function (F), you *MUST NOT* use
      `da_appd` inside F, because if an overflow occurs,
      `da_appd` will need to reallocate it's entire memory
      and so the original pointer might get freed and
      potentially causes use after free or SEGFAULT
  
      A solution would be to store the reference inside
      some struct and pass the reference of it the function F
      so `da_appd` will update the reference properly
  
      Another solution would be to use `da_funappd` macro:
      ```c
        // scope 1
        {
          T val = {0};
          T *arr = da_new (T);
          my_function ((void **) &arr, val);
        }
  
        void
        my_function (void **array, T data)
        {
          // append data to array
          // this might update @arr in scope 1
          da_funappd (array, data);
        }
      ```
 **/
#ifndef DYNAMIC_ARRAY__H__
#define DYNAMIC_ARRAY__H__

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef DADEFF
# define DADEFF static inline
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

/* users don't need to work with this struct */
typedef struct
{
  da_idx cap; /* capacity of array */
  da_idx size; /* length of array */
  da_idx cell_bytes; /* size of each cell */

  /* actual bytes of array */
  char arr[];
} dyna_t;

#ifdef _DA_DEBUG
# include <stdio.h>
# define da_fprintd(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
# define da_dprintf(format, ...) \
  da_fprintd ("[dyna %s:%d] "format, __func__, __LINE__, ##__VA_ARGS__)
#else
# define da_fprintd(format, ...)
# define da_dprintf(format, ...)
#endif /* _DA_DEBUG */

/* initial capacity */
#ifndef DA_INICAP
# define DA_INICAP 2
#endif

/* arrays growth factor and method */
#ifndef DA_GFACT
# define DA_GFACT 2
#endif
#ifndef DA_DO_GROW
/**
 *  by default, it doubles the capacity
 *  you might want to increase the capacity
 *  like: `cap += DA_GFACT`
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

/* internal __OFFSETOF macro, to give offset of @member in struct @T */
#define __OFFSETOF(T, member) ((size_t)((T *)(0))->member)

/** internal container_of macro
 *  users don't use this macro directly
 *  this macro, gives a pointer to the 'parent'
 *  struct of @arr the array pointer
 */
#define __DA_CONTAINEROF(arr_ptr) ({                      \
      const char *__ptr__ = (const char *)(arr_ptr);      \
      (dyna_t *)(__ptr__ - __OFFSETOF (dyna_t, arr));     \
    })

/**
 *  users normally don't use these functions
 *  and instead, use provided macros for
 *  generic type purposes and safety
 */
DADEFF dyna_t * __mk_da (da_sidx, da_sidx);
DADEFF da_sidx __da_appd (void **);
DADEFF void * __da_funappd (void **, da_sidx);
DADEFF void * __da_dup (void **);

#define DA_NNULL(arr) (NULL != arr)
/**
 *  External macros
 *  to be used by users
 */
// to free dynamic array @arr
#define da_free(arr) do {                          \
    if (DA_NNULL (arr)) {                          \
      dyna_t *__da__ = __DA_CONTAINEROF (arr);     \
      da_dprintf ("destroying %p\n", __da__);      \
      dyna_free (__da__);                          \
    }} while (0)

// to get length and capacity of @arr
#define da_sizeof(arr)                                  \
  (DA_NNULL (arr) ? __DA_CONTAINEROF (arr)->size : 0)
#define da_capof(arr) \
  (DA_NNULL (arr) ? __DA_CONTAINEROF (arr)->cap : 0)

// gives how many cells left until the next reallocation (at overflow)
#define da_leftof(arr) \
  (DA_NNULL (arr) ? (da_sidx)da_capof (arr) - (da_sidx)da_sizeof (arr) : 0)

/** da_new, da_newn
 *  only create `da` dynamic arrays with these macros
 *  @T: type of array, for example (char) or (char *)
 *  @return: pointer to @T array which you can read
 *    from it as a normal `T array[n]`
 *    only use `da_xxx` macros to append to it or free it
 */
#define da_new(T) da_newn (T, DA_INICAP)
#define da_newn(T, n) ({                          \
      dyna_t *__da__ = __mk_da (sizeof (T), n);   \
      (T *)(__da__->arr);                         \
    })

/**
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
#define da_appd(arr, val) do {                              \
    da_sidx __da_idx__;                                     \
    if ((__da_idx__ = __da_appd ((void **)&arr)) != -1) {   \
      arr[__da_idx__] = val;                                \
    }} while (0)

/**
 *  append an array to array macro
 *  @arr: dynamic array
 *  @src_arr: input array (to be appended to @arr)
 *  @len: length of @src_arr
 */
#define da_appd_arr(arr, src_arr, len) do {         \
    for (size_t __idx = 0; __idx < len; __idx++) {  \
      da_appd (arr, src_arr[__idx]);                \
    }} while (0)

/**
 *  drop array
 *  only sets size of @arr to zero
 *  Appenda one dynamic array to another
 *
 *  @dst_arr: destination  -  @src_arr: source
 *  It is safe to append an array to itself
 */
#define da_appd_da(dst_arr, src_arr) do {           \
    da_foreach (src_arr, __idx)                     \
      da_appd (dst_arr, src_arr[__idx]);            \
  } while (0)

 */
#define da_drop(arr) do {                           \
    if (DA_NNULL (arr)) {                           \
      dyna_t *__da__ = __DA_CONTAINEROF (arr);      \
      __da__->size = 0;                             \
    }} while (0)

/**
 *  in order to append @val to @arr from
 *  a different scope (like another function),
 *  as the primary pointer to @arr, sometimes needs
 *  to be updated, you can use this macro
 *  @arr must be (void **) pointing to the address
 *  of the primary array
 */
#define da_funappd(arr, val) do {                       \
    typeof (val) *__arr__;                              \
    if ((__arr__ = __da_funappd (arr, sizeof (val))))   \
      *__arr__ = val;                                   \
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

  da_dprintf ("allocated @%p, cell_size: %luB, "
              "size: %luB (%luB metadata + %luB array)\n",
              da,
              (size_t) cell_size,
              (size_t) ptrlen,
              (size_t) sizeof (dyna_t),
              (size_t) (cell_size * n));
  return da;
}

DADEFF da_sidx
__da_appd (void **arr)
{
  dyna_t *da;
  size_t new_size;

  if (!(da = __DA_CONTAINEROF (*arr)))
    return -1;

  if (da->size >= da->cap)
    {
      da_dprintf ("overflow %p, size=cap:%lu, cell_size:%luB\n",
                  da,
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
      da_dprintf ("realloc @%p, new size: %luB\n",
                  da,
                  (size_t) new_size);
    }

  return da->size++;
}

DADEFF void *
__da_funappd (void **arr, da_sidx cell_bytes)
{
  da_sidx idx2append;
  if ((idx2append = __da_appd (arr)) == -1)
    return NULL;

  return *(char **)arr + cell_bytes * (idx2append);
}

DADEFF void *
__da_dup (void **arr)
{
  dyna_t *da = __DA_CONTAINEROF (*arr);
  size_t lenof_da = da->size * da->cell_bytes + sizeof (dyna_t);
  dyna_t *new_da = malloc (lenof_da);
  memcpy (new_da, da, lenof_da);
  return &new_da->arr;
}

#endif /* DYNA_IMPLEMENTATION */
#endif /* DYNAMIC_ARRAY__H__ */
