/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>
   
  codeM is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.
  
  codeM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>. 
 */

/** file: codeM_shell.c
 *  separated out from: codeM.c
 *
 *  A minimal shell program for codeM library.
 *
 *  Usage:
 *    ./codeM  [OPTIONS]  [-c COMMANDS]  [ScriptFile.cm]
 *    see `./codeM -h' for more information
 *
 *  Shell commands:
 *    v,V       for validating codes
 *    c,C       for creating random city
 *    r,R,Q     for creating random codes
 *    f,s       for city lookup
 *    also a minimal concept of pipe `|' is implemented.
 *
 *  Compilation:
 *    to compile the CLI program:
 *      cc -ggdb -Wall -Wextra -Werror -o codeM codeM.c \
 *         -I../libs \
 *         $(pkg-config --libs --cflags readline)
 *
 *  Compilation options:
 *    `-D CLI_DEBUG':  print debug information
 **/
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define CODEM_IMPLEMENTATION
#define CODEM_FUZZY_SEARCH_CITYNAME
#include "codeM.c"


#ifdef _READLINE
#  define HAS_READLINE
#  if defined (_GNU_SOURCE) || defined (__linux__)
#    include <readline/readline.h>
#    include <readline/history.h>
#  elif defined (__APPLE__)
#    include <editline/readline.h> /* not tested */
#  endif
#endif /* _READLINE */

#ifdef CLI_DEBUG
/* debug macro to print codeM buffer */
#  define printd(param)                                 \
  printf ("[debug %s:%d] %s=[%s]\n",                    \
          __func__, __LINE__, __TOSTR__(param), param);
#  define dprintf(format, ...)                          \
  printf ("[debug %s:%d] "format,                       \
          __func__, __LINE__, __VA_ARGS__);
#else
#  define printd(param) do{} while (0)
#  define dprintf(format, ...) do{} while (0)
#endif

/**
 *  newline fprintf
 *  @format:  *only* use string "xxx"
 **/
#define fprintln(file, format, ...) \
  fprintf (file, format"\n", ##__VA_ARGS__)

/* normalize character to prevent printing non-ascii characters */
#define NORMCHAR(c) (((c) >= 0x20 && (c) <= 0x7E) ? (c) : '!')
/* free and set to null */
#define Free2Null(ptr) if (NULL != ptr){free (ptr); ptr = NULL;}
/* to string macros (this will not evaluate the input) */
#define __TOSTR__(x) #x
#define STR(x) __TOSTR__(x)

#undef MAX
#undef MIN
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))


static const char *PROMPT = "> ";
static const char PIPE = '|';
static const char *RD_PROMPT = "enter code: ";
static const char *CN_PROMPT = "enter name: ";
static const char *PATH_PROMPT = "enter path: ";
static const char *last_out; /* the last thing which was printed */

/**
 *  maximum length of a city name (in bytes)
 *  city names often consist of non-ASCII characters,
 *  so this value should only be used in scanf-like function calls,
 *  and no other assumption should be made about this value
 *  names within the `codeM_data.h` file consist of UTF-8 characters
 *  of length 2 (bytes), so the maximum length of a city name
 *  would be 64/2 = 32 (characters) [despite the space characters]
 */
#define CNAME_MAX_LEN 64
/* max buffer length for a (null terminated) city name */
#define CNAME_BUF_LEN 65
/* maximum codem and city name buffer length */
#define MAX_BUF_LEN MAX (CODEM_BUF_LEN, CNAME_BUF_LEN)

/**
 *  buffer for codem and city name
 *  used for all inputs and outputs and pipes
 */
static char buffer[MAX_BUF_LEN];
/**
 *  in the the following sscanf function calls,
 *  we used tmp_buffer as the destination buffer, where
 *  the length of the destination buffer specified
 *  in the sscanf's format parameter is either
 *  'CODEM_MAX_LEN' or 'CNAME_MAX_LEN'
 */
static char tmp_buffer[MAX_BUF_LEN];

