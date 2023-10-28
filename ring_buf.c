/**
 *    file: ring_buf.c
 *    created at: 7 Oct 2023
 *
 *    implements Ring buffer (Circular buffer)
 *
 *
 *    compilation:
 *     if you want to compile the executable test program:
 *       cc -Wall -Wextra -D RING_IMPLEMENTATION
 *                        -D RING_TEST -o ring ring_buf.c
 *
 *     if you include this program in your c file:
 *       `
 *          #define RING_IMPLEMENTATION
 *          #include "ring_buf.c"
 *       `
 *     define `RING_DEBUG` to enable debugging messages
 *
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define addMOD(x,y, m) x=(x+y)%m

#ifndef RINGBUFDEF
#define RINGBUFDEF static inline
#endif

#ifndef RING__H__
#define RING__H__
/* initialize the internal variables */
RINGBUFDEF void ring_init(char *ring_buf, size_t len);
/* writes one byte (char c) into the ring */
RINGBUFDEF void ring_write(char *ring_buf, char c);
/* writes n bytes from src into the ring */
RINGBUFDEF void ring_nwrite(char *ring_buf, const char *src, size_t n);
/* reads from the ring into the dest buffer */
RINGBUFDEF void ring_read(char *dest, const char *ring_buf);
/* prints out the ring */
RINGBUFDEF void ring_print(const char *ring_buf);
#endif

/* internal variables -- don't change them manually */
static size_t ring_len, ring_head, ring_idx;



/*-----------------------*/
/* Ring implementation   */
/*-----------------------*/
#ifdef RING_IMPLEMENTATION

RINGBUFDEF void
ring_init(char *ring_buf, size_t len)
{
  memset (ring_buf, 0, len);
  ring_len = len;
  ring_head = 0;
  ring_idx = 0;
}

RINGBUFDEF void
ring_write(char *ring_buf, char c)
{
  char *p;
  
  if (ring_idx == ring_len - 1)
    {
      /* reached the end of ring_buf */
      p = ring_buf;
      ring_idx = 0;
      ring_head = 1;
    }
  else
    {
      p = ring_buf + (++ring_idx);
      ring_head++;
    }
  
  *p = c;
  
#ifdef RING_DEBUG
  printf("(idx, head)=(%2ld, %2ld) -- ", ring_idx, ring_head);
#endif
}

RINGBUFDEF void
ring_nwrite(char *ring_buf, const char *src, size_t n)
{
  char *p;
  /* how many bytes from idx+1 to the end of the ring_buf */
  size_t end_offset = ring_len - ring_idx - 1;
  
  if (ring_idx == ring_len - 1)
    p = ring_buf;
  else p = ring_buf + ring_idx + 1;
  
  if (n > end_offset)
    {
      memcpy (p, src, end_offset);
      src += end_offset;
      
      /* reached the end of buffer */
      ring_head = 0;
      ring_idx = ring_len - 1;
      /* end_offset bytes was written */
      n -= end_offset;
      
      if (n > ring_len)
        {
          // if n = ring_len*k + b  where k=n/ring_len,
          // k-1 times writing will be overwritten,
          // so we only need to write the last ring_len bytes
          // therefor we ignore them by shifting src pointer
          src += (n / ring_len - 1) * ring_len;
          memcpy (ring_buf, src, ring_len);
          
          src += ring_len;
          n %= ring_len;
        }
      
      /* write the rest of n bytes ((n-end_offset)%ring_len bytes) */
      memcpy (ring_buf, src, n);
      addMOD(ring_idx, n, ring_len);
      ring_head += n;
    }
  else
    {
      /* we've got enough capacity to write all the n bytes */
      memcpy (p, src, n);
      addMOD(ring_idx, n, ring_len);
      ring_head += n;
    }
#ifdef RING_DEBUG
  printf("(idx, head)=(%2ld, %2ld) -- ", ring_idx, ring_head);
#endif
}

