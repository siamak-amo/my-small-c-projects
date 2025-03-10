/** file: key_extractor.c
    created on: 8 Sep 2024

    Keyword Extractor
    Extracts keywords out of the provided input
    It was previously named tokenizeIt

    Usage:  kextractor [OPTIONS]
      see help for details: `kextractor -h`

    Compilation:
      cc -ggdb -O3 -Wall -Wextra -Werror -I../libs \
        key_extractor.c -o kextractor

    Options:
      Output buffer capacity in bytes:
        `-D_BMAX="(1 * 1024)"`
 **/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>

#define Version "2"
#define PROGRAM_NAME "key_extractor"

#define CLI_IMPLEMENTATION
#define CLI_NO_GETOPT /* we handle options ourselves */
#include "clistd.h"

#define isbdigit(x) ((x)=='0' || (x)=='1')

# ifndef _BMAX
#  define _BMAX (4 * 1024) // 4Kb = 1 disk sector
# endif


static struct option const long_options[] =
{
  {"help",      no_argument,       NULL, 'h'},
  {"version",   no_argument,       NULL, 'v'},
  /* input /output file path */
  {"in",        required_argument, NULL, '0'},
  {"if",        required_argument, NULL, '0'},
  {"input",     required_argument, NULL, '0'},
  {"out",       required_argument, NULL, '1'},
  {"of",        required_argument, NULL, '1'},
  {"output",    required_argument, NULL, '1'},
  /* output append */
  {"oa",        required_argument, NULL, 'a'},
  {"oA",        required_argument, NULL, 'a'},
  /* append extra delimiter */
  {"add-delim", required_argument, NULL, 'd'},
  {"ext-delim", required_argument, NULL, 'd'},
  /* overwrite delimiters */
  {"set-delim", required_argument, NULL, 'D'},
  /* output format */
  {"format",    required_argument, NULL, 'o'},
  /* extra supports */
  {"js",        no_argument,       NULL, '2'},
  /* do not delete me */
  {NULL,        0,                 NULL,  0 },
};

#define DYNA_IMPLEMENTATION
#include "dyna.h"

#define TOKEN_MAX_BUF_LEN (512) // 0.5Kb
#define ML_IMPLEMENTATION
#include "mini-lexer.c"