/* script file path mode and error */
#define SC_FOPEN_FAILED -2
#define SC_INVALID_PATH -1
/* script file errors */
#define SCERR_FOPEN_FAILED "Could not open the script file"
#define SCERR_INVALID_PATH "Invalid script file path"
#define SCERR_UNKNOWN "Unknown error"
/* get string of error by error code */
#define script_strerr(code)                             \
  (code == SC_INVALID_PATH) ? SCERR_INVALID_PATH        \
  : (code == SC_FOPEN_FAILED) ? SCERR_FOPEN_FAILED      \
  : SCERR_UNKNOWN

/* a noise for random number generator */
static size_t noise = 0;

enum state_t {
  SHELL_MODE = 0,
  CMD_MODE,
  SCRIPT_MODE,
  EXITING
};

struct Conf {
  enum state_t state;
  bool silent_mode;           /* disables help and prompt */
  bool prompt;                /* printing ths PROMPT */
  bool ret2shell;             /* return to shell in script mode */
  bool EOO;                   /* End Of Options */
  bool commented;             /* section is commented */
  char *commands;             /* in command mode and shell mode when using readline */
  char *commandsH;            /* points to the .commands head */
  const char *__progname__;   /* name of the program */
  FILE *out;                  /* used by fprintf functions */
  FILE *script;               /* script.cm used in script mode */
};
static struct Conf *cfg;

/* state manipulating macros */
#define ENDOF_SILENT_MODE(cfg) do {    \
    cfg->prompt = true;                \
    cfg->silent_mode = false;          \
  } while (0)
#define GOTO_SILENT_MODE(cfg) do {     \
    cfg->prompt = false;               \
    cfg->silent_mode = true;           \
  } while (0)
#define GOTO_SCRIPT_MODE(cfg) do {     \
    GOTO_SILENT_MODE (cfg);            \
    cfg->state = SCRIPT_MODE;          \
  } while (0)
#define GOTO_CMD_MODE(cfg) do {        \
    GOTO_SILENT_MODE (cfg);            \
    cfg->state = CMD_MODE;             \
  } while (0)
#define GOTO_SHELL_MODE(cfg) do {      \
    ENDOF_SILENT_MODE (cfg);           \
    cfg->state = SHELL_MODE;           \
  } while (0)
#define GOTO_EXITING(cfg) cfg->state = EXITING
#define RET2SHELL(cfg) cfg->ret2shell = true
#define NotRET2SHELL(cfg) cfg->ret2shell = false;


static inline void
__stdin_flush ()
{
  int c;
  while ((c = getchar()) != '\n' && c != EOF);
}

static void
usage ()
{
  printf ("Usage: %s [OPTIONS] [COMMANDS] [ScriptFile.cm]\n"
          "SCRIPT FILE:\n"
          "  a text file, with codeM shell commands\n"
          "  add this shebang: `#!%s`\n"
          "OPTIONS:\n"
          "   -s:    silent mode\n"
          "   -S:    disable the prompt (when using pipe)\n"
          "   -c:    pass COMMANDS to be executed,\n"
          "            use: -c \"h\" to get help\n"
          "   -h:    help\n\n",
          cfg->__progname__, cfg->__progname__);
}

static inline void
__help_cmd ()
{
  fprintf (cfg->out,
           "Usage: ./codeM -c \"[COMMAND]\"\n"
           "COMMAND: sequence of shell mode commands\n"
           "commands could have one argument"
           " (Ex. `R 1234` ~ `R1234`)\n"
           "optionally separate commands by space, `;` or newline\n"
           "use \"H\" command to see more about commands\n\n");
}

static inline void
__help_shell ()
{
  fprintf (cfg->out,
           "v: validate                 -  V: make the input valid\n"
           "c: make randon city code    -  C: make random city name\n"
           "r: make random codem        -  R: random with prefix\n"
           "                               Q: random with suffix\n"
           "f: find city code by name   -  F: find city name by code\n"
           "s: search city code         -  S: search city name\n"
           "q: quit                     -  h: help\n"
           "!: to run a script file and return to the shell, and $ to run and exit\n"
           "to give the previous output to the next command use `|` (like shell)\n"
           "use e, E commands to echo and set the last output value respectively\n\n");
}

