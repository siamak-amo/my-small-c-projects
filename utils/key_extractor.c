/**
 *  file: key_extractor.c
 *  created on: 8 Sep 2024
 *
 *  Keyword Extractor
 *  Extracts keywords out of the provided input
 *  It was previously named tokenizeIt
 *
 *  Usage:  kextractor [OPTIONS]
 *  see help for details: `kextractor -h`
 *
 * Compilation:
 *   cc -Wall -Wextra -Werror -ggdb -O3 \
 *      -D_USE_BIO -I../libs \
 *      key_extractor.c -o kextractor
 *
 * Options:
 *   -D_USE_BIO:
 *      To compile with buffered_io.h
 *   -D_BMAX="(1 * 1024)":
 *      To max buffer length of buffered IO
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef _USE_BIO
# ifndef _BMAX
#  define _BMAX (1024) // 1Kb
# endif
# define BIO_IMPLEMENTATION
# include "buffered_io.h"
#endif

#define Version "2"
#define PROGRAM_NAME "key_extractor"
#define CLI_IMPLEMENTATION
#define CLI_NO_GETOPT /* we handle options ourselves */
#include "clistd.h"
#include <getopt.h>
static struct option const long_options[] =
{
  {"help",      no_argument,       NULL, 'h'},
  {"version",   no_argument,       NULL, 'v'},
  /* input /output file path */
  {"in",        required_argument, NULL, 'i'},
  {"if",        required_argument, NULL, 'i'},
  {"input",     required_argument, NULL, 'i'},
  {"out",       required_argument, NULL, 'o'},
  {"of",        required_argument, NULL, 'o'},
  {"output",    required_argument, NULL, 'o'},
  /* output append */
  {"oa",        required_argument, NULL, 'a'},
  {"oA",        required_argument, NULL, 'a'},
  {NULL,        0,                 NULL,  0 },
};


#define TOKEN_MAX_BUF_LEN (512) // 0.5Kb
#define ML_IMPLEMENTATION
#include "mini-lexer.c"

enum LANG
  {
    STR1 = 0,
    STR2,
    STR3,
  };

/**
 *  Any string types enclosed
 *  within these delimiters will be ignored
 */
 static struct Milexer_exp_ Expressions[] = {
  [STR1]          = {"\"", "\""},
  [STR2]          = {"'", "'"},
  [STR3]          = {"`", "`"},
};

/**
 *  This includes nearly all non-alphanumeric
 *  ASCII characters
 *
 *  It primarily allows only alphanumeric tokens
 *
 *  The presence of any prefixes of @ML fields
 *  (like @Expressions) within this context,
 *  causes conflicts and affects the expected behavior
 */
static const char *Delimiters[] = {
  "\x00\x21",   /* below '"' */
  "\x23\x2F",   /* between '"' and '0' */
  "\x3A\x40",   /* after '9' and before 'A' */
  "\x5B",       /* '[' */
  "\x5D",       /* ']' */
  "\x5E",       /* '^' */
  "\x60",       /* '`' */
  "\x7B\xFF",   /* after 'Z' */
};

int allow_numbers = false;
int allow_strings = false;

static const Milexer ML = {
  .expression   = GEN_MKCFG (Expressions),
  .delim_ranges = GEN_MKCFG (Delimiters),
};

typedef ssize_t (*loader) (int, void *, size_t);

int infd = STDIN_FILENO;
int ofd = STDOUT_FILENO;

#ifdef _USE_BIO
static BIO_t bio;
#endif

#ifdef _USE_BIO
# define Print(str) bio_fputs (&bio, str)
# define Putln() bio_puts (&bio, "")
#else
# define Print(str) dprintf (ofd, "%s", str)
# define Putln() dprintf (ofd, "\n")
#endif /* _USE_BIO */

void
usage (int)
{
  printf ("\
Usage: %s [OPTIONS]\n\
",
          program_name);

  printf ("\
Extracts alphanumeric keywords from the given input.\n\
If no input file is provided, it uses stdin and stdout\n\
");

  printf ("\n\
OPTIONS:\n\
     -i, --if          input file path\n\
     -o, --output      output file path\n\
     -a, --oA          output file path to append\n\
     -s                include strings\n\
     -n                include numbers\n\
");
}

