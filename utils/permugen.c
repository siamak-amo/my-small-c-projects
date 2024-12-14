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

/**
 *  file: permugen.c
 *  created on: 4 Sep 2024
 *
 *  Permugen, permutation generator utility
 *  to generate customizable permutations of given seeds
 *
 *  Usage:  ./permugen [OPTIONS] [ARGUMENTS]
 *          ./permugen -r [seed_1] ... [seed_N] [OPTIONS] [ARGUMENTS]
 *    see help function by `-h` option, for more details:
 *    $ ./permugen -h
 *
 *  Some usage examples:
 *   Normal permutation:
 *    - Alphanumeric permutations
 *    ./permugen                      # a-z and 0-9 of length 3
 *    ./permugen -s "\d"              # only digits 0-9
 *    ./permugen -s "\d \u"           # 0-9 and A-Z
 *    ./permugen -s "\d \u \l"        # 0-9 and A-Z and a-z
 *
 *    - To make permutations of A,B,C, and a,...,f
 *    ./permugen -s "[ABC] [a-f]" -d4             # of length 4
 *    ./permugen -s "[ABCa-f]" -d4                # equivalent
 *    ./permugen -s "[ABCa-f]" -D4                # depth range 1,...,4
 *    ./permugen --min-depth 3 --max-depth 5      # depth range 3,...,5
 *
 *    - To include word(s) in permutations
 *    ./permugen -s "{foo,bar}"
 *    ./permugen -s "[0-4] {foo,bar,baz}"         # to also include 0,...,4
 *    ./permugen -s "[xyz0-4] {foo,bar,baz}"      # to also include x,y,z
 *    ./permugen -s "[0-4] [x-z] {foo,bar,baz}"   # equivalent
 *
 *    - To include words from file
 *    ./permugen -s "/path/to/file"               # or use -S
 *    ./permugen -s "-"                           # read from stdin
 *    # include 0 to 5, ABC and also read from stdin
 *    ./permugen -s "[0-5] - {ABC}"
 *    ./permugen -s "[0-5] {ABC} /path/to/file"   # read from file
 *    ./permugen -s "- [0-5]{ABC} /path/to/file"  # also read from stdin
 *
 *    - Output formatting (separator `-p` and format `-f`)
 *      to disable backslash interpretation (default) use `-E`
 *    ./permugen --delim ", "                     # comma separated
 *    ./permugen --delim "\t"                     # tab separated
 *    # using `www.` as prefix and `.com` as suffix
 *    # you may use --prefix, --suffix for more control
 *    ./permugen --format "www. .com"
 *
 *   Regular permutation:
 *    Argument(s) of `-r` are the same as `-s`
 *    - First component: [0-2]  and  second component: AA,BB
 *    ./permugen -r "[0-2]" "{AA,BB}"
 *    - First component: dev,prod,www  and  second and third: [0-9]
 *    ./permugen -r "{dev,prod,www}" "[0-9]" "[0-9]"
 *
 *    To Reuse previously provided seeds (\n where n>=1)
 *    - Equivalent to the previous example
 *    ./permugen -r "{dev,prod,www}" "[0-9]" "\2"
 *    - First component: dev,prod  and  second component also has www
 *    ./permugen -r "{dev,prod}" "{www} \1"
 *
 *    - First component: dev,prod  and  second component: from file
 *    ./permugen -r "{dev,prod}" /path/to/wordlist
 *    ./permugen -r -- "{dev,prod}" "-"           # read from stdin
 *
 *
 *  Compilation:
 *    to compile with `buffered_io.h`:
 *      cc -ggdb -O3 -Wall -Wextra -Werror \
 *         -D_USE_BIO -I../libs \
 *         -o permugen permugen.c
 *
 *  Options:
 *    - To compile without buffered IO (less performance):
 *        remove `-D_USE_BIO`
 *    - To change the default buffered IO buffer length:
 *        define `_BMAX="1024 * 1"`  (in bytes)
 *
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>

static const char *__PROGNAME__ = "permugen";
static const char *__PROGVERSION__ = "v2.2";

/**
 *  using buffered_io.h for better performance
 *  this file should be available in ../libs
 */
#ifdef _USE_BIO
#  ifndef _BMAX
#    define _BMAX (sysconf (_SC_PAGESIZE) / 2) // 2kb
#  endif
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif

/* for backslash interpretation */
#define UNESCAPE_IMPLEMENTATION
#include "unescape.h"

/* for dynamic array */
#define DYNA_IMPLEMENTATION
#include "dyna.h"

