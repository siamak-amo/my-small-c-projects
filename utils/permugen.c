/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  Permugen is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Permugen is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: permugen.c
    created on: 4 Sep 2024

    Permugen - Permutation generator utility
    For generating customizable permutations from specified seeds

    Usage
      For more details, use the `-h` option:
      $ permugen --help

    Normal mode:  permugen [OPTIONS] -s [SEED_CONFIG]
      Cartesian product of the input with a certain depth
      {INPUT_SEED}x{INPUT_SEED}x...  (depth times)

     * Alphanumeric characters:
       $ permugen                                  # a-z and 0-9 of depth 3
       $ permugen -s "\d"                          # only digits 0-9
       $ permugen -s "\d \u"                       # 0-9 and A-Z
       $ permugen -s "\d \u \l"                    # 0-9 and A-Z and a-z

     * The set {XY, a,...,f}:
       $ permugen -s "[XY] [a-f]" -d2              # depth=2 (strict)
       $ permugen -s "[XYa-f]" -D4                 # depth range [1 to 4]
       $ permugen -s "[XYa-f]" -d 3-5              # depth range [3 to 5]

     * To include words:
       $ permugen -s "{foo,bar}"
       $ permugen -s "/path/to/wlist.txt ./wlist2.txt"
       $ permugen -s "-"                           # read from stdin

     * Combined Examples:
       $ permugen -s "{foo,bar} [x-z0-3]"          # foo,bar,x,y,z,0,1,2,3
       $ permugen -s "{foo,bar} [x-z0-3] ~/word_list.txt"

     * Output Format:
       - Permutation components separator (-p, --delim)
       $ permugen -p ", "                          # comma separated
       $ permugen -p "\t"                          # tab separated
       $ permugen -p ", "  -p "\t"                 # to get both of them

       - Global suffix and prefix
       $ permugen --pref "www." --suff ".com"


    Regular Mode:  permugen [OPTIONS] -r [SEED_CONFIG]...
      To manually specify components of the permutations
      {INPUT_SEED_1}x{INPUT_SEED_2}x...x{INPUT_SEED_N}

     * Basic Examples:
       - Cartesian product of {0,1,2}x{AA,BB}
       $ permugen -r  "[0-2]"  "{AA,BB}"

       - {dev,prod}x{admin,<wordlist.txt>}
       $ permugen -r  "{dev,prod}"  "{admin} /path/to/wordlist.txt"

       - To reuse previously provided seeds (\N starting from 1)
         {dev,prod}x{www,dev,prod}
       $ permugen -p. -r  "{dev,prod}"  "{www} \1"
         {dev,prod}x{2,3}x{2,3}
       $ permugen -r  "{dev,prod}"  "[2-3]"  "\2"

     * Depth in regular mode (count of output components)
       - To get permutations with exactly 3 components:
       $ permugen -r [INPUT_SEED]... -d3
       - With 2 and 3 components:
       $ permugen -r [INPUT_SEED]... -d 2-3

       - To read from stdin:
       $ permugen -r -- "{dev,prod}"  "-"
         or equivalently, avoid using end-of-options:
       $ permugen -r  "{dev,prod}"  " -"

     * Output formatting:
       Permugen supports format string similar to Python's f-strings
       - Note: using this option in normal mode  or  using manual
         prefix and suffix along with this option, is Undefined.

       - This substitutes the first `{}` in the format option
         with dev,prod  and  the second one with 0..5
       $ permugen -r "{dev,prod}"  "[0-5]" \
                 --format "https://{}.api.com/file_{}.txt"

       - This option, also supports left and right paddings
         like "%4s" and "%-5s" in C
       $ permugen -r "{A, AA, AAA}"  "{BBB, BBBB, B}" \
                 --format "{:4}, {:-5}"

     * Manual output formatting:
       - Using global prefix and suffix:
       $ permugen -s "[0-2]"  --prefix '('  --suffix ')'

       - The first component has `xxx` as a suffix,
         while the second component has `yyy` as a prefix
       $ permugen -r  "() {One} (xxx)"  "(yyy) {Two} ()"

       - The first component uses {} and the second one uses ()
       $ permugen -r  "({) {One} (})"  "(\() {Two} (\))"


   Compilation:
     cc -ggdb -O3 -Wall -Wextra -Werror -I../libs \
        -o permugen permugen.c

   Options:
    - To skip uniqueness of word seeds
       define `_SKIP_UNIQUE`
    - To skip freeing allocated memory before quitting
       define `_CLEANUP_NO_FREE`
    - To enable printing of debug information
       define `_DEBUG`
    - To use buffered IO library (deprecated)
       define `_USE_BIO`
       define `_BMAX="(1024 * 1)"` (=1024 bytes)
 **/
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

#define PROGRAM_NAME "permugen"
#define Version "2.20"

#define CLI_IMPLEMENTATION
#include "clistd.h"

/* Default permutaiton depth (normal mode) */
#define DEF_DEPTH 3

/* Maximum length of a word seed */
#ifndef WSEED_MAXLEN
# define WSEED_MAXLEN 511 // 1 byte for null-byte
#endif

/* Output stream buffer size (default = page_size) */
#ifndef _BMAX
# define _BMAX 4096
#endif

/* Maximum count of words in a seed */
#ifndef WSEED_MAXCNT
/* As our dynamic array grows by a factor of 2,
   it's more efficient for the max count to be a power of 2 */
# define WSEED_MAXCNT 8192
#endif

#ifndef IS_LINE_COMMENT
#define IS_LINE_COMMENT(line) (line[0] == '#')
#endif

/* Using nonlocking stdio functions */
#ifndef fast_fwrite /* equivalent to fwirte */
#define fast_fwrite fwrite_unlocked
#endif
#ifndef fast_putc /* equivalent to putc */
#define fast_putc putc_unlocked
#endif

/* Space padding */
const char *sp_padding = "                                ";
const int sp_padding_len = 32;

/**
 **  The following files are available in `../libs`:
 **
 **   - buffered_io.h:  --DEPRECATED--
 **                     To improve performance by buffering the
 **                     output stream (slightly faster then the default
 **                     setup which uses _IOFBF for the output stream)
 **   - unescape.h:     Handles backslash interpretation
 **   - dyna.h:         Dynamic array implementation
 **   - mini-lexer.h:   Lexer (used for regex parsing)
 **/
#ifdef _USE_BIO
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif /* _USE_BIO */

#define UNESCAPE_IMPLEMENTATION
#include "unescape.h"

#define DYNA_IMPLEMENTATION
#include "dyna.h"

#define ML_FLEX
#define YY_TOKEN_CAP 512  /* lexer token capacity */
#define ML_IMPLEMENTATION
#include "mini-lexer.h"

#undef STR
#define CSTR(var) #var
#define STR(var) CSTR (var)

#undef _Nullable
#define _Nullable
#define NOP() (UNUSED (NULL))

#ifdef _DEBUG /* debug macro */
#  define dprintf(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)

/* To print arrays with seperator & end suffix */
#  define printd_arr__H(arr, fmt, len, sep, end)    \
  for (ssize_t __idx = 0; __idx < len; __idx++) {   \
    dprintf (fmt "%s", arr[__idx],                  \
             (__idx < len-1) ? sep : end);          \
  }