int
safe_open (const char *restrict pathname, const char *restrict mode)
{
  FILE *f;
  if (!pathname || !mode)
    {
      warnf ("invalud filename");
      return -1;
    }
  if ((f = fopen (pathname, mode)) == NULL)
    {
      warnf ("could not open file -- (%s:%s)", mode, pathname);
      return -1;
    }
  return fileno (f);
}

/**
 *  @return:  0 -> success
 *     negative -> exit with 0 code
 *     positive -> exit with non-zero exit code
 */
int
parse_args (int argc, char **argv)
{
  int c;
  const char *params = "+i:o:a:vhns";
  while (1)
    {
      c = getopt_long (argc, argv, params, long_options, NULL);
      if (c == -1)
        break;
      switch (c)
        {
        case 'v':
          version_etc (stdout, program_name, Version);
          return 0;

        case 'h':
          usage (-1);
          return -1;

        case 'n':
          allow_numbers = true;
          break;

        case 's':
          allow_strings = true;
          break;

        case 'i':
          if (infd != STDIN_FILENO)
            close (infd);
          if ((infd = safe_open (optarg, "r")) == -1)
            return 1;
          break;

        case 'o':
          if (ofd != STDOUT_FILENO)
            close (ofd);
          if ((ofd = safe_open (optarg, "w")) == -1)
            return 1;
          break;

        case 'a':
          if (ofd != STDOUT_FILENO)
            close (ofd);
          if ((ofd = safe_open (optarg, "a")) == -1)
            return 1;
          break;

        default:
          break;
        }
    }
  return 0;
}

static inline int
token_out (const char *cstr)
{
  if (allow_numbers)
    {
      Print (cstr);
      return 1;
    }
  else
    {
      char *end;
      /* We assume strings staring with 0x are numbers */
      if (strncmp (cstr, "0x", 2) == 0)
        {
          return 0;
        }
      strtol (cstr, &end, 10);
      if (*(end + 1) == '\0' && errno == 0)
        {
          return 0;
        }
      Print (cstr);
      return 1;
    }
  return -1; /* unreachable */
}

int
main (int argc, char **argv)
{
  set_program_name (*argv);
  int ret = parse_args (argc, argv);
  if (ret > 0)
    return ret;
  else return 0;

  if (infd == ofd)
    {
      warnf ("output file was changed to stdout");
      close (ofd);
      ofd = STDOUT_FILENO;
    }

  int buf_len = TOKEN_MAX_BUF_LEN;
  char *buf = malloc (buf_len);
  Milexer_Token tk = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN);
  Milexer_Slice src = {.lazy = true};

  /**
   *  Initializing buffered IO
   *  As this uses `ofd` for output file descriptor
   *  of `bio`, Is must be done after any parsing argument
   *  that might change ofd
   */
#ifdef _USE_BIO
  int bio_cap = _BMAX;
  bio = bio_new (bio_cap, malloc (bio_cap), ofd);
#endif

  if (infd == STDIN_FILENO)
    {
      puts ("reading from stdin until EOF");
    }

  int len;
  for (int ret = 0; !NEXT_SHOULD_END (ret); )
    {
      ret = ml_next (&ML, &src, &tk, PFLAG_DEFAULT);
      switch (ret)
        {
        case NEXT_NEED_LOAD:
          if ((len = read (infd, buf, buf_len)) > 0)
            SET_ML_SLICE (&src, buf, len);
          else
            END_ML_SLICE (&src);
          break; 
 
        case NEXT_CHUNK:
          if (allow_strings || tk.type == TK_KEYWORD)
            token_out (tk.cstr);
          break;

        case NEXT_MATCH:
        case NEXT_ZTERM:
          if (allow_strings || tk.type == TK_KEYWORD)
            {
              if (token_out (tk.cstr))
                Putln ();
            }
          break;

        default: break;
        }
     }
  
  TOKEN_FREE (&tk);
  free (buf);

#ifdef _USE_BIO
  bio_flush (&bio);
  free (bio.buffer);
#endif

  return 0;
}
