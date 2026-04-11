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

    ** Macros in this library may evaluate parameters multiple times **
    **   Do NOT use function calls in argument(s) of da_xxx macros   **

    Usage:
    ```c
    #include <stdio.h>
    #include <stdlib.h>
  
    // Optionally, you can define these macros to determine
    // how the dynamic array grows.
    // In this example, capacity increases linearly by DA_GFACT.
    // By default, it grows exponentially by a factor of 1.5.
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
  
  
      // const C string array
      char **cstr = NULL;
  
      da_appd (cstr, "string0");
      da_appd (cstr, "string1");
      da_appd (cstr, "string2");


      // Struct array
      struct data {
        int index;
      };
      struct data *arr = NULL;

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
      char **arr = NULL;
      const char *source[] = {"a", "b", "c"};
      da_appd_arr (arr, source, 3);
      da_appd_aarr (arr, source); // when the length is known

      // Append a dynamic array to another one
      da_appd_da (cstr, arr);

      // Push, Pop, Top
      da_push (arr, "x");               // equivalent to append
      const char *TOP = da_top (arr);   // TOP is equals to "x"
                                        // or use `da_top_idx`
      da_pop (arr);                     // pop the top element
      da_popn (arr, 2);                 // pop the last 2 elements

      // Delete element
      da_unordered_delete (arr, 2);     // delete index 2
      // before:  @arr = [a, b, c, a, b, c, x]
      //                        ^-----------^ replace top and pop
      //  after:  @arr = [a, b, x, a, b, c]

      return 0;
    }
    ```
  
    Options:
      `_DA_DEBUG`:  to print some debugging information
      `DA_INICAP`:  the default initial capacity of arrays
      `DA_DO_GROW`: to define how arrays grow
      `DA_GFACT`:   growth factor (see the source code)
      `DA_FORCE_MEMCPY`:
                    to force using memcpy for assignments
  
    WARNING:
     The append macros should NOT be used to append to
     pointers from a different scope (e.g. in a function)
     as the reallocation of that pointer, will not update
     the primary pointer (so make it dangling pointer).

     Solutions:
      * Using a wrapper struct
      ```c
      struct my_data {
        char **array;  // dynamic array
      };

      void my_function (struct my_data *d)
      {
        da_appd (d->array, value);
      }
      ```

      * Using `da_aappd` macro:
      ```c
      char **array = NULL;  // The primary dynamic array

      // For pointers
      {
        void *ptr = &array;  // pointer to a dynamic array
        da_aappd (ptr, value);
      }

      // For functions
      void my_function (void *ptr)
      {
        da_aappd (ptr, value);
      }
      // usage: my_function (&array);
      ```

    Test program compilation:
      cc -xc -ggdb -Wall -Wextra -Werror \
        -D DYNA_IMPLEMENTATION -D DYNA_TEST -D_DA_DEBUG \
        -o dyna.test dyna.h
 **/
#ifndef DYNA__H__
#define DYNA__H__

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define DynaVersion "2.2"

#ifndef DYNADEF
# define DYNADEF static inline
#endif

#ifndef da_idx
# define da_idx intptr_t
# define da_sidx intptr_t
#endif

#ifndef _Nullable
#define _Nullable
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
#if defined(__GNUC__) || defined(__clang__)
  char arr[] __attribute__((aligned(8)));
#else
  void *__dummy; /* for alignment */
  char arr[];
#endif

} dyna_t;


/* Aliases to Dyna macros */
#define da_length            da_sizeof    /* occupied length of array */
#define da_capacity          da_capof     /* total  capacity of array */
#define da_push              da_appd      /* append  element to array */
#define da_push_many         da_appd_arr  /* many append */
#define da_top               da_last      /* get trailing element */
#define da_pop               da_pop1      /* delete 1 element */
#define da_pop_many          da_popn      /* many delete */


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

#ifndef DA_DO_GROW
/**
 *  Capacity grow at overflow macro
 */
# ifndef DA_GFACT
#  define DA_GFACT 1.5  /* Growth factor */
# endif
# define DA_DO_GROW(cap) ((cap) *= DA_GFACT)
/**
 *  Linear grow example:
 *  #define DA_DO_GROW(current_cap) ((current_cap) += 8)
 */
