/**
 *  file: tee.c
 *  created on: 26 Oct 2024
 *
 *  tee - read from standard input and
 *  write to standard output and files
 *
 *  Usage:
 *    tee [OPTIONS]... [FILE]...
 *  Options:
 *    -a, --append     append to given files
 *                     do not overwrite
 *
 *  Compilation:
 *    cc -ggdb -O3 -Wall -Wextra -Werror \
 *       -o tee tee.c
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define CLI_IMPLEMENTATION
#define CLI_NO_GETOPT /* we handle options ourselves */
#include "clistd.h"

/* max buffer length */
#ifndef _BMAX
# define _BMAX (sysconf (_SC_PAGESIZE) / 4) // 1kb
#endif

/* dynamic array for filepaths */
#define DA_DO_GROW(cap) ((cap) += DA_GFACT)
#define DYNA_IMPLEMENTATION
#include "dyna.h"

/* error returns */
#define OPT_ERR 1
#define FOPEN_ERR 2
#define MEM_ERR 3

static int out_flags = O_CREAT|O_WRONLY;
static mode_t out_mode = 0644;
// IO file descriptors
static int *out_fds = NULL;
static int in_fd = STDIN_FILENO;

#undef fdebug
#define fdebug(msg, ...) \
  fprintf (stderr, msg, ##__VA_ARGS__)
#undef debugf
#define debugf(msg, ...) \
  fdebug ("[DEBUG] "msg, ##__VA_ARGS__)


int
out_open (const char *out_pathname)
{
  if (out_pathname == NULL)
    {
      da_appd (out_fds, STDOUT_FILENO);
      return 0;
    }

  int fd = open (out_pathname, out_flags, out_mode);
  if (fd == -1)
    {
      warnf ("%s: %s", out_pathname, strerror (errno));
      return FOPEN_ERR;
    }
  da_appd (out_fds, fd);
  return 0;
}

int
parse_args (int argc, char **argv)
{
  /* long options */
#define LOPT(opt) \
  (strcmp (*argv, "--"opt) == 0)
#define LOPT2(opt1, opt2) \
  (LOPT (opt1) || LOPT (opt2))

  int ARGC = argc;
  char **ARGV = argv;
  for (--argc, ++argv; argc != 0; --argc, ++argv)
    {
      if (**argv == '-')
        {
          char opt = argv[0][1];
          switch (opt)
            {
            case 'a': // append
            Append_Mode:
              out_flags |= O_APPEND;
              break;

            case '-': // --option
              if (LOPT2 ("app", "append"))
                {
                  goto Append_Mode;
                }
              else
                {
                  goto Invalid_Opt;
                }

            default:
            Invalid_Opt:
              warnf ("invalid option -- %c", opt);
              return OPT_ERR;
            }
        }
    }

  /* truncate if not appending */
  if (out_flags & O_APPEND)
    out_flags &= ~O_TRUNC;
  else
    out_flags |= O_TRUNC;

  argc = ARGC;
  argv = ARGV;
  for (--argc, ++argv; argc != 0; --argc, ++argv)
    {
      if (**argv != '-') // filepath
        {
          int ret;
          if ((ret = out_open (*argv)) != 0)
            return ret;
        }
    }

  return 0;
}

int
main (int argc, char **argv)
{
  set_program_name (*argv);
  out_fds = da_new (int);
  /* parse options and open output files */
  int ret;
  if ((ret = parse_args (argc, argv)) != 0)
    return ret;
  /* number of output files */
  da_idx out_count = da_sizeof (out_fds);

  char *buffer = malloc (_BMAX);
  if (!buffer)
    return MEM_ERR;

#ifdef _DEBUG
  debugf ("buffer length: %lu\n", _BMAX);
  debugf ("input fileno: %d\n", in_fd);
  debugf ("output fileno's: ");
  for (da_idx i=0; i < out_count; ++i)
    fdebug ("%d ", out_fds[i]);
  fdebug ("\n");
  if (isatty (in_fd))
    {
      debugf ("reading input (%s):\n", ttyname (in_fd));
    }
#endif

#ifndef _DEBUG
  while (1)
#else
  size_t sys_count = 0; // syscall counter
  size_t total_read = 0;
  while (++sys_count)
#endif
    {
      ssize_t _r = read (in_fd, buffer, _BMAX);
      if (_r == -1 || _r == 0)
        {
          /* EOF or read error */
          goto End_of_Main;
        }
      else if (_r > 0)
        {
          /* write to standard output */
          write (STDOUT_FILENO, buffer, _r);

#ifdef _DEBUG
          debugf ("write syscall, %ld bytes\n", _r);
          total_read += _r;
#endif
          /* write on output file(s) */
          int fd;
          for (da_idx i=0; i < out_count; ++i)
            {
              fd = out_fds[i];
              ssize_t _w = write (fd, buffer, _r);
              if (_w < 0 || _w != _r)
                {
                  warnf ("write error -- %s", strerror (errno));
                  goto End_of_Main;
                }
            }
        }
    }
 End_of_Main:
  free (buffer);
  da_free (out_fds);

#ifdef _DEBUG
  if (out_count == 0)
    out_count = 1;
  debugf ("wrote %lu bytes with %lu read and "
          "%lu write syscalls\n"
          ,total_read, sys_count, sys_count*out_count);
#endif

  return 0;
}
