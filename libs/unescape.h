#ifndef UNESCAPE__H__
#define UNESCAPE__H__

/* Hex */
#define hchr2num(HH)                         \
  (  (HH >= '0' && HH <= '9') ? (HH - '0')   \
   : (HH >= 'A' && HH <= 'F') ? (HH - 'A')   \
   : (HH >= 'a' && HH <= 'f') ? (HH - 'a')   \
   : 0  )
/* Octal */
#define nchr2num(NNN)                           \
  (  (NNN >= '0' && NNN <= '7') ? (NNN - '0')   \
     : 0  )

/**
 *  Macros to set value of @ptr equal to @val
 *  and move @ptr forward by 1 or @n
 *  *BE* careful with NULL character, you don't probably
 *  mean to increase @ptr when encounter '\0'
 */
#define __next_eq(ptr, val) *((ptr)++) = val
#define __next_eqn(ptr, val, n) *((ptr) += n) = val

/**
 *
 *
 *
 */
ssize_t unescape (char *buff);
/**
 *
 *
 */
ssize_t unescape2 (char *restrict dest, const char *restrict src);


/* implementation */
#ifdef UNESCAPE_IMPLEMENTATION
ssize_t
unescape2 (char *restrict dest, const char *restrict src)
{
  char *w_ptr = dest;
  const char *r_ptr = src;
  for ( ;; ++r_ptr)
    {
      if (*r_ptr == '\0')
        {
          *w_ptr = '\0';
          break;
        }

      if (*r_ptr != '\\')
        {
          __next_eq (w_ptr, *r_ptr);
          continue;
        }

      switch (*(++r_ptr))
        {
#define nextw_b(val) {                          \
            if (val) __next_eq (w_ptr, val);    \
            else *w_ptr = val;                  \
            break;                              \
          }
        case '\0':
          goto reterr; /* invalid string */

          /* case of `\\x` --> `\\` and `x` */
        case '\\': nextw_b ('\\');
          /* simple escapes */
        case 'a': nextw_b ('\a');
        case 'b': nextw_b ('\b');
        case 'e': nextw_b ('\e');
        case 't': nextw_b ('\t');
        case 'n': nextw_b ('\n');
        case 'v': nextw_b ('\v');
        case 'f': nextw_b ('\f');
        case 'r': nextw_b ('\r');
        case '\'': nextw_b ('\'');
        case '\"': nextw_b ('\"');
          /* advanced escapes */
        case 'x': /* HEX \xHH */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 16;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 1;

            nextw_b (x);
            break;
          }
        case '0': /* Octal \0NNN */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 8 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 1;

            nextw_b (x);
            break;
          }
        }
    }

  /* success return */
  return (ssize_t)(w_ptr - dest);
  /* failure return */
 reterr:
  *w_ptr = '\0';
  return (ssize_t)(dest - w_ptr);
}

ssize_t
unescape (char *buff)
{
  char *w_ptr = buff;
  char *r_ptr = buff;
  for ( ;; ++r_ptr)
    {
      if (*r_ptr == '\0')
        {
          *w_ptr = '\0';
          break;
        }

      if (*r_ptr != '\\')
        {
          __next_eq (w_ptr, *r_ptr);
          continue;
        }

      switch (*(++r_ptr))
        {
#define nextw_b(val) {                          \
            if (val) __next_eq (w_ptr, val);    \
            else *w_ptr = val;                  \
            break;                              \
          }
        case '\0':
          goto reterr; /* invalid string */

          /* case of `\\x` --> `\\` and `x` */
        case '\\': nextw_b ('\\');
          /* simple escapes */
        case 'a': nextw_b ('\a');
        case 'b': nextw_b ('\b');
        case 'e': nextw_b ('\e');
        case 't': nextw_b ('\t');
        case 'n': nextw_b ('\n');
        case 'v': nextw_b ('\v');
        case 'f': nextw_b ('\f');
        case 'r': nextw_b ('\r');
        case '\'': nextw_b ('\'');
        case '\"': nextw_b ('\"');
          /* advanced escapes */
        case 'x': /* HEX \xHH */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 16;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += hchr2num (*r_ptr) * 1;

            nextw_b (x);
            break;
          }
        case '0': /* Octal \0NNN */
          {
            unsigned char x = 0;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 8 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 8;
            if (*(++r_ptr) == 0)
              goto reterr;
            x += nchr2num (*r_ptr) * 1 * 1;

            nextw_b (x);
            break;
          }
        }
    }

  /* success return */
  return (ssize_t)(w_ptr - buff);
  /* failure return */
 reterr:
  *w_ptr = '\0';
  return (ssize_t)(buff - w_ptr);
}

#endif /* UNESCAPE_IMPLEMENTATION */
#endif /* UNESCAPE__H__ */