/**
 *  Debug macro to print arrays
 *  Ex. `int arr[7]`:  `printd_arr (arr, "%d", 7);`
 */
#  define printd_arr(arr, fmt, len) do {                \
    if (len > 0 && arr) {                               \
      dprintf (CSTR (arr) "[.%d] = {", (int)(len));     \
      printd_arr__H (arr, fmt, len, ", ", "}\n");       \
    } else {                                            \
      dprintf (CSTR (arr) " is empty\n");               \
    }} while (0)

#else /* _DEBUG */
#  define dprintf(...) NOP()
#  define printd_arr(...) NOP()
#endif /* _DEBUG */

struct char_seed
{
  const char *c;
  int len;
};
const struct char_seed charseed_az = {"abcdefghijklmnopqrstuvwxyz", 26};
const struct char_seed charseed_AZ = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
const struct char_seed charseed_09 = {"0123456789", 10};

/* getopt */
const char *lopt_cstr = "s:S:o:a:p:d:D:f:0:1:2:3:4:5:vhrEe";
const struct option lopts[] =
  {
    /* Seeds */
    {"seed",             required_argument, NULL, 's'},
    {"raw-seed",         required_argument, NULL, '0'},
    {"raw-wseed",        required_argument, NULL, '5'},
    {"seed-path",        required_argument, NULL, 'S'},
    {"wseed-path",       required_argument, NULL, 'S'},
    /* Output file */
    {"output",           required_argument, NULL, 'o'},
    {"append",           required_argument, NULL, 'a'},
    {"oA",               required_argument, NULL, 'a'},
    {"help",             no_argument,       NULL, 'h'},
    /* Delimiter */
    {"delim",            required_argument, NULL, 'p'},
    {"delimiter",        required_argument, NULL, 'p'},
    /* Depth */
    {"depth",            required_argument, NULL, 'd'},
    {"depth-range",      required_argument, NULL, 'D'},
    {"range-depth",      required_argument, NULL, 'D'},
    {"range",            required_argument, NULL, 'D'},
    {"depth-from",       required_argument, NULL, '1'},
    {"from-depth",       required_argument, NULL, '1'},
    {"min-depth",        required_argument, NULL, '1'},
    {"depth-to",         required_argument, NULL, '2'},
    {"to-depth",         required_argument, NULL, '2'},
    {"max-depth",        required_argument, NULL, '2'},
    /* Format */
    {"format",           required_argument, NULL, 'f'},
    {"fmt",              required_argument, NULL, 'f'},
    {"pref",             required_argument, NULL, '3'},
    {"prefix",           required_argument, NULL, '3'},
    {"suff",             required_argument, NULL, '4'},
    {"suffix",           required_argument, NULL, '4'},
    /* Regular mode */
    {"regular",          no_argument,       NULL, 'r'},
    /* CLI */
    {"format-getline",   no_argument,       NULL, '9'},
    {"fgetline",         no_argument,       NULL, '9'},
    {"ignore-comments",  no_argument,       NULL, '8'},
    {"ignore-comment",   no_argument,       NULL, '8'},
    {"no-comment",       no_argument,       NULL, '8'},
    {"version",          no_argument,       NULL, 'v'},
    {"help",             no_argument,       NULL, 'h'},
    /* End of Options */
    {NULL,               0,                 NULL,  0 },
  };

/* Seed container */
struct Seed
{
  /* Character seeds */
  char *cseed;
  int cseed_len;

  /* Word seeds, (Dynamic array) */
  char **wseed;

  char *pref, *suff;
  int padding;
};

/* To make new seed and dynamic seed array */
static inline struct Seed * mk_seed (int c_len, int w_len);
#define mk_seed_arr(n) da_newn (struct Seed *, n)
/* To free the seed @s, allocated by the mk_seed function */
static inline void free_seed (struct Seed *s);

/**
 *  Mini-Lexer language
 *  Introduce argument regex parser
 */
enum LANG
  {
    PUNC_COMMA = 0,

    EXP_CURLY = 0,     /* {xxx} */
    EXP_PAREN,         /* (xxx) */
    EXP_BRACKET,       /* [xxx] */
  };

static struct Milexer_exp_ Puncs[] =
  {
    [PUNC_COMMA]         = {",", .disabled=true},
  };

static struct Milexer_exp_ Expressions[] =
  {
    [EXP_CURLY]          = {"{", "}"},
    [EXP_PAREN]          = {"(", ")"},
    [EXP_BRACKET]        = {"[", "]"},
  };
static struct Milexer_exp_ FormatExpressions[] =
  {
    [EXP_CURLY]          = {"{", "}"},
  };

/* Mini-Lexer seed option language */
static Milexer SeedML =
  {
    .expression = GEN_MLCFG (Expressions),
    .puncs      = GEN_MLCFG (Puncs),
  };
/* Mini-Lexer format option language */
static Milexer FormatML =
  {
    .expression = GEN_MLCFG (FormatExpressions),
  };

enum mode
  {
    NORMAL_MODE = 0,
    REGULAR_MODE,
  };

enum getline_flag_t
  {
    GETLINE_FMT        = 1 << 1,
    GETLINE_NO_COMMENT = 1 << 2,
  };
static inline bool getline_isvalid (const char *buff, int getline_flgs);

/* Permugen's main configuration */
struct Opt
{
  int mode;
  int escape_disabled; /* to disable backslash interpretation */
  int using_default_seed;
  int getline_flags;
  struct
  {
    int min, max;
  } depth;

  /* Seed Configuration (Normal mode) */
  struct Seed *global_seeds;

  /* Seed Configuration (Regular mode) */
  struct Seed **reg_seeds; /* Dynamic array */
  int reg_seeds_len;

  /* Output Configuration */
  FILE *outf;
  char *prefix, *suffix; /* by malloc */
  char **seps; /* component separator(s) (Dynamic array) */
  char *format;

  /* Output stream buffer */
#ifdef _USE_BIO
  BIO_t *bio;
#else
  char *streamout_buff;
#endif
};

/* Internal Macros */
#define MIN(x, y) ((x < y) ? x : y)
#define MAX(x, y) ((x < y) ? y : x)

/* To check @c belongs to the closed interval [a,b] */
#define IN_RANGE(c, a,b) ((c) >= (a) && (c) <= (b))
#define IS_DIGIT(c) IN_RANGE(c, '0', '9')

/**
 *  ASCII-printable character range
 *  Character seeds *MUST* be within this range
 */
#define MIN_ASCII_PR 0x20
#define MAX_ASCII_PR 0x7E
#define IS_CSEED_RANGE(c) (c >= MIN_ASCII_PR && c <= MAX_ASCII_PR)
#define CSEED_MAXLEN (MAX_ASCII_PR - MIN_ASCII_PR + 1)

/* NULL safe strcmp, unescape, free */
#define Strcmp(s1, s2) _Nullable                \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

#define UNESCAPE(cstr) do {                     \
    if (NULL != cstr)                           \
      unescape (cstr);                          \
  } while (0)

