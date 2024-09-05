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
 *    - [a-z] [0-9] permutations (default depth is 3):
 *    $ ./permugen
 *
 *    - from depth 2 to 5:
 *    $ ./permugen -fd 2 -td 5
 *
 *    - for depths 1, 2, 3, 4:
 *    $ ./permugen -D 4
 *
 *    - to specify seed, use `-s`:
 *      `-s a`      ->  for [a-z]
 *      `-s A`      ->  for [A-Z]
 *      `-s n`      ->  for [0-9]
 *      `-s wXYZ`   ->  for [X,Y,Z]
 *
 *      also to inject word(s) in permutations, use:
 *      `-s Wdev`         ->  for using `dev`
 *      `-s Wdev,test,m`  ->  for `dev`, `test`, `m`
 *
 *    # permutations of `1`, `2`, `test`, `dev`
 *    $ ./permugen -D 2 -s w12 -s Wtest,dev
 *
 *    - to add prefix and suffix to the output, use `-f`:
 *    $ ./permugen -f ".com"         ->      xyz.com
 *    $ ./permugen -f "www.:.com"    ->  www.xyz.com
 *    $ ./permugen -f "www.:"        ->  www.xyz
 *
 *  Compilation:
 *    provide the `buffered_io.h` file (or use `-I`):
 *      cc -ggdb -O3 -Wall -Wextra -Werror \
 *         -D_PERMUGEN_USE_BIO \
 *         -o permugen permugen.c
 *
 *  compilation options:
 *    - to compile without buffered IO (less performance)
 *      remove `-D_PERMUGEN_USE_BIO`
 *
 *    - define `-D_BMAX="1024 * 1"` to change the default
 *      buffered IO buffer length
 *
 **/
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/**
 *  using buffered_io.h for performance
 *  this file should be available in this repo
 */
#ifdef _PERMUGEN_USE_BIO
#  ifndef _BMAX
#    define _BMAX 1024
#  endif
#  define BIO_IMPLEMENTATION
#  include "buffered_io.h"
#endif

struct seed_part {
  const char *c;
  int len;
};

static const struct seed_part AZ = {"abcdefghijklmnopqrstuvwxyz", 26};
static const struct seed_part AZCAP = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
static const struct seed_part NUMS = {"0123456789", 10};
static const struct seed_part ESP = {"-_", 2};

static const int OOUT_FILE_OPTS = O_WRONLY | O_CREAT | O_TRUNC;
static const int SOUT_FILE_FLAGS = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

#define if_str(s1, s2)                          \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

struct Opt {
  /* seed chars */
  char *seed;
  int seed_len;

  /* word seed */
  const char **wseed;
  int wseed_len;

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


int
w_wl (const int depth, const struct Opt *opt) {
  int idxs[depth];
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
 *  like mempcpy but @dest elements are unique
 *  dest having *CAPACITY* 257 is always enough
 */
char *
memupcpy (char *dest, const char *src, int len)
{
  char *__p = dest;

  while (len > 0)
    {
      for (char *p = dest; *p != '\0'; p++)
        if (*p == *src)
          goto EOLoop;

      *(__p++) = *src;
      *__p = '\0';

    EOLoop:
      src++;
      len--;
    }

  return __p;
}

int
init_opt (int argc, char **argv, struct Opt *opt)
{
  char *__p = NULL;

#define seed_init(cap) {                        \
    opt->seed = malloc (cap);                   \
    __p = opt->seed;                            \
  }
#define __seed_init(cap)                        \
  if (opt->seed == NULL) { seed_init (257); }

#define next_opt(argc, argv) {argc--; argv++;}
#define getp(action) if (argc > 1) {            \
    next_opt(argc, argv);                       \
    char *val = *argv;                          \
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
          getp({
              int fd;
              if ((fd = open (val, OOUT_FILE_OPTS, SOUT_FILE_FLAGS)) > 0)
                opt->outfd = fd;
            });
        }
      
      if_opt ("-d", "--depth")
        {
          getp({
              opt->from_depth = atoi(val);
            });
        }

      if_opt ("-D", "--all-depth")
        {
          getp({
              opt->from_depth = 1;
              opt->to_depth = atoi(val);
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
          getp({
              opt->from_depth = atoi(val);
            });
        }

      if_opt3 ("-tf", "-td", "--to-depth")
        {
          getp({
              opt->to_depth = atoi(val);
            });
        }

      if_opt ("-f", "--format")
        {
          getp({
              opt->__suff = val;
              for (char *p = val; *p != '\0'; ++p)
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
          getp({
              if (val[0] == 'a')
                {
                  /* add a-z */
                  __seed_init();
                  __p = memupcpy (__p, AZ.c, AZ.len);
                }
              else if (val[0] == 'A')
                {
                  /* add A-Z */
                  __seed_init();
                  __p = memupcpy (__p, AZCAP.c, AZ.len);
                }
              else if (val[0] == 'N' || val[0] == 'n')
                {
                  /* add numbers */
                  __seed_init();
                  __p = memupcpy (__p, NUMS.c, NUMS.len);
                }
              else if (val[0] == 'S' || val[0] == 's')
                {
                  /* add some special characters */
                  __seed_init();
                  __p = memupcpy (__p, ESP.c, ESP.len);
                }
              else if (val[0] == 'w')
                {
                  /* to make a custom seed */
                  __seed_init();
                  __p = memupcpy (__p, val + 1, strlen (val + 1));
                }
              else if (val[0] == 'W')
                {
                  /* to use word(s) as seed */
                  char *sep = val + 1;
                  while (*sep)
                    {
                      opt->wseed = realloc (opt->wseed,
                              (opt->wseed_len + 1) * sizeof (char *));
                      opt->wseed[opt->wseed_len++] = sep;

                      for (; *sep != ',' && *sep != '\0'; ++sep);
                      if (*sep == ',')
                        *(sep++) = '\0';
                    }
                }
            });
        }
    }

  if (opt->seed == NULL)
    {
      /* seed has not specified by the user */
      seed_init (38);
      __p = mempcpy (__p, AZ.c, 26);
      __p = mempcpy (__p, NUMS.c, 10);
    }

  if (opt->from_depth <= 0)
    {
      /* depth has not specified by the user */
      opt->from_depth = 3;
      opt->to_depth = 3;
    }
  else if (opt->to_depth <= 0)
    {
      /* only from_depth has specified OR `-D` is being used */
      opt->to_depth = opt->from_depth;
    }

  opt->seed_len = (int)(__p - opt->seed);
  return 0;
}


int
main (int argc, char **argv)
{
  struct Opt opt = {0};
  init_opt (argc, argv, &opt);

#ifdef _PERMUGEN_USE_BIO
  BIO_t __bio = bio_new (_BMAX, malloc (_BMAX), opt.outfd);
  opt.bio = &__bio;
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
  close (opt.outfd);
  return 0;
}
