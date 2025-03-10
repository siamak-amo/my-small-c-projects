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

    Usage:
       For more details, use the `-h` option:
       $ permugen -h

    Normal mode:  permugen [OPTIONS] -s [SEED_CONFIG]
      Cartesian product of the input with a certain depth
      {INPUT_SEED}x{INPUT_SEED}x...  (depth times)

     * Alphanumeric characters:
       $ permugen                                  # a-z and 0-9 of depth 3
       $ permugen -s "\d"                          # only digits 0-9
       $ permugen -s "\d \u"                       # 0-9 and A-Z
       $ permugen -s "\d \u \l"                    # 0-9 and A-Z and a-z

     * The set {A,B,C , a,...,f}:
       $ permugen -d2 -s "[ABC] [a-f]"             # depth 2
       $ permugen -D4 -s "[ABCa-f]"                # depth range 1,...,4
       $ permugen --min-depth 3 --max-depth 5      # depth range 3,...,5

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

       - Global suffix and prefix
       $ permugen --pref "www." --suff ".com"


    Regular Mode:  permugen [OPTIONS] -r [SEED_CONFIG]...
      To manually specify components of the permutations
      {INPUT_SEED_1}x{INPUT_SEED_2}x...x{INPUT_SEED_N}

     * Basic Examples:
       - Cartesian product of {0,1,2}x{AA,BB}
       $ permugen -r "[0-2]" "{AA,BB}"

       - {dev,prod}x{admin,<wordlist.txt>}
       $ permugen -r "{dev,prod}"  "{admin} /path/to/wordlist.txt"

       - To reuse previously provided seeds (\N starting from 1)
         {dev,prod}x{www,dev,prod}
       $ permugen -p. -r "{dev,prod}"  "{www} \1"
         {dev,prod}x{2,3}x{2,3}
       $ permugen -r "{dev,prod}"  "[2-3]" "\2"

       - To read from stdin:
       $ permugen -r -- "{dev,prod}" "-"
         or equivalently, avoid using end-of-options:
       $ permugen -r "{dev,prod}" " -"

     * Custom prefix and suffix:
       - The first component has `xxx` as suffix and
         the second component has `yyy` as prefix
       $ permugen -r "() {One} (xxx)"  "(yyy)  {Two}  ()"
       - The first component uses {} and the second one uses ()
       $ permugen -r "({) {One} (})"  "(\()  {Two}  (\))"


   Compilation:
     cc -ggdb -O3 -Wall -Wextra -Werror -I../libs \
        -o permugen permugen.c

   Options:
    - To enable printing of debug information
       define `_DEBUG`
    - To use buffered IO library (deprecated)
       define `_USE_BIO`
    - To change the default buffered IO buffer capacity
       define `_BMAX="(1024 * 1)"` (=1024 bytes)
    - To skip freeing allocated memory before quitting
       define `_CLEANUP_NO_FREE`
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

#define PROGRAM_NAME "permugen"
#define Version "2.10"

#define CLI_IMPLEMENTATION
#include "clistd.h"

/* Default permutaiton depth (normal mode) */
#define DEF_DEPTH 3

/* Maximum length of character seeds */
#ifndef CSEED_MAXLEN
# define CSEED_MAXLEN 256
#endif

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
 **   - mini-lexer.c:   Lexer (used for regex parsing)
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
#include "mini-lexer.c"

#undef STR
#define __STR(var) #var
#define STR(var) __STR (var)

#ifdef _DEBUG /* debug macro */
#undef dprintf
#define dprintf(format, ...) fprintf (stderr, format, ##__VA_ARGS__)

/**
 *  Helper macro to print arrays with seperator & end suffix
 *  @T: printf format for @arr members, @len: length of @arr
 */
#  define printd_arr__H(arr, T, len, sep, end)  \
  for (int __idx = 0; __idx < len; __idx++) {   \
    dprintf (T"%s", arr[__idx],                 \
             (__idx < len-1) ? sep : end);      \
  }

/**
 *  Debug macro to print arrays of type @T and length @len
 *  Ex: to print `int arr[7]`:  `printd_arr (arr, "%d", 7);`
 *  Output format: 'arr[.7] = {0,1, ..., 6}'
 */