RINGBUFDEF void
ring_read(char *dest, const char *ring_buf)
{
  /* ring's range is {1, 2, ..., idx} */
  if (ring_idx >= ring_head)
    memcpy(dest, ring_buf + 1, ring_idx);
  else
    {
      /* {head, head+1, ..., len-1} U {0, 1, ..., head-1} */
      size_t offset = ring_head;
      const char *p = ring_buf + offset;
      char *d = dest;
      
      memcpy (d, p, (ring_len - offset));
      d += ring_len - offset;
      
      memcpy (d, ring_buf, offset);
    }
}

RINGBUFDEF void
ring_print(const char *ring_buf)
{
  if (ring_idx >= ring_head)
    {
      putc ('[', stdout);
      if (ring_len - ring_idx)
        printf("%-*s", (int)(ring_len - ring_idx), " ");
      fwrite (ring_buf+1, 1, ring_idx , stdout);
      puts ("]");
    }
  else
    {
      size_t offset = ring_head % ring_len;
      const char *p = ring_buf + offset;
      
      putc ('[', stdout);
      fwrite (p, 1, (ring_len - offset), stdout);
      fwrite (ring_buf, 1, offset, stdout);
      puts ("]");
    }
}
#endif



/*----------------------*/
/* test functions       */
/*----------------------*/
#ifdef RING_TEST
/*
 *   tests `ring_write` and `ring_read` functions
 *   by 4 times reading and writing on a ring
 **/
RINGBUFDEF void
test1_1(char *ring_buf, size_t LEN)
{
  char *read_buf = (char*) malloc (LEN);
  /* write a number with SECTION-1 spaces */
  size_t SECTION = 2;
  
  for (size_t i=0; i<4*LEN; ++i)
    {
      ring_write (ring_buf,
                  (i%SECTION)?' ':'0'+((i/SECTION)%10)); 
#if 1
      if (0==i%5)
        {
          ring_read (read_buf, ring_buf);
          
          if (ring_idx==ring_head)
            printf ("[%-*s%s] -- %ld bytes was read\n",
                   (int)(ring_len - ring_idx), " ", read_buf, i+1);
          else
            printf ("[%s] -- %ld bytes was read\n",
                    read_buf, i+1);
        }
      else
        ring_print (ring_buf);
#else
      ring_print (ring_buf);
#endif
    }
  
  free (read_buf);
}

RINGBUFDEF void
test1_2(char *ring_buf, size_t LEN)
{
  /* write `#` with SECTION-1 spaces */
  size_t SECTION = 5;
  
  for (size_t i=0; i<2*LEN; ++i)
    {
      ring_write (ring_buf, (i%SECTION)?' ':'#');
      ring_print (ring_buf);
    }
}

/*
 *    tests `ring_nwrite` and `ring_write` functions
 *    the following numbers have choosed based on
 *    LEN=10, so it covers almost all possible bugs
 *
 *    `X` character indicates out of boundary error
 **/
RINGBUFDEF void
test2_1(char *ring_buf)
{
  ring_nwrite(ring_buf, "xxxA*A*A*A*A*XXX"+3, 10);
  ring_print (ring_buf);
  ring_nwrite(ring_buf, "SOSXXX", 3);
  ring_print (ring_buf);
  ring_nwrite(ring_buf, "00112233445566778899XXX", 20);
  ring_print (ring_buf);
  ring_nwrite(ring_buf, "-*-XXX", 3);
  ring_print (ring_buf);
  ring_nwrite(ring_buf, "!!!!!@XXX", 6);
  ring_print (ring_buf);
  ring_nwrite(ring_buf, "#################################///////XXX", 40);
  ring_print (ring_buf);
  
  ring_write(ring_buf, '0');
  ring_print (ring_buf);
  ring_write(ring_buf, '1');
  ring_print (ring_buf);
  ring_write(ring_buf, '2');
  ring_print (ring_buf);
}


int
main(void)
{
  size_t LEN=10;
  
  char *buf = (char*)malloc (LEN);
  
  ring_init (buf, LEN);
  
  puts (" ** Running test1_1 **");
  test1_1 (buf, LEN);
  puts ("\n ** Running test1_2 **");
  test1_2 (buf, LEN);
  
  puts ("\n\n ** Running test2_1 **");
  test2_1 (buf);
  
  free (buf);
  
  return 0;
}
#endif
