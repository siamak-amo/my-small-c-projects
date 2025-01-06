/**
 *  file: mini-lexer.c
 *  created on: 16 Dec 2024
 *
 *  Minimal Lexer header-only library
 *  --Under Development--
 *
 */
#ifndef MINI_LEXER__H
#define MINI_LEXER__H

#include <string.h>
#include <stddef.h>


#ifdef _ML_DEBUG
#  include <stdio.h>
#  define fprintd(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)
#  define logf(format, ...) \
  fprintd ("%s:%d: "format"\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#  define fprintd(format, ...)
#  define logf(format, ...)
#endif

typedef struct Milexer_exp_
{
  const char *begin;
  const char *end;

  // int __tag; // internal nesting
} _exp_t;

/**
 *  Basic expression
 **/
typedef struct
{
  int len;
  const char **exp;
} Milexer_BEXP;

/**
 *  Advanced expression
 */
typedef struct
{
  int len;
  struct Milexer_exp_ *exp;
} Milexer_AEXP;


enum __buffer_state_t
  {
    /* space, \t, \r, \0, ... */
    SYN_DUMMY = 0,
    
    /* only backslah escape */
    SYN_ESCAPE,
    
    /* middle or beggining of a token */
    SYN_MIDDLE,

    /* when need to recover the previous puck */
    SYN_PUNC__,
    
    /* middle or beggining of a string or comment
       in this mode, nothing causes change of state
       but the specified end of expression */
    SYN_NO_DUMMY,
    SYN_NO_DUMMY__, /* when the prefix needs to be recovered */
    
    SYN_CHUNK, /* handling fragmentation */
    SYN_DONE /* token is ready */
  };

const char *milexer_buf_state[] = {
  [SYN_DUMMY]                   = "dummy",
  [SYN_ESCAPE]                  = "escape",
  [SYN_MIDDLE]                  = "inner token",
  [SYN_NO_DUMMY]                = "inner exp",
  [SYN_NO_DUMMY__]              = "recover exp",
  [SYN_PUNC__]                  = "recover punc",
  [SYN_DONE]                    = "done",
};

enum milexer_state_t
  {
    NEXT_MATCH = 0, /* got a token */
    
    /* no enough space left in the token's buffer
       keep reading the rest of the chunks */
    NEXT_CHUNK,
    
    /* parser has encountered a zero-byte \0
       you might want to end it up */
    NEXT_ZTERM,
    
    /* only in lazy loading
       you need to load the rest of your stuff */
    NEXT_NEED_LOAD,
    
    NEXT_END, /* nothing to do, end of parsing */
    NEXT_ERR, /* token buffer is null */
  };

const char *milexer_state_cstr[] = {
  [NEXT_MATCH]                   = "match",
  [NEXT_CHUNK]                   = "chunk",
  [NEXT_ZTERM]                   = "zero terminated",
  [NEXT_NEED_LOAD]               = "lazy load",
  [NEXT_END]                     = "eof parsing",
  [NEXT_ERR]                     = "error"
};

enum milexer_token_t
  {
    TK_NOT_SET = 0,

    TK_PUNCS,
    TK_KEYWORD,
    
    /* expression might be anything,
       strings like: "xxx" or stuff like: (xxx) or {xxx} */
    TK_EXPRESSION,
    
    /* basic comments (single line) */
    TK_BCOMMENT,
    
    /* advanced comments (multi-line comments) */
    TK_ACOMMENT,
  };

const char *milexer_token_type_cstr[] = {
  [TK_NOT_SET]                        = "NAN", /* unreachable */
  [TK_PUNCS]                          = "punctuation",
  [TK_KEYWORD]                        = "keyword",
  [TK_EXPRESSION]                     = "expression",
  [TK_BCOMMENT]                       = "comment",
  [TK_ACOMMENT]                       = "comment",
};

typedef struct
{
  enum milexer_token_t type;
  int id; /* when keywords is not null */

  /**
   *  when the token is an expression,
   *  set it 1 to only get inside of the expression
   **/
  char inner;
  /**
   *  ignore the space delimiter
   *  when it's 1, space is no longer a token delimiter
   **/
  char ignore_space;

  /**
   *  user of this library, allocates and frees
   *  the temporary buffer @str of length @len+1
   **/
  char *cstr;
  size_t len;

  /** internal **/
  size_t __idx;
  /* TODO: for nesting, these should be arrays */
  int __last_exp_idx;
  int __last_punc_idx;
} Milexer_Token;

#define TOKEN_IS_KNOWN(t) ((t)->id >= 0)
#define TOKEN_ALLOC(n) (Milexer_Token){.cstr=malloc (n+1), .len=n}
#define TOKEN_FREE(t) if ((t)->cstr) {free ((t)->cstr);}

/* internal */
#define __get_last_exp(ml, res) \
  ((ml)->expression.exp + (res)->__last_exp_idx)
#define __get_last_punc(ml, res) \
  ((ml)->puncs.exp[(res)->__last_punc_idx])

typedef struct
{
  /* configs */
  int eof_lazy; // End of Lazy loading
  enum __buffer_state_t state, prev_state;

  /* buffer & index & capacity */
  const char *buffer;
  size_t len, idx;
} Milexer_Slice;

typedef struct Milexer_t
{
  /* lazy mode */
  int lazy;

  /* configuration */
  Milexer_BEXP escape;    // Not implemented
  Milexer_BEXP puncs;
  Milexer_BEXP keywords;
  Milexer_AEXP expression;
  Milexer_BEXP b_comment; // Not implemented
  Milexer_AEXP a_comment; // Not implemented

  /**
   *  to retrieve the next token
   *  @src is the input buffer source and the result
   *  will be stored into @t
   *  handling memory of @src and @t is up to user of this library
   */
  int (*next)(const struct Milexer_t *,
              Milexer_Slice *src, Milexer_Token *t);
} Milexer;

#define GEN_lenof(arr) (sizeof (arr) / sizeof ((arr)[0]))
#define GEN_CFG(exp_ptr) {.exp=exp_ptr, .len=GEN_lenof (exp_ptr)}

/* initialize */
int milexer_init (Milexer *);


#ifdef ML_IMPLEMENTATION
/**
 **  Internal functions
 **  Only use Milexer.next()
 **/
static inline char *
__handle_puncs (const Milexer *ml, const Milexer_Slice *src,
                Milexer_Token *res)
{
  if (res->cstr[res->__idx - 1] < ' ')
    {
      return res->cstr + (res->__idx - 1);
    }

  switch (src->state)
    {
    case SYN_ESCAPE:
    case SYN_NO_DUMMY:
    case SYN_NO_DUMMY__:
      return NULL;

    default:
      char *p = res->cstr + (res->__idx - 1);
      if (*p < ' ' || (*p == ' ' && res->ignore_space == 0))
        {
          return p;
        }
      int longest_match_idx = -1;
      size_t longest_match_len = 0;
      for (int i=0; i < ml->puncs.len; ++i)
        {
          const char *punc = ml->puncs.exp[i];
          size_t len = strlen (punc);
          if (res->__idx < len)
            continue;
          char *p = res->cstr + res->__idx - len;
          if (strncmp (punc, p, len) == 0)
            {
              if (len >= longest_match_len)
                {
                  longest_match_len = len;
                  longest_match_idx = i;
                }
            }
        }
      if (longest_match_idx != -1)
        {
          res->__last_punc_idx = longest_match_idx;
          res->id = longest_match_idx;
          return res->cstr + (res->__idx - longest_match_len);
        }
      return NULL;
    }
}

static inline char *
__handle_expression (const Milexer *ml, const Milexer_Slice *src,
                     Milexer_Token *res) 
{
#define Return(n) do {                          \
    if (n) {                                    \
      logf ("match, d[.%lu]={%.*s} @%s",        \
            res->__idx,                         \
            (int)res->__idx,                    \
            res->cstr,                          \
            milexer_buf_state[ml->state]);      \
    } return n;                                 \
  } while (0)

  /* strstr function works with \0 */
  res->cstr[res->__idx] = '\0';
  
  switch (src->state)
    {
    case SYN_ESCAPE:
      Return (res->cstr + res->__idx);

    case SYN_NO_DUMMY:
    case SYN_NO_DUMMY__:
      /* looking for closing, O(1) */
        {
          _exp_t *e = __get_last_exp (ml, res);
          size_t len = strlen (e->end);

          if (res->__idx < len)
            break;
          char *p = res->cstr + res->__idx - len;
          if (strncmp (p, e->end, len) == 0)
            {
              res->id = res->__last_exp_idx;
              Return (p);
            }
        }
       Return (NULL);
      break;

    default:
      /* looking for opening, O(ml->expression.len) */
      for (int i=0; i < ml->expression.len; ++i)
        {
          char *p;
          _exp_t *e = ml->expression.exp + i;
          if ((p = strstr (res->cstr, e->begin)))
            {
              if (p == res->cstr && src->state == SYN_MIDDLE)
                Return (NULL);
              res->__last_exp_idx = i;
              Return (p);
            }
        }
      Return (0);
      break;
    }

#undef Return
  return NULL;
}