#endif

#ifndef dyna_alloc
# define dyna_alloc(s) malloc (s)
# define dyna_realloc(p, news) realloc (p, news)
# define dyna_free(p) free (p)
#endif

/* Internal DYNA_OFFSETOF, gives offset of @member in dyna_t */
#ifndef DYNA_NO_STDDEF
# include <stddef.h>
# define DYNA_OFFSETOF(member) offsetof (dyna_t, member)
#else
# define DYNA_OFFSETOF(member) ((size_t)((dyna_t *)(0))->member)
#endif /* DYNA_NO_STDDEF */

/** The internal container_of macro
 *  Users don't use this macro directly
 *  This macro, gives a pointer to the 'parent'
 *  struct of @arr the array pointer
 */
# define DA_CONTAINEROF(da_ptr) \
  ((dyna_t *)((char *)(da_ptr) - DYNA_OFFSETOF (arr)))

/**
 *  Users normally don't use these functions
 *  and instead, they use provided macros for
 *  generic type purposes and safety
 */
DYNADEF dyna_t * __mk_da (int n, int cell_bytes);
DYNADEF void * __da_dup (void *);
DYNADEF int __da_popn (void *, int n);
DYNADEF da_sidx __da_allocate (void *, int n, int cell_bytes);
#define __da_allocate1(ptr, cell_bytes) \
  __da_allocate (ptr, 1, cell_bytes)

/**
 *  If DA_FORCE_MEMCPY is defined, none of the append macros,
 *  will accept literals values, e.g. da_appd(arr, 5) will create a
 *  compilation error, use  `int n=5; da_appd(arr, n);` instead.
 */
#ifndef DA_ASSIGN
# ifndef DA_FORCE_MEMCPY
    /* compilers should optimize this, for large @rval */
#  define DA_ASSIGN(lptr, rval) (*(lptr) = (rval))
# else
#  define DA_ASSIGN(lptr, rval) memcpy (lptr, &(rval), sizeof (rval))
# endif
#endif /* DA_ASSIGN */

/**
 **  External macros
 **  To be used by the users of dyna.h
 **  All in O(1) (amortized)
 **/

// To free dynamic array @arr
#define da_free(arr) do {                           \
    if (NULL != arr) {                              \
      dyna_t *__da__ = DA_CONTAINEROF (arr);        \
      da_dprintf ("Destroying dyna @%p\n", __da__); \
      dyna_free (__da__);                           \
    }} while (0)

// To get length and capacity of @arr
#define da_sizeof(arr) \
  ((NULL != arr) ? DA_CONTAINEROF (arr)->size : 0)
#define da_capof(arr) \
  ((NULL != arr) ? DA_CONTAINEROF (arr)->cap : 0)

// Gives how many cells left until the next reallocation (at overflow)
#define da_leftof(arr) \
  ((NULL != arr) ? (da_sidx) da_capof (arr) - (da_sidx) da_sizeof (arr) : 0)

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
  ((T *)( ( (dyna_t *) __mk_da (n, sizeof (T)) )->arr ))

/**
 *  Duplicate a dynamic array
 *  returns a pointer to a new dynamic array
 *  which is a duplicate of @arr
 *  this also must be freed via `da_free`
 */
#define da_dup(arr) ((NULL != arr) ? __da_dup (&(arr)) : NULL)

/**
 *  For each macro
 *  To iterate over all elements of a dynamic array
 *  It gets expanded to a simple `for` statement
 */
#define da_foreach(__DA__, __IDX_NAME__) \
  da_for (__DA__, __IDX_NAME__, 0, ++(__IDX_NAME__))

#define da_for(__DA__, __IDX_NAME__, __IDX_START__, __IDX_NEXT__)   \
  for (da_idx __IDX_NAME__ = __IDX_START__,                         \
         __max_idx__ = da_sizeof (__DA__);                          \
       __IDX_NAME__ < __max_idx__;                                  \
       __IDX_NEXT__)

