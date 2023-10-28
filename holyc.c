#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

void (*run)();

void
__exit_point(void)
{
  printf ("your program exited abnormally!.\n");
  exit (1);
}


int
main(void)
{
  char *buf = mmap(NULL, 256,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_ANONYMOUS | MAP_SHARED,
                   -1, 0);

  memset (buf, 0x90, 256);


  uint64_t ep = (uint64_t) __exit_point;

  char *p = buf+(256-10);
  
  sprintf (p, "\x48\xba\x66\x66\x66\x66");
  memcpy(p+2, &ep, 8);
  sprintf (p+10, "\xff\xd2");
  
  run = (void(*)())buf;
  (*run)();
  
  munmap (buf, 256);
  return 0;
}