static void
help ()
{
  if (cfg->state == CMD_MODE)
    __help_cmd ();
  else
    __help_shell ();
}

/* super simple pseudo random number generator */
static size_t
ssrand ()
{
  size_t r = time (NULL) + noise++;

  for (int i=7; i>0; --i)
    {
      r *= 0x42;
      r += 0x666;
    }

  return r;
}

/**
 *  internal function to be used by codem_scanf and cname_scanf
 *  this function updates cfg->commands on CMD_MODE
 *  and expects the @sscanf_regex to have `%n` at the end
 *
 *  @return:
 *    on success  -> number of bytes read
 *    on failure  -> -1 on shell mode and 0 on command mode
 */
static inline int
scan__H (const char *restrict message,
         char *restrict dest, int dest_len,
         const char *restrict scan_format,
         const char *restrict sscan_regex)
{
  int n = 0;
  assert (dest_len <= MAX_BUF_LEN);

  if (cfg->state == SHELL_MODE)
    {
      if (cfg->prompt)
        printf (message);
      if (0 > scanf (scan_format, dest))
        return -1;
      if (0 > sscanf (dest, sscan_regex, tmp_buffer, &n))
        return -1;
      if (0 >= n)
        return -1;
      memcpy (dest, tmp_buffer, dest_len);
    }
  else if (cfg->state == SCRIPT_MODE)
    {
      if (0 > fscanf (cfg->script, scan_format, dest))
        return -1;
      if (0 > sscanf (dest, sscan_regex, tmp_buffer, &n))
        return -1;
      if (0 >= n)
        return -1;
      memcpy (dest, tmp_buffer, dest_len);
    }
  else if (cfg->state == CMD_MODE)
    {
      /* command mode */
      if (0 <= sscanf (cfg->commands, sscan_regex, dest, &n))
        {
          if (0 >= n)
            return 0;
          cfg->commands += n;
        }
      else
        n = 0;
    }

  return n;
}

static int
codem_scanf (const char *message, char dest[CODEM_BUF_LEN])
{
  int res = scan__H (message,
                     dest, CODEM_BUF_LEN,
                     "%"STR(CODEM_LEN)"s",              // "%10s"
                     " %"STR(CODEM_LEN)"[^;# ]%n");     // " %10[^;#]%n"
  if (res < 0)
    return -1;
  res = MIN (res, (int)strlen (dest));
  /* make the dest numeric */
  dest[res] = '\0';
  return res;
}

static int
cname_scanf (const char *message, char dest[CNAME_BUF_LEN])
{
  return scan__H (message,
                  dest, CNAME_BUF_LEN,
                  " %"STR(CNAME_MAX_LEN)"[^\r\n]",   // " %64[^\n\r]" (allows space)
                  " %"STR(CNAME_MAX_LEN)"[^;#]%n");  // " %64[^;#]%n"
}

static int
lastout_scanf (char *dest, size_t len)
{
  if (NULL == last_out)
    {
      *dest = '\0';
      return 0;
    }
  else
    {
      size_t last_len = strlen (last_out);
      len = MIN (len, last_len);
      memcpy (dest, last_out, len);
      dest[len] = '\0';
      return (int)len;
    }
}

/**
 *  fopen, for cfg->script
 *  there will be no way to access the previous script file
 *  after calling this function
 *  this function sets cfg->script to NULL on failure
 *  @return:
 *    0 on success and -1 on failure
 **/
static int
fopen_scirpt_file__H (const char *path)
{
  if (NULL != cfg->script)
    {
      fclose (cfg->script);
    }

  cfg->script = fopen (path, "r");
  if (NULL == cfg->script)
    return -1;
  else
    return 0;
}

/**
 *  readline helper function
 *  free the result when finished
 **/
char *
readline__H (const char *prompt)
{
#ifdef HAS_READLINE
  char *p = readline (prompt);
  char *res = malloc (strlen (p));
  sscanf (p, " %s ", res);
  free (p);
  return res;

#else
  UNUSED (prompt);
  char *p = NULL;
  size_t len;
  len = getline (&p, &len, stdin);
  if (len > 0)
    p[len - 1] = '\0';
  return p;
#endif
}

