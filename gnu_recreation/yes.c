/** file: yes.c
    created on: 8 Oct 2023
  
    yes! program
    
   compilation:
     cc -Wall -Wextra -Werror yes.c -I../libs -o yes
   options:
     -D ALIGN_NO: to use normal malloc, not aligned alloc
 **/

#include <stdio.h>
#include <unistd.h>
# include <stdlib.h>

#define KB 1024 // 1Kb
#define BUFSIZE (2 * KB)  // half of a page

#define Version "2"
#define CLI_IMPLEMENTATION
#include "clistd.h"

void
usage (int status)
{
  printf ("\
Usage: %s [STRING]...\n\
  or:  %s OPTION\n\
",
          program_name, program_name);
  fputs ("\
Repeatedly output a line with all specified STRING(s), or 'y'.\n\
", stdout);

  fputs (HELP_OPTION_DESCRIPTION, stdout);
  fputs (VERSION_OPTION_DESCRIPTION, stdout);

  if (status >= 0)
    exit (status);
}

int
main (int argc, char **argv)
{
  set_program_name (*argv);
  parse_std_options_only (argc, argv, program_name, Version, usage);

  /* Argument strings of the output */
  char **strs = argv + optind;
  if (optind == argc)
    {
      /* When no argument is passed */
      *strs = "y";
      argc++;
    }

  size_t cat_len = 0;
  for (int i=0; i < argc - optind; ++i)
    {
      cat_len += strlen (strs[i]) + 1;
    }

  size_t buf_size = BUFSIZE;
  if (cat_len > buf_size)
    buf_size = 2*cat_len;

#ifdef ALIGN_NO
    char *buf = (char*) malloc (buf_size);
#else
    int pg_size = getpagesize();
    char *buf = (char*) aligned_alloc (pg_size, buf_size);
#endif

    size_t buf_used = 0, arglen = 0;
    for (int i=0; i < argc - optind; ++i)
      {
        arglen = strlen (strs[i]);
        memcpy (buf + buf_used, strs[i], arglen);
        buf_used += arglen;
      buf[buf_used++] = ' ';
      }
    buf[buf_used - 1] = '\n';

    size_t copy_len = buf_used;
    for (size_t i = buf_size / copy_len; --i != 0; )
      {
        memcpy (buf + buf_used, buf, copy_len);
        buf_used += copy_len;
      }

    int ret = 0;
    while (ret >= 0)
      {
        ret = write (STDOUT_FILENO, buf, buf_used);
      }

#ifndef USE_STACK
    free (buf);
#endif

    return 0;
}
