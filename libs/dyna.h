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
  printf ("[debug %s:%d] "format, __func__, __LINE__, ##__VA_ARGS__)
#else
# define fprintd(format, ...)
# define dprintf(format, ...)
#endif /* _DA_DEBUG */

/* initial capacity */
#ifndef DA_INICAP
# define DA_INICAP 1
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
#define da_free(arr) free (da_headerof (arr))
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
      dprintf ("Overflow @%p: size:%lu, cap:%lu, arr_byte:%lu --> new size:",
              da, da->size, da->cap, da->arr_byte);
      da->cap *= 2;
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
