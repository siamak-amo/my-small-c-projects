/** file: dynamic_array.c
    created on: 20 Jan 2024
  
    implements dynamic array in C
  
    compilation:
      cc -Wall dynamic_array.c -D DA_IMPLEMENTATION
  
    define -D DA_TEST_PROG  ->  to compile the test program
           -D EXAMPLE_PROG  ->  to compile the example program
  
    to include in other c files:
    `
       #define DA_IMPLEMENTATION
       #include "dynamic_array.c"
    `
 */
#ifndef DYNAMIC_ARRAY__H__
#define DYNAMIC_ARRAY__H__
#ifndef DADEFF
#define DADEFF static inline
#endif
/* dynamic array */
#include <string.h>
#include <stdlib.h>

struct da_head_t{
  char *data;
  size_t cap, len, sizeof_data;
};
typedef struct da_head_t DA_H;

#define da_init(off)                               \
  &(DA_H){.data=malloc(off),                       \
      .cap=1, .len=0, .sizeof_data=off}

#define da_free(da) free (da->data)

/* internal macro - always use da_resize */
#define da_realloc(da)                          \
  (da)->data = realloc ((da)->data,             \
             ((da)->cap) * ((da)->sizeof_data))

#define da_resize(new_s, da) do {                       \
    (da)->cap = new_s;                                  \
    da_realloc (da);                                    \
    if ((da)->len > new_s) (da)->len = new_s; } while (0)
#endif

#ifdef DA_IMPLEMENTATION
/* dynamic array implementation */
DADEFF void *
da_get (size_t idx, DA_H *head)
{
  char *p = head->data;
  if (idx < head->len)
    p += (head->sizeof_data) * idx;
  else p = NULL;

  return p;
}

DADEFF void
da_set (size_t idx, DA_H *head, void *value)
{
  char *p = head->data;
  if (idx >= head->cap)
    {
      head->cap = idx+1;
      head->len = idx+1;
      da_realloc (head);
      p = head->data;
    }
  p += idx * head->sizeof_data;
  memcpy (p, value, head->sizeof_data);
}

DADEFF void
da_app (DA_H *head, void *value)
{
  char *p = head->data;
  if (head->len >= head->cap)
    {
      head->cap *= 2;
      da_realloc (head);
      p = head->data;
    }
  p += head->sizeof_data * (head->len++);
  memcpy (p, value, head->sizeof_data);
}
#endif

#ifdef DA_TEST_PROG
/* test program */
#include <stdio.h>
#include <assert.h>

struct data_t{
  int d;
};

#define da_printd(da_h)                                         \
  printf("addr: %p, cap: %-2lu, len: %-2lu, data_ptr: %p\n",    \
         da_h, da_h->cap, da_h->len, da_h->data);

#define mem_dump(T, member, da_h) do {                      \
    T *p;                                                   \
    for (size_t __i=0; __i<(da_h)->cap; ++__i){             \
      p = da_get(__i, list);                                \
      if (p==NULL)                                          \
        break;                                              \
      printf ("idx %-2lu @[%p] -- data: %d\n",              \
              __i, p, p->member);                           \
    }} while (0)

int
main (void)
{
  struct data_t *p;
  DA_H *list;

  list = da_init (sizeof(struct data_t));

  for (int _i=1; _i<=12; ++_i)
    da_app (list, &(struct data_t){_i});
  
  da_printd (list);
  assert (list->cap == 16 && list->len == 12 &&
          "Error -- after 12 da_app calls");
  puts ("da_app after init -- test passed");
  
  for (size_t _i=0; _i<list->len; ++_i)
    {
      p = da_get (_i, list);
      assert (p->d == (int)_i+1 &&
              "after da_app -- data is not intact");
    }
  puts ("da_get after append -- test passed");
  mem_dump (struct data_t, d, list);

  da_set (24, list,  &(struct data_t){42});
  da_printd (list);

  assert (list->cap == 25 && list->len == 25 &&
          "Error -- after accessing index 24");
  puts ("da_set test cap and len -- passed");
  p = da_get (24, list);
  assert (p->d == 42 &&
          "after da_set -- data is not intact");
  puts ("da_set test data -- passed");
  mem_dump (struct data_t, d, list);
 
  da_resize (10, list);
  da_printd (list);
  
  assert (list->cap == 10 &&
          "Error -- after resize");
  puts ("da_resize -- test passed");
  for (size_t _i=0; _i<list->len; ++_i)
    {
      p = da_get (_i, list);
      assert (p->d == (int)_i+1 &&
              "after da_resize -- data is not intact");
    }
  mem_dump (struct data_t, d, list);
  puts ("da_get after resize -- test passed");

  da_free (list);

  return 0;
}
#endif

#ifdef EXAMPLE_PROG
#ifdef DA_TEST_PROG
#error "could not compile both of example and test programs together"
#else
/* a simple example program */
#include <stdio.h>

typedef struct {
  int uid;
  char *name;
} user_t;

int
main (void)
{
  DA_H *list;
  user_t *p;

  /* initialize the list */
  list = da_init (sizeof(user_t));

  /* append values */
  da_app (list, &(user_t){0, "root"});
  da_app (list, &(user_t){0, "adm"});

  /* add value at arbitrary index */
  da_set (1000, list, &(user_t){1000, "toor"});

  /* get value at index */
  p = da_get (1000, list);
  printf ("uid: %-4d, name: %s\n", p->uid, p->name); 
  p = da_get (0, list);
  printf ("uid: %-4d, name: %s\n", p->uid, p->name);

  /* free the list */
  da_free (list);

  return 0;
}
#endif
#endif