#ifdef _CLEANUP_NO_FREE
# define safe_free(...) NOP()
#else
# define safe_free(ptr) if (ptr) {              \
    free (ptr);                                 \
    ptr = NULL;                                 \
  }
#endif /* _CLEANUP_NO_FREE */


static inline void
safe_fclose (FILE *_Nullable file)
{
  if (file)
    {
      fflush (file);
      fclose (file);
    }
}

FILE *
safe_fopen (const char *restrict pathname,
            const char *restrict mode)
{
  FILE *tmp;
  if (!pathname || !mode)
    {
      warnln ("invalud filename");
      return NULL;
    }
  if ((tmp = fopen (pathname, mode)) == NULL)
    {
      warnln ("could not open file %s:%s", mode, pathname);
    }
  return tmp;
}

/**
 *  Frees all allocated memory and closes all open files
 *  This function should be used in `on_exit`
 */
void
cleanup (int code, void *__opt)
{
  UNUSED (code);
  struct Opt *opt = (struct Opt *)__opt;
  if (!opt)
    return;

  /* Free the mini-lexer internals */
  yylex_destroy ();
  /* flush the output stream */
#ifdef _USE_BIO
  bio_flush (opt->bio);
#endif

  safe_fclose (opt->outf);
  /** Do *NOT* use stdio after this line! **/

#ifndef _CLEANUP_NO_FREE

  /* Free output stream buffers */
#ifdef _USE_BIO
  safe_free (opt->bio->buffer);
  safe_free (opt->bio);
#else
  safe_free (opt->streamout_buff);
#endif /* _USE_BIO */

  free_seed (opt->global_seeds);
  safe_free (opt->global_seeds);

  if (opt->reg_seeds)
    {
      da_foreach (opt->reg_seeds, i)
        {
          free_seed (opt->reg_seeds[i]);
          safe_free (opt->reg_seeds[i]);
        }
      da_free (opt->reg_seeds);
    }

  if (opt->seps)
    {
      da_free (opt->seps);
    }

#endif /* _CLEANUP_NO_FREE */
}

/**
 *  Appends characters from @src to @dst->cseed, until \0
 *  or !IS_CSEED_RANGE, returns the number of bytes written
 *  @dst->cseed will have unique chars after this call, if it did before
 *  Pass @len -1 to stop only at the null bytes
 *
 *  Returns the index of the last non-zero byte in @src
 */
int cseed_uniappd (struct Seed *dst, const char *src, int len);

/**
 *  Appends a copy of @str_word (using strdup malloc)
 *  to @dst->wseed after unescaping it (if not disabled)
 *  @dst->wseed will have unique words after this call, if it did before
 *  Uniqueness checking can be disabled via `_SKIP_UNIQUE` macro
 * Return:
 *   -1 on error, 1 on match found, 0 on a successful appending
 */
int wseed_uniappd (const struct Opt *opt,
                   struct Seed *dst, const char *str_word);
/**
 *  Using wseed_uniappd function, appends words from @stream
 *  to the seed @dst, line by line, and ignores commented lines by `#`
 */
void wseed_file_uniappd (const struct Opt *opt,
                         struct Seed *dst, FILE *stream);

/**
 *  Parses the @input regex and stores the result into @dst
 *  @input: "(prefix) [Cseed] {Wseed} /path/to/file (suffix)"
 */
void parse_seed_regex (struct Opt *opt,
                       struct Seed *dst, char *input);

/**
 *  Word Seed regex parser
 *  @input: `{...}`  -  comma-separated words
 *  Using wseed_uniappd, appends the result to @dst (with unescape)
 */
static inline void
pparse_wseed_regex (struct Opt *opt,
                    struct Seed *dst, char *input);

/**
 *  Character Seed regex parser
 *  @input: `[...]`  -  minimal character range(s) (with unescape)
 */
static inline void
pparse_cseed_regex (struct Opt *opt,
                    struct Seed *dst, char *input);

/**
 *  Detects file paths, previous seed indexes (\N)
 *  and other shortcuts like: '\d', '\u' and '\1'
 */
static inline void
pparse_keys_regex (struct Opt *opt,
                   struct Seed *dst, const char *input);

/**
 *  Initializes @dst->prefix/suffix (if NULL), via malloc
 *  @input: `(...)` (only in regular mode)
 *  First call provides prefix, second call suffix,
 *  other calls will be ignored
 */
static inline void
pparse_format_regex (struct Opt *opt,
                     struct Seed *dst, char *input);

/**
 *  This function sets appropriate prefix/suffix
 *  and adjustment value of all seeds, based on
 *  @input: `xxx{}yyy` or like rust/python format string
 *        `xxx{:<5}yyy`  and  `xxx{:-5}yyy`
 */
static void
pparse_format_option (const struct Opt *opt,
                      struct Seed **dst, int dst_len, char *input);

struct print_info
{
  int adjust; /* left (positive) and right (negative) padding */
  char newline; /* zero means no newline */
};

/** __fputs(), __fputc()
 *  fputs,fputc similar functions
 *  With newline and left/right adjustment support
 *  Only use Fprints and Fprintc macros
 *
 *  This function ensures @str has length of ABS(@adjust)
 *  for @adjust < 0 puts enough space before, and
 *  for @adjust > 0 puts enough space after @str
 */
static inline void
__fputs (const char *str, struct Opt *, const struct print_info *info);
static inline void
__fputc (char c, struct Opt *, const struct print_info *info);


