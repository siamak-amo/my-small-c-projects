/**
 *    file: defer.c
 *    created on: 4 Oct 2023
 *
 *    implements defer in C
 *
 **/


typedef int Errno;

// defer and return with exit code
#define returnd(code)                           \
  do{ __exit_code=code; goto _defer; } while(0);

#include <stdio.h>
#include <string.h>
#include <errno.h>

static Errno
defer_example1(void)
{
  int __exit_code;
  FILE *f = NULL;

  {
    if(1)
      returnd(42);
  }
  
 _defer:
  if(f) fclose(f);
  return __exit_code;
}



int
main(void)
{
  int err = defer_example1 ();

  if(err)
    printf ("Error %d -- %s", err, strerror(err));


  return 0;
}