static inline void
__handle_token_id (const Milexer *ml, Milexer_Token *res)
{
  if (res->type != TK_KEYWORD)
    return;
  if (ml->keywords.len > 0)
    {
      for (int i=0; i < ml->keywords.len; ++i)
        {
          const char *p = ml->keywords.exp[i];
          if (strcmp (p, res->cstr) == 0)
            {
              res->id = i;
              return;
            }
        }
      res->id = -1;
    }
}


static int
__next_token (const Milexer *ml, Milexer_Slice *src,
              Milexer_Token *res)
{
  (void)(ml);
  (void)(src);
  (void)(res);
  return 0;
}

static int
__next_token_lazy (const Milexer *ml, Milexer_Slice *src,
                   Milexer_Token *res)
{
#define LD_STATE(src) (src)->state = (src)->prev_state
#define ST_STATE(slice, new_state) do {         \
    (slice)->prev_state = (slice)->state;       \
    (slice)->state = new_state;                 \
  } while (0)

  if (res->cstr == NULL || res->len <= 0)
    return NEXT_ERR;
  
  if (src->state == SYN_NO_DUMMY__)
    {
      if (res->__last_exp_idx != -1 && res->inner == 0)
        {
          /* certainly the token type is expression */
          res->type = TK_EXPRESSION;
          const char *src = __get_last_exp (ml, res)->begin;
          char *__p = mempcpy (res->cstr, src, strlen (src));
          res->__idx += __p - res->cstr;
        }
      src->state = SYN_NO_DUMMY;
    }
  else if (src->state == SYN_PUNC__)
    {
      const char *lp = __get_last_punc (ml, res);
      *((char *)mempcpy (res->cstr, lp, strlen (lp))) = '\0';

      LD_STATE (src);
      res->type = TK_PUNCS;
      res->id = res->__last_punc_idx;
      return NEXT_MATCH;
    }
    
  if (src->idx > src->len)
    {
      if (src->eof_lazy)
        return NEXT_END;
      src->idx = 0;
      return NEXT_NEED_LOAD;
    }

  res->type = TK_NOT_SET;
  for (unsigned char p; src->idx < src->len; )
    {
      char *__startof_exp, *__startof_punc;
      
      p = src->buffer[src->idx++];
      res->cstr[res->__idx++] = p;

      //-- handling expressions --------//
      if ((__startof_exp = __handle_expression (ml, src, res)))
        {
          res->type = TK_EXPRESSION;
          if (src->state == SYN_ESCAPE)
            {
              LD_STATE (src);
            }
          else if (src->state == SYN_NO_DUMMY)
            {
              if (res->inner)
                *__startof_exp = '\0';
              ST_STATE (src, SYN_DONE);
              res->cstr[res->__idx] = 0;
              res->__idx = 0;
              return NEXT_MATCH;
            }
          else
            {
              /* begginign of an expression */
              if (__startof_exp == res->cstr)
                {
                  if (res->inner)
                    res->__idx = 0;
                  ST_STATE (src, SYN_NO_DUMMY);
                }
              else
                {
                  /**
                   *  xxx`expression`yyy
                   *  the expression is adjacent to another token
                   */
                  ST_STATE (src, SYN_NO_DUMMY__);
                  *__startof_exp = 0;
                  res->__idx = 0;
                  res->type = TK_KEYWORD;
                  __handle_token_id (ml, res);
                  return NEXT_MATCH;
                }
            }
        }
      //-- handling escape -------------//
      else if (p == '\\')
        {
          ST_STATE (src, SYN_ESCAPE);
        }
      //-- handling puncs --------------//
      else if ((__startof_punc = __handle_puncs (ml, src, res)))
        {
          if (*__startof_punc <= ' ')
            {
              /* normal token */
              if (res->__idx > 1)
                {
                  ST_STATE (src, SYN_DUMMY);
                  res->type = TK_KEYWORD;
                  res->cstr[res->__idx - 1] = '\0';
                  res->__idx = 0;
                  __handle_token_id (ml, res);
                  if (p == '\0')
                    return NEXT_ZTERM;
                  return NEXT_MATCH;
                }
              else
                res->__idx = 0;
            }
          else
            {
              /* punc */
              const char *p = __get_last_punc (ml, res);
              size_t len = strlen (p);
              if (len == res->__idx)
                {
                  /* just a simple punc */
                  ST_STATE (src, SYN_DUMMY);
                  res->type = TK_PUNCS;
                  res->cstr[res->__idx] = '\0';
                  res->__idx = 0;
                  return NEXT_MATCH;
                }
              else
                {
                  /* the punc have got some adjacents */
                  ST_STATE (src, SYN_PUNC__);
                  *__startof_punc = '\0';
                  res->__idx = 0;
                  res->type = TK_KEYWORD;
                  __handle_token_id (ml, res);
                  return NEXT_MATCH;
                }
            }
        }
      //-- detect & reset chunks -------//
      if (res->__idx == res->len)
        {
          if (res->type == TK_NOT_SET ||
              src->state == SYN_DUMMY || src->state == SYN_DONE)
            {
              /**
               *  we assume your keywords are smaller than
               *  the length of res->cstr buffer
               */
              res->id = -1;
              res->type = TK_KEYWORD;
            }
          /* max token len reached */
          ST_STATE (src, SYN_CHUNK);
          res->cstr[res->__idx + 1] = '\0';
          res->__idx = 0;
          return NEXT_CHUNK;
        }
      if (src->state == SYN_CHUNK)
        LD_STATE (src);
      //--------------------------------//
    }

  if (src->eof_lazy)
    {
      if (res->__idx > 1)
        {
          if (res->type == TK_NOT_SET)
            {
              res->type = TK_KEYWORD;
              __handle_token_id (ml, res);
            }
          return NEXT_END;
        }
      else if (res->__idx == 1 && *res->cstr > ' ')
        {
          res->type = TK_KEYWORD;
          return NEXT_END;
        }
      else
        {
          res->type = TK_NOT_SET;
          return NEXT_END;
        }
    }
  src->idx = 0;
  return NEXT_NEED_LOAD;
}


