/**
 *  file: dyna.h
 *  created on: 8 Oct 2024
 *
 *  Dynamic Arrya implementation
 *  based on `templates/slice.c` within this repository
 *
 *  Usage:
 *  ```{c}
 *    #define DYNA_IMPLEMENTATION
 *    #include "dyna.h"
 *
 *    int
 *    main (void)
 *    {
 *      // char test
 *      char *carr = da_new (char);
 *
 *      for (char c ='a'; c <= 'z'; ++c)
 *        da_appd (carr, c);
 *      da_appd (carr, '\0');
 *
 *      puts (carr); // must print ab...z
 *      da_free (carr);
 *
 *
 *      // C string test
 *      char **cstr = da_new (char *);
 *
 *      da_appd (cstr, "string0");
 *      da_appd (cstr, "string1");
 *      da_appd (cstr, "string2");
 *
 *      for (idx_t i=0; i < da_sizeof (cstr); ++i)
 *        printf ("str%lu: {%s}\n", i, cstr[i]);
 *
 *      da_free (cstr);
 *      return 0;
 *    }
 *  ```
 *
 *  Options:
 *    `_DA_DEBUG`:  to print some debugging information
 *    `DA_INICAP`:  the default initial capacity of arrays
 *    `DA_DO_GRO`:  to define how arrays grow
 *    `DA_GFACT`:   growth factor (see the source code)
 **/
#ifndef DYNAMIC_ARRAY__H__
#define DYNAMIC_ARRAY__H__

#include <string.h>
#include <stdlib.h>

#ifndef DADEFF
# define DADEFF static inline
#endif

/* array index types */
#ifndef idx_t
# define sidx_t ssize_t
# define idx_t size_t
#endif


#ifdef _DA_DEBUG
# include <stdio.h>
# define fprintd(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
# define dprintf(format, ...) \
  fprintd ("[debug %s:%d] "format, __func__, __LINE__, ##__VA_ARGS__)
#else
# define fprintd(format, ...)
# define dprintf(format, ...)
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
# define dyna_alloc malloc
#endif
#ifndef dyna_realloc
# define dyna_realloc realloc
#endif

/* users don't need to work with this */
typedef struct
{
  idx_t cap;
  idx_t size;
  int arr_byte; /* size of a sell */

  char arr[];
} Darray;


/* internal offsetof macro, to give offset of @member in struct @T */
#define offsetof(T, member) ((size_t)((T *)(0))->member)

/** internal container_of macro
 *  users don't use this macro directly
 *  this macro, gives a pointer to the 'parent'
 *  struct of @arr the array pointer
 */
#define __da_containerof(arr_ptr) ({                        \
      const char *__ptr = (const char *)(arr_ptr);          \
      (Darray *)(__ptr - offsetof (Darray, arr));           \
    })

/**
 *  users normally don't use these functions
 *  and instead, use provided macros for
 *  generic type purposes and safety
 */
#define DADECLARE(name, ret, ...) DADEFF ret name (__VA_ARGS__)
DADECLARE (__mk_da, Darray*, int, int);
DADECLARE (__da_appd, sidx_t, void**);


/**
 *  External macros
 *  to be used by users
 */
// to free dynamic array @arr
#define da_free(arr) free (__da_containerof (arr))
// to get length and capacity of @arr
#define da_sizeof(arr) (__da_containerof (arr)->size)
#define da_capof(arr) (__da_containerof (arr)->cap)
// gives how many sells left until the next reallocation (at overflow)
#define da_leftof(arr) ((sidx_t)da_capof (arr) - (sidx_t)da_sizeof (arr))

/** da_new, da_newn
 *  only create `da` dynamic arrays with these macros
 *  @T: type of array, for example (char) or (char *)
 *  @return: pointer to an @T array (pointer)
 *    which can be used as usual (like: `T array[.]`)
 */
#define da_new(T) da_newn (T, DA_INICAP)
#define da_newn(T, n) ({                         \
      Darray *__da = __mk_da (sizeof (T), n);    \
      (T *)(__da->arr);                          \
    })

/**
 *  append to array macro
 *  @arr: the pointer that da_new has provided
 *  @val: value of type T, type of the array
 */
#define da_appd(arr, val) do {                          \
    sidx_t i;                                           \
    if ((i = __da_appd ((void **)&arr)) != -1) {        \
      arr[i] = val;                                     \
    }} while (0)


#ifdef DYNA_IMPLEMENTATION

/** to make dynamic arrays
 *  with each sell of length @sizeof_arr
 *  and the initial capacity @n
 */
Darray *
__mk_da(int sizeof_arr, int n)
{
  size_t ptrlen = sizeof (Darray) + sizeof_arr * n;
  Darray *da = (Darray *) malloc (ptrlen);
  da->cap = n;
  da->size = 0;
  da->arr_byte = sizeof_arr;
  dprintf ("Dyna was allocated @%p[.%lu]\n", da, ptrlen);
  return da;
}

sidx_t
__da_appd (void **arr)
{
  Darray *da;
  if (!(da = __da_containerof (*arr)))
    return -1;
  if (da->size >= da->cap)
    {
      dprintf ("Overflow @%p, size=cap:%-2lu, arr_byte:%-2d --> new size:",
              da, da->cap, da->arr_byte);
      DA_DO_GROW (da->cap);
      size_t new_size = sizeof(Darray) + da->cap * da->arr_byte;
      fprintd (" %lu\n", new_size);
      da = realloc (da, new_size);
      if (!da)
        return -1;
      *arr = da->arr;
    }

  return da->size++;
}

#endif /* DYNA_IMPLEMENTATION */
#endif /* DYNAMIC_ARRAY__H__ */
