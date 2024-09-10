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
 *  Permutation Generator
 *
 *  Usage:
 *    - basic usage, default depth (length) is 3
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
 *    - To read these seed words from a file, (`-S` option):
 *    # wlist.txt is a newline-separated word list
 *    # lines starting with `#` will be ignored
 *    $ ./permugen -S /path/to/wlist.txt
 *
 *    - Example:
 *    # to see only permutations of the wordlist
 *    $ ./permugen -s s -S /path/to/wlist.txt
 *
 *    # to also include `-` and `_`
 *    $ ./permugen -s "s-_" -S /path/to/wlist.txt
 *
 *
 *    - To add prefix and suffix to the output, (`-f` option):
 *    $ ./permugen -f ".com"         ->      xyz.com
 *    $ ./permugen -f "www.:.com"    ->  www.xyz.com
 *    $ ./permugen -f "www.:"        ->  www.xyz
 *
 *  Compilation:
 *    provide the `buffered_io.h` file (or use `-I/path/to/buffered_io.h`):
 *      cc -ggdb -O3 -Wall -Wextra -Werror \
 *         -D_PERMUGEN_USE_BIO \
 *         -o permugen permugen.c
 *
 *  Other compilation options:
 *    - to compile without buffered IO (less performance)
 *      remove `-D_PERMUGEN_USE_BIO`
 *
 *    - define `-D_BMAX="1024 * 1"` to change the default
 *      buffered IO buffer length
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
#ifdef _PERMUGEN_USE_BIO
#  ifndef _BMAX
#    define _BMAX (sysconf (_SC_PAGESIZE) / 2)
#  endif
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif

#ifdef _DEBUG
#  include <stdio.h>
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

struct seed_part {
  const char *c;
  int len;
};

static const struct seed_part AZ = {"abcdefghijklmnopqrstuvwxyz", 26};
static const struct seed_part AZCAP = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
static const struct seed_part NUMS = {"0123456789", 10};

#define if_str(s1, s2)                          \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

struct Opt {
  /* seed chars */
  char *seed;
  int seed_len;

  /* word seed */
  char **wseed;
  int wseed_len;
  int __wseed_l; /* used for dynamic array */

  /* prefix and suffix of output */
  const char *__pref;
  const char *__suff;

  /* output conf */
  int outfd;
  int from_depth;
  int to_depth;

  /* buffered_io */
#ifdef _PERMUGEN_USE_BIO
  BIO_t *bio;
#endif

};


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

