#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <fcntl.h>
#include <sys/stat.h>

#define BS 128*1024

int
main (int argc, char **argv)
{
    int fd;
    size_t r;
    struct stat st;


    if (stat (argv[1], &st))
    {
        puts ("ERROR.");
        return 1;
    }

    size_t block_size = st.st_blksize * 8;
    int pg_size = getpagesize ();

//    printf ("blksize=%ld, pagesize=%ld.\n", block_size, pg_size);

#if 0
    char *buf = (char*) aligned_alloc (pg_size, block_size);
#else
    char *buf = (char*) aligned_alloc (pg_size, BS);
#endif
    fd = open (argv[1], 0);

    while ((r = read (fd, buf, block_size)) != 0)
        write (1, buf, r);

    free (buf);
}
