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
 *  Usage:
 *    - Basic usage
 *    # permutations of [a-z] and [0-9], of depth 3
 *    $ ./permugen
 *
 *    # of depth 2
 *    $ ./permugen -d 2
 *
 *    # with a depth less than 4
 *    $ ./permugen -D 4
 *
 *    # with a depth between 2 and 5:
 *    $ ./permugen -fd 2 -td 5
 *
 *    - To configure seeds, these can be used together:
 *      `-s a`            ->  [a-z]
 *      `-s A`            ->  [A-Z]
 *      `-s n`            ->  [0-9]
 *      `-s sXYZ`         ->  [X,Y,Z]  (custom seed)
 *                            for non-whitespace characters
 *
 *    - To inject word(s) in permutations (word seed):
 *      `-s Wdev`         ->  `dev`
 *      `-s Wdev,test,m`  ->  `dev`, `test`, `m`
 *
 *    - Examples:
 *    # permutations of `-`, `.`, `test`, `dev`
 *    $ ./permugen -s "s-. Wtest,dev"
 *    # to also include numbers and [A-Z]
 *    $ ./permugen -s "s-. Wtest,dev nA"
 *
 *    - To read seed words from file, (`-S` option)
 *      When file path is `-`, it uses stdin
 *    # wlist.txt is a newline-separated word list
 *    # lines starting with `#` will be ignored
 *    $ ./permugen -S /path/to/wlist.txt
 *
 *    - Example:
 *    # to make permutations of a wordlist
 *    # `-s s` means no character seed
 *    $ ./permugen -s s -S /path/to/wlist.txt
 *    # to also include `-` and `_`
 *    $ ./permugen -s "s-_" -S /path/to/wlist.txt
 *
 *    # to get permutations of words from stdin:
 *    $ cat wlist.txt | ./permugen -S - -ss
 *    # to also include `AAA` and `-`
 *    $ cat wlist.txt | ./permugen -S - -ss- -sWAAA
 *
 *    - To put delimiter `xxx` between each component of
 *      the permutations (`-p`, `--delimiter` option),
 *      xxx might have white space:
 *      `-p xxx`          ->  [c1]xxx[c2]xxx[c3]
 *      `-p.`             ->  `c1.c2.c3`  (suitable for subdomains)
 *      `-p ', '`         ->  `c1, c2, c3`
 *
 *    - To add prefix and suffix to the output:
 *    $ ./permugen -f "www."         ->  www.xyz
 *    $ ./permugen -f "www. .com"    ->  www.xyz.com
 *    $ ./permugen -f " .com"        ->      xyz.com
 *    - if you need to use space character, either use
 *      `\x20` in the prefix or use --prefix:
 *      `--prefix "aaa"`             ->  aaa.xyz
 *      `--suffix "bbb"`             ->      xyz.bbb
 *      where `aaa`, `bbb` might have space character
 *
 *    - To write the output on a file: `-o`, `--output`
 *      for appending: `-oA`, `-a`, `--append`
 *
 *  Compilation:
 *    to compile with `buffered_io.h`:
 *      cc -ggdb -O3 -Wall -Wextra -Werror \
 *         -D_USE_BIO -I../libs \
 *         -o permugen permugen.c
 *
 *  Compilation options:
 *    - To compile without buffered IO (less performance)
 *      remove `-D_USE_BIO`
 *
 *    - define `-D_BMAX="1024 * 1"` (in bytes) to change
 *      the default buffered IO buffer length
 *
 **/
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/**
 *  using buffered_io.h for better performance
 *  this file should be available in this repo
 */
#ifdef _USE_BIO
#  ifndef _BMAX
#    define _BMAX (sysconf (_SC_PAGESIZE) / 2) // 2kb
#  endif
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif

/* for using unescape function */
#define UNESCAPE_IMPLEMENTATION
#include "unescape.h"

