/**
 *  file: yes.c
 *  created on: 8 Oct 2023
 *
 *  yes! program
 *  
 *  compilation:
 *    CC -Wall -o y yes.c    (use aligned_alloc)
 *    -D ALIGN_NO            (use normal malloc) 
 *    -D USE_STACK           (only use stack)
 **/

#include <stdio.h>
#include <unistd.h>

#ifndef USE_STACK
#include <stdlib.h>
#endif

#define BUF_S 1024
static int pg_size = -1;  /* memory page size */


int
main (void)
{
#ifdef USE_STACK
    char buf[BUF_S];
#elif ALIGN_NO
    char *buf = (char*) malloc (BUF_S);
#else
    pg_size = getpagesize();
    char *buf = (char*) aligned_alloc (pg_size, BUF_S);
#endif
    
    /* fprintf (stderr, "buf_addr=%p - page_size=%d\n", buf, pg_size); */

    for (size_t i=0; i < BUF_S - 1;)
    {
        buf[i++] = 'y';
        buf[i++] = '\n';
    }

    while (1)
    {
        fwrite (buf, 1, BUF_S, stdout);
    }

#ifndef USE_STACK
    free (buf);
#endif

    return 0;
}