void
usage (int ecode)
{
  fprintf (stdout,"\
%s %s, permutation generator utility\n\n\
Usage:\n\
  normal mode: any possible permutation of given seed(s)\n\
      permugen [OPTIONS] -s [SEED_CONF]\n\n\
  regular mode: to specify seed(s) of each component manually\n\
  generated permutations will have exactly N components\n\
      permugen -r (SEED_CONF)... [OPTIONS]\n\
      permugen [OPTIONS] -r -- (SEED_CONF)...\n\
\n\
OPTIONS:\n\
  Common options:\n\
      -E                      disable backslash interpretation\n\
      -e                      enable backslash interpretation (default)\n\
      -r, --regular           regular mode\n\
      -o, --output            output file\n\
      -a, --oA --append       append to file\n\
      -p, --delimiter         permutations component separator\n\
          --prefix            global output prefix\n\
          --suffix            global output suffix\n\
      -f, --format            output format\n\
          --no-comment        ignore commented lines  while reading from file\n\
          --format-getline    separate words by space while reading from file\n\
      -h, --help              print help and exit\n\
      -v, --version           print version and exit\n\
\n\
  Depth settings:\n\
      -d, --depth             specify depth\n\
                              strict: '-d N' where N is a number\n\
                              range: '-d N-M' for range [N to M]\n\
      -D, --depth-range       depth range [1 to N]\n\
          --min-depth         minimum depth\n\
          --max-depth         maximum depth\n\
\n\
  Only in normal mode:\n\
      -S, --seed-path         word seed path\n\
                              pass - to read from stdin\n\
      -s, --seed              to configure global seeds (see ARGUMENTS)\n\
          --raw-seed          to configure character seeds\n\
          --raw-wseed         to add a single word to global seeds\n\
\n\
ARGUMENTS:\n\
  All argument values will be backslash-interpreted by default\n\
  disable this feature with `-E'\n\
\n\
  Seed: argument value of `-s, --seed' and `-r, --regular'\n\
        accepts any combination of the following patterns\n\
    '{word1,word2}'   to include `word1' and `word2'\n\
    '[XYZ]':          to include characters X,Y,Z\n\
    '[a-f]':          to include character range a,...,f\n\
    '\\N':             to reuse (append) previous seeds, only in regular mode\n\
                      where N is the index of a prior given seed, starting from 1\n\
    character range shortcuts:\n\
      '\\d' for [0-9],   '\\l','\\a' for [a-z],   '\\u','\\U','\\A' for [A-Z]\n\
    inside these regex's, you might also use:\n\
      '\\{ and \\['       for '{', '}' and '[', ']' characters\n\
      '\\, or \\x2c'      for comma, alternatively use --raw-xxx in normal mode\n\
      '\\xNN or \\0HHH'   hex and octal byte, for example: \\x5c for backslash\n\
                        see the raw section for more details\n\
    '-':              to read word seeds from the stdin up until Ctrl-D\n\
                      equivalently, an empty line and then the word `EOF'\n\
    `/path/to/file':  to read words from a file (line by line)\n\
                      lines with '#' will be ignored\n\
    `(pref) (suff)':  (in regular mode) to add custom prefix and suffix\n\
                      for parenthesis, use: \\( and \\)  or  \\x28 and \\x29\n\
                      the suffix will overwrite the separator if provided\n\
\n\
    Examples:\n\
      to include a,b and 0,...,9 and also words `foo' and `bar':\n\
       '[ab0-9] {foo,bar}'  or equivalently  '[ab] {foo,bar} [0-9]'\n\
      to also include words from wordlist.txt:\n\
       '[ab0-9] {foo,bar} /path/to/wordlist.txt'\n\
      to also read from stdin:\n\
       '- [ab0-9] {foo,bar} ~/wordlist.txt'\n\
\n\
  Output formating (-f, --format): \n\
    similar to Python's f-strings:  -f \"Name: {name} - Key={value} ...\"\n\
       it substitutes `{}` with the appropriate permutation component\n\
    also supports left/right paddings:\n\
       `{:7}`, `{:>7}`:  means left padding up to 7 characters\n\
      `{:-7}`, `{:<7}`:  means right padding\n\
\n\
  Raw: backslash interpretation usage\n\
       \\\\:  to pass a single '\\'\n\
            some shells might eliminate them, so it would be more convenient\n\
            to use this inside single quotes instead of double quotes\n\
       \\x:  for \\t, \\v, \\r, \\a, \\b, \\f, \\n \n\
     \\xHH:  byte with hexadecimal value HH (1 to 2 digits)\n\
    \\0NNN:  byte with octal value NNN (1 to 3 digits)\n\
"
           , program_name, Version);

  if (ecode >= 0)
    exit (ecode);
}

#ifndef _USE_BIO
# define Fwrite(str, n, opt) fast_fwrite (str, n, 1, opt->outf)
# define Putc(c, opt) fast_putc (c, opt->outf)
#else
# define Fwrite(str,  n, opt) bio_put (opt->bio, str, n)
# define Putc(c, opt) bio_putc (opt->bio, c)
#endif /* _USE_BIO */

/**
 *  Output of characters and strings macros
 *
 *  Fputs:  writes @str without its terminating null byte
 *  FPutc:  writes a character @c (as unsigned char)
 *  Puts:   writes @str like @Fputs and a trailing newline
 *  Putln:  writes a newline
 *
 *  Fprints and Fprintc: fput with left/right adjustment
 *  similar to "%5s" and "%-5c", but dynamic (use 5 or -5 in @n)
 */
#define Fputs(str, opt) Fwrite (str, strlen (str), opt)
#define Fputc(c, opt) Putc (c, opt)
#define Putln(opt) Fputc ('\n', opt)
#define Puts(str, opt) do { Fputs (str, opt); Putln (opt); } while (0)
#define Fprints(str, n, opt) \
  __fputs (str, opt, &(struct print_info){ .adjust=n, .newline=0 })
#define Fprintc(c, n, opt) \
  __fputc (c, opt, &(struct print_info){ .adjust=n })

/* Internal print padding macros */
#define __fput_handle_padding(len, adjust, action, opt) do {    \
    if (adjust + len < 0) /* left padding */                    \
      fwrite_space_pad (-adjust-len, opt);                      \
    action;                                                     \
    if (adjust - len > 0) /* right padding */                   \
      fwrite_space_pad (adjust-len, opt);                       \
  } while (0)

#define fwrite_space_pad(count, opt)                    \
  for (int n=count, to_fill=0; n > 0; n -= to_fill) {   \
    to_fill = MIN (sp_padding_len, n);                  \
    Fwrite (sp_padding, to_fill, opt);                  \
  }

static inline void
__fputs (const char *str, struct Opt *opt,
         const struct print_info *info)
{
  /**
   *  Unfortunately, we don't have access to _IO_sputn
   *  form glibc, but this should be fast as the real fputs
   */
  int len = strlen (str);
  __fput_handle_padding (len, info->adjust, {
      Fwrite (str, len, opt);
      if (info->newline)
        Putc (info->newline, opt);
    }, opt);
}

static inline void
__fputc (char c, struct Opt *opt, const struct print_info *info)
{
  __fput_handle_padding (1, info->adjust, {
      Putc (c, opt);
    }, opt);
}

/**
 *  The main logic of normal mode
 *  It should be called in a loop with @depth in [min_depth, max_depth]
 *  @idxs: a temporary buffer with capacity of @depth
 */
