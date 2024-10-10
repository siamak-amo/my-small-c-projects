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
 *      // char array
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
 *      // C string array
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
 *
 *
 *      // struct array
 *      struct data {
 *        int num;
 *      };
 *      struct data *arr = da_newn (struct data, 4);
 *
 *      for (int i=0; i<7; ++i)
 *      {
 *        struct data tmp = {i};
 *        da_appd (arr, tmp);
 *      }
 *
 *      for (int i=0; i<7; ++i)
 *        printf ("data[%i] - num: %d\n", i, arr[i].num);
 *
 *      da_free (arr);
 *
 *      return 0;
 *    }
 *  ```
 *
 *  Options:
 *    `_DA_DEBUG`:  to print some debugging information
 *    `DA_INICAP`:  the default initial capacity of arrays
 *    `DA_DO_GRO`:  to define how arrays grow
 *    `DA_GFACT`:   growth factor (see the source code)
 *
 *  WARNING:
 *    If an instance of this dynamic array, is being stored
 *    outside of scope of a function (F), you *MUST NOT* call
 *    `da_appd` inside the F, because if an overflow happens,
 *    `da_appd` will need to reallocate memory and so the original
 *    pointer outside of F gets freed and potentially cause
 *    use after free or SEGFAULT
 *    A solution would be to store the reference inside
 *    some struct and pass the reference of it the function F
 *    so `da_appd` will update the reference properly
 *
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
# define sidx_t int
# define idx_t unsigned int
#endif


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

/* users don't need to work with this */
typedef struct
{
  idx_t cap;
  idx_t size;
  idx_t cell_bytes; /* size of a cell */

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


#define DA_NNULL(arr) (NULL != arr)
/**
 *  External macros
 *  to be used by users
 */
// to free dynamic array @arr
#define da_free(arr) do {                         \
    if (DA_NNULL (arr)) {                         \
      Darray *__d = __da_containerof (arr);       \
      dyna_free (__d);                            \
      da_dprintf ("destroying %p\n", __d);        \
    }} while (0)

// to get length and capacity of @arr
#define da_sizeof(arr) \
  (DA_NNULL (arr) ? __da_containerof (arr)->size : 0)
#define da_capof(arr) \
  (DA_NNULL (arr) ? __da_containerof (arr)->cap : 0)

// gives how many cells left until the next reallocation (at overflow)
#define da_leftof(arr) \
  (DA_NNULL (arr) ? (sidx_t)da_capof (arr) - (sidx_t)da_sizeof (arr) : 0)

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
 *  with each cell of length @cell_size
 *  and the initial capacity @n
 */
Darray *
__mk_da(int cell_size, int n)
{
  if (0 == n)
    n = 1; /* prevent 0 capacity initialization */
  size_t ptrlen = sizeof (Darray) + cell_size * n;
  Darray *da = (Darray *) dyna_alloc (ptrlen);
  da->cap = n;
  da->size = 0;
  da->cell_bytes = cell_size;
  da_dprintf ("allocated @%p, cell_size: %dB, "
              "size: %luB (%luB metadata + %luB array)\n",
              da, cell_size, ptrlen, sizeof (Darray), cell_size * n);
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
      da_dprintf ("overflow @%p, size=cap:%-2lu, cell_size:%dB\n",
              da, da->cap, da->cell_bytes);
      DA_DO_GROW (da->cap);
      size_t new_size = sizeof(Darray) + da->cap * da->cell_bytes;
      da = dyna_realloc (da, new_size);
      if (!da)
        return -1;
      *arr = da->arr;
      da_dprintf ("reallocated @%p, new size: %luB\n", da, new_size);
    }

  return da->size++;
}

#endif /* DYNA_IMPLEMENTATION */
#endif /* DYNAMIC_ARRAY__H__ */
