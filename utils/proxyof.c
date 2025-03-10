/** file: proxyof.c
    created on: 5 Feb 2025

    Usage Examples:
      proxyof curl -I gnu.org
      http_proxy="localhost:8080" proxyof curl gnu.org
 **/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
extern char **environ;

#define PROGRAM_NAME "proxyof"

#define CLI_IMPLEMENTATION
#include "clistd.h"

enum proxy_type
  {
    DUMMY_PROXY = 0,
    HTTP_PROXY,
    SOCKS_PROXY
  };

/* Environment variables for proxy address */
const char *env_proxy_http[] = {
  "HTTP_PROXY", "HTTPS_PROXY",
  "http_proxy", "https_proxy",
};
const char *env_proxy_socks[] = {
  "SOCKS_PROXY", "SOCKS5_PROXY",
  "socks_proxy", "socks5_proxy",
};

#define lenof(arr) (sizeof (arr) / sizeof (*(arr)))

/**
 *  Is @str path to a file
 *  TODO: is checking str[0] == '/' or '.' enough?
 */
#define IS_PATH(str)                                    \
  (str[0] == '/' ||                                     \
   (str[0] == '.' && str[1] == '/') ||                  \
   (str[0] == '.' && str[1] == '.' && str[2] == '/'))

/* argv for the new process */
char **new_argv = NULL;
/* proxy option for the new process */
char *PROXY_OPT = NULL;
/* proto://host:port  or  host:port */
char *PROXY_ADDR = NULL;

typedef struct
{
  const char *prog_name; /* program name */
  const char *proxy_opt_http; /* http proxy option */
  const char *proxy_opt_socks; /* socks proxy option */
} proxy_opt;

const proxy_opt proxy_opts[] =
  {
    /* Supported programs */
    {.prog_name       = "curl",
     .proxy_opt_http  = "--proxy",
     .proxy_opt_socks = "--proxy"},


    /* The tester program (a.out) */
    {.prog_name       = "a.out",
     .proxy_opt_http  = "--http-proxy",
     .proxy_opt_socks = "--socks-proxy"},
    /* End of supported programs */
    {0}
  };


#ifndef PROXYOF_TESTER
/**
 *  Program Lookup
 *  @return: proxy option of the @prog_name
 *           NULL when it's not known
 */
static inline const char *
prog_lookup (const char *prog_name, int type)
{
  for (const proxy_opt *pa = proxy_opts;
       pa->prog_name != NULL; ++pa)
    {
      if (strcmp (prog_name, pa->prog_name) == 0)
        {
          switch (type)
            {
            case HTTP_PROXY:
              return pa->proxy_opt_http;

            case SOCKS_PROXY:
              return pa->proxy_opt_socks;

            default:
              return NULL;
            }
        }
    }
  return NULL;
}

/**
 *  Program name resolution
 *  Removes path and returns only the name:
 *    `/path/to/a.out`  ->  `a.out`
 *  Returns NULL on failure
 */
static inline char *
prog_resolve (char *prog)
{
  if (prog == NULL)
    {
      warnln ("invalid program name");
      return NULL;
    }

  /* Get the latest slash */
  char *name = strrchr (prog, '/');
  name = (name != NULL) ? name + 1 : prog;

  if (*name == '\0')
    return NULL;
  return name;
}

static inline int
get_proxy_env ()
{
  for (size_t i=0; i < lenof (env_proxy_http); ++i)
    {
      if ((PROXY_ADDR = getenv (env_proxy_http[i])))
        return HTTP_PROXY;
    }

  for (size_t i=0; i < lenof (env_proxy_socks); ++i)
    {
      if ((PROXY_ADDR = getenv (env_proxy_socks[i])))
        return SOCKS_PROXY;
    }

  return DUMMY_PROXY;
}

int
main (int argc, char **argv)
{
  set_program_name (*argv);

  if (argc < 2)
    {
      fprintf (stderr, "\
Usage:  proxyof PROGRAM_NAME [PROGRAM_OPTIONS]\n\
        proxyof /path/to/program [PROGRAM_OPTIONS]\n\
");
      return EXIT_FAILURE;
    }

  /* Name of the program to be executed */
  char *prog = prog_resolve (argv[1]);

  int proxy_type;

#ifndef DIE_WITHOUT_PROXY
  if ((proxy_type = get_proxy_env ()) == DUMMY_PROXY)
    {
      ++argv;
      if (IS_PATH (*argv))
        return execv (*argv, argv);
      else
        return execvp (prog, argv);
    }
#else
  if ((proxy_type = get_proxy_env ()) == DUMMY_PROXY)
    {
      warnln ("\
could not read proxy environment variables\n\
expected HTTP(s)_PROXY or SOCKS(5)_PROXY to be defined\
");
      return EXIT_FAILURE;
    }
#endif /* DIE_WITHOUT_PROXY */

  /**
   *  Proxy option of the program @prog
   *  We use strdup, because the exec syscall,
   *  does not expect constant argv
   */
  const char *opt = prog_lookup (prog, proxy_type);
  if (opt)
    {
      PROXY_OPT = strdup (opt);
    }
  else
    {
      warnln ("program is not supported");
      return EXIT_FAILURE;
    }

  /* new_argv = {argv} U {PROXY_OPT, PROXY, NULL} */
  new_argv = (char **) malloc ((argc + 3) * sizeof (char *));
  argc--;
  memcpy (new_argv, argv + 1, argc * sizeof (char *));

  /* Append option & proxy addr */
  new_argv[argc++] = PROXY_OPT;
  new_argv[argc++] = PROXY_ADDR;
  /* NULL termination */
  new_argv[argc] = NULL;

  if (IS_PATH (argv[1]))
    {
      /* running: /path/to/program */
      new_argv[0] = argv[1];
      return execv (*new_argv, new_argv);
    }
  else
    {
      /* running: @prog in PATH */
      new_argv[0] = prog;
      return execvp (prog, new_argv);
    }
}

#else /* PROXYOF_TESTER */
/**
 *  This is a simple program to test proxyof
 *  It only prints it's arguments (argv)
 */
int
main (int argc, char **argv)
{
  printf ("tester: ");
  for (--argc, ++argv ; argc != 0;
       --argc, ++argv)
    printf ("'%s', ", *argv);

  /* End of Options */
  puts ("EOO");
}
#endif /* PROXYOF_TESTER */
