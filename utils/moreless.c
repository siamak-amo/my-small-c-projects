/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  Moreless is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Moreless is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: moreless.c
    created on: 11 Feb 2025

   More Less
   A simple tool to view standard output through the less program

   ** This only works with the GNU C Library (glibc)
      For other libc implementations, you need to overwrite
      the equivalent `__libc_start_main` function **

   Usage:
     For a single command:
       $ LD_PRELOAD="/path/to/moreless.so" ls -ltrh
     Or make it permanent:
       $ export LD_PRELOAD="/path/to/moreless.so"
       Then run your commands.

     To exclude command(s) from moreless:
       See the default excludes in `DEFAULT_EXCLUDES`.

       To append to the default excludes:
       $ export MORELESS_EXCLUDE=":ls:mpv"
       To overwrite the default excludes:
       $ export MORELESS_EXCLUDE="less:tmux:mpv"

       Alternatively, set the environment variable `NO_LESS`
       to disable moreless.


   Here is a simple bash function to toggle moreless:
     ```{bash}
     function moreless ()
       {
         case $LD_PRELOAD in
         *"moreless.so"*)
           unset LD_PRELOAD
           echo "moreless disabled."
           ;;
         *)
           export LD_PRELOAD="/path/to/moreless.so"
           echo "moreless enabled."
           ;;
         esac
       }
     ```
     You might want to eliminate /path/to/moreless.so
     from LD_PRELOAD instead of using `unset LD_PRELOAD`.


   Compilation:
     cc -O3 -Wall -Wextra -fPIC -shared \
        -o moreless.so moreless.c

     Options to pass to the less process:
       define -D_LESS_OPTS='"-S", "-R", "-X"'
     For debugging:
       define -D_DEBUG
     To disable buffering in pipes:
       define -D IMMID_PIPE
     To disable overwriting isatty function
       define -D_DO_NOT_TRICK_ISATTY

   Known issues:
   - It does not work with the bash builtin functions (like: echo, pwd)

   - Each iteration of for/while loops in Bash runs a separate instance
     of the less program.

     A simple solution is using `bash -c`:
     $ ... | bash -c 'while read ln; do ls "$ln"; done'

     Another solution is to redirect the output to an another program:
     $ ... | while read ln; do ls --color "$ln"; done | less

   - Redirecting 2>&1 does not work (I don't know how to detect this redirection)

   - Some programs may look different (e.g. without color).
     Although we have overridden the isatty function, some programs have their own
     isatty implementation, which our override will not affect (like: grep).

   - The ps command exits with an exit code of 1 (EXIT_FAILURE).
     (likely because of it's unwanted child)
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifndef LESS
#  define LESS "less"
#endif

#ifndef _LESS_OPTS
#  define _LESS_OPTS "-R", "-X", "-S"
#endif

#define LESS_OPTS LESS, _LESS_OPTS

#ifndef _EXCLUDES
#  define _EXCLUDES \
  "less:man" \
  ":clear:cls:exit:reboot:shutdown" \
  ":cp:mv:dd:rm:rmdir:chmod:chown:sudo" \
  ":tmux:screen" \
  ":emacs:vi:vim:nvim:nano:hexedit" \
  ":mpv:mplayer"
#endif /* _EXCLUDES */

#ifndef _SHELLS
#  define _SHELLS \
  "sh:dash:bash:zsh:fish:csh"
#endif /* _SHELLS */

static const char *default_excludes = _EXCLUDES;
static const char *shells = _SHELLS;

#undef UNUSED
#define UNUSED(x) (void)(x)

/* Annotations */
#define __Parent__ /* used by the parent process */
#define __Child__ /* used by the child process */
#define __Overwrite__ /* when we overwrite a libc function */

enum parent_t
  {
    P_CHILD = 0,
    P_PARENT =1,
    P_ESCAPED
  };

#ifdef _DEBUG
const char *mode_cstr[] = {
  [P_CHILD]     = "child",
  [P_PARENT]    = "parent",
  [P_ESCAPED]   = "ignore",
};
#endif /* _DEBUG */

/**
 *  $ command_1 | command_2 | ... | last_command
 *    ^           ^                 ^
 *    P_ESCAPED   P_ESCAPED         P_PARENT
 *
 *  mode of the less process will be P_CHILD
 */
int mode = P_CHILD;

/* These functions call the real main function */
typedef int(*pre_main_t)(int argc, char **argv, char **envp);

/* Belongs to the actual binary (parent) */
static __Parent__ pre_main_t original_main;


static inline void
safe_fclose (FILE *f)
{
  int fd;
  if (f)
    {
      if ((fd = fileno (f)) < 0)
        return;
      if (fcntl (fd, F_GETFD) < 0)
        return;
      fflush (f);
      fclose (f);
    }
}

void
__attribute__((constructor)) init()
  __Parent__
{
}

void
__attribute__((destructor)) cleanup()
  __Parent__
{
  safe_fclose (stdout);
  safe_fclose (stderr);
  wait (NULL);
}

#ifndef _DO_NOT_TRICK_ISATTY
/**
 *  We overwrite the isatty function to trick the parent
 *  process to consider the stdout always a tty
 *  This makes them to look normal and have color
 */
int
isatty (int fd)
  __Overwrite__
  __Parent__
  __Child__
{
  struct stat buf;

  if (mode == P_PARENT && fd == STDOUT_FILENO)
    return 1;

  if (fstat (fd, &buf) == -1)
    return 0;
  return S_ISCHR (buf.st_mode);
}
#endif /* _DO_NOT_TRICK_ISATTY */

int
alter_main (int argc, char **argv, char **envp)
  __Child__
{
  UNUSED (argc);
  UNUSED (argv);
  UNUSED (envp);

#ifdef _DEBUG
  int c;
  puts ("===============");
  while ((c = getc (stdin)) > 0)
    {
      putc ((c > 0) ? c : '?', stdout);
    }
  puts ("======EOF======\n");
  return 0;
#else /* _DEBUG */

  const char *less = LESS;

  /**
   *  DO *NOT* DELETE ME
   *  This prevents recursive moreless
   *  Without unsetenv, moreless will be injected again
   *  in the less command and will call itself recursively
   *
   *  TODO: We must only eliminate /path/to/moreless.so,
   *        Not the whole of the environment variable.
   */
  unsetenv ("LD_PRELOAD");

  /**
   *  TODO: Is there any efficient way to accept
   *        these LESS_OPTS options dynamically?
   *        maybe from an environment variable.
   */
  int ret = execlp (less, LESS_OPTS, NULL);
  if (ret < 0)
    {
      fprintf (stderr, LESS" itself failed.\n");
      return -ret;
    }

#endif /* _DEBUG */

  return -1; /* unreachable */
}

/**
 *  The strchrnull function was available only via _GNU_SOURCE,
 *  so we have implemented our version.
 *
 *  The `strchrnull` function is like `strchr` except  that
 *  if @c is not found in @s, then it returns a pointer to
 *  the null byte at the end of @s, rather than NULL.
 */
static inline const char *
strchrnull (const char *s, int c)
{
  for (int chr = *s; chr != 0; chr = *(++s))
    {
      if (chr == c)
        return s;
    }
  return s; /* s[0] must be equal to 0 */
}

/**
 *  Checks if the specified string @needle is found
 *  in the colon-separated string @haystack.
 */
static inline int
excludestr (const char *restrict haystack,
            const char *restrict needle)
{
  const char *p = haystack;
  for (ssize_t n = 0;
       haystack != NULL && *p != 0;
       haystack += n+1)
    {
      p = strchrnull (haystack, ':');
      n = p - haystack;
      if (n > 0 &&
          0 == strncmp (needle, haystack, n))
        {
          return 1;
        }
    }
  return 0;
}

int
main_hook (int argc, char **argv, char **envp)
  __Parent__
  __Child__
{
  pid_t pid;
  int pipefd[2];
  const char *cmd = argv[0];

  /**
   *  When the stdout is not a tty, the current program is part of
   *  a pipe chain, so we should not run an unnecessary less process
   */
  if (!isatty (STDOUT_FILENO))
    {
      mode = P_ESCAPED;
      goto __original_main;
    }

  /**
   *  Exclude commands that need tty and
   *  included in MORELESS_EXCLUDE environment variable
   *  If MORELESS_EXCLUDE starts with `:`, then this
   *  must overwrite default_excludes
   */
  const char *excludes = getenv ("MORELESS_EXCLUDE");

  if (!excludes)
    excludes = default_excludes;
  if (*excludes == ':')
    {
      if (excludestr (excludes, cmd) ||
          excludestr (default_excludes, cmd))
        {
        __do_escape:
          {
            mode = P_ESCAPED;
            unsetenv ("LD_PRELOAD");
            goto __original_main;
          }
        }
    }
  else if (excludestr (excludes, cmd))
    goto __do_escape;
  else if (getenv ("NO_LESS"))
    goto __do_escape;

  /**
   *  When the user tries to run an interactive shell
   *  moreless shoud not less the output
   *
   *  TODO: Is this a good way to detect interactive shells?
   */
  if (excludestr (shells, cmd))
    {
      if (argc == 1) /* this is an interactive shell */
        goto __do_escape;
    }

  /* Create pipe and do fork */
  if (pipe (pipefd) < 0)
    {
      perror ("pipe");
      return EXIT_FAILURE;
    }
  if ((pid = fork ()) < 0)
    {
      perror ("fork");
      return EXIT_FAILURE;
    }

  if (pid == 0) __Child__
    {
      close (pipefd[1]);
      dup2 (pipefd[0], STDIN_FILENO);
      close (pipefd[0]);
    }
  else __Parent__
    {
      mode = P_PARENT;
      close (pipefd[0]);
      dup2 (pipefd[1], STDOUT_FILENO);
      close (pipefd[1]);

#ifdef IMMID_PIPE
      setvbuf (stdout, NULL, _IONBF, 0);
#endif
    }

  if (mode) __Parent__
    {
    __original_main:
#ifdef _DEBUG
      fprintf (stderr, "moreless[%s] --> %s\n", mode_cstr[mode], *argv);
#endif
      /* Continue to the real main function */
      return original_main (argc, argv, envp);
    }
  else __Child__
    {
#ifdef _DEBUG
      fprintf (stderr, "moreless[%s] less %s\n", mode_cstr[mode], *argv);
#endif
      /* The less command lives here */
      return alter_main (argc, argv, envp);
    }
}
__Overwrite__
/**
 **  Libc-dependent function overwrite
 **
 **  This section should overwrite the function
 **  that calls the real main function.
 **
 **  It should initialize the global variable `original_main`
 **  with the address of the real main function and
 **  then execute it's original (super) function, which
 **  can be found this way: `dlsym(RTLD_NEXT, "func_name")`.
 **/
#if defined (__GLIBC__) /* GNU Libc (glibc) */

/**
 *  This code is stolen from:
 *   <https://gist.github.com/apsun/1e144bf7639b22ff0097171fa0f8c6b1>
 *
 *  Wrapper for __libc_start_main() that replaces the real main
 *  function with our hooked version.
 */
int
__libc_start_main (
    pre_main_t main, // the real main function
    int argc, char **argv,
    pre_main_t init,
    void (* fini)(void),
    void (* rtld_fini)(void),
    void *stack_end)
  __Overwrite__
  __Parent__
  __Child__
{
  /* Save the real main function address */
  original_main = main;

  /* Find the real __libc_start_main()... */
  typeof (&__libc_start_main) super =
    dlsym (RTLD_NEXT, "__libc_start_main");
  /* ... and call it with our custom main function */
  return super (main_hook, argc, argv,
                init, fini, rtld_fini, stack_end);
}
#else
#  error "Your libc is not supported!"
// TODO: support them
#endif
