#ifndef CLI_STD__H__
#define CLI_STD__H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CLI_DEFAULT_GETOPT
# include <getopt.h>
#endif

#ifndef CLI_DEF
# define CLI_DEF static inline
#endif

const char *program_name = NULL;

#define UNUSED(x) (void)(x)

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
CLI_DEF void
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
CLI_DEF void
parse_std_options_only (int argc, char **argv,
                        const char *command_name, const char *version,
                        usage_fun usage);

#endif /* CLI_DEFAULT_GETOPT */


/**
 *  In Android (Termux), the `on_exit` function, does not exist;
 *  but they usually have `atexit` function.
 *
 *  This implements `on_exit` function, but with these limitations:
 *   1. It does not support program return code
 *      your 'function', always gets 0 in the first parameter
 *      TODO: Is there any workaround?
 *   2. It does NOT support multiple function registrations
 *      It can only be called once (returns failure afterward)
 *      TODO: Fix this.
 */
#ifdef __ANDROID__
static int __on_exit_code = 0; /* Where to get this?? */
static void *__on_exit_arg = NULL;
static void (*__on_exit_func)(int, void *);

void
__on_exit_wrapper (void)
{
  __on_exit_func (__on_exit_code, __on_exit_arg);
}

int
on_exit (void (*function)(int, void *), void *arg)
{
  if (__on_exit_func)
    return 1;

  __on_exit_arg = arg;
  if (function)
    {
      __on_exit_func = function;
      atexit (__on_exit_wrapper);
    }
  return 0;
}
#endif /* __ANDROID__ */


/**
 *  From: 'gnulib/lib/version-etc.c':version_etc_arn
 *  Prints version
 */
CLI_DEF void
version_etc (FILE *stream,
             const char *command_name, const char *version);

#ifdef CLI_IMPLEMENTATION
CLI_DEF void
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

CLI_DEF void
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
CLI_DEF void
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


/** ANSI color support

    Usage:
    1. Using the FCOLOR macro in printf format parameter:
       FCOLOR macro, create strings with appropriate color code
       which will get concatenated by the compiler:
         FCOLOR("Test: %s", F_RED())  generates:
         "[RED FG COLOR CODE]"  "Test: %s"  "[RESET COLOR]"

    2. Using COLOR_FMT and COLOR_ARG macros:
       COLOR_FMT("(%d)")  create  "%s(%d)%s"  and,
       COLOR_ARG(123, B_RED())  generates 3 arguments:
         "[RED BG COLOR CODE]",  123,  "[RESET COLOR]"

    For background color, B_xxx() and for foreground color F_xxx()
    macro can be used, these macros accept at most one argument:
     F_U, F_B, F_HI, F_HBI  and  B_HI for background.

    Example: these two printf calls, generate identical output
    ```{c}
      printf (FCOLOR("%s",   B_RED()) " -- "
              FCOLOR("`%s`", F_BLUE(F_U)) ".\n",
              "Error", "file.txt"
      );
      printf (COLOR_FMT("%s")  " -- " COLOR_FMT("`%s`")  ".\n",
              COLOR_ARG("Error", B_RED()),
              COLOR_ARG("file.txt", F_BLUE(F_UNDERLINE))
      );
    ```
**/
#define __ESC__  "\033["
#define COLOR_RESET   __ESC__ "0m"

#define FCOLOR(fmt, ...) __VA_ARGS__ fmt COLOR_RESET

#define COLOR_FMT(x) "%s" x "%s"
#define COLOR_ARG(x, __COLOR__) __COLOR__, x, COLOR_RESET

/* To be used as argument for F_xxx() and B_xxx() macros */
#define F_U     "4;3"    // Underline
#define F_B     "1;3"    // Bold
#define F_HI    "0;9"    // High intensity
#define F_BHI   "1;9"    // Bold high intensity
#define B_HI    "2;10"   // High intensity background

#define F_UNDERLINE F_U
#define F_BOLD F_B

#define __FG__  "0;3"    // Regular foreground (Internal)
#define __BG__  "4"      // Regular background (Internal)

#define VA_FIRST(x, ...) x
#define __RAW_COLOR__(code, ...) \
  __ESC__ VA_FIRST(__VA_ARGS__) code "m"

