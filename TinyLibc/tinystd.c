#include "tinystd.h"

extern int main();
/* env variables pointer (use getenv function) */
char **__libc_envp;

#ifdef _TLIBC_DEBUG
# define __libc_logf(fmt, ...) \
  dprintf (STDERR_FILENO, "tiny_libc: " fmt, ##__VA_ARGS__)
# define __libc_logln(fmt, ...) \
  __libc_logf (fmt "\n", ##__VA_ARGS__)
#else
# define __libc_logf(...) ((void) NULL)
# define __libc_logln(...) ((void) NULL)
#endif /* _TLIBC_DEBUG */

#define libc_warn(fmt, ...) \
  dprintf (STDERR_FILENO, "tiny_libc: " fmt, ##__VA_ARGS__)
#define libc_warnln(fmt, ...) \
    libc_warn(fmt "\n", ##__VA_ARGS__)

int
__dummy_main (int, char **)
{
  libc_warnln ("Your program does not have main function!");
  return EXIT_FAILURE;
}
weak_alias (__dummy_main, main);

/** std read **/
char
__getc ()
{
  char c = 0;
  if (read (STDIN_FILENO, &c, 1) == 1)
    return c;
  return 0;
}
weak_alias (__getc, getc);

/** std write **/
static inline char *
__sprintn (int n, int base, const char *alphabet)
{
#define __SNPRINTN_MAXLEN 63
  static char result[__SNPRINTN_MAXLEN + 1];
  if (0 == n)
    {
      *result = '0';
      *(result + 1) = 0;
      return result;
    }
  int was_neg = 0;
  if (n < 0)
    {
      n *= -1;
      was_neg = 1;
    }
  char *p = result + __SNPRINTN_MAXLEN;
  *p = 0;
  for (--p; p > result && 0 != n; --p)
    {
      *p = alphabet[n % base];
      n /= base;
    }
  if (was_neg)
    {
      *p = '-';
      return p;
    }
  return p + 1;
#undef __SNPRINTN_MAXLEN
}

int
__fputc (int fd, void *c)
{
  return write (fd, c, 1);
}
weak_alias (__fputc, fputc);

int
__dprintf (int fd, char _Nullable *format, ...)
{
  int n;
  va_list ap;
  va_start (ap, format);

  if (!format)
    dprintf (fd, "(null)\n");

  for (const char *p = format; *p != '\0'; ++p)
    {
      /* TODO: terrible performance */
      if (*p != '%')
        {
          fputc (fd, (void *)p);
          continue;
        }
      switch (*(++p))
        {
        case '%':
          fputc (fd, (void *)'%');
          break;

        case 'c':
          int c = va_arg (ap, int);
          fputc (fd, &c);
          break;
        case 's':
          dprintf (fd, va_arg (ap, char *));
          break;

        case 'd':
          n = va_arg (ap, int);
          dprintf (fd, __sprintn (n, 10, "0123456789"));
          break;
        case 'x':
          n = va_arg (ap, int);
          dprintf (fd, __sprintn (n, 16, "0123456789abcdef"));
          break;
        case 'X':
          n = va_arg (ap, int);
          dprintf (fd, __sprintn (n, 16, "0123456789ABCDEF"));
          break;

        default:
          break;
        }
    }
  return 0;
}
weak_alias (__dprintf, dprintf);

/** cstr functions **/
int
__strcmp (const char *s1, const char *s)
{
  /**
   *  TODO: wrong return value
   */
  for (; '\0' != *s1 && '\0' != *s; ++s1, ++s)
    {
      if (*s1 != *s)
        return 1;
    }
  return 0;
}
weak_alias (__strcmp, strcmp);

int
__strncmp (const char *s1, const char *s, size_t n)
{
  /**
   *  TODO: wrong return value
   */
  for (; n != 0 && '\0' != *s1 && '\0' != *s;
       --n, ++s1, ++s)
    {
      if (*s1 != *s)
        return 1;
    }
  return 0;
}
weak_alias (__strncmp, strncmp);

char *
__strchr (const char *cstr, int delim)
{
  for (const char *p = cstr; '\0' != *p; ++p)
    {
      if (*p == delim)
        return (char *) p;
    }
  return NULL;
}
weak_alias (__strchr, strchr);

char *
__strchrnul (const char *cstr, int delim)
{
  const char *p = cstr;
  for (; '\0' != *p; ++p)
    {
      if (*p == delim)
        return (char *) p;
    }
  return (char *)p;
}
weak_alias (__strchrnul, strchrnul);

size_t
__strlen (const char *buf)
{
  size_t i = 0;
  for (; *buf != '\0'; ++i, ++buf);
  return i;
}
weak_alias (__strlen, strlen);

/** mem functions **/
void *
__memset (void *dst, int c, size_t n)
{
  unsigned char *p = dst;
  for (; n != 0; --n, ++p)
    *p = c;
  return dst;
}
weak_alias (__memset, memset);

void *
__mempset (void *dst, int c, size_t n)
{
  return (char *)memset (dst, c, n) + n;
}
weak_alias (__mempset, mempset);

void *
__memcpy (void *dest, const void *src, size_t n)
{
  const unsigned char *sp = src;
  unsigned char *dp = dest;
  for (; n != 0; --n, ++sp, ++dp)
    *dp = *sp;
  return dest;
}
weak_alias (__memcpy, memcpy);

void *
__mempcpy (void *dest, const void *src, size_t n)
{
  const unsigned char *sp = src;
  unsigned char *dp = dest;
  for (; n != 0; --n, ++sp, ++dp)
    *dp = *sp;
  return dp;
}
weak_alias (__mempcpy, mempcpy);

int
__memcmp (const void *s1, const void *s2, size_t n)
{
  /* TODO: wrong return value */
  const unsigned char *p1 = s1, *p2 = s2;
  for (; n != 0; --n, ++p1, ++p2)
    {
      if (*p1 != *p2)
        return 1;
    }
  return 0;
}
weak_alias (__memcmp, memcmp);

/** env **/
char *
__getenv (const char _Nullable *name)
{
  if (!name)
    return NULL;
  int len = strlen (name);
  if (0 == len)
    return NULL;
  for (char **env = (char **)__libc_envp; *env; ++env)
    {
      if (strncmp (name, *env, len) == 0)
        return *env + len + 1; /* skiping `=` */
    }
  return NULL;
}
weak_alias (__getenv, getenv);

static inline void
__libc_initenv (volatile void *rsp,
                int *argc,
                void **argv)
{
#define WORD (sizeof (void *))
  {
    // rsp += WORD; /* skipping garbage */
    *argc = *(int *)rsp;

    rsp += WORD;
    *argv = (void **)rsp;

    rsp += WORD*5;
    __libc_envp = (char **)rsp;
  }
#undef WORD

  char *p = getenv ("TINY_LIBC");
  if (p)
    printf ("Iam sitting right here (%s)\n", p);
}


void
__libc_start_main__ (volatile void *start_rsp)
{
  int argc;
  void *argv;
  __libc_initenv (start_rsp, &argc, &argv);

  int ret;
  __libc_logln ("initializing finished", NULL);
  {
    /* Running main */
    ret = main (argc, argv);
  }

  __libc_logln ("main() returned: %d", ret);
  _exit (ret);
}
weak_alias (__libc_start_main__, __libc_start_main);

/**
 *  The start symbol 
 *  The GNU _start is before us (from crt0),
 *  so it takes care of the linking stuff
 *
 *  Also we can simply read argc,argv,env from $rsp
 *  As this is a naked function
 **/
void _Naked
_start (void)
{
  /**
   *  Bringing $rsp to C, so we can use it easily
   *  It's just a simple move $rsp into prev_rsp
   *
   *  TODO: Is this safe?
   *  TODO: Is this architecture independent?
   */
  volatile void *start_rsp;
  asm volatile (
                "mov %%rsp, %0"
                : "=r" (start_rsp)
                );
  __libc_start_main (start_rsp);
}

#undef __libc_logf
#undef __libc_logln
