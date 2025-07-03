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

    Permugen
    Permutation generator utility
    Generates customizable permutations from specified seeds

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
       $ permugen -s "[XY] [a-f]" -d2             # depth=2 (strict)
       $ permugen -s "[XYa-f]" -D4                # depth range [1 to 4]
       $ permugen -s "[XYa-f]" -d 3-5             # depth range [3 to 5]

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
       - To get '<S_i><S_j><S_k>' where i,j,k in [1 to N]:
       $ permugen -r [S_1]... [S_N] -d3
       - To also get '<S_i><S_j>':
       $ permugen -r [S_1]... [S_N] -d2-3

       - To read from stdin:
       $ permugen -r -- "{dev,prod}"  "-"
         or equivalently, avoid using end-of-options:
       $ permugen -r  "{dev,prod}"  " -"

     * Custom prefix and suffix:
       - The first component has `xxx` as suffix and
         the second component has `yyy` as prefix
       $ permugen -r  "() {One} (xxx)"  "(yyy) {Two} ()"
       - The first component uses {} and the second one uses ()
       $ permugen -r  "({) {One} (})"  "(\() {Two} (\))"


   Compilation:
     cc -ggdb -O3 -Wall -Wextra -Werror -I../libs \
        -o permugen permugen.c

   Options:
    - To skip freeing allocated memory before quitting
       define `_CLEANUP_NO_FREE`
    - To enable printing of debug information
       define `_DEBUG`
    - To use buffered IO library (deprecated)
       define `_USE_BIO`
       define `_BMAX="(1024 * 1)"` (=1024 bytes)
    - Use fscanf to read from streams
      If enabled, files will split by space characters
       define ONLY_FSCANF_STREAMS
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
#define Version "2.12"

#define CLI_IMPLEMENTATION
#include "clistd.h"

/* Default permutaiton depth (normal mode) */
#define DEF_DEPTH 3

/* Maximum length of a word seed */
#ifndef WSEED_MAXLEN
# define WSEED_MAXLEN 511 // 1 byte for null-byte
#endif

/* stdout buffer length */
#ifndef _BMAX
# define _BMAX 2048 // (page_size / 2) (bytes)
#endif

/* Maximum count of words in a seed */
#ifndef WSEED_MAXCNT
/* As our dynamic array grows by a factor of 2,
   it's more efficient for the max count to be a power of 2 */
# define WSEED_MAXCNT 8192
#endif

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

/**
 *  To prevent wseed fragmentation, this should be
 *  bigger than wseed max length
 */
#define TOKEN_MAX_BUF_LEN (WSEED_MAXLEN + 1)
#define ML_IMPLEMENTATION
#include "mini-lexer.h"

#undef STR
#define CSTR(var) #var
#define STR(var) CSTR (var)

#undef _Nullable
#define _Nullable
#define _nothing ((void) NULL)

#ifdef _DEBUG /* debug macro */
#  define dprintf(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)

/**
 *  Helper macro to print arrays with seperator & end suffix
 *  @T: printf format for @arr members, @len: length of @arr
 */
#  define printd_arr__H(arr, T, len, sep, end)      \
  for (ssize_t __idx = 0; __idx < len; __idx++) {   \
    dprintf (T"%s", arr[__idx],                     \
             (__idx < len-1) ? sep : end);          \
  }

/**
 *  Debug macro to print arrays of type @T and length @len
 *  Ex: to print `int arr[7]`:  `printd_arr (arr, "%d", 7);`
 *  Output format: 'arr[.7] = {0,1, ..., 6}'
 */
#  define printd_arr(arr, T, len) do {                  \
    if (len > 0 && arr) {                               \
      dprintf (CSTR (arr) "[.%d] = {", (int)(len));     \
      printd_arr__H (arr, T, len, ", ", "}\n");         \
    } else {                                            \
      dprintf (CSTR (arr) " is empty\n");               \
    }} while (0)

#else /* _DEBUG */
#  define dprintf(...) _nothing
#  define printd_arr(...) _nothing
#endif /* _DEBUG */