/* Foreground color definitions */
#define F_BLACK(...)   __RAW_COLOR__ ("0", ##__VA_ARGS__, __FG__)
#define F_RED(...)     __RAW_COLOR__ ("1", ##__VA_ARGS__, __FG__)
#define F_GREEN(...)   __RAW_COLOR__ ("2", ##__VA_ARGS__, __FG__)
#define F_YELLOW(...)  __RAW_COLOR__ ("3", ##__VA_ARGS__, __FG__)
#define F_BLUE(...)    __RAW_COLOR__ ("4", ##__VA_ARGS__, __FG__)
#define F_PURPLE(...)  __RAW_COLOR__ ("5", ##__VA_ARGS__, __FG__)
#define F_CYAN(...)    __RAW_COLOR__ ("6", ##__VA_ARGS__, __FG__)
#define F_WHITE(...)   __RAW_COLOR__ ("7", ##__VA_ARGS__, __FG__)

/* Background color definitions */
#define B_BLACK(...)   __RAW_COLOR__ ("0", ##__VA_ARGS__, __BG__)
#define B_RED(...)     __RAW_COLOR__ ("1", ##__VA_ARGS__, __BG__)
#define B_GREEN(...)   __RAW_COLOR__ ("2", ##__VA_ARGS__, __BG__)
#define B_YELLOW(...)  __RAW_COLOR__ ("3", ##__VA_ARGS__, __BG__)
#define B_BLUE(...)    __RAW_COLOR__ ("4", ##__VA_ARGS__, __BG__)
#define B_PURPLE(...)  __RAW_COLOR__ ("5", ##__VA_ARGS__, __BG__)
#define B_CYAN(...)    __RAW_COLOR__ ("6", ##__VA_ARGS__, __BG__)
#define B_WHITE(...)   __RAW_COLOR__ ("7", ##__VA_ARGS__, __BG__)
/* End of ANSI color support */


/**  Minimal ANSI terminal control support

     Example #1:  Using CURSAFE along with CUR_UP/DOWN
     ```{c}
       printf ("\n\n\n\n\n" // allocating 5 lines
               CURSAFE (
                   CUR_UP(1)  "We are at line 5"
                   CUR_UP(3)  "We are at line 2"
                   CUR_DOWN() "We are at line 3"
               )
       );
     ```

     Example #2:  A minimal progress-bar
     ```{c}
       // Using printf along with the progress function:
       //   printf( LINESAFE("format"), ...params );

       void
       progress (int percentage)
       {
         fprintf (stderr,
                  LINESAFE ("::  Progress %02d%%  ::"),
                  percentage
         );
       }
     ```
**/
/* Clear the terminal */
#define CLR_SCREEN  __ESC__ "2J"
#define __CLR__(N)  __ESC__ #N "K"  // Internal
#define CLR_RIGHT   __CLR__()       // Clear from the cursor to the end
#define CLR_LEFT    __CLR__(1)      // Clear from the cursor to the start
#define CLR_LINE    __CLR__(2)      // Clear the entire line

/* Cursor enabled */
#define CUR_ENABLE  __ESC__ "?25h"
#define CUR_DISABLE __ESC__ "?25l"

/* Cursor save (store: ST) and restore (load: LD) */
#define CUR_ST __ESC__ "s" 
#define CUR_LD __ESC__ "u"

/* Set the cursor to an arbitrary position  */
#define CUR_SET(X, Y) __ESC__ #Y ";" #X "H"
/* Moving the cursor to the start and the end of line */
#define CUR_START __ESC__ "0G"
#define CUR_END   __ESC__ "K"

/* Cursor safe, saves and restores the cursor */
#define CURSAFE(str) CUR_ST str CUR_LD CUR_START
/* To ensure printing of @str at the beginning of a cleared line */
#define CLEAN_LINE(str) CLR_LINE CUR_START str
#define CLEANLN CLEAN_LINE

/**
 *  CUR_UP(N), CUR_DOWN(N):
 *    Moves the cursor N lines up/down, at the beginning of line.
 *  CUR_RUP(N), CUR_RDOWN(N):
 *    Relative up/down, preserving x position of the cursor.
 */
#define __CUR_MOVE__(N, DIR) __ESC__ #N  DIR
#define CUR_FORWARD(N)  __CUR_MOVE__ (N, "C")
#define CUR_BACKWARD(N) __CUR_MOVE__ (N, "D")
#define CUR_RUP(N)      __CUR_MOVE__ (N, "A") 
#define CUR_RDOWN(N)    __CUR_MOVE__ (N, "B")
#define CUR_UP(N)         CUR_RUP(N)   CUR_START
#define CUR_DOWN(N)       CUR_RDOWN(N) CUR_START
/* End of ANSI terminal control */

#endif /* CLI_STD__H__ */