int
milexer_init (Milexer *ml)
{
  if (ml->lazy)
    {
      /* lazy loading */
      ml->next = __next_token_lazy;
    }
  else
    {
      /**
       *  assume the entire data is loaded
       *  into ml->src->buffer
       */
      ml->next = __next_token;
    }

  return 0;
}

#endif /* ML_IMPLEMENTATION */

#undef logf
#undef fprintd
#endif /* MINI_LEXER__H */


/**
 **  Test1 Program
 **/
#ifdef ML_EXAMPLE_1
#include <stdio.h>
#include <stdlib.h>
#include <readline/readline.h>

//-- The language ----------------------//
enum LANG
  {
    /* keywords */
    LANG_IF = 0,
    LANG_ELSE,
    LANG_FI,
    /* punctuations */
    PUNC_PLUS = 0,
    PUNC_MINUS,
    PUNC_MULT,
    PUNC_POW,
    PUNC_COMMA,
    PUNC_EQUAL,
    PUNC_NEQUAL,
    /* expressions */
    EXP_PAREN = 0,
    EXP_BRACE,
    EXP_STR,
    EXP_STR2,
  };
static const char *Keys[] = {
  [LANG_IF]         = "if",
  [LANG_ELSE]       = "else",
  [LANG_FI]         = "fi",
};
static const char *Puncs[] = {
  [PUNC_PLUS]       = "+",
  [PUNC_MINUS]      = "-",
  [PUNC_MULT]       = "*",
  [PUNC_POW]        = "^",
  [PUNC_COMMA]      = ",",
  [PUNC_EQUAL]      = "=", /* you cannot have "==" */
  [PUNC_NEQUAL]     = "!=", /* also "!===" */
};
static struct Milexer_exp_ Exp[] = {
  [EXP_PAREN]       = {"(", ")"},
  [EXP_BRACE]       = {"{", "}"},
  [EXP_STR]         = {"\"", "\""},
  [EXP_STR2]        = {"'", "'"},
};
static const char *puncs_cstr[] = {
  [PUNC_PLUS]       = "Plus",
  [PUNC_MINUS]      = "Minus",
  [PUNC_MULT]       = "Times",
  [PUNC_POW]        = "Power",
  [PUNC_COMMA]      = "Comma",
  [PUNC_EQUAL]      = "Equal",  /* you cannot have "==" */
  [PUNC_NEQUAL]     = "~Equal", /* also "!===" */
};
static const char *exp_cstr[] = {
  [EXP_PAREN]       = "(*)",
  [EXP_BRACE]       = "{*}",
  [EXP_STR]         = "\"*\"",
  [EXP_STR2]        = "'*'",
};
//--------------------------------------//