/* default permutaiton depth (normal mode) */
#define DEF_DEPTH 3

#undef STR
#define STR(var) #var

#ifdef _DEBUG
#undef dprintf
#define dprintf(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
/* use printd_arr */
#  define printd_arr__H(arr, T, len, sep, end)  \
  for (int __idx = 0; __idx < len; __idx++) {   \
    dprintf (T"%s", arr[__idx],                 \
             (__idx < len-1) ? sep : end);      \
  }
/**
 * debug printf for arrays
 * pass printf format for each element of @arr with @T
 * Ex: to print `int arr[7]`  -->  printd_arr (arr, "%d", 7);
 */
#  define printd_arr(arr, T, len)               \
  if (len > 0 && arr) {                         \
    dprintf (#arr"[.%d] = {", len);             \
    printd_arr__H (arr, T, len, ", ", "}\n");   \
  } else {                                      \
    dprintf (#arr" is empty\n");                \
  }
#else
#  undef dprintf
#  define dprintf(format, ...) (void)(format)
#  define printd_arr(arr, T, len) (void)(arr)
#endif /* _DEBUG */

#undef warnf
#define warnf(format, ...) \
  fprintf (stderr, "%s: "format"\n", __PROGNAME__, ##__VA_ARGS__)


struct char_seed
{
  const char *c;
  int len;
};
static const struct char_seed charseed_az = {"abcdefghijklmnopqrstuvwxyz", 26};
static const struct char_seed charseed_AZ = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
static const struct char_seed charseed_09 = {"0123456789", 10};

/**
 *  seeds container
 *  wseed within this struct is dynamic array
 *  only use the following functions and macros
 *  and `xxx_uniappd` and `parse_seed_regex`
 *  to manipulate this type
 *  to get the length of wseed, use `da_sizeof`
 */
struct Seed
{
  /* char seed */
  char *cseed;
  int cseed_len;
  /* word seed, dynamic array */
  char **wseed;
};
#define CSEED_MAXLEN 256

/* to make new seed and dynamic seed array */
static inline struct Seed * mk_seed (int c_len, int w_len);
static inline void free_seed (struct Seed *s);
static inline struct Seed * seeddup (const struct Seed *s);
#define mk_seed_arr(n) da_newn (struct Seed *, n)
#define drop_seeds(seed_ptr) do {               \
    seed_ptr->cseed_len = 0;                    \
    da_drop (seed_ptr->wseed);                  \
  } while (0)

/**
 *  the main configuration of permugen
 *  reg_seeds is a dynamic array of (Seed *)
 */
struct Opt
{
  /* main seed configuration */
  struct Seed *global_seeds;

  /* regular permutation */
  int _regular_mode;
  struct Seed **reg_seeds; /* dynamic array */

  /* output format */
  int escape_disabled; /* to disable backslash interpretation */
  char *__pref; /* prefix */
  char *__suff; /* suffix */
  char *__sep; /* separator */

  /* output conf */
  FILE *outf; /* output file */
  int from_depth; /* min depth */
  int to_depth; /* max depth */

  /* buffered_io */
#ifdef _USE_BIO
  BIO_t *bio;
#endif
};

/**
 *  these functions only need opt pointer to know
 *  whether backslash interpretation is disabled or not
 */
/* unique append to seed functions */
int cseed_uniappd (struct Seed *, const char *src, int len);
void wseed_uniappd (const struct Opt *, struct Seed *,
                    char *str_word);
/* simple seed regex handler function */
void parse_seed_regex (const struct Opt *, struct Seed *,
                       const char *str_regex);

#define _strcmp(s1, s2)                         \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

/* is non white-space ascii-printable */
#define IS_ASCII_PR(c) (c >= 0x21 && c <= 0x7E)
#define IS_NUMBER(c) (c >= '0' && c <= '9')

void
usage ()
{
  fprintf (stdout,
           "Permugen %s, permutation generator utility\n\n"
           "Usage:\n"
           "   normal mode: any possible permutation of given seed(s)\n"
           "       permugen [OPTIONS] [ARGUMENTS]\n\n"
           "  regular mode: to specify seed(s) of each component manually\n"
           "    generated permutations will have exactly N components\n"
           "       permugen -r [SEED 1] ... [SEED N] [OPTIONS]\n"
           "       permugen [OPTIONS] -r -- [SEED 1] ... [SEED N]\n"
           "\n"
           "OPTIONS:\n"
           "  Common options:\n"
           "      -E                      disable backslash interpretation\n"
           "      -e                      enable backslash interpretation (default)\n"
           "      -r, --regular           regular mode\n"
           "      -o, --output            output file\n"
           "  -a,-oA, --append            append to file\n"
           "      -p, --delimiter         permutations component separator\n"
           "      -f, --format            output format (see ARGUMENTS)\n"
           "          --prefix            output prefix\n"
           "          --suffix            output suffix\n"
           "\n"
           "  Only in normal mode:\n"
           "      -d, --depth             specify depth\n"
           "      -D, --depth-range       depth range\n"
           "     -df, --depth-from        specify min depth\n"
           "          --min-depth\n"
           "     -dt, --depth-to          specify max depth\n"
           "          --max-depth\n"
           "      -S, --seed-path         word seed path\n"
           "                              pass - to read from stdin\n"
           "      -s, --seed              to configure global seeds (see ARGUMENTS)\n"
           "          --raw-seed          to configure character seeds\n"
           "          --raw-wseed         to add a single word to global seeds\n"
           "\n"
           "ARGUMENTS:\n"
           "  Argument values of --format, --prefix, --suffix, --raw-xxx, and --delimiter\n"
           "  will be backslash-interpreted by default (disable it by `-E`)\n"
           "\n"
           "  Seed: argument value of `-s, --seed` and `-r, --regular`\n"
           "        accepts any combination of the following patterns\n"
           "    `{word1,word2}`   to include 'word1' and 'word2'\n"
           "    `[XYZ]`:          to include characters X,Y,Z\n"
           "    `[a-f]`:          to include character range a,...,f\n"
           "    `\\N`:             to reuse (append) previous seeds, only in regular mode\n"
           "                      where `N` is the index of a prior given seed, staring from 1\n"
           "    character range shortcuts:\n"
           "      '\\d' for [0-9],  '\\l','\\a' for [a-z],  '\\u','\\U','\\A' for [A-Z]\n"
           "    inside these regex's, you might also use:\n"
           "      '\\{ and \\['       for '{', '}' and '[', ']' characters\n"
           "      '\\, or \\x2c'      for comma, alternatively use --raw-xxx in normal mode\n"
           "      '\\xNN or \\0HHH'   hex and octal byte, for example: \\x5c for backslash\n"
           "                        see the raw section for more details\n"
           "    `-`:              to read seeds from stdin, it will continue reading\n"
           "                      until an empty line, and then the word `EOF`\n"
           "                      using Ctrl-D in regular mode, breaks the other dash options\n"
           "    `/path/to/file`:  to read words from a file (line by line)\n"
           "                      lines with '#' will be ignored\n"
           "    Example:\n"
           "      to include a,b and 0,...,9 and also words `foo` and `bar`:\n"
           "       \'[ab0-9] {foo,bar}\'  or equivalently  \'[ab] {foo,bar} [0-9]\'\n"
           "      to also include words from wordlist.txt:\n"
           "       \'[ab0-9] {foo,bar} /path/to/wordlist.txt\'\n"
           "      to also read from stdin:\n"
           "       \'- [ab0-9] {foo,bar} ~/wordlist.txt\'\n"
           "\n"
           "  Format: argument value of the common options `-f, --format`\n"
           "    \'AAA\':     to use AAA as the output prefix\n"
           "    \'AAA BBB\'  to use AAA as the prefix and BBB the as suffix\n"
           "    \' BBB\'     to use BBB as the output suffix\n"
           "               BBB might contain white-space character(s)\n"
           "    to have white-space in AAA, either use `\\x20` or --prefix and --suffix\n"
           "\n"
           "  Raw: backslash interpretation usage\n"
           "       \\\\:  to pass a single `\\`\n"
           "            some shells might eliminate them, so it would be more convenient\n"
           "            to use this inside single quotes instead of double quotes\n"
           "       \\x:  for \\t, \\v, \\r, \\a, \\b, \\f, \\n \n"
           "     \\xHH:  byte with hexadecimal value HH (1 to 2 digits)\n"
           "    \\0NNN:  byte with octal value NNN (1 to 3 digits)\n"
           ,__PROGVERSION__);
}

/**
 *  output of characters and strings
 *  these functions write on the corresponding
 *  output stream given in @opt->outf
 *
 *  Pfputs:  writes @str without its terminating null byte
 *  Pputs:   writes @str like @Pfputs and a trailing newline
 *  Pputc:   writes a character @c (as unsigned char)
 *  Pputln:  writes a newline
 */
#ifndef _USE_BIO
#  define Pfputs(str, opt) fputs (str, opt->outf)
#  define Pfputc(c, opt) putc (c, opt->outf)
#  define Pputln(opt) Pfputc ('\n', opt)
#  define Pputs(str, opt) (Pfputs (str, opt), Pputln(opt))
#else
#  define Pfputs(str, opt) bio_fputs (opt->bio, str)
#  define Pfputc(c, opt) bio_putc (opt->bio, c)
#  define Pputln(opt) bio_ln (opt->bio);
#  define Pputs(str, opt) bio_puts (opt->bio, str)
#endif

/**
 *  permutation generator main logic
 *  you need to call it in a loop from
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
  if (opt->__pref)
    Pfputs (opt->__pref, opt);
 Print_Loop: /* O(depth) */
  {
    int idx = idxs[i];
    if (idx < opt->global_seeds->cseed_len)
      {
        /* range of character seeds */
        Pfputc (opt->global_seeds->cseed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= opt->global_seeds->cseed_len;
        Pfputs (opt->global_seeds->wseed[idx], opt);
      }
    i++;
  }
  if (i < depth)
    {
      if (opt->__sep)
        Pfputs (opt->__sep, opt);
      goto Print_Loop;
    }
  /* End of Printing the current permutation */
  if (opt->__suff)
    Pputs (opt->__suff, opt);
  else
    Pputln (opt);


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
#endif
    }

  idxs[pos]++;
  goto Perm_Loop;
}

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
  if (opt->__pref)
    Pfputs (opt->__pref, opt);

 Print_Loop: /* O(S_i) */
  {
    int idx = idxs[i];
    struct Seed *current_seed = s[i];
    if (idx < current_seed->cseed_len)
      {
        /* range of character seeds */
        Pfputc (current_seed->cseed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= current_seed->cseed_len;
        Pfputs (current_seed->wseed[idx], opt);
      }
    i++;
  }
  if (i < depth)
    {
      if (opt->__sep)
        Pfputs (opt->__sep, opt);
      goto Print_Loop;
    }
  /* End of Printing the current permutation */
  if (opt->__suff)
    Pputs (opt->__suff, opt);
  else
    Pputln (opt);

  int pos;
  for (pos = depth-1;
       pos >= 0 && idxs[pos] == depths[pos];
       --pos)
    {
      idxs[pos] = 0;
    }

  if (pos < 0) /* end of permutations */
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
#endif
    }

  idxs[pos]++;
  goto Reg_Perm_Loop;

 Reg_Return:
  free (idxs);
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
  free (depths);
  return ret;
}