enum LANG
  {
    STR1 = 0,
    STR2,
    STR3,
    JS_REG, /* e.g. `const re = /.../;` */
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
static struct Milexer_exp_ Expressions_JS[] = {
  [STR1]          = {"\"", "\""},
  [STR2]          = {"'", "'"},
  [STR3]          = {"`", "`"},
  [JS_REG]        = {"/", "/"},
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
/* Extra delimiters, dynamic array (using dyna.h) */
static const char **Extra_Delims = NULL;

#define FLG(n) (1<<(n))
#define HAS_FLG(t, flg) ((t) & (flg))

enum ke_flag_t
  {
    NOT_SET = 0,

    /* Output format */
    O_ALLOW_KEY     = FLG (1),
    O_ALLOW_NUM     = FLG (2),
    O_ALLOW_STR     = FLG (3),
    O_FULL_STR      = FLG (4),
    O_DIS_STR       = FLG (5),
    O_PROVIDED      = FLG (15), // bound

    /* User configurations */
    C_EXT_DELIMS    = FLG (16),
    C_OVERW_DELIMS  = FLG (17),
    C_JAVASCRIPT    = FLG (18),
    C_PROVIDED      = FLG (31), // bound
  };

/* Output flags */
int kflags = 0;
int parse_flg = 0;

static Milexer ML = {0};

FILE *in_stream = NULL;
FILE *out_stream = NULL;
char *out_buff = NULL; // out_stream buffer

int in_buff_len = TOKEN_MAX_BUF_LEN;
char *in_buff = NULL; // milexer token buffer

#define Outstr(str) fprintf (out_stream, "%s", str)
#define Outln() Outstr ("\n")

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
         --output      output file path\n\
     -a, --oA          output file path to append\n\
     -d, --add-delim   to add extra delimiter(s)\n\
                       Example:  `-d_ -d \"ad\"` means `_` and `a`,...,`d`\n\
     -D                to overwrite the default delimiters\n\
     -o, --format      output format, `-o FLAG1:FLAG2:...`\n\
                           k:  enable keywords\n\
                         s,S:  enable strings and full strings\n\
                           z:  treat strings as normal tokens\n\
                           n:  include numbers\n\
                       Example: '-o Str:key:num'\n\
");
}

int
safe_open (const char *restrict pathname,
           const char *restrict mode,
           FILE **dest)
{
  FILE *f;
  if (!pathname || !mode)
    {
      warnln ("invalud filename");
      return 1;
    }
  if ((f = fopen (pathname, mode)) == NULL)
    {
      warnln ("could not open file -- (%s:%s)", mode, pathname);
      return 1;
    }

  if (*dest)
    fclose (*dest);
  *dest = f;
  return 0;
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
  const char *params = "+o:a:d:Dvh";
  while (1)
    {
      c = getopt_long (argc, argv, params, long_options, NULL);
      if (c == -1)
        return 0;

      switch (c)
        {
        case 'v':
          version_etc (stdout, program_name, Version);
          return 0;

        case 'h':
          usage (-1);
          return -1;

        case 'd':
          kflags |= C_EXT_DELIMS;
          da_appd (Extra_Delims, optarg);
          break;

        case 'D':
          kflags |= C_OVERW_DELIMS;
          break;

        case '0':
          if (safe_open (optarg, "r", &in_stream))
            return 1;
          break;

        case '1':
          if (safe_open (optarg, "w", &out_stream))
            return 1;
          break;

        case '2':
          kflags |= C_JAVASCRIPT;
          break;

        case 'a':
          if (safe_open (optarg, "a", &out_stream))
            return 1;
          break;

        case 'o':
          kflags |= O_PROVIDED;
          const char *p = optarg;
          do
            {
              switch (*p)
                {
                case 'k':
                case 'K':
                  kflags |= O_ALLOW_KEY;
                  break;
                case 's':
                  kflags |= O_ALLOW_STR;
                  break;
                case 'S':
                  kflags |= O_FULL_STR | O_ALLOW_STR;
                  break;
                case 'z':
                case 'Z':
                  kflags |= O_DIS_STR;
                  break;
                case 'n':
                case 'N':
                  kflags |= O_ALLOW_NUM;
                  break;
                }
            }
          while ((p = strchr (p, ':')) && *(++p));
          return 0;

        default:
          break;
        }
    }
  return 0;
}

static inline int
cstr_isanumber (const char *s)
{
  if (*s != '0')
    {
      for (; *s != '\0'; ++s)
        {
          if (!isdigit (*s))
            return 0;
        }
      return 1;
    }

  switch (*(++s))
    {
    case 'x': /* Hexadecimal value 0x??? */
      {
        for (++s; *s != '\0'; ++s)
          {
            if (!isxdigit (*s))
              return 0;
          }
      }
      return 1;

    case 'b': /* Binary number 0b??? */
      {
        for (++s; *s != '\0'; ++s)
          {
            if (!isbdigit (*s))
              return 0;
          }
      }
      return 1;

    default: /* 0??? is not a number (like: 0123) */
      return 0;
    }
  return 0;
}

static inline int
token_out (const Milexer_Token *tk)
{
  if (tk->type == TK_EXPRESSION)
    {
      /* string */
      if (HAS_FLG (kflags, O_ALLOW_STR))
        {
          /* Handle javascript regex */
          if (tk->id == JS_REG &&
              ! HAS_FLG (kflags, O_ALLOW_KEY))
            {
              return 0;
            }
          Outstr (tk->cstr);
          return 1;
        }
    }
  else if (tk->type == TK_KEYWORD)
    {
      if (cstr_isanumber (tk->cstr))
        {
          if (HAS_FLG (kflags, O_ALLOW_NUM))
            {
              Outstr (tk->cstr);
              return 1;
            }
        }
      else
        {
          if (HAS_FLG (kflags, O_ALLOW_KEY))
            {
              Outstr (tk->cstr);
              return 1;
            }
        }
    }
  return 0;
}

#define safe_free(malloc_ptr) do {              \
  if (malloc_ptr) {                             \
    free (malloc_ptr);                          \
    malloc_ptr = NULL;                          \
  }} while (0)

