#define _GNU_SOURCE // TODO: what about BSD / MAC ??
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dlfcn.h>

#ifndef LESS
#  define LESS "less"
#endif

#undef UNUSED
#define UNUSED(x) (void)(x)

int iam_parent = 0;
static int (*original_main)(int, char **, char **);

void
__attribute__((constructor)) init()
{
  pid_t pid;
  int pipefd[2];

  /**
   *  When stdout is not a tty, there is a pipe already
   *  so we should not affect moreless there
   */
  if (!isatty (STDOUT_FILENO))
    {
      iam_parent = 1;
      return;
    }
  
  if (pipe (pipefd) < 0)
    {
      perror ("pipe");
      return;
    }

  pid = fork ();
  if (pid < 0)
    {
      perror ("fork");
      return;
    }

  if (pid == 0)
    { /* Child process */
      close (pipefd[1]);
      dup2 (pipefd[0], STDIN_FILENO);
      close (pipefd[0]);
    }
  else
    { /* Parent process */
      // TODO: maybe provide a way to also pass stderr through.
      close (pipefd[0]);
      dup2 (pipefd[1], STDOUT_FILENO);
      close (pipefd[1]);
      iam_parent = 1;

#ifdef IMMID_PIPE
      setvbuf (stdout, NULL, _IONBF, 0);
#endif
    } 
}

void
__attribute__((destructor)) cleanup()
{
  fflush (stdout);
  fclose (stdout);
  fclose (stderr);
  wait (NULL);
}

int
alter_main (int argc, char **argv, char **envp)
{
  UNUSED (argc);
  UNUSED (argv);
  UNUSED (envp);

#ifdef _DEBUG
  int c;
  puts ("---------------------------------");
  while ((c = getc (stdin)) > 0)
    {
      printf ("%c", (c > 0) ? c : '?');
    }
  puts ("--EOF----------------------------\n");
  return 0;

#else /* _DEBUG */

  const char *less = LESS;
  /**
   *  DO *NOT* DELETE ME
   *  This will prevent recursive moreless
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

int
main_hook (int argc, char **argv, char **envp)
{
#ifdef _DEBUG
  /**
   *  `( -> )`  means only passing through
   *  `(less)`  means actually creating extra pipes
   *
   *  TODO: Turn this comment to a proper help function.
   */
  fprintf (stderr, "moreless(%s): handling %s\n",
           (iam_parent) ? " -> " : "less", *argv);
#endif /* _DEBUG */
  if (iam_parent)
    {
      /**
       *  Continue to the real main function
       *
       *  TODO: redirecting 2>&1 does not work.
       */
      return original_main (argc, argv, envp);
    }
  else
    {
      /* `less` command lives here */
      return alter_main (argc, argv, envp);
    }
}

/**
 *  This code was stolen from:
 *    <https://gist.github.com/apsun/1e144bf7639b22ff0097171fa0f8c6b1>
 *  Wrapper for __libc_start_main() that replaces the real main
 *  function with our hooked version.
 */
int
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
    typeof (&__libc_start_main) origin =
      dlsym (RTLD_NEXT, "__libc_start_main");
    /* ... and call it with our custom main function */
    return origin (main_hook, argc, argv,
                   init, fini, rtld_fini, stack_end);
}