/**
 *  Append to array macro
 *  @arr: pointer to the dynamic array
 *  @val: value of type T, to be appended to @arr
 */
#define da_appd(arr, val) do {                  \
    da_sidx __da_idx__ = da_allocate1 (arr);    \
    if (-1 != __da_idx__)                       \
      DA_ASSIGN ((arr) + __da_idx__, val);      \
  } while (0)

/**
 *  Appends a C array to a dynamic array
 *  Use da_appd_aarr when length of array is known at compile time
 *
 *  @dst_arr: destination dynamic array
 *  @src_arr: source array (normal C array)
 *  @count: length of @src_arr
 */
#define da_appd_arr(dst_arr, src_arr, count) do {           \
    int cell_b = sizeof (*(src_arr));                       \
    da_idx idx = __da_allocate (&(dst_arr), count, cell_b); \
    if (-1 != idx)                                          \
      memcpy (dst_arr + idx, src_arr, (count)*cell_b);      \
  } while (0)

/* Append constant array */
#define da_appd_aarr(dst_arr, src_arr) \
  da_appd_arr (dst_arr, src_arr, sizeof(src_arr) / sizeof(*(src_arr)))

/**
 *  Appenda one dynamic array to another
 *
 *  @dst_arr: destination  -  @src_arr: source
 *  It is safe to append an array to itself
 */
#define da_appd_da(dst_arr, src_arr) do {                   \
    int count = da_sizeof (src_arr);                        \
    if (count > 0) da_appd_arr (dst_arr, src_arr, count);   \
  } while (0)

/**
 *  Allocate memory for arrays
 *  (NOT safe to call from different scope)
 *
 *  This allocates enough space in @arr, and
 *  returns the appropriate index which can be used
 *  to copy the data to the array
 *
 *  @n: to allocate n entries in @arr
 */
#define da_allocate(arr, n) __da_allocate (&arr, n, sizeof (*(arr)))
#define da_allocate1(arr) __da_allocate1 (&arr, sizeof (*(arr)))

/**
 *  Drops contents of a dynamic array
 *  It only sets the size of @arr to zero,
 *  but will not free it's memory
 */
#define da_drop(arr) do {                           \
    if (NULL != arr) {                              \
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
#define da_aappd(arr, val) do {                             \
    da_idx i = __da_allocate1 (arr, sizeof (val));          \
    if (i >= 0) {                                           \
      char *__arr__ = *(char **)(arr) + i * sizeof (val);   \
      DA_ASSIGN ((typeof (val) *)__arr__, val);             \
    }                                                       \
  } while (0)

/**
 *  Delete element (pop) macros
 *  Only makes them reusable, does not free the memory
 *
 *  popn: deletes the last @n elements of array
 *  pop1: deletes the latest element of array
 */
#define da_popn(arr, n) __da_popn (&(arr), n)
#define da_pop1(arr) __da_popn (&(arr), 1)

/* Get the trailing element of @arr */
#define da_last(arr) arr[da_top_idx (arr)]

/**
 *  Delete Unordered macros
 *  To delete arbitrary index of array, when the order
 *  of the array is not important.
 */
#define da_unordered_delete(arr, idx) do {      \
    da_idx N = da_sizeof (arr);                 \
    if (N > 0  &&  idx >= 0  &&  idx < N) {     \
      if (idx < N-1)                            \
        DA_ASSIGN ((arr) + idx, arr[N-1]);      \
      da_pop1 (arr);                            \
    }                                           \
  } while (0)

/* Index of the last element of @arr */
DYNADEF int da_top_idx (void *_Nullable arr);

#ifdef DYNA_IMPLEMENTATION

dyna_t *
__mk_da (int n, int cell_size)
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

  assert (0 == ((uintptr_t) da->arr % (sizeof (void *))) &&
          "Broken alignment!"); /* this should be unreachable */
  return da;
}