int
token_expression (Milexer *ml,
                  Milexer_Slice *src,
                  Milexer_Token *t)
{
  int ret = 0;
  while (ret != NEXT_END)
    {
      ret = ml->next (ml, src, t);
      switch (ret)
        {
        case NEXT_MATCH:
        case NEXT_ZTERM:
        case NEXT_END:
        case NEXT_CHUNK:
          if (t->type == TK_KEYWORD)
            printf ("%s", t->cstr);
          else if (t->type == TK_PUNCS && t->id == PUNC_COMMA)
            printf ("\n");
          break;

        case NEXT_NEED_LOAD:
          return NEXT_NEED_LOAD;
        }
    }
  return NEXT_END;
}

int
main (void)
{
  char *line = NULL;
  size_t n;
  /* input source */
  Milexer_Slice src = {0};
  /* token type */
  Milexer_Token t = TOKEN_ALLOC (32);
  t.inner = 1; /* get inner expressions */

  /* Milexer initialization */
  Milexer ml = {
    .lazy = 1,

    .puncs       = GEN_CFG (Puncs),
    .keywords    = GEN_CFG (Keys),
    .expression  = GEN_CFG (Exp),
  };
  milexer_init (&ml);

  while (1)
    {
      /* Get the next token */
      int ret = ml.next (&ml, &src, &t);
      switch (ret)
        {
        case NEXT_NEED_LOAD:
          if (line)
            free (line);
          line = readline (">>> ");
          if (line == NULL)
            {
              src.eof_lazy = 1;
              src.len = 0;
            }
          else
            {
              src.buffer = line;
              /**
               *  We are testing
               *  but is this safe ??
               */
              n = strlen (line);
              line[n] = '\n';
              src.len = n+1;
            }
          break;

        case NEXT_MATCH:
        case NEXT_CHUNK:
        case NEXT_ZTERM:
          {
            printf ("%.*s", 3, milexer_token_type_cstr[t.type]);
            
            if (TOKEN_IS_KNOWN (&t))
              {
                switch (t.type)
                  {
                  case TK_KEYWORD:
                    printf ("[*]  %s", t.cstr);
                    break;
                  case TK_PUNCS:
                    printf ("[*]  %s", puncs_cstr[t.id]);
                    break;
                  case TK_EXPRESSION:
                    if (t.id == EXP_PAREN)
                      {
                        puts (" Tokens:");
                        Milexer_Slice second_src = {0};
                        Milexer_Token tmp = TOKEN_ALLOC (32);
                        while (1)
                          {
                            second_src.buffer = t.cstr;
                            second_src.len = strlen (t.cstr);
                            /**
                             *  when inner parenthesis is not a chunk,
                             *  the parser should not expect more chunks
                             */
                            second_src.eof_lazy = (ret != NEXT_CHUNK);
                            /**
                             *  we wnat to allow space character
                             *  inside the `(xxx)` expression
                             */
                            tmp.ignore_space = 1;
                            
                            int _ret = token_expression (&ml, &second_src, &tmp);
                            if (_ret != NEXT_NEED_LOAD || ret != NEXT_CHUNK)
                              break;
                            /* load the rest of inner parenthesis */
                            ret = ml.next (&ml, &src, &t);
                          }
                        TOKEN_FREE (&tmp);
                      }
                    else
                      printf ("%s  %s", exp_cstr[t.id], t.cstr);
                    break;

                  default:
                    printf ("[#%d] `%s`", t.id, t.cstr);
                    break;
                  }
              }
            else
              printf ("[U] `%s`", t.cstr);

            if (ret == NEXT_CHUNK)
              puts ("    <-- chunk");
            else
              puts ("");
          }
          break;

        case NEXT_END:
        case NEXT_ERR:
          goto End_of_Main;
        }
    }

 End_of_Main:
  TOKEN_FREE (&t);
  if (line)
    free (line);

  return 0;
}

#endif /* ML_EXAMPLE_1 */