int
__perm (const struct Opt *opt, const char *sep,
        int *idxs, int depth)
{
  int _max_depth = opt->global_seeds->cseed_len - 1 +
    (int) da_sizeof (opt->global_seeds->wseed);
  memset (idxs, 0, depth * sizeof (int));

  int i;
 Perm_Loop: /* O(seeds^depth) */
  i = 0;
  if (opt->prefix)
    Fputs (opt->prefix, opt);
  if (opt->global_seeds->pref)
    Fputs (opt->global_seeds->pref, opt);

  struct Seed *s = opt->global_seeds;
 Print_Loop: /* O(depth) */
  {
    int idx = idxs[i];
    if (idx < opt->global_seeds->cseed_len)
      {
        /* range of character seeds */
        Putc (s->cseed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= opt->global_seeds->cseed_len;
        Fputs (s->wseed[idx], opt);
      }
    i++;
  }
  if (i < depth)
    {
      if (sep)
        Fputs (sep, opt);
      goto Print_Loop;
    }

  /* End of printing of the current permutation */
  if (opt->global_seeds->suff)
    Fputs (opt->global_seeds->suff, opt);
  if (opt->suffix)
    Puts (opt->suffix, opt);
  else
    Putln (opt);


  int pos;
  for (pos = depth - 1;
       pos >= 0 && idxs[pos] == _max_depth;
       pos--)
    {
      idxs[pos] = 0;
    }

  if (pos < 0) /* End of Permutations */
    {
#ifdef _USE_BIO
      if (bio_err (opt->bio))
        {
          /* buffered_io write error */
          return bio_errno (opt->bio);
        }
      else
        return 0;
#else
      return 0;
#endif /* _USE_BIO */
    }

  idxs[pos]++;
  goto Perm_Loop;
}

int
perm (const struct Opt *opt)
{
  ssize_t seps_len = da_sizeof (opt->seps);
  int *tmp = (int *) calloc (opt->depth.max, sizeof (int));

  for (int dep = opt->depth.min, ret = 0;
       dep <= opt->depth.max; ++dep)
    {
      for (ssize_t i=0; i < seps_len; ++i)
        {
          if (0 != (ret = __perm (opt, opt->seps[i], tmp, dep)))
            return ret;
        }
    }
  safe_free (tmp);
  return 0;
}

/**
 *  The main logic of regular mode
 *  It should be called by the `regular_perm` function
 *
 *  @size and @offset are used for handling depth
 *  @lens and @idxs both have size @size
 *  @idxs: temporary buffer for internal use of this function
 *  @lens: total length of ea                ch regular seed:
 *         lens[i] = len(s[i]->cssed) + len(s[i]->wseed)
 */
int
__regular_perm (struct Opt *opt,
                const int *lens, int *idxs,
                int size, int offset,
                const char *sep)
{
  int ret;
  /* permutation indexes */
  memset (idxs, 0, size * sizeof (int));

  /* Offset of @depths also must apply to seeds */
  lens += offset;
  struct Seed **s = opt->reg_seeds + offset;

  /**
   *  O(S_m * ... * S_n)
   *  where: S_i = @lens[i], for i in [m to n], and
   *         m = @offset  and  n = @size + @offset
   */
  int i;
 Reg_Perm_Loop:
  i = 0;
  struct Seed *current_seed;
  if (opt->prefix)
    Fputs (opt->prefix, opt);

 Print_Loop: /* O(S_i) */
  {
    int idx = idxs[i];
    current_seed = s[i];
    if (current_seed->pref)
      Fputs (current_seed->pref, opt);
    if (idx < current_seed->cseed_len)
      {
        /* Range of character seeds */
        Fprintc (current_seed->cseed[idx], current_seed->padding, opt);
      }
    else
      {
        /* Range of word seeds */
        idx -= current_seed->cseed_len;
        Fprints (current_seed->wseed[idx], current_seed->padding, opt);
      }
    i++;
  }
  if (current_seed->suff)
    Fputs (current_seed->suff, opt);
  if (i < size)
    {
      if (sep)
        {
          /**
           *  Print the separator only when the current seed
           *  has no suffix (suffix must overwrite the separator)
           */
          if (!current_seed->suff || *current_seed->suff == '\0')
            Fputs (sep, opt);
        }
      goto Print_Loop;
    }
  /* End of Printing of the current permutation */
  if (opt->suffix)
    Puts (opt->suffix, opt);
  else
    Putln (opt);

  int pos;
  for (pos = size - 1;
       pos >= 0 && idxs[pos] == lens[pos];
       --pos)
    {
      idxs[pos] = 0;
    }

  if (pos < 0) /* End of Permutations */
    {
#ifdef _USE_BIO
      if (bio_err (opt->bio))
        {
          /* buffered_io write error */
          ret = bio_errno (opt->bio);
          goto Reg_Return;
        }
      else
        {
          ret = 0;
          goto Reg_Return;
        }
#else
      ret = 0;
      goto Reg_Return;
#endif /* _USE_BIO */
    }

  idxs[pos]++;
  goto Reg_Perm_Loop;

 Reg_Return:
  return ret;
}

static int
regular_perm (struct Opt *opt)
{
  int ret = 0;
  int seeds_len = (int) da_sizeof (opt->reg_seeds);
  int idxs_len_bytes = seeds_len * sizeof (int);
  int *tmp = malloc (idxs_len_bytes),
    *lengths = malloc (idxs_len_bytes);

  /* Initialize @lengths by length of each seed array */
  struct Seed *s = NULL;
  for (int i=0; i < seeds_len &&
         NULL != (s = opt->reg_seeds[i]); ++i)
    {
      int total = s->cseed_len + da_sizeof (s->wseed) - 1;
      if (total < 0)
        goto _return; /* Unreachable */
      lengths[i] = total;
    }

  /* Handle the output depth */
  int start = opt->depth.min;
  int end = opt->depth.max;
  ssize_t seps_len = da_sizeof (opt->seps);

  for (int window = start; window <= end; ++window)
    {
      if (0 == window)
        continue;
      for (int offset = 0; offset + window <= seeds_len; ++offset)
        {
          for (ssize_t i=0; i < seps_len; ++i)
            {
              ret = __regular_perm (opt,
                                    lengths, tmp,
                                    window, offset,
                                    opt->seps[i]);
              if (0 != ret)
                break;
            }
        }
    }

 _return:
  safe_free (tmp);
  safe_free (lengths);
  return ret;
}

int
cseed_uniappd (struct Seed *s, const char *src, int len)
{
  int rw = 0;
  while (0 != len && *src)
    {
      if (*src == '\0')
        break;
      if (!IS_CSEED_RANGE (*src))
        goto END_OF_LOOP;

      for (int idx = s->cseed_len - 1; idx >= 0; idx--)
        {
          if (*src == s->cseed[idx])
            goto END_OF_LOOP;
        }
      s->cseed[s->cseed_len] = *src;
      s->cseed_len++;

    END_OF_LOOP:
      src++;
      len--;
      rw++;
    }

  return (rw > 0) ? rw - 1 : 0;
}

int
wseed_uniappd (const struct Opt *opt,
               struct Seed *s, const char *str_word)
{
  if (!s->wseed || !str_word)
    return -1;

  char *word = strdup (str_word);
  if (!opt->escape_disabled)
    UNESCAPE (word);

  size_t len = da_sizeof (s->wseed);
  if (len >= WSEED_MAXCNT)
    return -1;
#ifndef _SKIP_UNIQUE
  for (size_t i=0; i < len; ++i)
    {
      if (Strcmp (s->wseed[i], word))
        {
          safe_free (word);
          return 1;
        }
    }
#endif /* _SKIP_UNIQUE */
  da_appd (s->wseed, word);
  return 0;
}

static inline bool
getline_isvalid (const char *buff, int getline_flgs)
{
  if (buff[0] < MIN_ASCII_PR)
    return false;
  if (getline_flgs & GETLINE_NO_COMMENT)
    {
      if (IS_LINE_COMMENT (buff))
        return false;
    }
  return true;
}

/**
 *  similar to fnprint, with format="%s"
 *  reads in at most @len-1 characters from stream
 */
int
fnscans (char *buff, int len, FILE *stream)
{
  int idx = 0;
  for (int c=0; idx < len-1; ++idx)
    {
      c = getc (stream);
      if (EOF == c)
        {
          if (0 == idx)
            return EOF;
          else
            break;
        }
      if (c <= ' ')
        break;
      buff[idx] = c;
    }
  buff[idx] = '\0';
  return 1;
}

/**
 *  Reads in at most @len-1 characters from stream
 * Return:
 *  on success:  0
 *  if the end of input is reached:  EOF
 *  when line is commented/invalid:  positive value
 */
static inline int
wseed_getline (char *buff, int len, FILE *stream, int flags)
  
{
  if (feof (stream))
    return EOF;

  if (HAS_FLAG (flags, GETLINE_FMT))
    {
      if (fnscans (buff, len, stream) < 0)
        return EOF;
    }
  else /* simple getline */
    {
      if (! fgets (buff, len, stream))
        return 1;
      char *p = strpbrk (buff, "\r\n");
      if (! p)
        return 1; /* invalid */
      *p = '\0';
    }
  if (! getline_isvalid (buff, flags))
    return 1;
  return 0;
}

void
wseed_file_uniappd (const struct Opt *opt,
                    struct Seed *dst, FILE *stream)
{
  if (!stream || feof (stream))
    {
      if (stream == stdin)
        {
          if (freopen ("/dev/tty", "r", stdin) == NULL)
            {
              warnln ("could not open stdin -- %s", strerror (errno));
              return;
            }
        }
      else
        {
          warnln ("could not read from file -- %s", strerror (errno));
          return;
        }
    }

  if (stream == stdin && isatty (fileno (stream)))
    {
      switch (opt->mode)
        {
        case REGULAR_MODE:
          fprintf (stderr,
                   "Reading words for the seed #%d until EOF:\n",
                   opt->reg_seeds_len + 1);
          break;

        case NORMAL_MODE:
          fprintf (stderr, "Reading words until EOF:\n");
          break;
        }
    }

  const int line_cap = WSEED_MAXLEN + 1;
  char *line = malloc (line_cap);
  while (1)
    {
      int ret = wseed_getline (line, line_cap,
                               stream, opt->getline_flags);
      if (ret < 0)
        break;
      if (ret > 0)
        continue;
      if (wseed_uniappd (opt, dst, line) < 0)
        break;
    }
  safe_free (line);
}

static int
opt_getopt (int argc, char **argv, struct Opt *opt)
{
  int idx = 0;

#define NOT_IN_REGULAR_MODE()                                           \
  if (opt->mode == REGULAR_MODE) {                                      \
    warnln ("wrong regular mode option (%s) was ignored",               \
            LASTOPT (argv));                                            \
    break;                                                              \
  }

  while (1)
    {
      int flag = getopt_long (argc, argv, lopt_cstr, lopts, &idx);
      if (flag == -1)
        {
          /* End of Options */
          break;
        }

      switch (flag)
        {
        case 'h':
          usage (EXIT_SUCCESS);
          return 1;
        case 'v':
          fprintf (stderr, "%s: v%s\n", program_name, Version);
          return 1;

          /* Common options  */
        case 'E':
          opt->escape_disabled = 1;
          break;
        case 'e':
          opt->escape_disabled = 0;
          break;
        case 'o': /* outout */
          opt->outf = safe_fopen (optarg, "w");
          break;
        case 'a': /* append */
          opt->outf = safe_fopen (optarg, "a");
          break;
        case '3': /* prefix */
          opt->prefix = optarg;
          break;
        case '4': /* suffix */
          opt->suffix = optarg;
          break;

        case 'p': /* separator(s) */
          if (NULL == opt->seps[0])
            opt->seps[0] = optarg;
          else
            da_appd (opt->seps, optarg);
          break;

        case 'd': /* depth interval */
          {
            char *p = strchr (optarg, '-');
            if (NULL == p)
              {
                opt->depth.min = atoi (optarg);
              }
            else
              {
                *p = '\0';
                opt->depth.min = atoi (optarg);
                opt->depth.max = atoi (p+1);
              }
            break;
          }
        case 'D': /* depth 1...N */
          opt->depth.min = 1;
          opt->depth.max = atoi (optarg);
          break;
        case '1': /* min depth */
          opt->depth.min = atoi (optarg);
          break;
        case '2': /* max depth */
          opt->depth.max = atoi (optarg);
          break;
        case 'f':
          opt->format = optarg;
          break;

          /* Only in normal mode */
        case 'S': /* wseed from file or stdin */
          NOT_IN_REGULAR_MODE ()
          {
            FILE *wseed_f = stdin;
            if (!Strcmp (optarg, "-"))
              wseed_f = safe_fopen (optarg, "r");

            wseed_file_uniappd (opt, opt->global_seeds, wseed_f);
            if (wseed_f != stdin)
              safe_fclose (wseed_f);
          }
          break;

        case 's': /* seed configuration */
          NOT_IN_REGULAR_MODE ()
          {
            opt->using_default_seed = 0;
            parse_seed_regex (opt, opt->global_seeds, optarg);
          }
          break;

        case '0': /* raw seed */
          NOT_IN_REGULAR_MODE ()
          {
            opt->using_default_seed = 0;
            if (!opt->escape_disabled)
              UNESCAPE (optarg);
            cseed_uniappd (opt->global_seeds, optarg, strlen (optarg));
          }
          break;

        case '5': /* raw word seed */
          NOT_IN_REGULAR_MODE ();
          wseed_uniappd (opt, opt->global_seeds, optarg);
          break;

          /* Regular mode enable */
        case 'r':
          NOT_IN_REGULAR_MODE ();
          {
            int end_of_options = 0;
            opt->using_default_seed = 0;
            opt->mode = REGULAR_MODE;
            if (opt->reg_seeds)
              break;
            opt->reg_seeds = mk_seed_arr (1);

            for (int i=optind; i < argc; ++i)
              {
                if (*argv[i] == '-' && !end_of_options)
                  {
                    if (argv[i][1] == '-' && argv[i][2] == '\0')
                      {
                        optind++;
                        end_of_options = 1;
                      }
                    else
                      {
                        /* End of `-r` arguments */
                        break;
                      }
                  }
                else
                  {
                    optind++;
                    struct Seed *tmp = mk_seed (CSEED_MAXLEN, 1);
                    parse_seed_regex (opt, tmp, argv[i]);
                    if (tmp->cseed_len == 0 && da_sizeof (tmp->wseed) == 0)
                      {
                        warnln ("empty regular seed configuration was ignored");
                        free_seed (tmp);
                      }
                    else
                      {
                        opt->reg_seeds_len++;
                        da_appd (opt->reg_seeds, tmp);
                      }
                  }
              }
          }
          break;

        case '9':
          opt->getline_flags |= GETLINE_FMT;
          break;
        case '8':
          opt->getline_flags |= GETLINE_NO_COMMENT;
          break;

        default:
          break;
        }
    }
  return 0;
}

/**
 *  opt_init: initializes the default values of @opt
 *  if they have not been set yet.
 */
static void
opt_init (struct Opt *opt)
{
  int max_depth = 0;
  struct Seed **current_seed = NULL;
  switch (opt->mode)
    {
    case REGULAR_MODE:
      current_seed = opt->reg_seeds;
      max_depth = da_sizeof (current_seed);

      if (opt->depth.min == 0 && opt->depth.max == 0)
        {
          opt->depth.min = max_depth;
          opt->depth.max = max_depth;
          break;
        }

      /* Fix up invalid ranges */
      opt->depth.min = MAX (opt->depth.min, 1);
      opt->depth.min = MIN (opt->depth.min, max_depth);
      opt->depth.max = MAX (opt->depth.max, opt->depth.min);
      opt->depth.max = MIN (opt->depth.max, max_depth);
      break;

    case NORMAL_MODE:
      max_depth = 1;
      current_seed = &opt->global_seeds;
      if (opt->global_seeds->cseed_len == 0 && opt->using_default_seed)
        {
          /* Default global_seeds */
          cseed_uniappd (opt->global_seeds,
                         charseed_az.c, charseed_az.len);
          cseed_uniappd (opt->global_seeds,
                         charseed_09.c, charseed_09.len);
        }

      if (opt->depth.min <= 0 && opt->depth.max <= 0)
        {
          opt->depth.min = DEF_DEPTH;
          opt->depth.max = DEF_DEPTH;
        }
      else if (opt->depth.max <= 0)
        {
          opt->depth.max = opt->depth.min;
        }
      if (opt->depth.min > opt->depth.max)
        {
          /* Invalid min and max depths, set them equal */
          opt->depth.max = opt->depth.min;
        }
      break;
    }

  /* Interpret backslash characters */
  if (!opt->escape_disabled)
    {
      UNESCAPE (opt->prefix);
      UNESCAPE (opt->suffix);
      da_foreach (opt->seps, i)
        {
          UNESCAPE (opt->seps[i]);
        }
    }

  /* Output format */
  if (opt->format)
    pparse_format_option (opt, current_seed, max_depth, opt->format);

  /* Initializing the output stream buffer */
#ifndef _USE_BIO
  opt->streamout_buff = malloc (_BMAX);
  if (!!setvbuf (opt->outf, opt->streamout_buff, _IOFBF, _BMAX))
    warnln ("failed to set buffer for stdout!");
#else
  opt->bio = malloc (sizeof (BIO_t));
  *opt->bio = bio_new (_BMAX, malloc (_BMAX), fileno (opt->outf));
  dprintf ("* buffer length of buffered_io: %d bytes\n", _BMAX);
# endif /* _USE_BIO */
}

static inline void
free_seed (struct Seed *s)
{
  if (!s)
    return;
  safe_free (s->pref);
  safe_free (s->suff);
  safe_free (s->cseed);
  if (s->wseed)
    {
      da_foreach (s->wseed, i)
        {
          safe_free (s->wseed[i]);
        }
      da_free (s->wseed);
    }
}

static inline struct Seed *
mk_seed (int c_len, int w_len)
{
  struct Seed *s = malloc (sizeof (struct Seed));
  if (!s)
    return NULL;
  memset (s, 0, sizeof (struct Seed));
  s->cseed = malloc (c_len);
  s->wseed = da_newn (char *, w_len);
  return s;
}

static struct Opt *
mk_opt (void)
{
  static struct Opt opt = {0};

  opt.global_seeds = mk_seed (CSEED_MAXLEN, 1);
  opt.using_default_seed = 1;

  opt.seps = da_newn (char *, 1);
  da_appd (opt.seps, NULL);

  opt.outf = stdout;
  opt.mode = NORMAL_MODE;
  opt.escape_disabled = false;

  return &opt;
}

int
main (int argc, char **argv)
{
  struct Opt *opt;
  set_program_name (*argv);

  opt = mk_opt ();
  on_exit (cleanup, opt);

  /* Read CLI options & set the defaults */
  {
    int ret = opt_getopt (argc, argv, opt);
    if (ret)
      return EXIT_FAILURE;
    opt_init (opt);
  }

  switch (opt->mode)
    {
    case REGULAR_MODE:
      if (0 == opt->reg_seeds_len)
        {
          warnln ("empty regular permutation");
          return EXIT_FAILURE;
        }
      break;

    case NORMAL_MODE:
      if (0 == opt->global_seeds->cseed_len &&
          0 == da_sizeof (opt->global_seeds->wseed))
        {
          warnln ("empty permutation");
          return EXIT_FAILURE;
        }
      break;
    }

#ifdef _DEBUG
  switch (opt->mode)
    {
    case REGULAR_MODE:
      {
        size_t len = da_sizeof (opt->reg_seeds);
        dprintf ("* regular mode\n");
        dprintf ("* %s[.%lu] = {\n", CSTR (opt->reg_seeds), (size_t)len);
        for (size_t i=0; i < len; ++i)
          {
            struct Seed *s = opt->reg_seeds[i];
            dprintf ("    [%lu] = {\n      ", i);
            printd_arr (s->cseed, "`%c`", s->cseed_len);
            dprintf ("      ");
            printd_arr (s->wseed, "`%s`", (int)da_sizeof (s->wseed));
            if (s->pref)
              dprintf ("      prefix = \"%s\"\n", s->pref);
            if (s->suff)
              dprintf ("      suffix = \"%s\"\n", s->suff);
            dprintf ("    }%s", (i+1 < len) ? ",\n" : "\n");
          }
        dprintf ("  }\n");
      }
      break;

    case NORMAL_MODE:
      {
        dprintf ("* normal mode\n");
        dprintf ("    ");
        printd_arr (opt->global_seeds->cseed, "`%c`",
                    opt->global_seeds->cseed_len);
        dprintf ("    ");
        printd_arr (opt->global_seeds->wseed, "`%s`",
                    da_sizeof (opt->global_seeds->wseed));
        if (opt->depth.min != opt->depth.max)
          dprintf ("* depth range: [%d to %d]\n",
                   opt->depth.min, opt->depth.max);
        else
          dprintf ("* depth: %d\n", opt->depth.min);
      }
      break;
    }
  if (opt->escape_disabled)
    dprintf ("- backslash interpretation is disabled\n");
  if (opt->seps[0])
    {
      dprintf ("* delimiters: ");
      printd_arr (opt->seps, "`%s`", da_sizeof (opt->seps));
    }
  dprintf ("* permutations:\n");
#endif /* _DEBUG */


  /* Generating permutations */
  switch (opt->mode)
    {
    case REGULAR_MODE:
      regular_perm (opt);
      break;

    case NORMAL_MODE:
      perm (opt);
      break;
    }

  return 0;
}

/**
 *  Path resolver
 *  Supported formats: '~/' , '../' , '/./'
 */
static char *
path_resolution (const char *path_cstr)
{
  static char PATH[PATH_MAX];
  char *tmp;
  size_t len = strlen (path_cstr);

  if (*path_cstr == '~')
    {
      const char *home = getenv ("HOME");
      if (! home)
        {
          warnln ("$HOME is (null), could not use '~'");
          return NULL;
        }
      ++path_cstr; /* ignore ~ character */
      tmp = malloc (len + strlen (home));
      strcat (tmp, home);
      strcat (tmp, path_cstr);
    }
  else
    {
      tmp = strdup (path_cstr);
    }

  UNESCAPE (tmp); /* \<space> */
  errno = 0;
  if (NULL == realpath (tmp, PATH))
    {
      warnln ("path resolution failed -- %s (%s)",
              strerror (errno), tmp);
      return NULL;
    }
  safe_free (tmp);
  return PATH;
}


/**
 **  Regex parser functions
 **/
static inline void
pparse_cseed_regex (struct Opt *opt,
                    struct Seed *dst, char *input)
{
  if (! opt->escape_disabled)
    UNESCAPE (input);

  char lastc = 0;
  for (; '\0' != *input; ++input)
    {
      if ('-' != *input || 0 == lastc)
        {
          lastc = *input;
          cseed_uniappd (dst, input, 1);
          continue;
        }
      if ('\0' == *(input++)) /* skipping `-` */
        {
          cseed_uniappd (dst, "-", 1);
          break;
        }

      int range_len = *input - lastc + 1;
      const char *range = NULL; /* NULL means custom range */

#define MK_RANGE(charseed) do {                             \
        if (! IN_RANGE (*input, (charseed).c[0],            \
                       (charseed).c[(charseed).len - 1]))   \
          range_len = 1;                                    \
        range = charseed.c + (lastc - charseed.c[0]);       \
      } while (0)

           if (IN_RANGE (lastc, 'a', 'z'))
        MK_RANGE (charseed_az);
      else if (IN_RANGE (lastc, 'A', 'Z'))
        MK_RANGE (charseed_AZ);
      else if (IN_RANGE (lastc, '0', '9'))
        MK_RANGE (charseed_09);

      if (range)
        cseed_uniappd (dst, range, range_len);
      else
        {
          for (int i=0; i < range_len; ++i, ++lastc)
            cseed_uniappd (dst, &lastc, 1);
        }
      lastc = 0;
    }
#undef MK_RANGE
}