struct char_seed
{
  const char *c;
  int len;
};
const struct char_seed charseed_az = {"abcdefghijklmnopqrstuvwxyz", 26};
const struct char_seed charseed_AZ = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
const struct char_seed charseed_09 = {"0123456789", 10};

/* To check @c belongs to the closed interval [a,b] */
#define IN_RANGE(c, a,b) ((c) >= (a) && (c) <= (b))

/**
 *  Seeds Container 
 *  To get the length of `wseed`, use `da_sizeof`.
 *  `cseed`, `pref`, and `suff` must be allocated using `malloc`
 *  `wseed` must be allocated using `da_new` (dyna.h)
 */
struct Seed
{
  /* Character seed */
  char *cseed;
  int cseed_len;

  /* Word seed, dynamic array */
  char **wseed;

  /* Only in regular mode */
  char *pref;
  char *suff;
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
    /* General parser */
    PUNC_COMMA = 0,

    /* Special parser */
    EXP_PAREN = 0,     /* (xxx) */
    EXP_CBRACE,        /* {xxx} */
    EXP_SBRACKET,      /* [xxx] */
  };

static const char *Puncs[] =
  {
    [PUNC_COMMA]         = ",",
  };

static struct Milexer_exp_ Expressions[] =
  {
    [EXP_PAREN]          = {"(", ")"},
    [EXP_CBRACE]         = {"{", "}"},
    [EXP_SBRACKET]       = {"[", "]"},
  };

static Milexer ML =
  {
    .expression = GEN_MKCFG (Expressions),
    .puncs      = GEN_MKCFG (Puncs),
  };

/**
 *  Permugen Regex Parser
 *  general_{src,tk}: for parsing option value of -r and -s
 *  special_{src,tk}: when general_tk needs parsing
 *                    (e.g., '{AA,BB}' or '[a-b]')
 */
struct permugex
{
  Milexer *ml;
  Milexer_Slice general_src, special_src; /* input sources */
  Milexer_Token general_tk, special_tk;   /* result tokens */
};

enum mode
  {
    NORMAL_MODE = 0,
    REGULAR_MODE,
  };

/* Permugen's main configuration */
struct Opt
{
  /* General configuration */
  int mode;
  int escape_disabled; /* to disable backslash interpretation */
  int using_default_seed;

  struct
  {
    int min, max;
  } depth;

  /* Seed Configuration (Normal mode) */
  struct Seed *global_seeds;

  /* Seed Configuration (Regular mode) */
  struct Seed **reg_seeds; /* dynamic array of Seed struct */
  int reg_seeds_len;

  /* Output Configuration */
  FILE *outf; /* output file */
  char *prefix;
  char *suffix;
  char **seps; /* component separator(s) (dynamic array) */

  /* Output stream buffer */
#ifdef _USE_BIO
  BIO_t *bio;
#else
  char *streamout_buff;
#endif

  /* regex parser */
  struct permugex parser;
};

/* Internal Macros */
#define MIN(x, y) ((x < y) ? x : y)
#define MAX(x, y) ((x < y) ? y : x)

#define Strcmp(s1, s2)                          \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

/* NULL safe unescape */
#define UNESCAPE(cstr) do {                     \
    if (NULL != cstr)                           \
      unescape (cstr);                          \
  } while (0)

#undef IS_NUMBER
#define IS_NUMBER(c) (c >= '0' && c <= '9')

/**
 *  ASCII-printable character range
 *  Character seeds *MUST* be within this range
 */
#define MIN_ASCII_PR 0x20
#define MAX_ASCII_PR 0x7E
#define IS_ASCII_PR(c) (c >= MIN_ASCII_PR && c <= MAX_ASCII_PR)
/* Maximum length of character seeds */
#define CSEED_MAXLEN (MAX_ASCII_PR - MIN_ASCII_PR + 1)

/**
 *  Retrieves the last option from argv (e.g., '--xxx',
 *  '-x', or '-x<VALUE>') that was provided before optind
 *  This is useful for printing the exact option, when
 *  only the optarg (the value of that option) is available
 */
#define LASTOPT(argv)                                       \
  ((NULL != optarg && '-' != argv[optind - 1][0]) ?         \
   argv[optind - 2] : argv[optind - 1])