DYNADEF da_sidx
__da_allocate (void *__arr, int n, int cell_bytes)
{
  dyna_t *da;
  void **arr = (void **)__arr;
  size_t old_size, new_size;

  if (! *arr)
    {
      da = __mk_da (n, cell_bytes);
      *arr = da->arr;
    }
  else if (!(da = DA_CONTAINEROF (*arr)))
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

/** DEPRECATED CODE SECTION **
 ** Do not use these functions, use __da_allocate instead
 **
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
**
**/

DYNADEF void *
__da_dup (void *__arr)
{
  void **arr = (void **)__arr;
  dyna_t *da = DA_CONTAINEROF (*arr);
  size_t lenof_da = da->size * da->cell_bytes + sizeof (dyna_t);
  dyna_t *new_da = malloc (lenof_da);
  memcpy (new_da, da, lenof_da);
  return &new_da->arr;
}

/* Returns how many elements have been deleted */
DYNADEF int
__da_popn (void *_Nullable __arr, int n)
{
  if (!__arr || n <= 0)
    return 0;
  dyna_t *da = DA_CONTAINEROF (*(void **)__arr);
  if (da->size < n)
    n = da->size;
  da->size -= n;
  return n;
}

DYNADEF int
da_top_idx (void *_Nullable __arr)
{
  if (! __arr)
    return 0;
  dyna_t *da = DA_CONTAINEROF (__arr);
  if (da->size)
    return da->size - 1;
  return 0;
}

#endif /* DYNA_IMPLEMENTATION */
#undef _Nullable
#endif /* DYNA__H__ */


#ifdef DYNA_TEST
#include <stdio.h>
#include <stdlib.h>

#define LOG(fmt, ...) fprintf (stderr, fmt, ##__VA_ARGS__)
#define STRCOLOR(code, cstr) "\033[" #code "m" cstr "\033[0m"
#define STRGREEN(cstr) STRCOLOR (32, cstr) /* Green string */
#define STRRED_B(cstr) STRCOLOR (41, cstr) /* Red background string */

/* Test assert macro, returns true if assertion success */
#define tassert(expression, msg)                         \
  ({                                                     \
    LOG ("Test: %s...  ", msg);                          \
    int exp_v = expression;                              \
    if (! exp_v) {                                       \
      LOG ("\n%s:%d:  '%s' failed.\n"                    \
           "     assertion: `%s'  ",                     \
           __FILE__, __LINE__, msg, #expression);        \
      do { FAIL (); } while (0);                         \
    } else PASS ();                                      \
    (exp_v == true);  /* return value */                 \
  })


int
main (void)
{
#define PASS() LOG (STRGREEN("pass") ".\n")
#define FAIL() LOG (STRRED_B("FAIL") ".\n"); return EXIT_FAILURE;

  puts (" * Elementary tests *");
  {
    char **strings = NULL;

    char *s0 = "constant:s0";
    da_appd (strings, s0);
    tassert (strings != NULL  &&  da_sizeof(strings) == 1,
          "append value to NULL array test");

    const char *s1 = "malloc:s1";
    da_appd (strings, strdup (s1));

    tassert (strings[0] == s0 &&
          strcmp (strings[1], s1) == 0,
          "expected values in the array");
    tassert (da_sizeof(strings) == 2, "size of array test");
  }

  puts ("\n * Longer struct array *");
  {
    struct data {int a,b,c,d;} tmp = {.a=0, .b=1, .c=2, .d=3};
    struct data *data_array = NULL;

    da_appd (data_array, tmp);
    da_appd (data_array, tmp);
    da_appd (data_array, tmp);

    struct data d3 = data_array[2];
    tassert (d3.a == 0  &&  d3.b == 1 &&
             d3.c == 2  &&  d3.d == 3, "struct value test");
  }

  puts ("\n * Many append and duplicate test *");
  {
    int *numbers = NULL;

    int arr[] = {1, 2};
    da_appd_aarr (numbers, arr);
    tassert (numbers[0] == 1  && numbers[1] == 2,
          "basic da_appd_aarr test");

    arr[0] = 10; arr[1] = 20;
    da_appd_arr (numbers, arr, 2);
    tassert (numbers[0] == 1  && numbers[1] == 2 &&
          numbers[2] == 10 && numbers[3] == 20,
          "after data source change");

    /* make a copy of @numbers, and append @numbers to itself */
    int *numbers2 = da_dup (numbers);

    da_appd_da (numbers, numbers);
    tassert (da_sizeof (numbers) == 8,
             "length check after da_append_da");
    tassert (memcmp (numbers, &numbers[4], 4*sizeof(int)) == 0,
             "value check after da_append_da");

    /* Now, modify the primary array, @numbers2 must be preserved */
    memset (numbers, 0, 4*sizeof(int));
    tassert (da_sizeof (numbers2) == 4,
             "length of duplicated array is intact");
    tassert (memcmp (numbers2, &numbers[4], 4*sizeof(int)) == 0,
             "contents of duplicated array is intact");
  }

  puts ("\n * Advanced many append test *");
  {
    int *numbers = NULL;

    /* array is empty, so da_allocate should return index 0 */
    int idx = da_allocate (numbers, 3);
    tassert (idx == 0 && da_capof (numbers) == 3,
             "da_allocate for NULL array");

    /* now, @numbers have 3 elements, so the next
       da_allocate should start from index 3 */
    idx = da_allocate (numbers, 4);
    tassert (idx == 3, "da_allocate on a None NULL array");
    tassert (da_sizeof(numbers) == 7, "sizeof array after allocate");
  }

  int *numbers = NULL;
  puts ("\n * Advanced append (different scope) test *");
  {
    /* simulate a function call:  `void fun(void *ptr) {...}` */
    void *ptr = &numbers;

    int v = 666;
    da_aappd (ptr, v);
    tassert (numbers != NULL, "initializing after da_aappd call");
    tassert (da_sizeof (numbers) == 1  &&  numbers[0] == v,
             "value correctly appended after da_aappd call");

    v = 1;  da_aappd (ptr, v);
    v = 2;  da_aappd (ptr, v);
    tassert (da_sizeof (numbers) == 3,
             "sizeof array after multiple da_aappd calls");
    tassert (numbers[0] == 666 &&
             numbers[1] == 1   &&  numbers[2] == 2,
             "correct values on array after da_aappd calls");
  }

  puts ("\n * Pop / Top / Delete test *");
  {
    char *carr = NULL;
    tassert (da_top_idx (carr) == 0, "top index of empty array");

    da_appd_arr (carr, "0123456789abcdef", 16); /* avoid \0 byte */
    tassert (da_top_idx (carr) == 15, "top index test");
    tassert (da_top (carr) == 'f', "top value test");

    da_popn (carr, 10);
    tassert (da_sizeof (carr) == 6,
             "sizeof array after popN 6 elements");
    tassert (da_top_idx (carr) == 5, "top index after popN");

    da_pop(carr);  da_pop(carr);
    tassert (da_sizeof (carr) == 4,
             "sizeof array after 2 other pops");
    tassert (strncmp (carr, "0123", 4) == 0,
             "check content of array after pop");

    da_unordered_delete (carr, 1);
    tassert (da_sizeof (carr) == 3, "delete unordered element");
    tassert (strncmp (carr, "032", 3) == 0, "content after delete");

    da_unordered_delete (carr, 0);
    tassert (da_sizeof (carr) == 2, "delete unordered element");
    tassert (strncmp (carr, "23", 2) == 0, "content after delete");

    /* Checking edge cases */
    da_popn (carr, 666);
    tassert (da_sizeof (carr) == 0, "popN all elements");
    tassert (da_top_idx (carr) == 0, "top index of empty array");

    da_appd (carr, '*');
    da_pop (carr);
    tassert (da_sizeof (carr) == 0, "pop latest elements");
    da_pop (carr);  da_pop (carr);
    tassert (da_sizeof (carr) == 0, "pop empty array");

    da_appd (carr, '*');
    da_unordered_delete (carr, 0);
    tassert (da_sizeof (carr) == 0, "delete latest elements");
    da_unordered_delete (carr, 0);  da_unordered_delete (carr, 0);
    tassert (da_sizeof (carr) == 0, "delete from empty array");
  }

  puts ("\n * "   STRGREEN("All tests passed")   " *");
  return EXIT_SUCCESS;
}

#endif /* DYNA_TEST */
