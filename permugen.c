#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

struct seed_part {
  const char *c;
  int len;
};

static const struct seed_part AZ = {"abcdefghijklmnopqrstuvwxyz", 26};
static const struct seed_part AZCAP = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26};
static const struct seed_part NUMS = {"0123456789", 10};
static const struct seed_part ESP = {"-_", 2};

static int OOUT_FILE_OPTS = O_WRONLY | O_CREAT | O_TRUNC;
static int SOUT_FILE_FLAGS = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;

#define if_str(s1, s2)                          \
  ((s1) != NULL && (s2) != NULL &&              \
   strcmp ((s1), (s2)) == 0)

struct Opt {
  int outfd;
  char *seed;
  int seed_len;
  int from_depth;
  int to_depth;
};


int
w_wl (const int depth, const struct Opt *opt) {
  int rw;
  int idxs[depth];
  memset (idxs, 0, depth * sizeof (int));

  char buff[depth + 1];
  buff[depth] = '\n';

 WL_Loop:
  for (int i = 0; i < depth; ++i)
    {
      buff[i] = opt->seed[idxs[i]];
    }

  if ((rw = write (opt->outfd, buff, depth + 1)) < 0)
    {
      /* write error */
      return rw;
    }

  int pos;
  for (pos = depth - 1; pos >= 0 && idxs[pos] == opt->seed_len - 1; pos--)
    idxs[pos] = 0;

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
  char *__p;

#define seed_init(cap) {                        \
    opt->seed = malloc (cap);                   \
    __p = opt->seed;                            \
  }

#define next_opt(argc, argv) {argc--; argv++;}
#define getp(action) if (argc > 1) {            \
    next_opt(argc, argv);                       \
    const char *val = *argv;                    \
    action;                                     \
  }
  
#define if_opt(__short, __long) \
  if (if_str(*argv, __short) || if_str(*argv, __long))
#define if_opt3(__p1, __p2, __p3) \
  if (if_str(*argv, __p1) || if_str(*argv, __p2) || if_str(*argv, __p3))

  opt->outfd = 1; // stdout

  for (argc--, argv++; argc >= 0; argc--, argv++)
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
              opt->to_depth = atoi(*argv);
            });
        }

      if_opt3 ("-df", "-fd", "--from-depth")
        {
          getp({
              opt->from_depth = atoi(*argv);
            });
        }

      if_opt3 ("-tf", "-td", "--to-depth")
        {
          getp({
              opt->to_depth = atoi(*argv);
            });
        }

      if_opt ("-s", "--seed")
        {
          getp({
              if (opt->seed == NULL)
                seed_init (257); 

              if (val[0] == 'a')
                {
                  /* add a-z */
                  __p = memupcpy (__p, AZ.c, AZ.len);
                }
              else if (val[0] == 'A')
                {
                  /* add A-Z */
                  __p = memupcpy (__p, AZ.c, AZ.len);
                }
              else if (val[0] == 'N' || val[0] == 'n')
                {
                  /* add numbers */
                  __p = memupcpy (__p, NUMS.c, NUMS.len);
                }
              else if (val[0] == 'S' || val[0] == 's')
                {
                  /* add some special characters */
                  __p = memupcpy (__p, ESP.c, ESP.len);
                }
              else if (val[0] == 'w')
                {
                  /**
                   *  make a custom seed
                   *  usage:  `-s wabcd67`
                   *       will make a seed containing {a,b,c,d,6,7}
                   */
                  __p = memupcpy (__p, val + 1, strlen (val + 1));
                }
              else if (val[0] == 'W')
                {
                  // not implemented yet
                  // add a word in permutation
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
}


int
main (int argc, char **argv)
{
  struct Opt opt = {0};
  init_opt (argc, argv, &opt);

  int rw_err = 0;
  for (int d = opt.from_depth; d <= opt.to_depth; ++d)
    if ((rw_err = w_wl (d, &opt)) < 0)
      break;
        
  if (opt.seed)
    free (opt.seed);
  close (opt.outfd);
  return 0;
}