static void
exec_command (char prev_comm, char comm)
{
  dprintf ("running: (%c), prev_command: (%c)\n",
           NORMCHAR (comm), NORMCHAR (prev_comm));
  int res, off;
  const char *p;
  char *file_path = NULL;

  if (cfg->commented)
    {
      switch (comm)
        {
          /* end of commented section */
        case '\n':
        case '\r':
        case ';':
          cfg->commented = false;
          break;

        default:
          return;
        }
    }

  switch (comm)
    {
      /* validation */
    case 'v':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CODEM_LEN))
            break;
        }
      else if (0 > codem_scanf (RD_PROMPT, buffer))
        break;
      if (0 != codem_norm (buffer))
        {
          fprintln (stderr, "Could not normalize the input");
          break;
        }
      printd (buffer);
      if (codem_isvalidn (buffer))
        {
          fprintln (stdout, "%s", (last_out = "OK."));
          if (!codem_ccode_isvalid (buffer))
            fprintln (stdout, "city code was not found.");
        }
      else
        fprintln (stdout, "%s", (last_out = "Not Valid."));
      break;

      /* make a code valid */
    case 'V':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CODEM_LEN))
            break;
        }
      else if (0 > codem_scanf (RD_PROMPT, buffer))
        break;
      if (0 != codem_norm (buffer))
        {
          fprintln (stderr, "Could not normalize the input");
          break;
        }
      printd (buffer);
      codem_set_ctrl_digit (buffer);
      last_out = buffer;
      fprintln (stdout, "%s", last_out);
      break;

      /* make a random city code */
    case 'c':
      codem_rand_ccode (buffer);
      last_out = buffer;
      printf ("%.3s\n", last_out);
      break;

      /* make a random city name */
    case 'C':
      codem_rand_ccode (buffer);
      last_out = codem_cname (buffer);
      fprintln (stdout, "%s", last_out);
      break;

      /* make a random code */
    case 'r':
      codem_rand2 (buffer);
      last_out = buffer;
      fprintln (stdout, "%s", last_out);
      break;

      /* make a random code by prefix */
    case 'R':
      if (PIPE == prev_comm)
        {
          if (0 > (off = lastout_scanf (buffer, CODEM_LEN)))
            break;
        }
      else
        off = codem_scanf (RD_PROMPT, buffer);
      printd (buffer);
      if (off < 0)
        break;
      else if (off > CODEM_LEN)
        assert (0 && "Invalid offset of codem_scanf");
      else
        {
          codem_randp (buffer, off);
          last_out = buffer;
          fprintln (stdout, "%s", last_out);
        }
      break;

    case 'Q':
      if (PIPE == prev_comm)
        {
          if (0 > (off = lastout_scanf (buffer, CODEM_LEN)))
            break;
        }
      else
        off = codem_scanf (RD_PROMPT, buffer);
      if (off < 0)
        break;
      else if (off > CODEM_LEN)
        assert (0 && "Invalid offset of codem_scanf");
      printd (buffer);
      if (0 != codem_norm (buffer))
        {
          fprintln (stderr, "Could not normalize the input");
          break;
        }
      else
        {
          if (codem_rands (buffer, off))
            last_out = buffer;
          else
            last_out = "failure";
          fprintln (stdout, "%s", last_out);
        }
      break;

      /* find city name by code */
    case 'F':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CODEM_LEN))
            break;
        }
      else if (0 > codem_scanf (RD_PROMPT, buffer))
        break;
      printd (buffer);
      last_out = codem_cname (buffer);
      fprintln (stdout, "%s", last_out);
      break;

      /* find city code by city name */
    case 'f':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CNAME_MAX_LEN))
            break;
        }
      else if (0 > cname_scanf (CN_PROMPT, buffer))
        break;
      res = codem_cname_search (buffer);
      printd (buffer);
      p = codem_ccode (res);
      if (res < 0)
          fprintln (stdout, "%s", p);
      else
        {
          for (; *p != 0; p += CC_LEN)
            printf ("%.3s\n", p);
          /* set the last_out to the last code */
          last_out = p - CC_LEN;
        }
      break;

      /* search city code */
    case 's':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CNAME_MAX_LEN))
            break;
        }
      else if (0 > codem_scanf (CN_PROMPT, buffer))
        break;
      printd (buffer);
      res = codem_ccode_idx (buffer);
      if (res < 0)
        fprintln (stdout, "%s", CCERR_NOT_FOUND);
      else
        {
          p = codem_ccode (res);
          for (; *p != 0; p += CC_LEN)
            printf ("%.3s\n", p);
          /* set the last_out to the last code */
          last_out = p - CC_LEN;
        }
      break;

      /* search city name */
    case 'S':
      if (PIPE == prev_comm)
        {
          if (0 >= lastout_scanf (buffer, CNAME_MAX_LEN))
            break;
        }
      else if (0 > cname_scanf (CN_PROMPT, buffer))
        break;
      printd (buffer);
      res = codem_cname_search (buffer);
      p = codem_cname_byidx (res);
      last_out = p;
      fprintln (stdout, "%s", last_out);
      break;

    case 'e':
      if (PIPE == prev_comm)
        fprintln (stdout, "%s", last_out);
      else
        fprintln (stderr, "only use this command with pipe");
      break;

    case 'E':
      if (PIPE == prev_comm)
        {
          fprintln (stderr, "do not use this command with pipe");
          break;
        }
      else if (0 > cname_scanf ("enter value: ", buffer))
        break;
      printd (buffer);
      last_out = buffer;
      break;

    case '!':
      cfg->commented = true;
      switch (cfg->state)
        {
        case SHELL_MODE:
          {
            /* get the path (readline) -> execute it -> return back to the shell */
            file_path = readline__H (PATH_PROMPT);
            if (!fopen_scirpt_file__H (file_path))
              {
                RET2SHELL (cfg);
                GOTO_SCRIPT_MODE (cfg);
                cfg->commented = false;
#ifndef HAS_READLINE
                __stdin_flush ();
#endif
              }
            else
              {
                fprintln (stdout, "%s (%s)", SCERR_FOPEN_FAILED, file_path);
              }
            Free2Null (file_path);
          }
          break;

        case CMD_MODE:
        case SCRIPT_MODE:
          {
            /* only return to the shell */
            cfg->commented = false;
            GOTO_SHELL_MODE (cfg);
            RET2SHELL (cfg);
          }
          break;

        default:
          break;
        }
      break;

    case '$':
      cfg->commented = true;
      switch (cfg->state)
        {
        case SHELL_MODE:
          {
            /* get the path (readline) -> execute it -> exit */
            file_path = readline__H (PATH_PROMPT);
            if (!fopen_scirpt_file__H (file_path))
              {
                NotRET2SHELL (cfg);
                GOTO_SCRIPT_MODE (cfg);
                cfg->commented = false;
                __stdin_flush ();
              }
            else
              {
                fprintln (stdout, "%s (%s)", SCERR_FOPEN_FAILED, file_path);
              }
            Free2Null (file_path);
          }
          break;

        case SCRIPT_MODE:
          {
            /* get the path -> execute it, with no other assumption */
            size_t cap;
            size_t len = getline (&file_path, &cap, cfg->script);
            if (len < 1)
              {
                fprintln (stderr, SCERR_INVALID_PATH);
                break;
              }
            file_path[--len] = '\0'; /* remove the new line at the end */
            if (!fopen_scirpt_file__H (file_path))
              {
                /* just continue the execution */
                cfg->commented = false;
                break;
              }
            else
              {
                /**
                 *  the previous script file has been closed
                 *  so there is no option but to exit
                 */
                fprintln (stderr, "%s (%s)", SCERR_FOPEN_FAILED, file_path);
                GOTO_EXITING (cfg);
                break;
              }
            Free2Null (file_path);
          }
          break;

        case CMD_MODE:
          fprintln (stderr, "the `$` command is not supported in command mode");
          break;

        default:
          break;
        }
      break;

    case 'h':
      help ();
      break;

    case 'H':
      __help_shell ();
      break;

    case 'u':
      usage ();
      break;

    case 'q':
      GOTO_EXITING (cfg);
      break;

    case '\n':
    case '\r':
    case '\\': /* handled by the normalize_command function */
    case ' ': /* separator */
    case ';': /* separator */
    case '|': /* use the output of the previous command */
      break;

    case '#': /* comment */
      cfg->commented = true;
      break;

      /* invalid command */
    default:
      switch (prev_comm)
        {
        case '\n':
        case '\0':
        case ' ':
        case ';':
        case '|':
        InvalidCommand:
          fprintln (stderr, "Invalid command -- (%c)", comm);
          break;

        default:
          if (cfg->state == CMD_MODE)
            goto InvalidCommand;
          else
            break;
        }
      break;
    }
}

