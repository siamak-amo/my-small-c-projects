/**
 *  file: key_extractor.c  (old tokenizeIt.c)
 *  created on: 8 Sep 2024
 *
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define TOKEN_MAX_BUF_LEN (512) // 0.5Kb
#define ML_IMPLEMENTATION
#include "mini-lexer.c"

#ifdef _USE_BIO
# ifndef _BMAX
#  define _BMAX (1024) // 1Kb
# endif
# define BIO_IMPLEMENTATION
# include "buffered_io.h"
#endif


enum LANG
  {
    STR1 = 0,
    STR2,
    STR3,
  };

/**
 *  Any string types enclosed
 *  within these delimiters will be ignored
 */
 static struct Milexer_exp_ Expressions[] = {
  [STR1]          = {"\"", "\""},
  [STR2]          = {"'", "'"},
  [STR3]          = {"`", "`"},
};

/**
 *  This includes nearly all non-alphanumeric
 *  ASCII characters
 *
 *  It primarily allows only alphanumeric tokens
 *
 *  The presence of any prefixes of @ML fields
 *  (like @Expressions) within this context,
 *  causes conflicts and affects the expected behavior
 */
static const char *Delimiters[] = {
  "\x00\x21",    /* below '"' */
  "\x23\x2F",   /* between '"' and '0' */
  "\x3A\x40",   /* after '9' and before 'A' */
  "\x5B",       /* '[' */
  "\x5D",       /* ']' */
  "\x5E",       /* '^' */
  "\x60",       /* '`' */
  "\x7B\xFF",   /* after 'Z' */
};

static const Milexer ML = {
  .expression   = GEN_MKCFG (Expressions),
  .delim_ranges = GEN_MKCFG (Delimiters),
};

typedef ssize_t (*loader) (int, void *, size_t);

int infd = STDIN_FILENO;
int ofd = STDOUT_FILENO;

#ifdef _USE_BIO
static BIO_t bio;
#endif

#ifdef _USE_BIO
# define Print(str) bio_fputs (&bio, str)
# define Putln() bio_puts (&bio, "")
#else
# define Print(str) dprintf (ofd, "%s", str)
# define Putln() dprintf (ofd, "\n")
#endif /* _USE_BIO */


int
main (void)
{
static inline int
token_out (const char *cstr)
{
  if (allow_numbers)
    {
      Print (cstr);
      return 1;
    }
  else
    {
      char *end;
      /* We assume strings staring with 0x are numbers */
      if (strncmp (cstr, "0x", 2) == 0)
        {
          return 0;
        }
      strtol (cstr, &end, 10);
      if (*(end + 1) == '\0' && errno == 0)
        {
          return 0;
        }
      Print (cstr);
      return 1;
    }
  return -1; /* unreachable */
}

  int buf_len = TOKEN_MAX_BUF_LEN;
  char *buf = malloc (buf_len);
  Milexer_Token tk = TOKEN_ALLOC (TOKEN_MAX_BUF_LEN);
  Milexer_Slice src = {.lazy = true};

  /**
   *  Initializing buffered IO
   *  As this uses `ofd` for output file descriptor
   *  of `bio`, Is must be done after any parsing argument
   *  that might change ofd
   */
#ifdef _USE_BIO
  int bio_cap = _BMAX;
  bio = bio_new (bio_cap, malloc (bio_cap), ofd);
#endif

  int len;
  for (int ret = 0; !NEXT_SHOULD_END (ret); )
    {
      ret = ml_next (&ML, &src, &tk, PFLAG_DEFAULT);
      switch (ret)
        {
        case NEXT_NEED_LOAD:
          if ((len = read (infd, buf, buf_len)) > 0)
            SET_ML_SLICE (&src, buf, len);
          else
            END_ML_SLICE (&src);
          break; 
 
        case NEXT_CHUNK:
          if (tk.type == TK_KEYWORD)
            token_out (tk.cstr);
          break;
        case NEXT_MATCH:
        case NEXT_ZTERM:
          if (tk.type == TK_KEYWORD)
              if (token_out (tk.cstr))
                Putln ();
          break;

        default: break;
        }
     }
  
  TOKEN_FREE (&tk);
  free (buf);

#ifdef _USE_BIO
  bio_flush (&bio);
  free (bio.buffer);
#endif

  return 0;
}