int
w_wl (const int depth, const struct Opt *opt)
{
  int idxs[depth];
  if (opt->seed_len == 0 && opt->wseed_len == 0)
    return 0;
  memset (idxs, 0, depth * sizeof (int));

 WL_Loop:
#ifndef _PERMUGEN_USE_BIO
  /* print the prefix */
  if (opt->__pref)
    write (opt->outfd, opt->__pref, strlen (opt->__pref));
  /* print the permutation */
  for (int i = 0; i < depth; ++i)
    {
      int idx = idxs[i];
      if (idx < opt->seed_len)
        write (opt->outfd, opt->seed + idx, 1);
      else
        {
          idx -= opt->seed_len;
          const char *__w = opt->wseed[idx];
          write (opt->outfd, __w, strlen (__w));
        }
    }
  if (opt->__suff)
    write (opt->outfd, opt->__suff, strlen (opt->__suff));
  write (opt->outfd, "\n", 1);
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
 *  unique append char(s) from @srt into @dest
 *  @dest with *CAPACITY* 256 is always enough
 *  time: O(src_len * dest_len);
 *  updates @dest_len and
 *  returns number of bytes written
 */
int
uniappend (char *restrict dest, int *dest_len,
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

int
init_opt (int argc, char **argv, struct Opt *opt)
{
  char *__p = NULL;

#define __seed_init(cap) {                      \
    opt->seed = malloc (cap);                   \
    __p = opt->seed;                            \
  }

#define next_opt(argc, argv) {argc--; argv++;}
#define getARG(action) if (argc > 1) {          \
    next_opt(argc, argv);                       \
    char *ARG = *argv;                          \
    action;                                     \
  }
  
#define if_opt(__short, __long) \
  if (if_str(*argv, __short) || if_str(*argv, __long))
#define if_opt3(__p1, __p2, __p3) \
  if (if_str(*argv, __p1) || if_str(*argv, __p2) || if_str(*argv, __p3))

  opt->outfd = 1; // stdout

  for (argc--, argv++; argc > 0; argc--, argv++)
    {
      if_opt ("-o", "--output")
        {
          getARG({
              FILE *o = fopen (ARG, "w");
              if (o)
                opt->outfd = fileno (o);
            });
        }
      
      if_opt ("-d", "--depth")
        {
          getARG({
              opt->from_depth = atoi(ARG);
            });
        }

      if_opt ("-D", "--all-depth")
        {
          getARG({
              opt->from_depth = 1;
              opt->to_depth = atoi(ARG);
            });
        }

      /* -dx and -Dx where x must be a number */
      if (strncmp (*argv, "-d", 2) == 0 && strlen(*argv) > 2)
        {
          opt->from_depth = atoi(*argv + 2);
        }
      if (strncmp (*argv, "-D", 2) == 0 && strlen(*argv) > 2)
        {
          opt->from_depth = 1;
          opt->to_depth = atoi(*argv + 2);
        }

      if_opt3 ("-df", "-fd", "--from-depth")
        {
          getARG({
              opt->from_depth = atoi(ARG);
            });
        }

      if_opt3 ("-tf", "-td", "--to-depth")
        {
          getARG({
              opt->to_depth = atoi(ARG);
            });
        }

      if_opt ("-f", "--format")
        {
          getARG({
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

      if_opt ("-s", "--seed")
        {
          getARG({
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
                      uniappend (opt->seed, &opt->seed_len, AZ.c, AZ.len);
                      break;
                      
                    case 'A': /* add [A-Z] */
                      uniappend (opt->seed, &opt->seed_len, AZCAP.c, AZ.len);
                      break;
                    case 'n': /* add [0-9] */
                      uniappend (opt->seed, &opt->seed_len, NUMS.c, NUMS.len);
                      break;
                    case 's': /* add custom seed(s) */
                      /**
                       *  we know uniappend will stop coping when
                       *  encounters '\0' and ' ' (space);
                       *  so, this call with 256 as src_len, will not
                       *  corrupt the rest of the user seed options
                       */
                      c += uniappend (opt->seed, &opt->seed_len, c+1, 256);
                      break;
                    }
                }
            });
        }

      if_opt3 ("-S", "--wseed-path", "--seed-path")
        {
          getARG({
              size_t __len;
              char *__line = NULL;
              FILE *wseed_f = fopen (ARG, "r");
              if (!wseed_f)
                continue;
              while (1)
                {
                  if (getline (&__line, &__len, wseed_f) < 0)
                    break;
                  if (strlen (__line) > 1 &&
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

#ifdef _PERMUGEN_USE_BIO
  int cap = _BMAX;
  BIO_t __bio = bio_new (cap, malloc (cap), opt.outfd);
  opt.bio = &__bio;
#endif

#ifdef _DEBUG
  printd_arr (opt.seed, "`%c`", opt.seed_len);
  printd_arr (opt.wseed, "`%s`", opt.wseed_len);
  printf ("* depth: from %d to %d\n", opt.from_depth, opt.to_depth);
  puts ("Permutations:");
#endif

  int rw_err = 0;
  for (int d = opt.from_depth; d <= opt.to_depth; ++d)
    if ((rw_err = w_wl (d, &opt)) < 0)
      break;

#ifdef _PERMUGEN_USE_BIO
  bio_flush (opt.bio);
  free (opt.bio->buffer);
#endif

  if (opt.seed)
    free (opt.seed);
  if (opt.wseed)
    wseed_free (&opt);

  /* close any non-stdout file descriptors */
  if (opt.outfd != 1)
    close (opt.outfd);

  return 0;
}