void
cleanup (int, void *)
{
  if (out_stream)
    {
      fflush (out_stream);
      /* Do NOT use stdout after this line! */
      fclose (out_stream);
    }
  safe_free (out_buff);
  safe_free (in_buff);
}

int
kinit (void)
{
  /* Initialize ML expressions */
  if (HAS_FLG (kflags, C_JAVASCRIPT))
    ML.expression = MKEXP (Expressions_JS);
  else
    ML.expression = MKEXP (Expressions);

  /* Default output configuration */
  if (! HAS_FLG (kflags, O_PROVIDED))
    {
      kflags = O_ALLOW_KEY | O_ALLOW_STR;
    }

  /* Check empty output */
  if (!HAS_FLG (kflags, O_ALLOW_KEY) &&
      !HAS_FLG (kflags, O_ALLOW_STR) &&
      !HAS_FLG (kflags, O_ALLOW_NUM))
    {
      warnln ("empty output, provide output flag");
      return 1;
    }

  /* Delimiters */
  if (HAS_FLG (kflags, C_OVERW_DELIMS))
    {
      ML.delim_ranges.exp = Extra_Delims;
    }
  else
    {
      for (size_t i=0; i < GEN_LENOF (Delimiters); ++i)
        da_appd (Extra_Delims, Delimiters[i]);
      ML.delim_ranges.exp = Extra_Delims;
    }

  /**
   *  When the disable string flag is set, we must treat strings
   *  as normal tokens, which means, Milexer should not presses
   *  expressions, as they are responsible for parsing strings
   */
  if (HAS_FLG (kflags, O_DIS_STR))
    {
      ML.expression.len = 0;
      ML.expression.exp = NULL;

      /**
       *  Append string all prefixes to Milexer delimiters
       *  so, they will no appear in the output
       */
      for (size_t i=0; i < GEN_LENOF (Expressions); ++i)
        da_appd (Extra_Delims, Expressions[i].begin);
    }

  if (HAS_FLG (kflags, O_FULL_STR))
    {
      parse_flg = PFLAG_DEFAULT;
    }
  else
    {
      /* get contents of strings */
      parse_flg = PFLAG_INEXP;
    }
  return 0;
}

int
main (int argc, char **argv)
{
  int ret;
  set_program_name (*argv);
  on_exit (cleanup, NULL);

  /* extra delimiter (dynamic array) */
  Extra_Delims = da_new (const char *);

  if ((ret = parse_args (argc, argv)))
    return (ret > 0) ? ret : 0;

  if ((ret = kinit ()))
    return ret;

  Milexer_Token tk = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN);
  Milexer_Slice src = {.lazy = true};

  /* Initialize IO buffers */
  {
    out_buff = malloc (_BMAX);
    in_buff = malloc (in_buff_len);
    if (!in_stream)
      in_stream = stdin;
    if (!out_stream)
      out_stream = stdout;
    if (setvbuf (out_stream, out_buff, _IOFBF, _BMAX) < 0)
      {
        warnln ("could not set _IOFBF for output stream");
        safe_free (out_buff);
      }
  }

  if (in_stream == stdin && isatty (fileno (in_stream)))
    {
      warnln ("reading from stdin until EOF");
    }

  /* Update the length of delimiter ranges */
  ML.delim_ranges.len = da_sizeof (Extra_Delims);

  int len;
  for (int ret = 0; !NEXT_SHOULD_END (ret); )
    {
      ret = ml_next (&ML, &src, &tk, parse_flg);
      switch (ret)
        {
        case NEXT_NEED_LOAD:
          len = read (fileno (in_stream),
                      in_buff,
                      in_buff_len);
          if (len > 0)
            SET_ML_SLICE (&src, in_buff, in_buff_len);
          else
            END_ML_SLICE (&src);
          break; 
 
        case NEXT_CHUNK:
          token_out (&tk);
          break;
        case NEXT_MATCH:
        case NEXT_ZTERM:
          if (token_out (&tk))
            Outln ();
          break;

        default:
          break;
        }
    }
  
  TOKEN_FREE (&tk);
  return 0;
}
