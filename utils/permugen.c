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
 *
 *  Usage:
 *    - Basic usage, default depth (length) is 3
 *    # permutation of [a-z] [0-9], with depth 2
 *    $ ./permugen -d 2
 *
 *    - From depth 2 to 5:
 *    $ ./permugen -fd 2 -td 5
 *
 *    - For depths <= 4
 *    $ ./permugen -D 4
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
 *
 *    # to also include numbers and [A-Z]
 *    $ ./permugen -s "s-. Wtest,dev nA"
 *
 *
 *    - To read seed words from a file, (`-S` option)
 *      When file path is `-`, it uses stdin
 *    # wlist.txt is a newline-separated word list
 *    # lines starting with `#` will be ignored
 *    $ ./permugen -S /path/to/wlist.txt
 *
 *    - Example:
 *    # to get permutations of a wordlist
 *    # `-s s` means no character seed
 *    $ ./permugen -s s -S /path/to/wlist.txt
 *
 *    # to also include `-` and `_`
 *    $ ./permugen -s "s-_" -S /path/to/wlist.txt
 *
 *    # to get permutations of words from stdin:
 *    $ cat wlist.txt | ./permugen -S - -ss
 *    # to also include `AAA` and `-`
 *    $ cat wlist.txt | ./permugen -S - -ss- -sWAAA
 *
 *    - To add prefix and suffix to the output:
 *    $ ./permugen -f ".com"         ->      xyz.com
 *    $ ./permugen -f "www.:.com"    ->  www.xyz.com
 *    $ ./permugen -f "www.:"        ->  www.xyz
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
#include <fcntl.h>

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

#ifdef _DEBUG
/* use printd_arr */
#  define printd_arr__H(arr, T, len, sep, end)  \
  for (int __idx = 0; __idx < len; __idx++) {   \
    printf (T"%s", arr[__idx],                  \
            (__idx < len-1) ? sep : end);       \
  }
/**
 * debug printf for arrays
 * pass printf format for each element of @arr with @T
 * Ex: to print `int arr[7]`  -->  printd_arr (arr, "%d", 7);
 */