/**
 *  uniquely appends char(s) from @src to @dest
 *  until reaches \0 or non-printable characters
 *  @dest with *CAPACITY* 256 is always enough
 *  time: O(src_len * dest_len);
 *  updates @dest_len and returns number of bytes written
 */
int
cseed_uniappd (struct Seed *s, const char *src, int len)
{
  int rw = 0;
  while (len > 0 && *src)
    {
      if (*src == '\0')
        break;
      if (!IS_ASCII_PR (*src))
        goto END_OF_LOOP;

      for (int __i = s->cseed_len - 1; __i >= 0; __i--)
        {
          if (*src == s->cseed[__i])
            goto END_OF_LOOP;
        }
      s->cseed[s->cseed_len] = *src;
      s->cseed_len++;
      rw++;

    END_OF_LOOP:
      src++;
      len--;
    }

  return rw;
}

/**
 *  uniquely appends reference of @word
 *  to @opt->wseed (dynamic array)
 *  use strdup when @word gets dereferenced
 */
void
wseed_uniappd (const struct Opt *opt,
               struct Seed *s, char *str_word)
{
  if (!s->wseed || !str_word)
    return;
  for (da_idx i=0; i < da_sizeof (s->wseed); ++i)
    {
      if (_strcmp (s->wseed[i], str_word))
        return;
    }