/**
 *  normalize command
 *  this function sets both of comm and prev_comm to space
 *  character, when prev_comm is `\` (except pipe `|`)
 *  also handles space characters after pipe command (passes the pipe)
 **/
static inline void
normalize_command (char *restrict prev_comm,
                   char *restrict comm)
{
  if ('\\' == *prev_comm) /* escape */
    {
      *prev_comm = ' ';
      if ('|' != *comm)
        *comm = ' ';
    }
  if (' ' == *comm && '|' == *prev_comm) /* pipe */
    {
      *comm = '|';
      *prev_comm = ' ';
    }
}

/**
 *  pare cmdline options
 *  @return:
 *    negative on failure and `0` on success
 **/
static inline int
parse_options (int argc, char **argv)
{
  cfg->__progname__ = argv[0];
  for (argc--, argv++; argc > 0; argc--, argv++)
    {
      if (cfg->EOO)
        {
          if (strlen (*argv) != 0)
            {
              size_t cmd_len = strlen (cfg->commands);
              cfg->commands = realloc (cfg->commands, cmd_len + strlen (*argv) + 1);
              cfg->commandsH = cfg->commands;
              strcpy (cfg->commands + cmd_len, *argv);
            }
        }
      /* script filename */
      if (argv[0][0] != '-')
        {
          if (strlen (*argv) > 3)
            {
              GOTO_SCRIPT_MODE (cfg);
              cfg->script = fopen (*argv, "r");
              if (NULL == cfg->script)
                {
                  fprintf (stderr, "Could not open file (%s)", argv[0]);
                  return -2;
                }
            }
        }
      /* normal option */
      if (argv[0][0] == '-')
        {
          switch (argv[0][1])
            {
            case 's':
              cfg->silent_mode = true;
              break;

            case 'S':
              cfg->prompt = false;
              break;

            case 'c':
              cfg->silent_mode = true;
              cfg->prompt = false;
              if (argc == 1)
                cfg->commands = "h";
              else
                {
                  cfg->commands = *(argv+1);
                  argc--;
                  argv++;
                }
              cfg->state = CMD_MODE;
              break;

            case 'h':
              usage ();
              GOTO_EXITING (cfg);
              break;

            case '-':
              cfg->EOO = true;
              if (NULL != cfg->commands)
                {
                  const char *__p = cfg->commands;
                  cfg->commands = malloc (strlen (__p) + 1);
                  cfg->commandsH = cfg->commands;
                  strcpy (cfg->commands, __p);
                }
              else
                {
                  cfg->commands = malloc (1);
                  *cfg->commands = '\0';
                  GOTO_CMD_MODE (cfg);
                }
              break;

            default:
              fprintf (stderr, "Invalid option (%s)", argv[0]);
              return -2;
            }
        }
    }
  return 0;
}

