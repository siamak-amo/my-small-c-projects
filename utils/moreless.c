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
   A simple tool to view programs output using the less command

   ** This only works with the GNU C Library (glibc)
      For other libc implementations, you need to overwrite
      the equivalent `__libc_start_main` function **

   Usage:
     For a single command:
       $ LD_PRELOAD=/path/to/moreless.so ls -ltrh

     To make it permanent:
       $ export LD_PRELOAD=/path/to/moreless.so
       Then run your commands.

     To exclude command(s) from moreless:
       $ export MORELESS_EXCLUDE="less:tmux:mpv"
       Colon separated command names in the
       `MORELESS_EXCLUDE` environment variable.
       By default it excludes less, tmux, screen.


   Compilation:
     cc -O3 -Wall -Wextra -fPIC -shared \
        -o moreless.so moreless.c

     For debugging:
       define -D_DEBUG
     To disable buffering in pipes:
       define -D IMMID_PIPE
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>
#include <string.h>

#ifndef LESS
#  define LESS "less"
#endif

#ifndef DEFAULT_EXCLUDES
#  define DEFAULT_EXCLUDES \
  "less:tmux:screen:vi:vim:nvim"
#endif

#undef UNUSED
#define UNUSED(x) (void)(x)

#define _PARENT /* belongs to the parent */
#define _CHILD /* belongs to the forked process */

int iam_parent = 0;

/* These functions call the real main function */
typedef int(*pre_main_t)(int argc, char **argv, char **envp);

/* Belongs to the actual binary (parent) */
static _PARENT pre_main_t original_main;


void _PARENT
__attribute__((constructor)) init()
{
}

void _PARENT
__attribute__((destructor)) cleanup()
{
  fflush (stdout);
  fclose (stdout);
  fclose (stderr);
  wait (NULL);
}

int _CHILD
alter_main (int argc, char **argv, char **envp)
{
  UNUSED (argc);
  UNUSED (argv);
  UNUSED (envp);

#ifdef _DEBUG
  int c;
  puts ("===============");
  while ((c = getc (stdin)) > 0)
    {
      printf ("%c", (c > 0) ? c : '?');
    }
  puts ("======EOF======\n");
  return 0;
#else /* _DEBUG */

  const char *less = LESS;
  /**
   *  DO *NOT* DELETE ME
   *  This will prevent recursive moreless
   *  Without unsetenv, moreless will be injected again
   *  in the less command and will call itself recursively
   *
   *  TODO: This must only delete /path/to/moreless.so
   *        not the whole of the env variable.
   */
  unsetenv ("LD_PRELOAD");

  // TODO: provide a way to pass less option(s).
  int ret = execlp (less, less, NULL);
  if (ret < 0)
    {
      fprintf (stderr, LESS" itself failed.\n");
      fflush (stdout);
      fclose (stdout);
      fclose (stdin);
      return -ret;
    }

#endif /* _DEBUG */

  return -1; /* unreachable */
}

/**
 *  The `strchrnul` function is like `strchr` except  that
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

int
main_hook (int argc, char **argv, char **envp)
{
  pid_t pid;
  int pipefd[2];
  const char *cmd = argv[0];

  /**
   *  When stdout is not a tty, there is a pipe already
   *  so we should not affect moreless there
   */
  if (!isatty (STDOUT_FILENO))
    {
      goto __original_main;
    }

  /**
   *  Exclude commands that need tty
   *  Command names, `:` separated, from
   *  the MORELESS_EXCLUDE environment variable
   */
  const char *excludes = getenv ("MORELESS_EXCLUDE");
  if (!excludes)
    excludes = DEFAULT_EXCLUDES;
  const char *p = excludes;

  for (ssize_t n = 0;
       excludes != NULL && *p != 0;
       excludes += n+1)
    {
      p = strchrnull (excludes, ':');
      n = p - excludes;
      if (n > 0 && 0 == strncmp (cmd, excludes, n))
        {
          unsetenv ("LD_PRELOAD");
          goto __original_main;
        }
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

  if (pid == 0) _CHILD  /* Child process */
    {
      close (pipefd[1]);
      dup2 (pipefd[0], STDIN_FILENO);
      close (pipefd[0]);
    }
  else _PARENT /* Parent process */
    {
      /**
       *  TODO: Maybe provide a way to also pass
       *  stderr through.
       *  currently, 2>&1 does not work
       */
      close (pipefd[0]);
      dup2 (pipefd[1], STDOUT_FILENO);
      close (pipefd[1]);
      iam_parent = 1;

#ifdef IMMID_PIPE
      setvbuf (stdout, NULL, _IONBF, 0);
#endif
    }

#ifdef _DEBUG
  /**
   *  `( -> )`  means only passing through
   *  `(less)`  means actually creating extra pipes
   */
  fprintf (stderr, "moreless(%s): handling %s\n",
           (iam_parent) ? " -> " : "less", *argv);
#endif /* _DEBUG */

  if (iam_parent) _PARENT
    {
    __original_main:
      /* Continue to the real main function */
      return original_main (argc, argv, envp);
    }
  else _CHILD
    {
      /* The less command lives here */
      return alter_main (argc, argv, envp);
    }
}

/**
 *  Libc-dependent function name
 *
 *  This section should overwrite the function
 *  that calls the actual main function.
 *
 *  It should initialize the global variable `original_main`
 *  with the address of the actual main function and
 *  then execute it's original (super) function, which
 *  can be found this way: `dlsym(RTLD_NEXT, "func_name")`.
 */
#if defined(__GLIBC__) /* GNU Libc (glibc) */
/**
 *  This code was stolen from:
 *    <https://gist.github.com/apsun/1e144bf7639b22ff0097171fa0f8c6b1>
 *  Wrapper for __libc_start_main() that replaces the real main
 *  function with our hooked version.
 */
int _PARENT
__libc_start_main (
    int (* main)(int, char **, char **),
    int argc, char **argv,
    int (* init)(int, char **, char **),
    void (* fini)(void),
    void (* rtld_fini)(void),
    void *stack_end)
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