static inline void
pparse_wseed_regex (struct Opt *opt,
                    struct Seed *dst, char *input)
{
  int type;
  YY_BUFFER_STATE *buffer = yy_scan_string( input );
  /* We only need to parse comma in this case: {xx,yy,...} */
  ML_ENABLE (&Puncs[PUNC_COMMA]);
  {
    while ( (type = yylex()) != -1 )
      {
        if (TK_KEYWORD == type)
          {
            wseed_uniappd (opt, dst, yytext);
          }
      }
  }
  ML_DISABLE (&Puncs[PUNC_COMMA]); /* Undo PUNC_COMMA enabled */
  yy_delete_buffer( buffer );
}

static inline void
pparse_format_padding (struct Seed *dst, char *input)
{
  int padd = 0;
  if ((input = strchr (input, ':')))
    {
      switch (*(++input))
        {
        case '-': /* right padding */
        case '<':
          padd = atoi (input + 1);
          break;
        case '+': /* left padding */
        case '>':
          padd = (-1) * atoi (input + 1);
          break;
        default:
          padd = (-1) * atoi (input);
        }
      if (padd)
        dst->padding = padd;
    }
}

static void
pparse_format_option (const struct Opt *opt,
                      struct Seed **dst, int dst_len, char *input)
{
  yyml = &FormatML;
  YY_BUFFER_STATE *buffer = yy_scan_buffer( input, strlen (input) );