  if (!opt->escape_disabled)
    unescape (str_word);
  da_appd (s->wseed, str_word);
}

void
wseed_fileappd (const struct Opt *opt, struct Seed *s, FILE *f)
{
  size_t __len;
  char *__line = NULL;
  int empty_prevline = 0;

  if (!f || ferror (f) || feof (f))
    {
      warnf ("reading from file failed -- file is not usable");
      return;
    }
  if (f == stdin && isatty (fileno (f)))
    fprintf (stderr, "Reading words until EOF:\n");

  while (1)
    {
      if (getline (&__line, &__len, f) < 0)
        break;
      if (__line && *__line &&
          __line[0] != '#') // commented line
        {
          int idx = strlen (__line) - 1;
          for (; idx >= 0; --idx)
            {
              /* remove non-printable characters */
              if (__line[idx] > 0 && __line[idx] < 0x20)
                __line[idx] = '\0';
              else
                break;
            }
          if (idx >= 0)
            {
              if (empty_prevline && _strcmp (__line, "EOF"))
                break;
              wseed_uniappd (opt, s, strdup (__line));
            }
          else
            empty_prevline = 1;
        }
    }
  if (__line)
    free (__line);
}

/**
 *  safe file open (fopen)
 *  only if it could open @pathname changes @dest[0]
 *  @mode is the same as fopen mode
 */