#ifdef HAS_READLINE
static int
rlcmd_scanf (char *c)
{
  if (NULL == cfg->commands)
    {
      cfg->commands = readline ((cfg->prompt) ? PROMPT : NULL);
      if (NULL == cfg->commands)
        return EOF;
      cfg->commandsH = cfg->commands;
      *c = ';';
    }
  else
    {
      if (*cfg->commands == '\0')
        {
          free (cfg->commandsH);
          cfg->commands = NULL;
          *c = ';';
        }
      else
        {
          if (*cfg->commands >= 0x20 && *cfg->commands <= 0x7E)
            *c = *cfg->commands;
          else
            *c = ' ';
          cfg->commands++;
        }
    }
  return 0;
}
#endif


int
main (int argc, char **argv)
{
  char comm = '\0', prev_comm = comm;

  cfg = &(struct Conf){
    .silent_mode = false,
    .state = SHELL_MODE,
    .prompt = true,
    .commands = NULL,
    .commandsH = NULL,
    .EOO = false,
    .commented = false,
    .ret2shell = false,
    .out = (!isatty (fileno (stdout))) ? stderr : stdout
  };

  /* initialize codeM random number generator function */
  codem_rand_init (ssrand);

  /* parsing cmdline arguments */
  if (parse_options (argc, argv) < 0)
    {
      fprintf (stderr, " -- exiting.\n");
      return 1;
    }
  /* disable the prompt when `stdin` is not a tty (using pipe) */
  if (!isatty (fileno (stdin)))
    GOTO_SILENT_MODE (cfg);

  /* continue to command mode or shell mode */
  while (EXITING != cfg->state)
  switch (cfg->state)
    {
    case CMD_MODE:
      while (cfg->state == CMD_MODE)
        {
          prev_comm = comm;
          comm = *cfg->commands;
          if ('\0' == comm)
            {
              GOTO_EXITING (cfg);
              break;
            }
          cfg->commands++;
          /* interpretation of backslash escapes */
          normalize_command (&prev_comm, &comm);
          /* execute the current command */
          exec_command (prev_comm, comm);
        }
      break;

    case SHELL_MODE:
      if (!cfg->silent_mode && cfg->prompt && !cfg->ret2shell)
        {
          fprintf (stdout, "codeM Shell Mode!\n");
          usage ();
          help ();
        }
      while (cfg->state == SHELL_MODE)
        {
#ifndef HAS_READLINE
          /* print the prompt */
          if (cfg->prompt)
            {
              switch (comm)
                {
                case '\n':
                case '\r':
                case '\0':
                case '!':
                case '$':
                  fprintf (stdout, PROMPT);
                  break;

                default:
                  break;
                }
            }
#endif
          /* read new command until EOF */
          prev_comm = comm;

#ifndef HAS_READLINE
          if (EOF == scanf ("%c", &comm))
            {
              if (cfg->prompt)
                fprintf (stdout, "\n");
              GOTO_EXITING (cfg);
              continue;
            }
#else
          if (EOF == rlcmd_scanf (&comm))
            {
              GOTO_EXITING (cfg);
              continue;
            }
#endif
          /* interpretation of backslash escapes */
          normalize_command (&prev_comm, &comm);
          /* execute the current command */
          exec_command (prev_comm, comm);
        }
      break;

    case SCRIPT_MODE:
      while (cfg->state == SCRIPT_MODE)
        {
          /* read new command until EOF */
          prev_comm = comm;
          if (NULL == cfg->script)
            {
              GOTO_EXITING (cfg);
              continue;
            }
          if (EOF == fscanf (cfg->script, "%c", &comm))
            {
              GOTO_EXITING (cfg);
              continue;
            }
          /* interpretation of backslash escapes */
          normalize_command (&prev_comm, &comm);
          /* execute the current command */
          exec_command (prev_comm, comm);
        }
      if (cfg->script)
        {
          fclose (cfg->script);
          cfg->script = NULL;
        }
      if (cfg->ret2shell)
        GOTO_SHELL_MODE (cfg);

      break;

    case EXITING:
    default:
      break;
    }

  /**
   *  cfg->commandsH != NULL indicates that,
   *  the cfg->commands has been allocated using
   *  malloc, otherwise it's a pointer to some argv
   *  and should not be freed
   **/
  if (NULL != cfg->commandsH)
    {
      free (cfg->commandsH);
    }
   return 0;
}