  char *prev_curly = input;
  for (int i=0, type=0; i<dst_len && type != -1; type = yylex())
    {
      if (TK_EXPRESSION == type)
        {
          /* Handling left/right padding adjustment */
          pparse_format_padding (dst[i], yytext);

          /* Setting prefix and suffix */
          input[yycolumn-1] = '\0';
          if (!opt->escape_disabled)
            UNESCAPE (prev_curly);
          dst[i]->pref = strdup (prev_curly);
          prev_curly = input + (yycolumn + yyleng);
          i++;
        }
    }
  if (!opt->escape_disabled)
    UNESCAPE (prev_curly);
  dst[dst_len - 1]->suff = strdup (prev_curly);

  yy_delete_buffer( buffer );
}

static inline void
pparse_format_regex (struct Opt *opt,
                     struct Seed *dst, char *input)
{
  if (!input)
    return;
  if (! opt->escape_disabled)
    UNESCAPE (input);

  if (dst->pref == NULL)
    dst->pref = strdup (input);
  else if (dst->suff == NULL)
    dst->suff = strdup (input);
  else
    warnln ("extra format was ignored");
}

static inline void
pparse_keys_regex (struct Opt *opt,
                   struct Seed *dst, const char *input)
{
  char *path;

  for (; '\\' == *input; ++input)
    {
      /* Handle '\N' where N is index of a seed */
      input++;
      if (REGULAR_MODE != opt->mode && IS_DIGIT (*input))
        {
          warnln ("regular mode specific shortcut \\%c was ignored", *input);
          continue;
        }
      if (REGULAR_MODE == opt->mode && IS_DIGIT (*input))
        {
          int n = strtol (input, NULL, 10) - 1;
          if (n == opt->reg_seeds_len)
            {
              warnln ("circular append was ignored");
              continue;
            }
          else if (n >= opt->reg_seeds_len)
            {
              warnln ("seed index %d is out of bound", n+1);
              continue;
            }
          else if (n < 0)
            {
              warnln ("invalid seed index");
              continue;
            }
          else /* valid index */
            {
              struct Seed *src = opt->reg_seeds[n];
              cseed_uniappd (dst, src->cseed, src->cseed_len);
              da_foreach (src->wseed, i)
                {
                  wseed_uniappd (opt, dst, src->wseed[i]);
                }
            }
          continue;
        }

      /* Got \N where N is not a digit */
      switch (*input)
        {
        case 'd': /* Digits 0-9 */
          cseed_uniappd (dst, charseed_09.c, charseed_09.len);
          break;

        case 'l': /* Lowercase letters */
        case 'a':
          cseed_uniappd (dst, charseed_az.c, charseed_az.len);
          break;

        case 'U': /* Uppercase letters */
        case 'u':
        case 'A':
          cseed_uniappd (dst, charseed_AZ.c, charseed_AZ.len);
          break;

        default:
          warnln ("invalid shortcut \\%c was ignored", *input);
        }
    }