void
safe_fopen (FILE **dest,
            const char *restrict pathname,
            const char *restrict mode)
{
  FILE *__tmp;
  if (!pathname || !mode)
    {
      warnf ("invalud filename");
      return;
    }
  if (!(__tmp = fopen (pathname, mode)))
    {
      warnf ("fould not open file -- (%s:%s)", mode, pathname);
      return;
    }
  if (ferror (__tmp) || feof (__tmp))
    {
      warnf ("file is not usable -- (%s)", pathname);
      return;
    }
  *dest = __tmp;
}

/* CLI options, getopt */
const struct option lopts[] = {
  /* seeds */
  {"seed", required_argument, NULL, 's'},
  {"raw-seed", required_argument, NULL, '0'},
  {"raw-wseed", required_argument, NULL, '5'},
  {"seed-path", required_argument, NULL, 'S'},
  {"wseed-path", required_argument, NULL, 'S'},
  /* output file */
  {"output", required_argument, NULL, 'o'},
  {"append", required_argument, NULL, 'a'},
  {"oA", required_argument, NULL, 'a'},
  {"help", no_argument, NULL, 'h'},
  /* delimiter */
  {"delim", required_argument, NULL, 'p'},
  {"delimiter", required_argument, NULL, 'p'},
  /* depth */
  {"depth", required_argument, NULL, 'd'},
  {"depth-range", required_argument, NULL, 'D'},
  {"range-depth", required_argument, NULL, 'D'},
  {"range", required_argument, NULL, 'D'},
  {"df", required_argument, NULL, '1'},
  {"depth-from", required_argument, NULL, '1'},
  {"from-depth", required_argument, NULL, '1'},
  {"min-depth", required_argument, NULL, '1'},
  {"dt", required_argument, NULL, '2'},
  {"depth-to", required_argument, NULL, '2'},
  {"to-depth", required_argument, NULL, '2'},
  {"max-depth", required_argument, NULL, '2'},
  /* format */
  {"format", required_argument, NULL, 'f'},
  {"pref", required_argument, NULL, '3'},
  {"prefix", required_argument, NULL, '3'},
  {"suff", required_argument, NULL, '4'},
  {"suffix", required_argument, NULL, '4'},
  {"regular", no_argument, NULL, 'r'},

  /* end of options */
  {NULL, 0, NULL, 0}
};

