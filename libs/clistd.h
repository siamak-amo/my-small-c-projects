#ifndef CLI_STD__H__
#define CLI_STD__H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CLI_DEFAULT_GETOPT
# include <getopt.h>
#endif

#ifndef __cli_def__
# define __cli_def__ static inline
#endif

const char *program_name = NULL;

#undef warnf
#define warnf(format, ...) \
  fprintf (stderr, "%s: "format, program_name, ##__VA_ARGS__)
#undef warnln
#define warnln(format, ...) \
  warnf (format "\n", ##__VA_ARGS__)

/**
 *  LASTOPT retrieves the latest option from argv, like:
 *  '--xxx', '-x', or '-x<VALUE>', that was provided before optind.
 *
 *  This is useful for printing the exact option name, when
 *  only the optarg (Argument of that option) is available.
 */
#define LASTOPT(argv)                                       \
  ((NULL != optarg && '-' != argv[optind - 1][0]) ?         \
   argv[optind - 2] : argv[optind - 1])

/**
 *  The usage function prints usage of your program
 *  and when ecode >= 0, it should call the exit function
 *  with ecode as the exit code
 */
typedef void (*usage_fun) (int ecode);

/**
 *  From: 'gnulib/lib/progname.c':set_program_name
 *  Sets the program_name pointer
 */
__cli_def__ void
set_program_name (const char *argv0);


#define HELP_OPTION_DESCRIPTION    "-h, --help        prints usage\n"
#define VERSION_OPTION_DESCRIPTION "-v, --version     prints version\n"

#ifdef CLI_DEFAULT_GETOPT
static struct option const long_options[] =
{
  {"help",      no_argument, NULL, 'h'},
  {"version",   no_argument, NULL, 'v'},
  {NULL,        0,           NULL,  0 },
};

/**
 *  From: 'gnulib/lib/long-options.c':parse_gnu_standard_options_only
 *  Pasres help and version options and 
 */
__cli_def__ void
parse_std_options_only (int argc, char **argv,
                        const char *command_name, const char *version,
                        usage_fun usage);

#endif /* CLI_DEFAULT_GETOPT */

/**
 *  From: 'gnulib/lib/version-etc.c':version_etc_arn
 *  Prints version
 */
__cli_def__ void
version_etc (FILE *stream,
             const char *command_name, const char *version);

#ifdef CLI_IMPLEMENTATION
__cli_def__ void
set_program_name (const char *argv0)
{
  if (argv0 == NULL)
    {
    null_argv0:
      fputs ("An invalid argv[0] was passed through an exec system call.\n",
             stderr);
      abort ();
    }
  /* get the latest slash */
  char *__p = strrchr (argv0, '/');
  /**
   *  The GNU libc also checks for '/.libs/'
   *  But is it necessary for us??
   */
  program_name = (__p != NULL) ? __p + 1 : argv0;

  if (*program_name == '\0')
    goto null_argv0;
}

__cli_def__ void
version_etc (FILE *stream, const char *command_name, const char *version)
{
  if (command_name)
    fprintf (stream, "%s - v%s\n", command_name, version);
  else
    fprintf (stream, "%s\n", version);
}


#ifdef CLI_DEFAULT_GETOPT
/**
 *  From: 'gnulib/lib/long-options.c':parse_gnu_standard_options_only
 *  Pasres help and version options and 
 */
__cli_def__ void
parse_std_options_only (int argc, char **argv,
                        const char *command_name, const char *version,
                        usage_fun usage)
{
  int c;
  int saved_opterr = opterr;
  /* Print an error message for unrecognized options.  */
  opterr = 1;

  if ((c = getopt_long (argc, argv, "+hv", long_options, NULL)) != -1)
    {
      switch (c)
        {
        case 'h':
          (*usage) (EXIT_SUCCESS);
          break;

        case 'v':
          {
            version_etc (stdout, command_name, version);
            exit (EXIT_SUCCESS);
          }

        default:
          (*usage) (1);
          break;
        }
    }
  /* Restore previous value.  */
  opterr = saved_opterr;
}
#endif /* CLI_DEFAULT_GETOPT */


#endif /* CLI_IMPLEMENTATION */

#endif /* CLI_STD__H__ */