  switch (*input)
    {
    case '\0':
      break;

    case '.': /* File path */
    case '/':
    case '~':
      if ((path = path_resolution (input)))
        {
          FILE *stream = safe_fopen (path, "r");
          if (stream)
            {
              wseed_file_uniappd (opt, dst, stream);
              if (stream != stdin)
                safe_fclose (stream);
            }
        }
      break;

    default:
      break;
    }
}

void
parse_seed_regex (struct Opt *opt,
                  struct Seed *dst, char *input)
{
  int type = 0;
  yyml = &SeedML;
  YY_BUFFER_STATE *buffer = yy_scan_buffer( input, strlen (input) );
  while ( (type = yylex()) != -1 )
    {
      switch (type)
        {
        case TK_KEYWORD:
          {
            if ('-' == *yytext)  /* Read from stdin */
              wseed_file_uniappd (opt, dst, stdin);
            else /* Shortcut or read from file path */
              pparse_keys_regex (opt, dst, yytext);
            break;
          }

        case TK_EXPRESSION:
          switch (yyid)
            {
            case EXP_BRACKET:
              pparse_cseed_regex (opt, dst, yytext);
              break;

            case EXP_CURLY:
              pparse_wseed_regex (opt, dst, yytext);
              break;

            case EXP_PAREN:
              pparse_format_regex (opt, dst, yytext);
              break;

            default:
              break;
            }
          break;

        default:
          break;
        }
    }
  yy_delete_buffer( buffer );
}