#  define printd_arr(arr, T, len) do {          \
  if (len > 0 && arr) {                         \
    dprintf (#arr"[.%d] = {", len);             \
    printd_arr__H (arr, T, len, ", ", "}\n");   \
  } else {                                      \
    dprintf (#arr" is empty\n");                \
  }} while (0)

#else /* _DEBUG */
#  undef dprintf
#  define dprintf(format, ...) (void)(format)
#  define printd_arr(arr, T, len) (void)(arr)
#endif /* _DEBUG */

struct char_seed
{
  const char *c;
  int len;
};
const struct char_seed charseed_az = {"abcdefghijklmnopqrstuvwxyz", 26};
const struct char_seed charseed_AZ = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
const struct char_seed charseed_09 = {"0123456789", 10};

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
    PUNC_DASH,

    /* Special parser */
    EXP_PAREN = 0,     /* (xxx) */
    EXP_CBRACE,        /* {xxx} */
    EXP_SBRACKET,      /* [xxx] */
  };

static const char *Puncs[] = {
  [PUNC_COMMA]         = ",",
  [PUNC_DASH]          = "-",
};

static struct Milexer_exp_ Expressions[] = {
  [EXP_PAREN]          = {"(", ")"},
  [EXP_CBRACE]         = {"{", "}"},
  [EXP_SBRACKET]       = {"[", "]"},
};

static const Milexer ML = {
  .expression = GEN_MKCFG (Expressions),
};

static const Milexer ML_IN = {
  .puncs = GEN_MKCFG (Puncs),
};

/**
 *  Permugen Regex Parser (arguments of '-r' and '-s')
 *  general_{src,tk}: for parsing arguments themselves
 *  special_{src,tk}: when contents need parsing (like '{A,B}')
 */
struct permugex
{
  /* ml parses general_xxx and ml_in parses special_xxx */
  const Milexer *ml, *ml_in;
  Milexer_Slice general_src, special_src; /* input sources */
  Milexer_Token general_tk, special_tk;   /* result tokens */
};

/* Permugen's main configuration */
struct Opt
{
  /* General configuration */
  int escape_disabled; /* to disable backslash interpretation */
  int from_depth; /* min depth */
  int to_depth;   /* max depth */

  /* Seed Configuration (Normal mode) */
  struct Seed *global_seeds;

  /* Seed Configuration (Regular mode) */
  int _regular_mode; /* equal to `1 + len(reg_seeds)` */
  struct Seed **reg_seeds; /* dynamic array of Seed struct */

  /* Output Configuration */
  FILE *outf; /* output file */
  char *prefix;
  char *suffix;
  char *separator; /* between components of permutations */

  /* Output stream buffer */
#ifdef _USE_BIO
  BIO_t *bio;
#else
  char *streamout_buff;
#endif

  /* regex parser */
  struct permugex parser;
};

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

/* Internal Macros */
#define Strcmp(s1, s2)                          \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

#undef IS_NUMBER
#define IS_NUMBER(c) (c >= '0' && c <= '9')

/* ASCII-printable, Non-white-space */
#define IS_ASCII_PR(c) (c >= 0x20 && c <= 0x7E)

#ifndef _Nullable
# define _Nullable
#endif

#ifdef _CLEANUP_NO_FREE
# define safe_free(ptr)
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
   *  Each element of this array must is a seed pointer,
   *  allocated via `mk_seed` and must be freed using `free_seed`
   */
  if (opt->reg_seeds)
    {
      for (ssize_t i=0; i < da_sizeof (opt->reg_seeds); ++i)
        {
          free_seed (opt->reg_seeds[i]);
          safe_free (opt->reg_seeds[i]);
        }
      da_free (opt->reg_seeds);
    }

  /**
   *  Two tokens have been allocated for regex parsing
   */
  TOKEN_FREE (&opt->parser.general_tk);
  TOKEN_FREE (&opt->parser.special_tk);
#endif /* _CLEANUP_NO_FREE */
}

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
\n\
  Only in normal mode:\n\
      -d, --depth             specify depth\n\
      -D, --depth-range       depth range\n\
     -df, --depth-from        specify min depth\n\
          --min-depth\n\
     -dt, --depth-to          specify max depth\n\
          --max-depth\n\
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
 *  Newln:  writes a newline
 */