#  define printd_arr(arr, T, len)               \
  if (len > 0) {                                \
    printf ("* "#arr"[%d] = {", len);           \
    printd_arr__H (arr, T, len, ", ", "}\n");   \
  } else {                                      \
    puts ("- "#arr" is empty");                 \
  }
#endif

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

struct Opt {
  /* seed */
  char *seed;
  int seed_len;
  /* word seed */
  char **wseed;
  int wseed_len;
  int __wseed_l; /* to make wseed dynamic-array */

  /* prefix and suffix of the result */
  const char *__pref;
  const char *__suff;

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
w_wl (const int depth, const struct Opt *opt)
{
  int idxs[depth];
  memset (idxs, 0, depth * sizeof (int));

 WL_Loop:
#ifndef _USE_BIO
  /* implementation without using buffered_io */
  /* print the prefix */
  if (opt->__pref)
    fprintf (opt->outf, opt->__pref);
  /* print the permutation */
  for (int i = 0; i < depth; ++i)
    {
      int idx = idxs[i];
      if (idx < opt->seed_len)
        putc (opt->seed[idx], opt->outf);
      else
        {
          idx -= opt->seed_len;
          const char *__w = opt->wseed[idx];
          fprintf (opt->outf, __w);
        }
    }
  if (opt->__suff)
    fprintf (opt->outf, opt->__suff);
  putc ('\n', opt->outf);
  if (errno != 0)
    return errno;

#else /* using buffered_io */
  if (opt->__pref)
    bio_fputs (opt->bio, opt->__pref);
  for (int i = 0; i < depth; ++i)
    {
      int idx = idxs[i];
      if (idx < opt->seed_len)
        bio_putc (opt->bio, opt->seed[idx]);
      else
        {
          idx -= opt->seed_len;
          bio_fputs (opt->bio, opt->wseed[idx]);
        }

      if (bio_err (opt->bio))
        return bio_errno (opt->bio);
    }
    if (opt->__suff)
      bio_puts (opt->bio, opt->__suff);
    else
      bio_ln(opt->bio);
#endif

  int pos;
  for (pos = depth - 1;
       pos >= 0 &&
         idxs[pos] == opt->seed_len - 1 + opt->wseed_len;
       pos--)
    {
      idxs[pos] = 0;
    }

  if (pos < 0)
    return 0;

  idxs[pos]++;
  goto WL_Loop;
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

int
init_opt (int argc, char **argv, struct Opt *opt)
{
  char *__p = NULL;

#define __seed_init(cap) {                      \
    opt->seed = malloc (cap);                   \
    __p = opt->seed;                            \
  }

#define next_opt(n) {argc -= n; argv += n;}
/** get argument macro
 *  in the @action `char *ARG` is available and is
 *  pointing to the value of the current argument
 *  @short_len: strlen of the name of argument
 *    if the short version of --output is `-o` => pass it 2
 *  pass @short_len=0 to disable packed arguments
 *  like `-d1` instead of `-d 1` */
#define getARG(short_len, action)               \
  if (short_len > 0 &&                          \
      argv[0][short_len] != '\0') {             \
    /* arg is packed like `-d1` or `-D1` */     \
    char *ARG = *argv + short_len;              \
    action;                                     \
  } else if (argc > 1) {                        \
    /* arg is like `-d 1` or `-o /tmp/file` */  \
    next_opt (1);                               \
    char *ARG = *argv;                          \
    action;                                     \
  } else {                                      \
    argerr (*argv, "needs an argument");        \
  }
  
#define _strcmp(s1, s2)                         \
  ((s1) != NULL && (s2) != NULL &&              \
   strncmp ((s1), (s2), strlen (s2)) == 0)

#define cmp_opt(__name) (_strcmp (*argv, __name))
#define if_opt(__short, __long) \
  if (cmp_opt (__short) || cmp_opt (__long))
#define if_opt3(__p1, __p2, __p3) \
  if (cmp_opt (__p1) || cmp_opt (__p2) || cmp_opt (__p3))

  opt->outf = stdout; // stdout

  for (argc--, argv++; argc > 0; argc--, argv++)
    {
      if_opt ("-o", "--output")
        {
          getARG(2, {
              safe_fopen (&opt->outf, ARG, "w");
            });
        }
      else
      if_opt3 ("-a", "-oA", "--output")
        {
          getARG(0, {
              safe_fopen (&opt->outf, ARG, "a");
            });
        }
      else
      if_opt ("-d", "--depth") // set depth
        {
          getARG(2, {
              opt->from_depth = atoi(ARG);
            });
        }
      else
      if_opt3 ("-D", "--depth-range", "--depth-up2") // depth range
        {
          getARG(2, {
              opt->from_depth = 1;
              opt->to_depth = atoi(ARG);
            });
        }
      else
      if_opt3 ("-df", "-fd", "--from-depth") // min depth
        {
          getARG(3, {
              opt->from_depth = atoi(ARG);
            });
        }
      else
      if_opt3 ("-tf", "-td", "--to-depth") // max depth
        {
          getARG(3, {
              opt->to_depth = atoi(ARG);
            });
        }
      else
      if_opt ("-f", "--format") // output format
        {
          getARG(2, {
              opt->__suff = ARG;
              for (char *p = ARG; *p != '\0'; ++p)
                if (*p == ':')
                  {
                    *p = '\0';

                    if (strlen (opt->__suff) != 0)
                      opt->__pref = opt->__suff;
                    else
                      opt->__pref = NULL;

                    if (strlen (p + 1) != 0)
                      opt->__suff = p + 1;
                    else
                      opt->__suff = NULL;

                    break;
                  }
            })
        }
      else
      if_opt ("-s", "--seed") // specify seed
        {
          getARG(2, {
              for (char *c = ARG; *c != '\0'; ++c)
                {
                  if (*c == 'a' || *c == 'A' || *c == 'n'
                      || *c == 's' || *c == 'w')
                    {
                      if (opt->seed == NULL)
                        __seed_init (256);
                    }
                  switch (*c)
                    {
                    case 'W': /* add word seed(s) */
                      {
                        for (char *prev_sep = ++c;; ++c)
                          {
                            if (*c == ',')
                              {
                                *(c++) = '\0';
                                wseed_append (opt, prev_sep);
                                prev_sep = c;
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
                }
            });
        }
      else
      if_opt3 ("-S", "--wseed-path", "--seed-path") // word seed path
        {
          getARG(2, {
              size_t __len;
              char *__line = NULL;
              FILE *wseed_f = stdin;
              /* using ARG value as filepath otherwise stdin */
              if (!_strcmp (ARG, "-"))
                safe_fopen (&wseed_f, ARG, "r");
              if (wseed_f == stdin)
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
            });
        }
      else
        {
          argerr (*argv, "Unknown flag");
        }
    }

  if (opt->seed == NULL)
    {
      /* seed not specified by the user */
      __seed_init (38);
      __p = mempcpy (__p, AZ.c, 26);
      __p = mempcpy (__p, NUMS.c, 10);

      opt->seed_len = (int)(__p - opt->seed);
    }

  if (opt->from_depth <= 0)
    {
      /* depth not specified by the user */
      opt->from_depth = 3;
      opt->to_depth = 3;
    }
  else if (opt->to_depth <= 0)
    {
      /* only from_depth is specified OR `-D` is being used */
      opt->to_depth = opt->from_depth;
    }

  return 0;
}


int
main (int argc, char **argv)
{
  struct Opt opt = {0};
  init_opt (argc, argv, &opt);

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
  printf ("* depth: from %d to %d\n", opt.from_depth, opt.to_depth);

# ifdef _USE_BIO
  printf ("* buffered_io buffer length: %ld bytes\n", _BMAX);
# else
  puts ("- compiled without buffered_io");
# endif /* _USE_BIO */

  puts ("* permutations:");
#endif /* _DEBUG */

  int rw_err = 0;
  for (int d = opt.from_depth; d <= opt.to_depth; ++d)
    {
      if ((rw_err = w_wl (d, &opt)) < 0)
        break; /* END OF Permutations */
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
  if (fileno (opt.outf) != 1)
    fclose (opt.outf);

  return 0;
}