#ifdef _CLEANUP_NO_FREE
# define safe_free(...) _nothing
#else
# define safe_free(ptr) do {                    \
    if (ptr) {                                  \
      free (ptr);                               \
      ptr = NULL;                               \
    }} while (0)
#endif /* _CLEANUP_NO_FREE */

static inline void
safe_fclose (FILE *_Nullable f)
{
  if (f)
    {
      fflush (f);
      fclose (f);
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
 *  This function should be called by `on_exit`
 *  The pointer `__opt` *MUST* be accessible
 *  outside of the main function's scope
 */
void
cleanup (int, void *__opt)
{
  struct Opt *opt = (struct Opt *)__opt;
  if (!opt)
    return;

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

  /**
   * `global_seed` is not a dynamic array.
   *  It should have been allocated using malloc
   */
  free_seed (opt->global_seeds);
  safe_free (opt->global_seeds);

  /**
   *  `reg_seeds` is a dynamic array (using dyna.h)
   *  Each element of this array, is a seed pointer,
   *  allocated via `mk_seed` and must be freed using `free_seed`
   */
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
      /**
       *  Elements of opt->seps come from getopt (optarg)
       *  which should NOT be freed by us
       */
      da_free (opt->seps);
    }

  /**
   *  Two tokens have been allocated for regex parsing
   */
  TOKEN_FREE (&opt->parser.general_tk);
  TOKEN_FREE (&opt->parser.special_tk);
#endif /* _CLEANUP_NO_FREE */
}

/**
 *  Appends characters from @src to @s->cseed, until \0
 *  or !IS_ASCII_PR, returns the number of bytes written
 *  @s->cseed will have unique chars after this call, if it did before
 *  Pass `len = -1` to stop only at the null byte
 *
 *  Returns the index of the last non-zero byte in @src
 */
int cseed_uniappd (struct Seed *s, const char *src, int len);

/**
 *  Appends a copy of @str_word (using strdup malloc)
 *  to @s->wseed after unescaping it (if not disabled)
 *  @s->wseed will have unique words after this call, if it did before
 *
 *  Returns -1 on error, 1 on match found, 0 on a successful appending
 */
int wseed_uniappd (const struct Opt *, struct Seed *s,
                   const char *str_word);
/**
 *  Using wseed_uniappd function, appends words from @f
 *  to the seed @s, line by line, and ignores commented lines by `#`
 */
void
wseed_file_uniappd (const struct Opt *, struct Seed *s, FILE *f);

/**
 *  Parses the @input regex and stores the result in @s
 *  @input: "(prefix) [Cseed] {Wseed} /path/to/file (suffix)"
 *  If `prefix` and `suffix` are not NULL, they are allocated using
 *  `strdup`, and `wseed` is appended using `wseed_uniappd` function
 */
void parse_seed_regex (struct Opt *, struct Seed *s,
                       const char *input);

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
          --prefix            output prefix\n\
          --suffix            output suffix\n\
      -h, --help              print help and exit\n\
      -v, --version           print version and exit\n\n\
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
  disable this feature with `-E`\n\
\n\
  Seed: argument value of `-s, --seed` and `-r, --regular`\n\
        accepts any combination of the following patterns\n\
    `{word1,word2}`   to include 'word1' and 'word2'\n\
    `[XYZ]`:          to include characters X,Y,Z\n\
    `[a-f]`:          to include character range a,...,f\n\
    `\\N`:             to reuse (append) previous seeds, only in regular mode\n\
                      where `N` is the index of a prior given seed, starting from 1\n\
    character range shortcuts:\n\
      '\\d' for [0-9],  '\\l','\\a' for [a-z],  '\\u','\\U','\\A' for [A-Z]\n\
    inside these regex's, you might also use:\n\
      '\\{ and \\['       for '{', '}' and '[', ']' characters\n\
      '\\, or \\x2c'      for comma, alternatively use --raw-xxx in normal mode\n\
      '\\xNN or \\0HHH'   hex and octal byte, for example: \\x5c for backslash\n\
                        see the raw section for more details\n\
    `-`:              to read word seeds from the stdin up until Ctrl-D\n\
                      equivalently, an empty line and then the word `EOF`\n\
    `/path/to/file`:  to read words from a file (line by line)\n\
                      lines with '#' will be ignored\n\
    `(pref) (suff)`:  (in regular mode) to add custom prefix and suffix\n\
                      for parenthesis, use: \\( and \\)  or  \\x28 and \\x29\n\
                      the suffix will overwrite the separator if provided\n\
\n\
    Examples:\n\
      to include a,b and 0,...,9 and also words `foo` and `bar`:\n\
       \'[ab0-9] {foo,bar}\'  or equivalently  \'[ab] {foo,bar} [0-9]\'\n\
      to also include words from wordlist.txt:\n\
       \'[ab0-9] {foo,bar} /path/to/wordlist.txt\'\n\
      to also read from stdin:\n\
       \'- [ab0-9] {foo,bar} ~/wordlist.txt\'\n\
\n\
  Raw: backslash interpretation usage\n\
       \\\\:  to pass a single `\\`\n\
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

/**
 *  Output of characters and strings
 *  These macros write @str on @opt->outf
 *
 *  Fputs:  writes @str without its terminating null byte
 *  Puts:   writes @str like @Fputs and a trailing newline
 *  Putc:   writes a character @c (as unsigned char)
 *  Putln:  writes a newline
 */
#ifndef _USE_BIO
#  define Fputs(str, opt) fputs (str, opt->outf)
#  define Putc(c, opt) putc (c, opt->outf)
#  define Puts(str, opt) fprintf (opt->outf, "%s\n", str)
#  define Putln(opt) Putc ('\n', opt);
#else
#  define Fputs(str, opt) bio_fputs (opt->bio, str)
#  define Putc(c, opt) bio_putc (opt->bio, c)
#  define Puts(str, opt) bio_puts (opt->bio, str)
#  define Putln(opt) Putc ('\n', opt);
#endif /* _USE_BIO */

/**
 *  The main logic of normal mode
 *  It should be called it in a loop from
 *  depth=min_depth to depth=max_depth
 */
int
__perm (const struct Opt *opt, const char *sep, const int depth)
{
  int _max_depth = opt->global_seeds->cseed_len - 1 +
    (int) da_sizeof (opt->global_seeds->wseed);
  int idxs[depth];
  memset (idxs, 0, depth * sizeof (int));

 Perm_Loop: /* O(seeds^depth) */
  int i = 0;
  if (opt->prefix)
    Fputs (opt->prefix, opt);
  if (opt->global_seeds->pref)
    Fputs (opt->global_seeds->pref, opt);

 Print_Loop: /* O(depth) */
  {
    int idx = idxs[i];
    if (idx < opt->global_seeds->cseed_len)
      {
        /* range of character seeds */
        Putc (opt->global_seeds->cseed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= opt->global_seeds->cseed_len;
        Fputs (opt->global_seeds->wseed[idx], opt);
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

  for (int dep = opt->depth.min, ret = 0;
       dep <= opt->depth.max; ++dep)
    {
      for (ssize_t i=0; i < seps_len; ++i)
        {
          if (0 != (ret = __perm (opt, opt->seps[i], dep)))
            return ret;
        }
    }
  return 0;
}

/**
 *  The main logic of regular mode
 *  It should be called by the `regular_perm` function
 *
 *  @lens and @idxs both have size @size
 *  @idxs: temporary array for internal use of this function
 *  @lens: total length of each **s = opt->reg_seeds:
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
 Reg_Perm_Loop:
  int i = 0;
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
        Putc (current_seed->cseed[idx], opt);
      }
    else
      {
        /* Range of word seeds */
        idx -= current_seed->cseed_len;
        Fputs (current_seed->wseed[idx], opt);
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
  /* End of Printing the current permutation */
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
      if (!IS_ASCII_PR (*src))
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
  for (size_t i=0; i < len; ++i)
    {
      if (Strcmp (s->wseed[i], word))
        {
          safe_free (word);
          return 1;
        }
    }
  da_appd (s->wseed, word);
  return 0;
}

void
wseed_file_uniappd (const struct Opt *opt, struct Seed *s, FILE *f)
{
  if (!f || feof (f))
    {
      if (f == stdin)
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

  if (f == stdin && isatty (fileno (f)))
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

  char *line = malloc (WSEED_MAXLEN + 1);
  while (1)
    {
#ifndef ONLY_FSCANF_STREAMS
      if (! fgets (line, WSEED_MAXLEN, f))
        break;
      line[strcspn (line, "\n")] = '\0';
#else /* Treat space as delimiter */
      if (fscanf (f, "%" STR (WSEED_MAXLEN) "s",  line) < 0)
        break;
#endif
      /* Ignore empty and comment lines */
      if (MIN_ASCII_PR > line[0] || '#' == line[0])
        break;
      /* This should break when wseed max count is reached */
      if (wseed_uniappd (opt, s, line) < 0)
        break;
    }
  safe_free (line);
}

/* CLI options, getopt */
const struct option lopts[] =
  {
    /* seeds */
    {"seed",             required_argument, NULL, 's'},
    {"raw-seed",         required_argument, NULL, '0'},
    {"raw-wseed",        required_argument, NULL, '5'},
    {"seed-path",        required_argument, NULL, 'S'},
    {"wseed-path",       required_argument, NULL, 'S'},
    /* output file */
    {"output",           required_argument, NULL, 'o'},
    {"append",           required_argument, NULL, 'a'},
    {"oA",               required_argument, NULL, 'a'},
    {"help",             no_argument,       NULL, 'h'},
    /* delimiter */
    {"delim",            required_argument, NULL, 'p'},
    {"delimiter",        required_argument, NULL, 'p'},
    /* depth */
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
    /* format */
    {"pref",             required_argument, NULL, '3'},
    {"prefix",           required_argument, NULL, '3'},
    {"suff",             required_argument, NULL, '4'},
    {"suffix",           required_argument, NULL, '4'},
    /* regular mode */
    {"regular",          no_argument,       NULL, 'r'},
    /* CLI */
    {"version",          no_argument,       NULL, 'v'},
    {"help",             no_argument,       NULL, 'h'},
    /* end of options */
    {NULL,               0,                 NULL,  0 },
  };

static int
opt_getopt (int argc, char **argv, struct Opt *opt)
{
  /* we use 0,1,2,... as `helper` options and only to use getopt */
  const char *lopt_cstr = "s:S:o:a:p:d:D:0:1:2:3:4:5:vhrEe";
  int idx = 0;

  /* Usage: case X:  NOT_IN_REGULAR_MODE("X") {...} */
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

        case 'p': /* separator */
          if (NULL == opt->seps[0])
            opt->seps[0] = optarg;
          else
            da_appd (opt->seps, optarg);
          break;

        case 'd': /* depth, format: '-d N' or '-d N,M' */
          {
            char *p = strchr (optarg, '-');
            if (!p)
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
        case 'D': /* depth range */
          opt->depth.min = 1;
          opt->depth.max = atoi (optarg);
          break;
        case '1': /* min depth */
          opt->depth.min = atoi (optarg);
          break;
        case '2': /* max depth */
          opt->depth.max = atoi (optarg);
          break;

          /* Only in normal mode */
        case 'S': /* wseed file / stdin */
          NOT_IN_REGULAR_MODE ()
          {
            FILE *wseed_f = stdin;
            /* using optarg value as filepath otherwise stdin */
            if (!Strcmp (optarg, "-"))
              wseed_f = safe_fopen (optarg, "r");

            /* read from file and append to wseed */
            wseed_file_uniappd (opt, opt->global_seeds, wseed_f);

            if (wseed_f != stdin)
              safe_fclose (wseed_f);
          }
          break;

        case 's': /* seed configuration */
          NOT_IN_REGULAR_MODE ()
          {
            opt->using_default_seed = 0;
            /* This option disables the default seed config */
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

        default:
          break;
        }
    }
  return 0;
}

static void
opt_init (struct Opt *opt)
{
  /**
   *  Initializing the default values if not set
   */
  if (opt->outf == NULL)
    opt->outf = stdout;

  switch (opt->mode)
    {
    case REGULAR_MODE:
      int max_depth = da_sizeof (opt->reg_seeds);

      if (opt->depth.min == 0 && opt->depth.max == 0)
        {
          /* Uninitialized */
          opt->depth.min = max_depth;
          opt->depth.max = max_depth;
          break;
        }

      /* Fix invalid ranges */
      opt->depth.min = MAX (opt->depth.min, 1);
      opt->depth.min = MIN (opt->depth.min, max_depth);
      opt->depth.max = MAX (opt->depth.max, opt->depth.min);
      opt->depth.max = MIN (opt->depth.max, max_depth);
      break;

    case NORMAL_MODE:
      if (opt->global_seeds->cseed_len == 0 && opt->using_default_seed)
        {
          /* initializing with the default seed [a-z0-9] */
          cseed_uniappd (opt->global_seeds,
                         charseed_az.c, charseed_az.len);
          cseed_uniappd (opt->global_seeds,
                         charseed_09.c, charseed_09.len);
        }

      if (opt->depth.min <= 0 && opt->depth.max <= 0)
        {
          /* Using the default values when not specified */
          opt->depth.min = DEF_DEPTH;
          opt->depth.max = DEF_DEPTH;
        }
      else if (opt->depth.max <= 0)
        {
          /* Only depth.min is specified OR `-D` is being used */
          opt->depth.max = opt->depth.min;
        }
      if (opt->depth.min > opt->depth.max)
        {
          /* Invalid min and max depths */
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
mk_opt ()
{
  static struct Opt opt = {0};
  {
    /* Dynamic arrays */
    opt.global_seeds = mk_seed (CSEED_MAXLEN, 1);
    opt.using_default_seed = 1;

    opt.seps = da_newn (char *, 1);
    da_appd (opt.seps, NULL);
  }

  {
    /* Initializing the parser */
    opt.parser = (struct permugex) {
      .ml            = &ML,
      /* parser inputs */
      .general_src   = {.lazy = 0},
      .special_src   = {.lazy = 0},
      /* parser token output */
      .general_tk    = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN),
      .special_tk    = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN),
    };
  }

  return &opt;
}

int
main (int argc, char **argv)
{
  struct Opt *opt;
  set_program_name (*argv);

  opt = mk_opt ();
  on_exit (cleanup, opt);

  /* Read options */
  if (opt_getopt (argc, argv, opt))
    return EXIT_FAILURE;
  /* Set defaults and finalize opt */
  opt_init (opt);

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

  /* Initializing the output stream buffer */
#ifndef _USE_BIO
  opt->streamout_buff = malloc (_BMAX);
  if (!!setvbuf (opt->outf, opt->streamout_buff, _IOFBF, _BMAX))
    {
      warnln ("failed to set buffer for stdout");
      exit (EXIT_FAILURE);
    }
#else
  opt->bio = malloc (sizeof (BIO_t));
  *opt->bio = bio_new (_BMAX, malloc (_BMAX), fileno (opt->outf));
  dprintf ("* buffer length of buffered_io: %d bytes\n", _BMAX);
# endif /* _USE_BIO */

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


/* Internal regex parser functions */
/**
 *  Word Seed regex parser
 *  inside `{...}`  -  comma-separated words
 *  wseed_uniappd call will backslash interpret the result
 *  This function uses special_tk and special_src for parsing
 */
static inline void
pparse_wseed_regex (struct Opt *, struct Seed *dst_seed);

/**
 *  Character Seed regex parser
 *  inside: `[...]`
 *  It backslash interprets the result if not disabled
 */
static inline void
pparse_cseed_regex (struct Opt *, struct Seed *dst_seed);

/**
 *  Detects file paths, previous seed indexes (\N)
 *  and other shortcuts like \d, \u, and \U
 *  This function uses special_tk and special_src for parsing
 */
static inline void
pparse_keys_regex (struct Opt *, struct Seed *dst_seed,
                   const char *input);
/**
 *  Provides prefix and suffix of @dst_seed using strdup,
 *  inside: `(...)` (only in regular mode)
 *  The first call sets @dst_seed->pref (prefix) and
 *  the second call sets @dst_seed->suff (suffix)
 */
static inline void
pparse_format_regex (struct Opt *,
                     struct Seed *dst_seed, char *input);

/**
 *  Path resolver
 *  Supported formats: '~/' , '../' , '/./'
 */
static char *
path_resolution (const char *path_cstr)
{
  char *tmp;
  size_t len = strlen (path_cstr);
  static char PATH[PATH_MAX];

  if (*path_cstr == '~')
    {
      const char* home = getenv ("HOME");
      if (!home)
        {
          warnln ("$HOME is null");
          return NULL;
        }
      tmp = malloc (len + strlen (home) + 1);
      char *p;
      p = mempcpy (tmp, home, strlen (home));
      p = mempcpy (p, path_cstr + 1, len - 1);
      *p = '\0';
    }
  else
    {
      tmp = malloc (len + 1);
      *((char *) mempcpy (tmp, path_cstr, len)) = '\0';
    }

  /* Interpret `\<space>` */
  UNESCAPE (tmp);

  if (realpath (tmp, PATH))
    {
      safe_free (tmp);
      return PATH;
    }
  else
    {
      warnln ("path resolution failed -- %s (%s)", strerror (errno), tmp);
      safe_free (tmp);
      return NULL;
    }
}

static inline void
pparse_cseed_regex (struct Opt *opt, struct Seed *dst_seed)
{
  char lastc = 0, *tok = opt->parser.general_tk.cstr;
  if (!opt->escape_disabled)
    UNESCAPE (tok);

  for (char *p = tok; '\0' != *p; ++p)
    {
      if ('-' != *p || 0 == lastc)
        {
          lastc = *p;
          cseed_uniappd (dst_seed, p, 1);
          continue;
        }
      if ('\0' == *(p++)) /* skipping `-` */
        {
          cseed_uniappd (dst_seed, "-", 1);
          break;
        }

      int range_len = *p - lastc + 1;
      const char *range = NULL; /* NULL means custom range */
#define MK_RANGE(charseed) do {                             \
        if (! IN_RANGE (*p, (charseed).c[0],                \
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
        cseed_uniappd (dst_seed, range, range_len);
      else
        {
          for (int i=0; i < range_len; ++i, ++lastc)
            cseed_uniappd (dst_seed, &lastc, 1);
        }
      lastc = 0;
    }
#undef MK_RANGE
}

static inline void
pparse_wseed_regex (struct Opt *opt, struct Seed *dst_seed)
{
  Milexer_Token *tmp = &opt->parser.special_tk;

  for (int _ret = 0; !NEXT_SHOULD_END (_ret); )
    {
      _ret = ml_next (opt->parser.ml,
                      &opt->parser.special_src,
                      tmp,
                      PFLAG_DEFAULT);
      if (NEXT_SHOULD_LOAD (_ret))
        break;

      /**
       *  As the length of @tmp->cstr is >= to WSEED_MAXLEN,
       *  we handle fragmentation (NEXT_CHUNK) as
       *  if we have received a new wseed (each chunk)
       */
      if (tmp->type == TK_KEYWORD)
        {
          /* This must be freed in the free_seed function */
          wseed_uniappd (opt, dst_seed, tmp->cstr);
        }
    }
}

static inline void
pparse_format_regex (struct Opt *opt,
                     struct Seed *dst_seed, char *input)
{
  if (!input)
    return;
  if (!opt->escape_disabled)
    UNESCAPE (input);

  if (dst_seed->pref == NULL)
    dst_seed->pref = strdup (input);
  else if (dst_seed->suff == NULL)
    dst_seed->suff = strdup (input);
  else
    warnln ("extra format was ignored");
}

static inline void
pparse_keys_regex (struct Opt *opt, struct Seed *dst_seed,
                   const char *input)
{
  switch (*input)
    {
    case '\0':
      break;

      /* Shortcuts */
    case '\\':
      {
        /**
         *  Check for \N where N represents the index of a
         *  previously provided seed
         */
        input++;
        if (opt->mode == REGULAR_MODE && IS_NUMBER (*input))
          {
            int n = strtol (input, NULL, 10) - 1;
            /* Handle invalid indexes */
            if (n == opt->reg_seeds_len)
              {
                warnln ("circular append was ignored");
                break;
              }
            else if (n >= opt->reg_seeds_len)
              {
                warnln ("seed index %d is out of bound", n+1);
                break;
              }
            else if (n < 0)
              {
                warnln ("invalid seed index");
                break;
              }
            else /* valid index */
              {
                struct Seed *src = opt->reg_seeds[n];
                /* append csseds */
                cseed_uniappd (dst_seed, src->cseed, src->cseed_len);
                /* append wseeds */
                da_foreach (src->wseed, i)
                  {
                    wseed_uniappd (opt, dst_seed, src->wseed[i]);
                  }
              }
            break;
          }

        /* Got \N where N is not a number */
        switch (*input)
          {
          case 'd': /* digits 0-9 */
            cseed_uniappd (dst_seed,
                           charseed_09.c, charseed_09.len);
            break;

          case 'l': /* lowercase letters */
          case 'a':
            cseed_uniappd (dst_seed,
                           charseed_az.c, charseed_az.len);
            break;

          case 'U': /* uppercase letters */
          case 'u':
          case 'A':
            cseed_uniappd (dst_seed,
                           charseed_AZ.c, charseed_AZ.len);
            break;

          default:
            warnln ("invalid shortcut \\%c was ignored", *input);
          }
      }
      break;

      /* File path */
    case '.':
    case '/':
    case '~':
      char *path;
      if ((path = path_resolution (input)))
        {
          FILE *f = safe_fopen (path, "r");
          if (f)
            {
              wseed_file_uniappd (opt, dst_seed, f);
              if (f != stdin)
                safe_fclose (f);
            }
        }
      break;

    default:
      break;
    }
}

void
parse_seed_regex (struct Opt *opt, struct Seed *dst_seed,
                  const char *input)
{
  char *extended_token = NULL;
  Milexer *ml = opt->parser.ml;
  Milexer_Token *tmp = &opt->parser.general_tk;

  /* Mini-lexer internal initialization */
  SET_ML_SLICE (&opt->parser.general_src,
                input,
                strlen (input));

  int ret = 0;
  while (!NEXT_SHOULD_END (ret))
    {
      ret = ml_next (ml,
                     &opt->parser.general_src,
                     tmp,
                     PFLAG_INEXP);

      switch (tmp->type)
        {
        case TK_KEYWORD:
          {
            /**
             *  These tokens may represent a file path
             *  or `-`, which indicates reading from stdin.
             *  We assume that file paths do not start with `-`.
             */
            if ('-' == *tmp->cstr)
              {
                /* Read from stdin */
                wseed_file_uniappd (opt, dst_seed, stdin);
              }
            else
              {
                /**
                 *  Since the `pparse_keys_regex` function is NOT
                 *  fragment-safe, we should receive the entire token using
                 *  the `catstr` function from the mini-lexer library.
                 */
                if (!ml_catcstr (&extended_token, tmp->cstr, ret))
                  break;
                pparse_keys_regex (opt, dst_seed, tmp->cstr);
              }
            break;
          }

        case TK_EXPRESSION:
          char *__cstr = tmp->cstr;
          SET_ML_SLICE (&opt->parser.special_src,
                        __cstr,
                        strlen (__cstr));

          switch (tmp->id)
            {
            case EXP_SBRACKET:
              /* Parsing contents of `[-xA-Zy-]` as cssed */
              pparse_cseed_regex (opt, dst_seed);
              break;

            case EXP_CBRACE:
              /* Parsing contents of `{xxx,yyy}` as wseed */
              pparse_wseed_regex (opt, dst_seed);
              break;

            case EXP_PAREN:
              /* Parsing `(xxx)` as seed prefix OR suffix */
              pparse_format_regex (opt, dst_seed, tmp->cstr);
              break;

            default:
              break;
            }
          break;

        default:
          break;
        }
    }
  safe_free (extended_token);
}