#ifndef _USE_BIO
#  define Fputs(str, opt) fputs (str, opt->outf)
#  define Putc(c, opt) putc (c, opt->outf)
#  define Puts(str, opt) fprintf (opt->outf, "%s\n", str)
#  define Newln(opt) Putc ('\n', opt);
#else
#  define Fputs(str, opt) bio_fputs (opt->bio, str)
#  define Putc(c, opt) bio_putc (opt->bio, c)
#  define Puts(str, opt) bio_puts (opt->bio, str)
#  define Newln(opt) Putc ('\n', opt);
#endif /* _USE_BIO */

/**
 *  The main logic of normal mode
 *  It should be called it in a loop from
 *  depth=min_depth to depth=max_depth
 */
int
perm (const int depth, const struct Opt *opt)
{
  int _max_depth = opt->global_seeds->cseed_len - 1 +
    (int) da_sizeof (opt->global_seeds->wseed);
  int idxs[depth];
  memset (idxs, 0, depth * sizeof (int));

 Perm_Loop: /* O(seeds^depth) */
  int i = 0;
  if (opt->prefix)
    Fputs (opt->prefix, opt);
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
      if (opt->separator)
        Fputs (opt->separator, opt);
      goto Print_Loop;
    }
  /* End of Printing the current permutation */
  if (opt->suffix)
    Puts (opt->suffix, opt);
  else
    Newln (opt);


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

/**
 *  The main logic of regular mode
 *  It should be called by the `regular_perm` function
 */
int
__regular_perm (struct Opt *opt, int *depths, int depth)
{
  int ret;
  /* permutation indexes */
  int *idxs = malloc (depth * sizeof (int));
  memset (idxs, 0, depth * sizeof (int));

  struct Seed **s = opt->reg_seeds;
  /**
   *  O(S_1 * ... * S_n)
   *  where: n=depth and S_i = depths[i]
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
        /* range of character seeds */
        Putc (current_seed->cseed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= current_seed->cseed_len;
        Fputs (current_seed->wseed[idx], opt);
      }
    i++;
  }
  if (current_seed->suff)
    Fputs (current_seed->suff, opt);
  if (i < depth)
    {
      if (opt->separator)
        {
          /**
           *  Print the separator only when the current seed
           *  has no suffix (suffix must overwrite the separator)
           */
          if (!current_seed->suff || *current_seed->suff == '\0')
            Fputs (opt->separator, opt);
        }
      goto Print_Loop;
    }
  /* End of Printing the current permutation */
  if (opt->suffix)
    Puts (opt->suffix, opt);
  else
    Newln (opt);

  int pos;
  for (pos = depth-1;
       pos >= 0 && idxs[pos] == depths[pos];
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
  safe_free (idxs);
  return ret;
}

int
regular_perm (struct Opt *opt)
{
  int ret = 0;
  struct Seed *s = NULL;

  /* count of seed configurations */
  int seeds_count = (int) da_sizeof (opt->reg_seeds);
  /* len(cseed)+len(wseed) of each configuration */
  int *depths = malloc (seeds_count * sizeof (int));

  for (int i=0; i < seeds_count && (s = opt->reg_seeds[i]); ++i)
    {
      if ((depths[i] = (s->cseed_len) + da_sizeof (s->wseed) - 1) < 0)
        goto _return; /* unreachable */
    }

  ret = __regular_perm (opt, depths, seeds_count);
 _return:
  safe_free (depths);
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
    unescape (word);

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
  if (!f || ferror (f) || feof (f))
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
      if (opt->_regular_mode)
        {
          fprintf (stderr,
                   "Reading words for the seed #%d until EOF:\n",
                   opt->_regular_mode);
        }
      else
        fprintf (stderr, "Reading words until EOF:\n");
    }

  char *line = malloc (WSEED_MAXLEN + 1);
  while (1)
    {
      if (fscanf (f, "%" STR (WSEED_MAXLEN) "s",  line) < 0)
        break;

      size_t len;
      /* ignore empty and commented lines */
      if ((len = strlen (line)) > 0 && line[0] != '#')
        {
          /* remove non-printable characters */
          size_t idx = 0;
          for (; idx < len; ++idx)
            {
              if (line[idx] > 0 && line[idx] < 0x20)
                {
                  line[idx] = '\0';
                  break;
                }
            }
          if (idx)
            {
              if (wseed_uniappd (opt, s, line) < 0)
                break;
            }
        }
    }
    safe_free (line);
}