int
init_opt (int argc, char **argv, struct Opt *opt)
{
#define CASE_NOT_IN_REG_MODE(option) \
  NOT_IN_REG_MODE (option, break)
#define NOT_IN_REG_MODE(option, action)                                 \
  if (opt->_regular_mode) {                                             \
    warnf ("wrong regular mode option (%s) was ignored", option);       \
    action;                                                             \
  }

  int idx = 0, flag, using_default_seed = 1;
  while (1)
    {
      /* we use 0,1,2,... as `helper` options and only to use getopt */
      flag = getopt_long (argc, argv,
                          "s:S:o:a:p:d:f:D:0:1:2:3:4:5:hrEe", lopts, &idx);
        if (flag == -1)
          {
            /* End of Options */
            break;
          }

      switch (flag)
        {
        case 'h':
          usage ();
          return 1;
        case 'E':
          opt->escape_disabled = 1;
          break;
        case 'e':
          opt->escape_disabled = 0;
          break;
        case 'o': /* outout */
          safe_fopen (&opt->outf, optarg, "w");
          break;
        case 'a': /* append */
          safe_fopen (&opt->outf, optarg, "a");
          break;
        case 'd': /* depth */
          opt->from_depth = atoi (optarg);
          break;
        case 'D': /* depth range */
          opt->from_depth = 1;
          opt->to_depth = atoi (optarg);
          break;
        case 'p': /* delimiter */
          opt->__sep = optarg;
          break;
        case '3': /* prefix */
          opt->__pref = optarg;
          break;
        case '4': /* suffix */
          opt->__suff = optarg;
          break;
        case '1': /* depth from */
          opt->from_depth = atoi (optarg);
          break;
        case '2': /* depth to */
          opt->to_depth = atoi (optarg);
          break;
        case 'f': /* format */
          {
            opt->__pref = optarg;
            for (char *p = optarg; *p != '\0'; ++p)
              {
                if (*p == ' ')
                  {
                    *(p++) = '\0';
                    if (*p != '\0')
                      opt->__suff = p;
                    break;
                  }
              }
            if (opt->__pref && opt->__pref[0] == '\0')
              opt->__pref = NULL;
            break;
          }
        case 'S': /* wseed file / stdin */
          {
            CASE_NOT_IN_REG_MODE(argv[optind-2]);
            FILE *wseed_f = stdin;
            /* using optarg value as filepath otherwise stdin */
            if (!_strcmp (optarg, "-"))
              safe_fopen (&wseed_f, optarg, "r");

            /* read from file and append to wseed */
            wseed_fileappd (opt, opt->global_seeds, wseed_f);

            if (wseed_f != stdin)
              fclose (wseed_f);
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
          CASE_NOT_IN_REG_MODE(argv[optind-2]);
          using_default_seed = 0;
          if (!opt->escape_disabled)
            unescape (optarg);
          cseed_uniappd (opt->global_seeds, optarg, strlen (optarg));
          break;

        case '5': /* raw word seed */
          CASE_NOT_IN_REG_MODE(argv[optind-2]);
          if (!opt->escape_disabled)
            unescape (optarg);
          wseed_uniappd (opt, opt->global_seeds, optarg);
          break;

        case 'r': /* regular mode */
          struct Seed *tmp = mk_seed (CSEED_MAXLEN, 1);
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
                  drop_seeds (tmp);
                  optind++;
                  parse_seed_regex (opt, tmp, argv[i]);
                  if (tmp->cseed_len == 0 && da_sizeof (tmp->wseed) == 0)
                    {
                      warnf ("empty regular seed configuration was ignored");
                    }
                  else
                    {
                      opt->_regular_mode++;
                      da_appd (opt->reg_seeds, seeddup (tmp));
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
   *  when they are not specified by the user
   */
  if (opt->outf == NULL)
    opt->outf = stdout;

  if (opt->_regular_mode > 0)
    { /* regular mode */
    }
  else
  { /* normal mode */
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

  /* interpreting backslash character(s) */
  if (!opt->escape_disabled)
    {
      if (opt->__pref != NULL)
        unescape (opt->__pref);
      if (opt->__suff != NULL)
        unescape (opt->__suff);
      if (opt->__sep != NULL)
        unescape (opt->__sep);
    }

  return 0;
}

static inline struct Seed *
seeddup (const struct Seed *s)
{
  int clen = s->cseed_len;
  int wlen = da_sizeof (s->wseed);

  struct Seed *res = mk_seed (clen, wlen);
  res->cseed_len = clen;
  memcpy (res->cseed, s->cseed, clen);
  res->wseed = da_dup (s->wseed);

  return res;
}

static inline void
free_seed (struct Seed *s)
{
  if (!s)
    return;
  if (s->cseed)
    free (s->cseed);
  if (s->wseed)
    da_free (s->wseed);
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
    struct Opt opt = {0};

  { /* initializing options */
    opt.global_seeds = mk_seed (CSEED_MAXLEN, 1);
    if (init_opt (argc, argv, &opt))
      goto EndOfMain;

    if (opt._regular_mode > 0)
      {
        /* _regular_mode = 1 + length of seeds */
        if (opt._regular_mode == 1)
          {
            warnf ("empty regular permutation");
            goto EndOfMain;
          }
      }
    else /* normal mode */
      if (opt.global_seeds->cseed_len == 0 &&
             da_sizeof (opt.global_seeds->wseed) == 0)
      {
        warnf ("empty permutation");
        goto EndOfMain;
      }
  }

#ifdef _USE_BIO
  int cap = _BMAX;
  BIO_t __bio = bio_new (cap, malloc (cap), fileno (opt.outf));
  opt.bio = &__bio;
  dprintf ("* buffer length of buffered_io: %ld bytes\n", _BMAX);
#else
  dprintf ("- compiled without buffered_io\n");
# endif /* _USE_BIO */

  /* print some debug information */
  if (opt._regular_mode)
    {
      da_idx len = da_sizeof (opt.reg_seeds);
      dprintf ("* regular mode\n");
      dprintf ("* %s[.%lu] = {\n", STR (opt.reg_seeds), (size_t)len);
      for (da_idx i=0; i < len; ++i)
        {
          struct Seed *s = opt.reg_seeds[i];
          dprintf ("    %s[%d] = {\n      ", STR (opt.reg_seeds), i);
          printd_arr (s->cseed, "`%c`", s->cseed_len);
          dprintf ("      ");
          printd_arr (s->wseed, "`%s`", (int)da_sizeof (s->wseed));
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
  if (opt.__sep)
    dprintf ("* delimiter: `%s`\n", opt.__sep);
  dprintf ("* permutations:\n");


  /* the main logic of making permutations */
  if (opt._regular_mode > 0)
    {
      regular_perm (&opt);
    }
  else
    {
      int rw_err = 0;
      for (int d = opt.from_depth; d <= opt.to_depth; ++d)
        {
          if ((rw_err = perm (d, &opt)) != 0)
            break;
        }
    }

#ifdef _USE_BIO
  bio_flush (opt.bio);
  free (opt.bio->buffer);
#endif


 EndOfMain:
  {
    /**
     *  global_seed itself is not a dynamic array
     *  and is allocated using malloc
     */
    free_seed (opt.global_seeds);
    free (opt.global_seeds);

    /**
     *  reg_seeds is a dynamic array
     *  and each seed of it must be freed
     *  also they are allocated with `seeddup`
     */
    if (opt.reg_seeds)
      {
        for (da_idx i=0; i < da_sizeof (opt.reg_seeds); ++i)
          {
            free_seed (opt.reg_seeds[i]);
            free (opt.reg_seeds[i]);
          }
        da_free (opt.reg_seeds);
      }
  }
  /* close any non-stdout file descriptors */
  if (opt.outf && fileno (opt.outf) != 1)
    fclose (opt.outf);

  return 0;
}


/**
 *  internal regex functions
 *  these functions parse argument of `-s` and `-r`
 *  which is a simple regex to configure seeds
 */

/** wseed regex parser
 *  inside `{...}` - comma-separated values
 *  it backslash interprets them when not disabled
 *  comma is not allowed in wseeds, use \x2c
 *  returns pointer to the end of regex
 */
const char *
pparse_wseed_regex (const struct Opt *opt,
                    struct Seed *s, const char *p)
{
  const char *next_p = p + 1;
  const char *start = p;
  size_t len = 0;

#define __forward(n) (p += n, next_p += n)
#define seedout() do {                          \
    char *str = malloc (++len);                 \
    memcpy (str, start, len);                   \
    str[len - 1] = '\0';                        \
    wseed_uniappd (opt, s, str);                \
  } while (0)

  for (char prev_p = *p; *p != '\0'; prev_p = *p, __forward (1))
    {
      switch (*p)
        {
        case '\0':
          return p;

        case '}':
          if (prev_p == '\\')
            goto Wcontinue;
          if (len > 0)
            seedout ();
          return p+1;

        case ',':
          if (prev_p == '\\')
            goto Wcontinue;
          if (len > 0)
            seedout ();
          start = p+1, len = 0;
          break;

        default:
        Wcontinue:
          len++;
        }
    }
  return p;
#undef __forward
#undef seedout
}

/** character seed regex parser
 *  inside `[...]`
 *  it does not backslash interpret @p
 *  returns pointer to the end of regex
 */
const char *
pparse_cseed_regex (struct Seed *s, const char *p)
{
  const char *next_p = p + 1;
  static char seed = 0;

#define __forward(n) (p += n, next_p += n)
#define seedout() cseed_uniappd (s, &seed, 1)

  for (; *p != '\0'; __forward (1))
    {
      switch (*p)
        {
        case '\0':
          /* End of Regex */
          return p;

        case '\\':
          if (*next_p == ']' || *next_p == '[')
            {
              seed = *next_p;
              __forward (1);
              seedout ();
            }
          break;

        case ']':
          return p;

        case '-':
          if (*(p - 1) != '[')
            {
              if (*next_p != ']' && *next_p != '\\')
                {
                  for (seed = *(p - 1); seed <= *next_p; ++seed)
                    {
                      seedout ();
                    }
                  __forward (1);
                }
              else
                {
                  seed = *p;
                  seedout ();
                }
            }
          else
            {
              seed = *p;
              seedout ();
            }
          break;

        default:
          {
            switch (*next_p)
              {
              case '-':
                break;

              case '\0':
              case ']':
              default:
                seed = *p;
                seedout ();
              }
          }
        }
    }
  return p;
#undef __forward
#undef seedout
}

/**
 *  resolves: `~/` , `/../` , `/./`
 *  path is not necessarily null-terminated
 */
static char *
path_resolution (const char *path, size_t len)
{
  static char PATH[PATH_MAX];
  char *tmp;
  if (*path == '~')
    {
      const char* home = getenv ("HOME");
      if (!home)
        return NULL;
      tmp = malloc (len + strlen (home) + 1);
      char *p;
      p = mempcpy (tmp, home, strlen (home));
      p = mempcpy (p, path + 1, len - 1);
      *p = '\0';
    }
  else
    {
      tmp = malloc (len + 1);
      *((char *) mempcpy (tmp, path, len)) = '\0';
    }

  /* interpret `\<space>` */
  unescape (tmp);

  if (realpath (tmp, PATH))
    {
      free (tmp);
      return PATH;
    }
  else
    {
      free (tmp);
      return NULL;
    }
}


  /**
   *  to parse @input regex and store the output in @s
   *  @input: " - [..\[..]  {..\{..} /path/to/file"
   *  supported file path formats:
   *    `/tmp/wl.txt`, `~/wl.txt`, `./wl.txt`, `../wl.txt`
   *  `-` in @input means to read from stdin and inside [] means range
   *  [...] is used for cseed and {...} for wseed
   */
void
parse_seed_regex (const struct Opt *opt,
                  struct Seed *s, const char *input)
{
  for (char prev_p = 0; *input != '\0'; prev_p = *input)
    {
      switch (*input)
        {
        case '\0':
          goto End_of_Parsing;

          /* shortcuts */
        case '\\':
          input++;
          /* check for \n where n is the index of a
           * previously provided seed */
          if (opt->_regular_mode && IS_NUMBER (*input))
            {
              char *p = NULL;
              int n = strtol (input, &p, 10) - 1;
              if (p)
                input = p;
              if (n >= 0 && n < opt->_regular_mode - 1)
                { /* valid index */
                  struct Seed *_src = opt->reg_seeds[n];
                  /* append csseds */
                  cseed_uniappd (s, _src->cseed, _src->cseed_len);
                  /* append wseeds */
                  for (da_idx i=0; i < da_sizeof(_src->wseed); ++i)
                    {
                      wseed_uniappd (opt, s, _src->wseed[i]);
                    }
                }
              else
                {
                  if (n == opt->_regular_mode - 1)
                    warnf ("circular append was ignored");
                  else if (n >= opt->_regular_mode)
                    warnf ("seed index %d is out of bound", n+1);
                  else if (n < 0)
                    warnf ("invalid seed index");
                }
              goto End_of_Shortcut_Parsing;
            }
          /* when n is not a number */
          switch (*input)
            {
            case 'd': /* digits 0-9 */
              cseed_uniappd (s, charseed_09.c, charseed_09.len);
              break;
            case 'l': /* lowercase letters */
            case 'a':
              cseed_uniappd (s, charseed_az.c, charseed_az.len);
              break;
            case 'U': /* uppercase letters */
            case 'u':
            case 'A':
              cseed_uniappd (s, charseed_AZ.c, charseed_AZ.len);
              break;

            default:
              warnf ("invalid shortcut \\%c was ignored", *input);
            }
        End_of_Shortcut_Parsing:
          input++;
          break;

          /* file path */
        case '.':
          if (input[1] == '/' ||
              (input[1] == '.' && input[2] == '/'))
            goto File_Path_Parsing;
          break;
        case '/':
        case '~':
          {
          File_Path_Parsing:
            const char *__input = input;
            size_t inplen = 0;
            for (; *__input != '\0'; __input++)
              {
                if (*__input == ' ' && *(__input-1) != '\\')
                  break;
                inplen++;
              }
            if (*__input != '\0')
              __input++;

            char *path;
            FILE *f = NULL;
            if (!(path = path_resolution (input, inplen)))
              goto End_of_File_Path;
            safe_fopen (&f, path, "r");
            if (f)
              {
                wseed_fileappd (opt, s, f);
                if (f != stdin)
                  fclose (f);
              }
          End_of_File_Path:
            input = __input; /* update the input pointer */
            break;
          }

        case '-': /* read from stdin */
          {
            wseed_fileappd (opt, s, stdin);
            input++;
            break;
          }

        case '[':
          if (prev_p != '\\')
            {
              input = pparse_cseed_regex (s, ++input);
            }
          break;

        case '{':
          if (prev_p != '\\')
            {
              input = pparse_wseed_regex (opt, s, ++input);
            }
          break;

        default:
          input++;
          continue;
        }
    }
 End_of_Parsing:
}
