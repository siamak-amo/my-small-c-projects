/** file: tape_mem.h
    created on: 24 Jun 2024
  
    Tape like memory allocator
  
    This library does not allocate the memory itself, and doing so
    is up to users so they can do it in a way they prefer
    This library does not include either stdlib or mmap
    Reallocating that memory is possible but it most likely makes
    all your pointers invalid, instead you may allocate a large amount
    of memory, and let your operation system handle it
  
    Compilation:
      to compile the test program:
        cc -x c -ggdb -Wall -Wextra -Werror \
           -D TAPE_MEM_IMPLEMENTATION  \
           -D TAPE_MEM_TEST \
           -o test.out tape_mem.h
  
      to include in other files:
      ```c
      #include <stlib.h>
  
      #define TAPE_MEM_IMPLEMENTATION
      #include "tape_mem.h"
  
      int
      main (void)
      {
        DBuffer tmp = {0};
        Tape mem = new_tape (1 * 1024 * 1024); // 1G
  
        // allocate memory in a way you prefer
        // large enough for your usage
        // it wont get consumed until you start using it
        mem.data = malloc (mem.cap);
  
        // do something here
        // see the example in the test program
        tmp.len = 4;
        tmp.data = "One";
        tape_append (&mem, &tmp);
  
        char *d = tape_get (1);
        DBuffer *db = bufferof (d);
        printf ("len: %lu, val: %s\n", db->len, d);
  
        return 0;
      }
      ```
 **/
#ifndef TAPE_MEM__H__
#define TAPE_MEM__H__
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define BUF_MAX_LEN (256*1024)
struct buff_t {
  size_t len; /*length of data (constant) */
  char *data;
};
typedef struct buff_t DBuffer; /* data buffer */
/**
 *  this is not size of your DBuffer, this is size of
 *  the memory that is needed to write the entire DBuffer
 *  plus the value of it's data (consider it as `char data[]`)
 */
#define buffer_of_size(size) (size + sizeof (DBuffer))
#define sizeof_buffer(buf) ((buf)->len + sizeof (DBuffer))
#define bufferof(data_ptr) \
  (DBuffer*)(data_ptr - offsetof (DBuffer, data))

struct tape_t {
  size_t len, cap;
  char *data; /* array of buff_t structs */
};
typedef struct tape_t Tape;
#define new_tape(capacity)                      \
  (Tape){.len=0, .cap=capacity, .data=NULL,}

#ifndef TAPEMEMDEF
#  define TAPEMEMDEF static inline
#endif

/* function definitions */
/**
 *  append to a tape
 *  @return:  NULL on failure,
 *            pointer to the written DBuffer on success
 */ 
TAPEMEMDEF char *tape_append (Tape *tape, const DBuffer *buf);

/**
 *  get the DBuffer at @index
 *  @index:  starts from 1
 *  @return: NULL on failure,
 *           pointer to the read DBuffer on success
 */
TAPEMEMDEF char *tape_get (const Tape *tape, size_t index);

#endif /* TAPE_MEM__H__ */


#ifdef TAPE_MEM_IMPLEMENTATION
TAPEMEMDEF char *
tape_append (Tape *tape, const DBuffer *buf)
{
  if (NULL == tape->data || 0 == tape->cap || 0 == buf->len)
    return NULL;

  size_t __buf_size = sizeof_buffer (buf);
  if (__buf_size > BUF_MAX_LEN)
    return NULL;
  if (tape->len + __buf_size >= tape->cap)
    return NULL;

  char *p = memcpy (tape->data + tape->len, buf, offsetof (DBuffer, data));
  memcpy (p + offsetof (DBuffer, data), buf->data, buf->len);
  tape->len += __buf_size;
  return p + offsetof (DBuffer, data);
}

TAPEMEMDEF char *
tape_get (const Tape *tape, size_t index)
{
  char *p = tape->data;
  size_t p_len = tape->len;

  if (NULL == tape->data)
    return NULL;

  DBuffer *buf = (DBuffer *)p;
  while (0 != index && 0 != p_len)
    {
      buf = (DBuffer *)p;
      size_t __buf_size = buffer_of_size (buf->len);
      assert ((__buf_size < BUF_MAX_LEN) && (0 != buf->len) &&
              "broken logic or memory corruption");
      p_len -= __buf_size;
      p += __buf_size;
      index--;
    }

  if (index == 0)
    return (char *)buf + offsetof (DBuffer, data);
  else
    return NULL;
}

#endif /* TAPE_MEM_IMPLEMENTATION */


/* the test program */
#ifdef TAPE_MEM_TEST
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main (void)
{
  DBuffer tmp = {0};
  Tape mem = new_tape (getpagesize ()); // 4k

  printf ("allocating %luK... ", mem.cap / 1024);
  mem.data = malloc (mem.cap);
  if (mem.data)
    printf ("done\n");
  else
    {
      printf ("no\nmem allocation failed.\n");
      return 1;
    }

  printf ("adding items... ");
  tmp.len = 4;
  tmp.data = "One";
  tape_append (&mem, &tmp);
  
  tmp.len = 32;
  tmp.data = "2024";
  tape_append (&mem, &tmp);
  
  tmp.len = 4;
  tmp.data = "XXX";
  tape_append (&mem, &tmp);
  printf ("done\n");

  
  char *data_at = NULL;
  DBuffer *btmp = NULL;

  printf ("testing tape_get... ");
  data_at = tape_get (&mem, 1);
  assert (NULL != data_at);
  assert (0 == strcmp (data_at, "One"));
  // puts (data_at);
  btmp = bufferof (data_at);
  assert (btmp->len == 4);

  data_at = tape_get (&mem, 2);
  assert (NULL != data_at);
  assert (0 == strcmp (data_at, "2024"));
  // puts (data_at);
  btmp = bufferof (data_at);
  assert (btmp->len == 32);
  
  data_at = tape_get (&mem, 3);
  assert (NULL != data_at);
  assert (0 == strcmp (data_at, "XXX"));
  // puts (data_at);
  btmp = bufferof (data_at);
  assert (btmp->len == 4);

  data_at = tape_get (&mem, 4);
  assert (NULL == data_at);
  printf ("done\n");
 
  free (mem.data);
  return 0;
}
#endif /* TAPE_MEM_TEST */