/* CLI options, getopt */
const struct option lopts[] = {
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
  {"df",               required_argument, NULL, '1'},
  {"depth-from",       required_argument, NULL, '1'},
  {"from-depth",       required_argument, NULL, '1'},
  {"min-depth",        required_argument, NULL, '1'},
  {"dt",               required_argument, NULL, '2'},
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
  /* end of options */
  {NULL,               0,                 NULL,  0 },
};

int
init_opt (int argc, char **argv, struct Opt *opt)
{
#define CASE_NOT_IN_REG_MODE(option) \
  NOT_IN_REG_MODE (option, break)

#define NOT_IN_REG_MODE(option, action)                             \
  if (opt->_regular_mode) {                                         \
    warnln ("wrong regular mode option (%s) was ignored", option);  \
    action;                                                         \
  }

  /* we use 0,1,2,... as `helper` options and only to use getopt */
  const char *lopt_cstr = "s:S:o:a:p:d:D:0:1:2:3:4:5:hrEe";

  int idx = 0, using_default_seed = 1;
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
        case 'd': /* depth */
          opt->from_depth = atoi (optarg);
          break;
        case 'D': /* depth range */
          opt->from_depth = 1;
          opt->to_depth = atoi (optarg);
          break;
        case 'p': /* delimiter */
          opt->separator = optarg;
          break;
        case '3': /* prefix */
          opt->prefix = optarg;
          break;
        case '4': /* suffix */
          opt->suffix = optarg;
          break;
        case '1': /* depth from */
          opt->from_depth = atoi (optarg);
          break;
        case '2': /* depth to */
          opt->to_depth = atoi (optarg);
          break;

        case 'S': /* wseed file / stdin */
          {
            CASE_NOT_IN_REG_MODE(argv[optind-2]);
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
          {
            CASE_NOT_IN_REG_MODE(argv[optind-2]);
            using_default_seed = 0;
            /* this option disables the default seed config */
            parse_seed_regex (opt, opt->global_seeds, optarg);
          }
          break;

        case '0': /* raw seed */
          {
            CASE_NOT_IN_REG_MODE(argv[optind-2]);
            using_default_seed = 0;
            if (!opt->escape_disabled)
              unescape (optarg);
            cseed_uniappd (opt->global_seeds, optarg, strlen (optarg));
          }
          break;

        case '5': /* raw word seed */
          CASE_NOT_IN_REG_MODE(argv[optind-2]);
          wseed_uniappd (opt, opt->global_seeds, optarg);
          break;

        case 'r': /* regular mode */
          {
            int end_of_options = 0;
            using_default_seed = 0;
            opt->_regular_mode = 1;
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
                        /* end of `-r` arguments */
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
                        opt->_regular_mode++;
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

  /**
   *  Initializing the default values
   */
  if (opt->outf == NULL)
    opt->outf = stdout;

  if (opt->_regular_mode > 0)
    {
      /* regular mode */
    }
  else
    {
      /* normal mode */
      if (opt->global_seeds->cseed_len == 0 && using_default_seed)
        {
          /* initializing with the default seed [a-z0-9] */
          cseed_uniappd (opt->global_seeds,
                         charseed_az.c, charseed_az.len);
          cseed_uniappd (opt->global_seeds,
                         charseed_09.c, charseed_09.len);
        }

      if (opt->from_depth <= 0 && opt->to_depth <= 0)
        {
          /* using the default values when not specified */
          opt->from_depth = DEF_DEPTH;
          opt->to_depth = DEF_DEPTH;
        }
      else if (opt->to_depth <= 0)
        {
          /* only from_depth is specified OR `-D` is being used */
          opt->to_depth = opt->from_depth;
        }
      if (opt->from_depth > opt->to_depth)
        {
          /* invalid min and max depths */
          opt->to_depth = opt->from_depth;
        }
    }
  /* Interpret backslash characters */
  if (!opt->escape_disabled)
    {
      if (opt->prefix != NULL)
        unescape (opt->prefix);
      if (opt->suffix != NULL)
        unescape (opt->suffix);
      if (opt->separator != NULL)
        unescape (opt->separator);
    }

  return 0;
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
      for (ssize_t i=0; i < da_sizeof (s->wseed); ++i)
        safe_free (s->wseed[i]);
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

int
main (int argc, char **argv)
{
  static struct Opt opt = {0};
  set_program_name (*argv);
  on_exit (cleanup, &opt);

  {
    /* initializing the parser */
    opt.parser = (struct permugex) {
      .ml    = &ML,
      .ml_in = &ML_IN,

      .general_src   = {.lazy = 0},
      .special_src   = {.lazy = 0},

      .general_tk    = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN),
      .special_tk    = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN),
    };
  }

  {
    /* Initializing options */
    opt.global_seeds = mk_seed (CSEED_MAXLEN, 1);
    if (init_opt (argc, argv, &opt))
      return EXIT_FAILURE;

    /* Generate permutations */
    if (opt._regular_mode > 0)
      {
        /* Regular mode, _regular_mode = 1 + length of reg_seeds */
        if (opt._regular_mode == 1)
          {
            warnln ("empty regular permutation");
            return EXIT_FAILURE;
          }
      }
    else
      {
        /* Normal mode */
        if (opt.global_seeds->cseed_len == 0 &&
            da_sizeof (opt.global_seeds->wseed) == 0)
          {
            warnln ("empty permutation");
            return EXIT_FAILURE;
          }
      }
  }

  /* Initializing the output stream buffer */
#ifndef _USE_BIO
  opt.streamout_buff = malloc (_BMAX);
  if (!!setvbuf (opt.outf, opt.streamout_buff, _IOFBF, _BMAX))
    {
      warnln ("failed to set buffer for stdout");
      exit (EXIT_FAILURE);
    }
#else
  opt.bio = malloc (sizeof (BIO_t));
  *opt.bio = bio_new (_BMAX, malloc (_BMAX), fileno (opt.outf));
  dprintf ("* buffer length of buffered_io: %d bytes\n", _BMAX);
# endif /* _USE_BIO */

  if (opt._regular_mode)
    {
      size_t len = da_sizeof (opt.reg_seeds);
      dprintf ("* regular mode\n");
      dprintf ("* %s[.%lu] = {\n", STR (opt.reg_seeds), (size_t)len);
      for (size_t i=0; i < len; ++i)
        {
          struct Seed *s = opt.reg_seeds[i];
          dprintf ("    %s[%lu] = {\n      ", STR (opt.reg_seeds), i);
          printd_arr (s->cseed, "`%c`", s->cseed_len);
          dprintf ("      ");
          printd_arr (s->wseed, "`%s`", (int)da_sizeof (s->wseed));
          if (s->pref)
            dprintf ("      prefix = \"%s\"\n", s->pref);
          if (s->suff)
            dprintf ("      suffix = \"%s\"\n", s->suff);
          dprintf ("    }\n");
        }
      dprintf ("  }\n");
    }
  else
    {
      dprintf ("* normal mode\n");
      dprintf ("    ");
      printd_arr (opt.global_seeds->cseed, "`%c`", opt.global_seeds->cseed_len);
      dprintf ("    ");
      printd_arr (opt.global_seeds->wseed, "`%s`", (int) da_sizeof (opt.global_seeds->wseed));
      dprintf ("* depth: from %d to %d\n", opt.from_depth, opt.to_depth);
    }
  if (opt.escape_disabled)
    dprintf ("- backslash interpretation is disabled\n");
  if (opt.separator)
    dprintf ("* delimiter: `%s`\n", opt.separator);
  dprintf ("* permutations:\n");


  /* Generating permutations */
  if (opt._regular_mode > 0)
    {
      regular_perm (&opt);
    }
  else
    {
      int rw_err = 0;
      for (int d = opt.from_depth; d <= opt.to_depth; ++d)
        {
          if (0 != (rw_err = perm (d, &opt)))
            break;
        }
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
 *  This function uses special_tk and special_src for parsing
 */
static inline void
pparse_cseed_regex (struct Opt *, struct Seed *dst_seed);
/**
 *  Detects file paths, previous seed indexes (\N)
 *  and other shortcuts like \d, \u, and \U
 *  This function uses special_tk and special_src for parsing
 */
static inline void
pparse_keys_regex (struct Opt *opt, struct Seed *dst_seed,
                   const char *input);
/**
 *  Provides prefix and suffix of @dst_seed using strdup,
 *  inside: `(...)` (only in regular mode)
 *  The first call sets @dst_seed->pref (prefix) and
 *  the second call sets @dst_seed->suff (suffix)
 */
static inline void
pparse_format_regex (struct Opt *, struct Seed *dst_seed,
                     char *input);

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
  unescape (tmp);

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
  int lastc = 0, dash = 0;
  Milexer_Token *tmp = &opt->parser.special_tk;

  for (int _ret = 0; !NEXT_SHOULD_END (_ret); )
    {
      /* parsing with IGSPACE to allow space character */
      _ret = ml_next (opt->parser.ml_in,
                      &opt->parser.special_src,
                      tmp,
                      PFLAG_IGSPACE);
      if (NEXT_SHOULD_LOAD (_ret))
        break;

      if (!opt->escape_disabled)
        unescape (tmp->cstr);
      if (tmp->type == TK_PUNCS && tmp->id == PUNC_DASH)
        {
          dash++;
        }
      else if (tmp->type == TK_KEYWORD)
        {
          char *p = tmp->cstr;
          if (dash == 0)
            {
              /**
               *  Case: a simple section with no dash involved
               *  This appends characters of @p to dst_seed->cseed
               */
            simple_section:
              if (*p)
                {
                  if (!opt->escape_disabled)
                    unescape (p);
                  int len = cseed_uniappd (dst_seed, p, -1);
                  lastc = p[len];
                }
            }
          else /* dash >= 1 */
            {
              if (*tmp->cstr && lastc)
                {
                  /**
                   *  Case: range of characters
                   *  from @lastc, to @tmp->cstr[0]
                   */
                  if (lastc < *tmp->cstr)
                    {
                      /* valid range, append the range */
                      p = tmp->cstr + 1;
                      for (char c = lastc; c <= *tmp->cstr; ++c)
                        cseed_uniappd (dst_seed, &c, 1);
                    }
                  else /* invalid range, treat as a simple section */
                    p = tmp->cstr;
                  dash--;
                  lastc = 0;
                  /* the rest of tmp->cstr is a simple section */
                  goto simple_section;
                }
              else
                {
                  /* Extra dash and then, a simple section */
                  p = tmp->cstr;
                  goto simple_section;
                }
            }
        }
    }
  /* Extra dash means the dash character itself */
  if (dash > 0)
    cseed_uniappd (dst_seed, "-", 1);
}

static inline void
pparse_wseed_regex (struct Opt *opt, struct Seed *dst_seed)
{
  Milexer_Token *tmp = &opt->parser.special_tk;
  for (int _ret = 0; !NEXT_SHOULD_END (_ret); )
    {
      _ret = ml_next (opt->parser.ml_in,
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
pparse_format_regex (struct Opt *opt, struct Seed *dst_seed,
                     char *input)
{
  if (!input)
    return;
  if (!opt->escape_disabled)
    unescape (input);

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
        if (opt->_regular_mode && IS_NUMBER (*input))
          {
            int n = strtol (input, NULL, 10) - 1;
            if (n >= 0 && n < opt->_regular_mode - 1)
              {
                /* valid index */
                struct Seed *_src = opt->reg_seeds[n];
                /* append csseds */
                cseed_uniappd (dst_seed,
                               _src->cseed, _src->cseed_len);
                /* append wseeds */
                for (ssize_t i=0; i < da_sizeof(_src->wseed); ++i)
                  {
                    wseed_uniappd (opt, dst_seed, _src->wseed[i]);
                  }
              }
            else
              {
                /* Invalid index */
                if (n == opt->_regular_mode - 1)
                  warnln ("circular append was ignored");
                else if (n >= opt->_regular_mode)
                  warnln ("seed index %d is out of bound", n+1);
                else if (n < 0)
                  warnln ("invalid seed index");
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
  Milexer_Token *tmp = &opt->parser.general_tk;
  /* Mini-lexer internal initialization */
  SET_ML_SLICE (&opt->parser.general_src,
                input, strlen (input));

  int ret = 0;
  while (!NEXT_SHOULD_END (ret))
    {
      ret = ml_next (opt->parser.ml,
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
              /* Parsing contents of `[xxx]` as cssed */
              pparse_cseed_regex (opt, dst_seed);
              break;

            case EXP_CBRACE:
              /* Parsing contents of `{xxx}` as wseed */
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