#ifdef _DEBUG
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
  if (len > 0) {                                \
    dprintf ("* "#arr"[.%d] = {", len);         \
    printd_arr__H (arr, T, len, ", ", "}\n");   \
  } else {                                      \
    dprintf ("- "#arr" is empty\n");            \
  }
#endif /* _DEBUG */


#define errorf(format, ...) \
  fprintf (stderr, format"\n", ##__VA_ARGS__)
#define argerr(arg, message) \
  errorf ("(%s) was ignored -- "message, arg)
// prints perror at the end
#define perrorf(format, ...) \
  (fprintf (stderr, format, ##__VA_ARGS__), perror(NULL))

struct seed_part {
  const char *c;
  int len;
};
static const struct seed_part AZ = {"abcdefghijklmnopqrstuvwxyz", 26};
static const struct seed_part AZCAP = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
static const struct seed_part NUMS = {"0123456789", 10};
static const char *__progname__;

struct Opt {
  /* char seed(s) */
  char *seed;
  int seed_len;
  /* word seed(s) */
  char **wseed;
  int wseed_len; /* count of word seeds (wseed) */
  int __wseed_l; /* internal, dynamic-array wseed */

  /* output format */
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

#define __seed_init(cap) {                      \
    opt->seed = malloc (cap);                   \
    __p = opt->seed;                            \
  }
#define _strcmp(s1, s2)                         \
  ((s1) != NULL && (s2) != NULL &&              \
   strncmp ((s1), (s2), strlen (s2)) == 0)

void
usage ()
{
  fprintf (stdout,
           "Permugen, permutation generator utility\n"
           "Usage: %s [OPTIONS]\n\n"
           "OPTIONS:\n"
           "      -d, --depth             specify depth\n"
           "      -D, --depth-range       depth range\n"
           "     -df, --depth-from        specify min depth\n"
           "     -dt, --depth-to          specify max depth\n"
           "      -o, --output            output file\n"
           "  -a,-oA, --append            append to file\n"
           "      -p, --delimiter         permutations component separator\n"
           "      -S, --seed-path         word seed path\n"
           "                              pass - to read from stdin\n"
           "      -s, --seed              specify character seeds\n"
           "      -f, --format            output format\n"
           "          --prefix            output prefix\n"
           "          --suffix            output suffix\n\n"
           "ARGUMENTS:\n"
           "  Argument values of --format, --prefix, --suffix and --delim (-p)\n"
           "  will be backslash-interpreted by default\n\n"
           "  format:\n"
           "          `AAA`:     to use AAA as the output prefix\n"
           "          `AAA BBB`  to use AAA as prefix and BBB as suffix\n"
           "          ` BBB`     to use BBB as suffix\n"
           "          BBB might contain white-space character(s)\n"
           "          to have white-space in AAA, either use `\\x20` or --prefix\n\n"
           "    seed: there are three types of seeds available\n"
           "          built-in:     `sa` ~ [a-z],  `sA` ~ [A-Z],  `sn` ~ [0-9]\n"
           "          custom seeds: `ssXYZ...` for X,Y,Z,... characters\n"
           "          word seeds:   `sWxxx,yyy,zzz` for words xxx, yyy, zzz\n",
           __progname__);
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

// this will duplicate @ptr and append it to @opt->wseed
void
wseed_append (struct Opt *opt, const char *ptr)
{
  if (opt->wseed_len >= opt->__wseed_l)
    {
      opt->__wseed_l = (opt->__wseed_l + 2) * 2;
      opt->wseed = realloc (opt->wseed,
                            opt->__wseed_l * sizeof (char *));
    }
  opt->wseed[opt->wseed_len++] = strdup (ptr);
}

void
wseed_free (struct Opt *opt)
{
  for (int i=0; i<opt->wseed_len; ++i)
    free (opt->wseed[i]);
  free(opt->wseed);
}

/**
 *  permutation generator main logic
 *  you need to call it in a loop from
 *  depth=min_depth to depth=max_depth
 */
int
perm (const int depth, const struct Opt *opt)
{
  int idxs[depth];
  memset (idxs, 0, depth * sizeof (int));

 Perm_Loop: /* O(seeds^depth) */
  int i = 0;
  if (opt->__pref)
    Pfputs (opt->__pref, opt);
 Print_Loop: /* O(depth) */
  {
    int idx = idxs[i];
    if (idx < opt->seed_len)
      {
        /* range of character seeds */
        Pfputc (opt->seed[idx], opt);
      }
    else
      {
        /* range of word seeds */
        idx -= opt->seed_len;
        Pfputs (opt->wseed[idx], opt);
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
       pos >= 0 &&
         idxs[pos] == opt->seed_len - 1 + opt->wseed_len;
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

/**
 *  uniquely appends char(s) from @srt to @dest
 *  @dest with *CAPACITY* 256 is always enough
 *  time: O(src_len * dest_len);
 *  updates @dest_len and returns number of bytes written
 */
int
uniappd (char *restrict dest, int *dest_len,
         const char *restrict src, int src_len)
{
  int rw = 0;
  while (src_len > 0)
    {
      if (*src == '\0' || *src == ' ')
        break;
      for (int __i = *dest_len - 1; __i >= 0; __i--)
        {
          if (*src == dest[__i])
            goto END_OF_LOOP;
        }
      dest[(*dest_len)++] = *src;
      rw++;

    END_OF_LOOP:
      src++;
      src_len--;
    }

  return rw;
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
      errorf ("Invalud filename");
      return;
    }
  if (!(__tmp = fopen (pathname, mode)))
    {
      perrorf ("Could not open file (%s:%s) -- ", mode, pathname);
      return;
    }
  *dest = __tmp;
}

/* CLI options, getopt */
const struct option lopts[] = {
  /* seeds */
  {"seed", required_argument, NULL, 's'},
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
  {"df", required_argument, NULL, '1'},
  {"depth-from", required_argument, NULL, '1'},
  {"dt", required_argument, NULL, '2'},
  {"depth-to", required_argument, NULL, '2'},
  /* format */
  {"format", required_argument, NULL, 'f'},
  {"pref", required_argument, NULL, '3'},
  {"prefix", required_argument, NULL, '3'},
  {"suff", required_argument, NULL, '4'},
  {"suffix", required_argument, NULL, '4'},
  {NULL, 0, NULL, 0} // End of Options
};

int
init_opt (int argc, char **argv, struct Opt *opt)
{
  char *__p = NULL;
  int idx = 0, flag;

  while (1)
    {
      /* we use 1,2,3,4 as `helper` options and only to use getopt */
      if ((flag = getopt_long (argc, argv,
                               "s:S:o:a:p:d:f:D:1:2:3:4:h", lopts, &idx)) == -1)
        {
          /* End of Options */
          break;
        }

      switch (flag)
        {
        case 'h':
          usage ();
          return 1;
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
            size_t __len;
            char *__line = NULL;
            FILE *wseed_f = stdin;
            /* using optarg value as filepath otherwise stdin */
            if (!_strcmp (optarg, "-"))
              safe_fopen (&wseed_f, optarg, "r");
            if (wseed_f == stdin && isatty (fileno (stdin)))
              fprintf (stderr, "reading words from stdin:\n");

            while (1)
              {
                if (getline (&__line, &__len, wseed_f) < 0)
                  break;
                if (__line && strlen (__line) > 1 &&
                    __line[0] != '#') // commented line
                  {
                    __line[strlen (__line) - 1] = '\0';
                    wseed_append (opt, __line);
                  }
              }
            if (__line)
              free (__line);
            fclose (wseed_f);
          }
          break;
        case 's': /* seeds */
          {
            for (char *c = optarg;; ++c)
              {
                if (*c == 'a' || *c == 'A' || *c == 'n'
                    || *c == 's' || *c == 'w')
                  {
                    if (opt->seed == NULL)
                      __seed_init (256);
                  }
                switch (*c)
                  {
                  case ' ':
                    break; /* separator */

                  case 'W': /* add word seed(s) */
                    {
                      for (char *prev_sep = ++c;; ++c)
                        {
                          if (*c == ',')
                            {
                              *(c++) = '\0';
                              wseed_append (opt, prev_sep);
                              prev_sep = c;
                              if (*c == '\0' || *c == ' ')
                                break;
                            }
                          else if (*c == '\0' || *c == ' ')
                            {
                              if (prev_sep != c)
                                {
                                  *c = '\0';
                                  wseed_append (opt, prev_sep);
                                }
                              break;
                            }
                        }
                      break;
                    }

                  case 'a': /* add [a-z] */
                    uniappd (opt->seed, &opt->seed_len, AZ.c, AZ.len);
                    break;
                  case 'A': /* add [A-Z] */
                    uniappd (opt->seed, &opt->seed_len, AZCAP.c, AZ.len);
                    break;
                  case 'n': /* add [0-9] */
                    uniappd (opt->seed, &opt->seed_len, NUMS.c, NUMS.len);
                    break;
                  case 's': /* add custom seed(s) */
                    /**
                     *  we know uniappd will stop coping when
                     *  encounters '\0' and ' ' (space);
                     *  so, this call with 256 as src_len, will not
                     *  corrupt the rest of the user seed options
                     */
                    c += uniappd (opt->seed, &opt->seed_len, c+1, 256);
                    break;
                  }

                /* End of Argument of `-s` */
                if (*c == '\0')
                  break;
              }
          }
          break;

        default:
          break;
        }
    }

  /**
   *  Initializing the default values
   *  when their not specified by the user
   */
  {
    if (opt->outf == NULL)
      opt->outf = stdout;

    if (opt->seed == NULL)
      {
        __seed_init (38);
        __p = mempcpy (__p, AZ.c, 26);
        __p = mempcpy (__p, NUMS.c, 10);

        opt->seed_len = (int)(__p - opt->seed);
      }

    if (opt->from_depth <= 0)
      {
        opt->from_depth = 3;
      }
    else if (opt->to_depth <= 0)
      {
        /* only from_depth is specified OR `-D` is being used */
        opt->to_depth = opt->from_depth;
      }
  }

  /* interpreting backslash character(s) */
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

int
main (int argc, char **argv)
{
  struct Opt opt = {0};
  __progname__ = *argv;
  if (init_opt (argc, argv, &opt))
    goto EndOfMain;

  if (opt.seed_len == 0 && opt.wseed_len == 0)
    {
      errorf ("Warning -- Empty permutation!");
      goto EndOfMain;
    }

#ifdef _USE_BIO
  int cap = _BMAX;
  BIO_t __bio = bio_new (cap, malloc (cap), fileno (opt.outf));
  opt.bio = &__bio;
#endif

#ifdef _DEBUG
  /* print some debug information */
  printd_arr (opt.seed, "`%c`", opt.seed_len);
  printd_arr (opt.wseed, "`%s`", opt.wseed_len);
  if (opt.__sep)
    dprintf ("* delimiter: `%s`\n", opt.__sep);
  dprintf ("* depth: from %d to %d\n", opt.from_depth, opt.to_depth);

# ifdef _USE_BIO
  dprintf ("* buffered_io buffer length: %ld bytes\n", _BMAX);
# else
  dprintf ("- compiled without buffered_io\n");
# endif /* _USE_BIO */
  dprintf ("* permutations:\n");
#endif /* _DEBUG */

  int rw_err = 0;
  for (int d = opt.from_depth; d <= opt.to_depth; ++d)
    {
      if ((rw_err = perm (d, &opt)) != 0)
        break;
    }

#ifdef _USE_BIO
  bio_flush (opt.bio);
  free (opt.bio->buffer);
#endif

 EndOfMain:
  if (opt.seed)
    free (opt.seed);
  if (opt.wseed)
    wseed_free (&opt);
  /* close any non-stdout file descriptors */
  if (opt.outf && fileno (opt.outf) != 1)
    fclose (opt.outf);

  return 0;
}
