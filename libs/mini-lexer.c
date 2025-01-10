/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

  Mini-Lexer is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  Mini-Lexer is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 *  file: mini-lexer.c
 *  created on: 16 Dec 2024
 *
 *  Minimal Lexer header-only library
 *  --Under Development--
 *
 *
 *
 *  Compilation:
 *   The test program:
 *     cc -O0 -ggdb -Wall -Wextra -Werror \
 *        -D ML_IMPLEMENTATION -D ML_TEST_1 \
 *        mini-lexer.c -o test.out
 *   The example program:
 *     pass `-D ML_EXAMPLE_1` instead of `ML_TEST_1`
 *
 *   Compilation Options:
 *     Debug Info:  define `-D_ML_DEBUG`
 */
#ifndef MINI_LEXER__H
#define MINI_LEXER__H

#include <string.h>
#include <stddef.h>
#include <stdbool.h>

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

    /* when recovering the previous punc is needed */
    SYN_PUNC__,
    
    /**
     *  Middle or beggining of an expression
     *  Only the corresponding suffix will change this state
     */
    SYN_NO_DUMMY,
    SYN_NO_DUMMY__, /* when recovering the prefix is needed */

    /* line or section is commented */
    SYN_COMM,
    SYN_ML_COMM, /* multi-line comments */
    
    SYN_CHUNK, /* handling fragmentation */
    SYN_DONE /* token is ready */
  };

const char *milexer_state_cstr[] = {
  [SYN_DUMMY]        = "dummy",
  [SYN_ESCAPE]       = "escape",
  [SYN_MIDDLE]       = "middle",
  [SYN_PUNC__]       = "punc__",
  [SYN_NO_DUMMY]     = "no_dummy",
  [SYN_NO_DUMMY__]   = "no_dummy__",
  [SYN_COMM]         = "comm",
  [SYN_ML_COMM]      = "ml_comm",
  [SYN_CHUNK]        = "chunk",
  [SYN_DONE]         = "done",
};

enum milexer_state_t
  {
    NEXT_MATCH = 0, /* got a token */
    
    /**
     *  Not enough space left in the token's buffer, 
     *  so the token was fragmented
     *  You will receive the remaining chunk(s)
     *  until you get a NEXT_MATCH return code
     */
    NEXT_CHUNK,
    
    /**
     *  The parser has encountered a zero-byte
     *  You might want to terminate parsing
     */
    NEXT_ZTERM,
    
    /**
     *  Only in lazy loading:
     *  Parsing the current slice source is done
     *  You need to load the remaining of data
     */
    NEXT_NEED_LOAD,
    
    NEXT_END, /* nothing to do, end of parsing */
    NEXT_ERR, /* error */
  };

#define NEXT_SHOULD_END(ret) \
  (ret == NEXT_END || ret == NEXT_ERR)
#define NEXT_SHOULD_LOAD(ret) \
  (ret == NEXT_NEED_LOAD || NEXT_SHOULD_END (ret))

const char *milexer_next_cstr[] = {
  [NEXT_MATCH]                   = "Match",
  [NEXT_CHUNK]                   = "Chunk",
  [NEXT_ZTERM]                   = "zero-byte",
  [NEXT_NEED_LOAD]               = "Load",
  [NEXT_END]                     = "END",
  [NEXT_ERR]                     = "Error"
};


#define __flag__(n) (1 << (n))
enum milexer_parsing_flag_t
  {
    /* Default behavior */
    PFLAG_DEFAULT = 0,

    /**
     *  To Retrieve the contents of expressions 
     *  without their prefix and suffix.
     */
    PFLAG_INEXP =      __flag__ (0),

    /**
     *  To allow space(s) in tokens
     *  The whitespace (0x20) character is no longer
     *  treated as a delimiter while using this flag
     */
    PFLAG_IGSPACE =    __flag__ (1),

    /**
     *  When `delim_ranges` is available, it will overwrite 
     *  the default delimiters (range 0, 0x20)
     *  To include them as well, use this flag
     */
    PFLAG_ALLDELIMS =  __flag__ (2),

    /**
     *  To retrive tokens of commented sections
     *  which are ignored by default
     */
    PFLAG_INCOMMENT =  __flag__ (3),
  };

enum milexer_token_t
  { 
    TK_NOT_SET = 0,
    TK_COMMENT, /* commented */

    /* usable tokens */
    TK_PUNCS,
    TK_KEYWORD,
    TK_EXPRESSION,
  };

const char *milexer_token_type_cstr[] = {
  [TK_NOT_SET]         = "NAN",
  [TK_PUNCS]           = "Punctuation",
  [TK_KEYWORD]         = "Keyword",
  [TK_EXPRESSION]      = "Expression",
  [TK_COMMENT]         = "Comment",
};

typedef struct
{
  enum milexer_token_t type;

  /**
   *  Index of the token in the corresponding
   *  Milexer configuration field, punc/keyword/...
   *  -1 means the token is not recognized
   */
  int id;

  /**
   *  The user of this library is responsible for allocating
   *  and freeing the buffer @cstr of length `@len + 1`
   *  `TOKEN_ALLOC` macro does this using malloc
   *  We guarantee that @cstr is always null-terminated
   */
  char *cstr;
  size_t len;

  /* Internal */
  size_t __idx;
} Milexer_Token;

#define TOKEN_IS_KNOWN(t) ((t)->id >= 0)
#define TOKEN_ALLOC(n) (Milexer_Token){.cstr=malloc (n+1), .len=n}
#define TOKEN_FREE(t) if ((t)->cstr) {free ((t)->cstr);}
#define TOKEN_DROP(t) ((t)->__idx = 0, (t)->type = TK_NOT_SET)

/* Internal macros */
#define __get_last_exp(ml, src) \
  ((ml)->expression.exp + (src)->__last_exp_idx)
#define __get_last_punc(ml, src) \
  ((ml)->puncs.exp[(src)->__last_punc_idx])


typedef struct
{
  /* lazy mode */
  int lazy;

  /* End of lazy loading */
  int eof_lazy;

  /* Internal state of the buffer, SYN_xxx */
  enum __buffer_state_t state, prev_state;

  /* buffer & index & capacity */
  const char *buffer;
  size_t cap, idx;

  /* Internal */
  /* TODO: for nesting, these should be arrays */
  int __last_exp_idx;
  int __last_punc_idx;
  const char *__last_comm;
} Milexer_Slice;

#define SET_SLICE(src, buf, n) \
  ((src)->buffer = buf, (src)->cap = n, (src)->idx = 0)
/* to indicate that lazy loading is over */
#define END_SLICE(src) ((src)->cap = 0, (src)->eof_lazy = 1)
/* set new state & load the previous state */
#define LD_STATE(src) (src)->state = (src)->prev_state
#define ST_STATE(slice, new_state) \
  ((slice)->prev_state = (slice)->state, (slice)->state = new_state)


typedef struct Milexer_t
{
  /* Configurations */
  Milexer_BEXP escape;    // Not implemented
  Milexer_BEXP puncs;
  Milexer_BEXP keywords;
  Milexer_AEXP expression;
  Milexer_BEXP b_comment;
  Milexer_AEXP a_comment;
  /**
   *  Delimiter ranges; each entry in this field
   *  must be 1 or 2-byte string, which defines a range
   *  of characters that will be treated as token delimiters
   *
   *  For example: "\x01\x12" describes the range [0x01, 0x12],
   *  while "\x42" represents a single character 0x42
   */
  Milexer_BEXP delim_ranges;

} Milexer;

#define GEN_LENOF(arr) (sizeof (arr) / sizeof ((arr)[0]))
#define GEN_MKCFG(exp_ptr) {.exp = exp_ptr, .len = GEN_LENOF (exp_ptr)}


/**
 **  Function definitions
 **  you shoud only call these functions
 **/

/**
 *  Milexer init, this function should be called
 *  at the beginning, before `next()`
 */
int milexer_init (Milexer *, bool lazy_mode);

/**
 *  To set the ID of keyword tokens
 *  @return 0 on success, -1 if not detected
 */
int ml_set_keyword_id (const Milexer *, Milexer_Token *t);

/**
 *  Retrieves the next token
 *
 *  @src is the input buffer source and
 *  The result will be stored in @t
 *
 *  Memory management for @src and @t is up to
 *  the user of this library
 *  @src and @t buffers *MUST* be distinct
 */
int ml_next (const Milexer *,
             Milexer_Slice *src, Milexer_Token *t,
             int flags);


/**
 **  Internal functions
 **/
#ifdef ML_IMPLEMENTATION

/* returns @p when @p is a delimiter, and -1 on null-byte */
static inline int
__detect_delim (const Milexer *ml, unsigned char p, int flags)
{
  if (p == 0)
    return -1;
  if (ml->delim_ranges.len == 0 || (flags & PFLAG_ALLDELIMS))
    {
      /* default delimiters */
      if (p < ' ' ||
          (p == ' ' && !(flags & PFLAG_IGSPACE)))
        {
          return p;
        }
    }
  if (ml->delim_ranges.len > 0 || (flags & PFLAG_ALLDELIMS))
    {
      for (int i=0; i < ml->delim_ranges.len; ++i)
        {
          const char *__p = ml->delim_ranges.exp[i];
          if (__p[1] != '\0')
            {
              if (p >= __p[0] && p <= __p[1])
                return p;
            }
          else if (p == __p[0])
            return p;
        }
    }
  return 0;
}

static inline char *
__detect_puncs (const Milexer *ml, Milexer_Slice *src,
                Milexer_Token *res)
{
  int longest_match_idx = -1;
  size_t longest_match_len = 0;
  res->cstr[res->__idx] = '\0';
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
      src->__last_punc_idx = longest_match_idx;
      res->id = longest_match_idx;
      return res->cstr + (res->__idx - longest_match_len);
    }
  return NULL;
}

static inline char *
__is_expression_suff (const Milexer *ml, Milexer_Slice *src,
                     Milexer_Token *tk)
{
  /* looking for closing, O(1) */
  _exp_t *e = __get_last_exp (ml, src);
  size_t len = strlen (e->end);

  if (tk->__idx < len)
    return NULL;;
  
  char *p = tk->cstr + tk->__idx;
  *p = '\0';
  p -= len;
  if (tk->__idx - len >= 1 && *p == '\\')
    return NULL;

  if (strncmp (p, e->end, len) == 0)
    {
      tk->id = src->__last_exp_idx;
      return p;
    }
  return NULL;
}

static inline char *
__is_mline_commented_suff (const Milexer *ml, Milexer_Slice *src,
                       Milexer_Token *tk)
{
  for (int i=0; i < ml->a_comment.len; ++i)
    {
      const char *pref = ml->a_comment.exp[i].end;
      size_t len = strlen (pref);
      char *__cstr = tk->cstr + tk->__idx - len;

      if (strncmp (pref, __cstr, len) == 0)
        {
          src->__last_comm = pref;
          return __cstr;
        }
    }
  return NULL;
}

static inline char *
__is_sline_commented_pref (const Milexer *ml, Milexer_Slice *src,
                       Milexer_Token *tk)
{
  for (int i=0; i < ml->b_comment.len; ++i)
    {
      const char *pref = ml->b_comment.exp[i];
      size_t len = strlen (pref);
      char *__cstr = tk->cstr + tk->__idx - len;

      if (strncmp (pref, __cstr, len) == 0)
        {
          src->__last_comm = pref;
          return __cstr;
        }
    }
  return NULL;
}

static inline char *
__is_mline_commented_pref (const Milexer *ml, Milexer_Slice *src,
                       Milexer_Token *tk)
{
  for (int i=0; i < ml->a_comment.len; ++i)
    {
      const char *pref = ml->a_comment.exp[i].begin;
      size_t len = strlen (pref);
      char *__cstr = tk->cstr + tk->__idx - len;

      if (strncmp (pref, __cstr, len) == 0)
        {
          src->__last_comm = pref;
          return __cstr;
        }
    }
  return NULL;
}

static inline char *
__is_expression_pref (const Milexer *ml, Milexer_Slice *src,
                     Milexer_Token *tk)
{
  /* looking for opening, O(ml->expression.len) */
  char *p;
  char *__cstr = tk->cstr;
  __cstr[tk->__idx] = '\0';
  if ((p = strrchr (__cstr, '\\')))
    {
      if ((size_t)(p - __cstr + 2) >= tk->__idx)
        return NULL;
      __cstr = p + 2;
    }
  for (int i=0; i < ml->expression.len; ++i)
    {
      _exp_t *e = ml->expression.exp + i;
      if ((p = strstr (__cstr, e->begin)))
        {
          src->__last_exp_idx = i;
          return p;
        }
    }
  return NULL;
}

int
ml_set_keyword_id (const Milexer *ml, Milexer_Token *res)
{
  if (res->type != TK_KEYWORD)
    return -1;
  if (ml->keywords.len > 0)
    {
      for (int i=0; i < ml->keywords.len; ++i)
        {
          const char *p = ml->keywords.exp[i];
          if (strcmp (p, res->cstr) == 0)
            {
              res->id = i;
              return 0;
            }
        }
      res->id = -1;
    }
  return -1;
}

int
ml_next (const Milexer *ml, Milexer_Slice *src,
                   Milexer_Token *tk, int flags)
{
  if (tk->cstr == NULL || tk->len <= 0 || tk->cstr == src->buffer)
    return NEXT_ERR;
  /* check end of src slice */
  if (src->idx >= src->cap)
    {
      if (src->eof_lazy)
        return NEXT_END;
      src->idx = 0;
      if (tk->__idx == 0)
        *tk->cstr = '\0';
      return NEXT_NEED_LOAD;
    }

  /* pre parsing */
  tk->type = TK_NOT_SET;
  switch (src->state)
    {
    case SYN_NO_DUMMY__:
      if (!(flags & PFLAG_INEXP) && src->__last_exp_idx != -1)
        {
          /* certainly the token type is expression */
          tk->type = TK_EXPRESSION;
          const char *le = __get_last_exp (ml, src)->begin;
          char *__p = mempcpy (tk->cstr, le, strlen (le));
          tk->__idx += __p - tk->cstr;
        }
      src->state = SYN_NO_DUMMY;
      break;

    case SYN_PUNC__:
      const char *lp = __get_last_punc (ml, src);
      *((char *)mempcpy (tk->cstr, lp, strlen (lp))) = '\0';

      LD_STATE (src);
      tk->type = TK_PUNCS;
      tk->id = src->__last_punc_idx;
      return NEXT_MATCH;
      break;

    case SYN_CHUNK:
      LD_STATE (src);
      break;

    case SYN_ML_COMM:
      if (src->__last_comm && (flags & PFLAG_INCOMMENT))
        {
          tk->type = TK_COMMENT;
          size_t len = strlen (src->__last_comm);
          *((char *) mempcpy (tk->cstr, src->__last_comm, len)) = 0;
          tk->__idx = len;
          src->__last_comm = NULL;
        }
      break;

    default:
      break;
    }

  /* parsing main logic */
  const char *p;
  char *dst = tk->cstr;
  for (; src->idx < src->cap; )
    {
      p = src->buffer + (src->idx++);
      dst = tk->cstr + (tk->__idx++);
      *dst = *p;
      
      //-- detect & reset chunks -------//
      if (tk->__idx == tk->len)
        {
          if ((src->state != SYN_COMM && src->state != SYN_ML_COMM)
              || (flags & PFLAG_INCOMMENT))
            {
              if (tk->type == TK_NOT_SET ||
                  src->state == SYN_DUMMY || src->state == SYN_DONE)
                {
                  /**
                   *  we assume your keywords are smaller than
                   *  the length of tk->cstr buffer
                   */
                  tk->id = -1;
                  if (src->state == SYN_COMM || src->state == SYN_ML_COMM)
                    {
                      tk->type = TK_COMMENT;
                    }
                  else
                    {
                      tk->type = TK_KEYWORD;
                      ml_set_keyword_id (ml, tk);
                    }
                }
              /* max token len reached */
              ST_STATE (src, SYN_CHUNK);
              tk->cstr[tk->__idx + 1] = '\0';
              tk->__idx = 0;
              return NEXT_CHUNK;
            }
          else
            tk->__idx = 0;
        }
      //--------------------------------//
      char *__ptr, c;
      /* logf ("'%c' - %s, %s", *p,
            milexer_state_cstr[src->state],
            milexer_token_type_cstr[tk->type]); */
      switch (src->state)
        {
        case SYN_ESCAPE:
          LD_STATE (src);
          break;

        case SYN_COMM:
          if (*p == '\n' || *p == '\r')
            {
              ST_STATE (src, SYN_DUMMY);
              tk->type = TK_COMMENT;
              tk->__idx = 0;
              if (flags & PFLAG_INCOMMENT)
                {
                  *(dst) = '\0';
                  return NEXT_MATCH;
                }
              else
                {
                  tk->__idx = 0;
                  *tk->cstr = '\0';
                }
            }
          break;

          case SYN_ML_COMM:
          if ((__ptr = __is_mline_commented_suff (ml, src, tk)))
            {
              ST_STATE (src, SYN_DUMMY);
              tk->__idx = 0;
              if (flags & PFLAG_INCOMMENT)
                {
                  *(dst + 1) = '\0';
                  tk->type = TK_COMMENT;
                  return NEXT_MATCH;
                }
            }
          break;
          
        case SYN_DUMMY:
          if ((__ptr = __is_sline_commented_pref (ml, src, tk)))
            {
              tk->type = TK_COMMENT;
              ST_STATE (src, SYN_COMM);
            }
          else if ((__ptr = __is_mline_commented_pref (ml, src, tk)))
            {
              tk->type = TK_COMMENT;
              ST_STATE (src, SYN_ML_COMM);
            }
          else if ((__ptr = __is_expression_pref (ml, src, tk)))
            {
              tk->type = TK_EXPRESSION;
              if (__ptr == tk->cstr)
                {
                  ST_STATE (src, SYN_NO_DUMMY);
                  if (flags & PFLAG_INEXP)
                    {
                      *__ptr = '\0';
                      tk->__idx = 0;
                    }
                }
              else
                {
                  ST_STATE (src, SYN_NO_DUMMY__);
                  *dst = '\0';
                  tk->__idx = 0;
                  return NEXT_MATCH;
                }
            }
          else if ((__ptr = __detect_puncs (ml, src, tk)))
            {
              tk->type = TK_PUNCS;
              *(__ptr + 1) = '\0';
              tk->__idx = 0;
              return NEXT_MATCH;
            }
          else if ((c = __detect_delim (ml, *p, flags)) == 0)
            {
              if (c == -1)
                {
                  tk->__idx = 0;
                  return NEXT_ZTERM;
                }
              src->state = SYN_MIDDLE;
            }
          else
            {
              tk->__idx = 0;
            }
          break;

        case SYN_MIDDLE:
          if ((__ptr = __is_sline_commented_pref (ml, src, tk)))
            {
              ST_STATE (src, SYN_COMM);
              if (__ptr == tk->cstr)
                {
                  break;
                }
              else
                {
                  if (tk->type == TK_NOT_SET)
                    {
                      tk->type = TK_KEYWORD; 
                      ml_set_keyword_id (ml, tk);
                    }
                  tk->__idx = 0;
                  *__ptr = 0;
                  return NEXT_MATCH;
                }
            }
          else if ((__ptr = __is_mline_commented_pref (ml, src, tk)))
            {
              ST_STATE (src, SYN_ML_COMM);
              if (__ptr == tk->cstr)
                {
                  break;
                }
              else
                {
                  if (tk->type == TK_NOT_SET)
                    {
                      tk->type = TK_KEYWORD; 
                      ml_set_keyword_id (ml, tk);
                    }
                  tk->__idx = 0;
                  *__ptr = 0;
                  return NEXT_MATCH;
                }
            }
          else if ((c = __detect_delim (ml, *p, flags)) != 0)
            {
              if (c == -1)
                {
                  tk->__idx = 0;
                  return NEXT_ZTERM;
                } 
              if (tk->__idx > 1)
                {
                  *dst = '\0';
                  if (tk->type == TK_NOT_SET)
                    {
                      tk->type = TK_KEYWORD; 
                      ml_set_keyword_id (ml, tk);
                    }
                  ST_STATE (src, SYN_DUMMY);
                  tk->__idx = 0;
                  return NEXT_MATCH;
                }
              else
                {
                  tk->__idx = 0;
                }
            }
          else if (__detect_puncs (ml, src, tk))
            {
              const char *_punc = __get_last_punc (ml, src);
              size_t n = strlen (_punc);
              if (n == tk->__idx)
                {
                  tk->type = TK_PUNCS;
                  tk->__idx = 0;
                  return NEXT_MATCH;
                }
              else
                {
                  tk->type = TK_KEYWORD;
                  ml_set_keyword_id (ml, tk);
                  *(dst - n + 1) = '\0';
                  tk->__idx = 0;
                  ST_STATE (src, SYN_PUNC__);
                  return NEXT_MATCH;
                }
            }
          else if ((__ptr = __is_expression_pref (ml, src, tk)))
            {
              if (__ptr != tk->cstr)
                {
                  ST_STATE (src, SYN_NO_DUMMY__);
                  *(__ptr) = '\0';
                  tk->__idx = 0;
                  tk->type = TK_KEYWORD;
                  ml_set_keyword_id (ml, tk);
                  return NEXT_MATCH;
                }
              else
                {
                  tk->type = TK_EXPRESSION;
                  if (flags & PFLAG_INEXP)
                    tk->__idx = 0;
                  ST_STATE (src, SYN_NO_DUMMY);
                }
            }
          break;

        case SYN_NO_DUMMY:
          if ((__ptr = __is_expression_suff (ml, src, tk)))
            {
              tk->type = TK_EXPRESSION;
              if (flags & PFLAG_INEXP)
                *__ptr = '\0';
              ST_STATE (src, SYN_DUMMY);
              tk->__idx = 0;
              *(dst + 1) = '\0';
              return NEXT_MATCH;
            }
          break;

        default:
          break;
        }

      /* check for escape */
      if (*p == '\\')
        ST_STATE (src, SYN_ESCAPE);
    }

  /* check for end of lazy loading */
  if (src->eof_lazy)
    {
      if (tk->__idx >= 1)
        {
          if (tk->type == TK_NOT_SET)
            {
              tk->type = TK_KEYWORD;
              ml_set_keyword_id (ml, tk);
            }
          return NEXT_END;
        }
       else
        {
          tk->type = TK_NOT_SET;
          return NEXT_END;
        }
    }
  /* end of src slice */
  src->idx = 0;
  *(dst + 1) = '\0';
  if (tk->type == TK_NOT_SET)
    {
      tk->type = TK_KEYWORD; 
      ml_set_keyword_id (ml, tk);
    }
  return NEXT_NEED_LOAD;
}

#endif /* ML_IMPLEMENTATION */

#undef logf
#undef fprintd
#endif /* MINI_LEXER__H */



/**
 **  Common headers for both
 **  example_1 and test_1 programs
 **/
#if defined (ML_EXAMPLE_1) || defined (ML_TEST_1)
# include <stdio.h>
# include <stdlib.h>
# include <stdbool.h>

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
    // PUNC_DIV,
    PUNC_COMMA,
    PUNC_EQUAL,
    PUNC_NEQUAL,
    /* expressions */
    EXP_PAREN = 0,
    EXP_BRACE,
    EXP_STR,
    EXP_STR2,
    EXP_LONG,
    /* single-line comments */
    COMM_1 = 0,
    COMM_2,
    /* multi-line comments */
    COMM_ml = 0,
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
  // [PUNC_DIV]        = "/", otherwise we cannot have /*comment*/
  [PUNC_COMMA]      = ",",
  [PUNC_EQUAL]      = "=", /* you cannot have "==" */
  [PUNC_NEQUAL]     = "!=", /* also "!===" */
};
static struct Milexer_exp_ Exp[] = {
  [EXP_PAREN]       = {"(", ")"},
  [EXP_BRACE]       = {"{", "}"},
  [EXP_STR]         = {"\"", "\""},
  [EXP_STR2]        = {"'", "'"},
  [EXP_LONG]        = {"<<", ">>"},
};
static const char *Comm[] = {
  [COMM_1]          = "#",
  [COMM_2]          = "//",
};
static struct Milexer_exp_ Comm_ml[] = {
  [COMM_ml]         = {"/*", "*/"},
};
//-- Milexer main configuration --------//
static Milexer ml = {
    .puncs       = GEN_MKCFG (Puncs),
    .keywords    = GEN_MKCFG (Keys),
    .expression  = GEN_MKCFG (Exp),
    .b_comment   = GEN_MKCFG (Comm),
    .a_comment   = GEN_MKCFG (Comm_ml),
  };
//--------------------------------------//
#endif /* defined (ML_EXAMPLE_1) || defined (ML_TEST_1) */



/**
 **  Example_1 program
 **/
#ifdef ML_EXAMPLE_1
static const char *exp_cstr[] = {
  [EXP_PAREN]       = "(*)",
  [EXP_BRACE]       = "{*}",
  [EXP_STR]         = "\"*\"",
  [EXP_STR2]        = "'*'",
};

static const char *puncs_cstr[] = {
  [PUNC_PLUS]       = "Plus",
  [PUNC_MINUS]      = "Minus",
  [PUNC_MULT]       = "Times",
  // [PUNC_DIV]        = "Division",
  [PUNC_COMMA]      = "Comma",
  [PUNC_EQUAL]      = "Equal",
  [PUNC_NEQUAL]     = "~Equal",
};


int
main (void)
{
  /* input source */
  Milexer_Slice src = {.lazy = true};
  /* token type */
  Milexer_Token tk = TOKEN_ALLOC (32);

  char *line = NULL;
  const int flg = PFLAG_INEXP;
  for (int ret = 0; !NEXT_SHOULD_END (ret); )
    {
      /* Get the next token */
      ret = ml_next (&ml, &src, &tk, flg);
      switch (ret)
        {
        case NEXT_NEED_LOAD:
          ssize_t n;
          printf (">>> ");
          if ((n = getline (&line, (size_t *)&n, stdin)) < 0)
            END_SLICE (&src);
          else
            SET_SLICE (&src, line, n);
          break;

        case NEXT_MATCH:
        case NEXT_CHUNK:
        case NEXT_ZTERM:
          {
            /* print the type of the token @t */
            printf ("%.*s", 3, milexer_token_type_cstr[tk.type]);
            switch (tk.type)
              {
              case TK_KEYWORD:
                printf ("[%c]  `%s`", TOKEN_IS_KNOWN (&tk) ?'*':'-', tk.cstr);
                break;
              case TK_PUNCS:
                printf ("[*]   %s", puncs_cstr[tk.id]);
                break;
              case TK_EXPRESSION:
                printf ("%s", exp_cstr[tk.id]);
                if (tk.id != EXP_PAREN) /* is not parenthesis */
                  printf ("   `%s`", tk.cstr);
                else
                  {
                    /**
                     *  This example parses the contents of expressions
                     *  enclosed in parenthesis: given: '(xxx)' -> parsing 'xxx'
                     *  We use `t.cstr` as the input buffer in `second_src`
                     *  Therefore, we must allocate a new token, otherwise
                     *  the parser will fail to parse the input and store
                     *  the result in the same location simultaneously
                     */
                    puts (":");
                    Milexer_Slice second_src = {0};
                    Milexer_Token tmp = TOKEN_ALLOC (32);
                    for (;;)
                      {
                        /**
                         *  When the inner parentheses token is not a chunk,
                         *  the parser should not expect additional chunks
                         */
                        second_src.eof_lazy = (ret != NEXT_CHUNK);
                        /* prepare the new input source buffer */
                        SET_SLICE (&second_src, tk.cstr, strlen (tk.cstr));

                        for (int _ret = 0; !NEXT_SHOULD_LOAD (_ret); )
                          {
                            /* allow space character in tokens */
                            _ret = ml_next (&ml, &second_src, &tmp, PFLAG_IGSPACE);

                            if (tmp.type == TK_KEYWORD)
                              printf ("%s", tmp.cstr);
                            else if (tmp.type == TK_PUNCS && tmp.id == PUNC_COMMA)
                              puts ("");
                          }

                        /* load the remaining chunks of the inner parentheses, if any */
                        if (ret != NEXT_CHUNK)
                          break;
                        ret = ml_next (&ml, &src, &tk, flg);
                      }
                    TOKEN_FREE (&tmp);
                  }
                break;

              default: /* t.type */
                break;
              }

            /* detecting chunk */
            if (ret == NEXT_CHUNK)
              puts ("    <-- chunk");
            else
              puts ("");
          }
          break;
        }
    }

  TOKEN_FREE (&tk);
  if (line)
    free (line);
  puts ("Bye");

  return 0;
}
#endif /* ML_EXAMPLE_1 */



/**
 **  test_1 program
 **/
#ifdef ML_TEST_1

typedef struct
{
  int test_number;
  int parsing_flags;
  char *input;
  const Milexer_Token* etk; /* expected tokens */
} test_t;


/* testing token */
static Milexer_Token tk;
static Milexer_Slice src = {0};

int
do_test__H (test_t *t, Milexer_Slice *src)
{
#define Return(n, format, ...)                      \
  if (n == -1) {                                    \
    puts ("pass");                                  \
  } else {                                          \
    printf ("fail!\n test %d:%d:  "format"\n",      \
            t->test_number, n, ##__VA_ARGS__);      \
  } return n;

  int ret = 0, counter = 1;
  for (const Milexer_Token *tcase = t->etk;
       tcase != NULL && tcase->cstr != NULL; ++tcase, ++counter)
    {
      if (NEXT_SHOULD_END (ret))
        {
          Return (counter, "unexpected NEXT_END");
        }

      ret = ml_next (&ml, src, &tk, t->parsing_flags);
#ifdef _ML_DEBUG
      printf (" test %d:%d: expect `%s`... ", t->test_number, counter, tcase->cstr);
#endif

      if (strcmp (tcase->cstr, tk.cstr) != 0)
        {
          Return (counter, "token `%s` != expected `%s`",
                  tk.cstr, tcase->cstr);
        }
      if (tcase->type != TK_NOT_SET && tcase->type != tk.type)
        {
          Return (counter, "token type `%s` != expected `%s`",
                  milexer_token_type_cstr[tk.type],
                  milexer_token_type_cstr[tcase->type]);
        }
      if (ret == NEXT_NEED_LOAD && src->eof_lazy)
        {
          Return (counter, "unexpected NEXT_NEED_LOAD");
        }

#ifdef _ML_DEBUG
      printf ("pass\n");
#endif

    }

  Return (-1, "");
#undef Return
}

int
do_test (test_t *t, const char *msg, Milexer_Slice *src)
{
  int ret;
#ifdef _ML_DEBUG
  printf ("Test #%d: %s\n", t->test_number, msg);
#else
  printf ("Test #%d: %s... ", t->test_number, msg);
#endif

  SET_SLICE (src, t->input, strlen (t->input));
  if ((ret = do_test__H (t, src)) != -1)
    return ret;
  return -1;
}

int
main (void)
{
#define DO_TEST(test, msg)                         \
  if (do_test (test, msg, &src) != -1)             \
    { ret = 1; goto eo_main; }

  test_t t = {0};
  int ret = 0;
  tk = TOKEN_ALLOC (16);

  puts ("-- elementary tests -- ");
  {
    t = (test_t) {
      .test_number = 1,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "aa bb ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "aa"},
        {.type = TK_KEYWORD, .cstr = "bb"},
        {0}
      }};
    DO_TEST (&t, "space delimiter");
    
    /* this test does not have a tailing delimiter
       and the parser must continue reading */
    t = (test_t) {
      .test_number = 2,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "ccc",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "ccc"},
        {0}
      }};
    DO_TEST (&t, "after delimiter");

    t = (test_t) {
      .test_number = 3,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "xxx    ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "cccxxx"},
        {0}
      }};
    DO_TEST (&t, "after no delimiter");
  }

  puts ("-- long tokens --");
  {
    t = (test_t) {
      .test_number = 4,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "aaaaaaaaaaaaaabcdefghi",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "aaaaaaaaaaaaaabc"},
        {.type = TK_KEYWORD, .cstr = "defghi"},
        {0}
      }};
    DO_TEST (&t, "after load recovery");

    t = (test_t) {
      .test_number = 5,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "6789abcdef\t",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "defghi6789abcdef"},
        {0}
      }};
    DO_TEST (&t, "load after fragmentation");
  }

  puts ("-- expressions & punctuations --");
  {
    t = (test_t) {
      .test_number = 6,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA + BBB (te st) ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA"},
        {.type = TK_PUNCS,      .cstr = "+"},
        {.type = TK_KEYWORD,    .cstr = "BBB"},
        {.type = TK_EXPRESSION, .cstr = "(te st)"},
        {0}
      }};
    DO_TEST (&t, "basic puncs & expressions");

    t = (test_t) {
      .test_number = 7,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "()AAA+{a string . }(t e s t)",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION, .cstr = "()"},
        {.type = TK_KEYWORD,    .cstr = "AAA"},
        {.type = TK_PUNCS,      .cstr = "+"},
        {.type = TK_EXPRESSION, .cstr = "{a string . }"},
        {.type = TK_EXPRESSION, .cstr = "(t e s t)"},
        {0}
      }};
    DO_TEST (&t, "adjacent puncs & expressions");

    /* the previous test didn't have tailing delimiter,
       but as it was an expression, the parser must
       treat this one as a separate token */
    t = (test_t) {
      .test_number = 8,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AA!=BB!= CC !=DD",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,      .cstr = "AA"},
        {.type = TK_PUNCS,        .cstr = "!="},
        {.type = TK_KEYWORD,      .cstr = "BB"},
        {.type = TK_PUNCS,        .cstr = "!="},
        {.type = TK_KEYWORD,      .cstr = "CC"},
        {.type = TK_PUNCS,        .cstr = "!="},
        {.type = TK_KEYWORD,      .cstr = "DD"},
        {0}
      }};
    DO_TEST (&t, "multi-character puncs");
    
    t = (test_t) {
      .test_number = 9,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "!= EEE ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,      .cstr = "DD"},
        {.type = TK_PUNCS,        .cstr = "!="},
        {.type = TK_KEYWORD,      .cstr = "EEE"},
        {0}
      }};
    DO_TEST (&t, "punc after load");

    /* long expression prefix & suffix */
    t = (test_t) {
      .test_number = 10,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "aa<<e x>>+<< AA>> <<BB >>",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "aa"},
        {.type = TK_EXPRESSION,    .cstr = "<<e x>>"},
        {.type = TK_PUNCS,         .cstr = "+"},
        {.type = TK_EXPRESSION,    .cstr = "<< AA>>"},
        {.type = TK_EXPRESSION,    .cstr = "<<BB >>"},
        {0}
      }};
    DO_TEST (&t, "expressions with long prefix & suffix");
 
    /* fragmented expressions */
    t = (test_t) {
      .test_number = 11,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "<<0123456789a b c d e f>><<>>",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION,    .cstr = "<<0123456789a b "},
        {.type = TK_EXPRESSION,    .cstr = "c d e f>>"},
        {.type = TK_EXPRESSION,    .cstr = "<<>>"},
        {0}
      }};
    DO_TEST (&t, "inner long expressions");
  }

  puts ("-- parser flags --");
  {
    t = (test_t) {
      .test_number = 12,
      .parsing_flags = PFLAG_IGSPACE,
      .input = "a b c (x y z)  de f\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "a b c "},
        {.type = TK_EXPRESSION,    .cstr = "(x y z)"},
        {.type = TK_KEYWORD,       .cstr = "  de f"},
        {0}
      }};
    DO_TEST (&t, "ignore space flag");
    
    t = (test_t) {
      .test_number = 13,
      .parsing_flags = PFLAG_INEXP,
      .input = "AA'++'{ x y z}(test 2 . )",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AA"},
        {.type = TK_EXPRESSION,    .cstr = "++"},
        {.type = TK_EXPRESSION,    .cstr = " x y z"},
        {.type = TK_EXPRESSION,    .cstr = "test 2 . "},
        {0}
      }};
    DO_TEST (&t, "inner expression flag");
    
    t = (test_t) {
      .test_number = 14,
      .parsing_flags = PFLAG_INEXP,
      .input = "<<o n e>><<t w o>> <<x y z >><<>>",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION,    .cstr = "o n e"},
        {.type = TK_EXPRESSION,    .cstr = "t w o"},
        {.type = TK_EXPRESSION,    .cstr = "x y z "},
        {.type = TK_EXPRESSION,    .cstr = ""},
        {0}
      }};
    DO_TEST (&t, "inner long expressions");
  }

  puts ("-- custom delimiters --");
  {
    /* making `.`,`@` and `0`,...,`9` delimiters */
    const char *delims[] = {".", "09", "@"};
    ml.delim_ranges = (Milexer_BEXP)GEN_MKCFG (delims);
    {
      t = (test_t) {
        .test_number = 15,
        .parsing_flags = PFLAG_INEXP,
        .input = "a@b cde0123 test.1xyz42",
        .etk = (Milexer_Token []){
          {.type = TK_KEYWORD,    .cstr = "a"},
          {.type = TK_KEYWORD,    .cstr = "b cde"},
          {.type = TK_KEYWORD,    .cstr = " test"},
          {.type = TK_KEYWORD,    .cstr = "xyz"},
          {0}
        }};
      DO_TEST (&t, "basic custom delimiter");

      t = (test_t) {
        .test_number = 16,
        .parsing_flags = PFLAG_ALLDELIMS,
        .input = "a b cde0123 test.1xyz42",
        .etk = (Milexer_Token []){
          {.type = TK_KEYWORD,    .cstr = "a"},
          {.type = TK_KEYWORD,    .cstr = "b"},
          {.type = TK_KEYWORD,    .cstr = "cde"},
          {.type = TK_KEYWORD,    .cstr = "test"},
          {.type = TK_KEYWORD,    .cstr = "xyz"},
          {0}
        }};
      DO_TEST (&t, "with all delimiters flag");
    }
    /* unset the custom delimiters */
    ml.delim_ranges = (Milexer_BEXP){0};
  }

  puts ("-- escape --");
  {
    t = (test_t) {
      .test_number = 17,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "\\(xx(aa\\)bb)\\)yyy ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "\\(xx"},
        {.type = TK_EXPRESSION, .cstr = "(aa\\)bb)"},
        {.type = TK_KEYWORD,    .cstr = "\\)yyy"},
        {0}
      }};
    DO_TEST (&t, "basic escape in expressions");
      
    t = (test_t) {
      .test_number = 18,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "(xx\\))(aa\\)bb \\)cc\\) dd \\) )",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION,    .cstr = "(xx\\))"},
        {.type = TK_EXPRESSION,    .cstr = "(aa\\)bb \\)cc\\) d"},
        {.type = TK_EXPRESSION,    .cstr = "d \\) )"},
        {0}
      }};
    DO_TEST (&t, "complex escape in expressions");

    t = (test_t) {
      .test_number = 19,
      .parsing_flags = PFLAG_INEXP,
      .input = "(\\(\\(\\(\\)test)\\(\\((yy)",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION,    .cstr = "\\(\\(\\(\\)test"},
        {.type = TK_KEYWORD,       .cstr = "\\(\\("},
        {.type = TK_EXPRESSION,    .cstr = "yy"},
        {0}
      }};
    DO_TEST (&t, "escape & inner expression flag");
  }

  puts ("-- single-line comment --");
  {
    t = (test_t) {
      .test_number = 20,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "#0123456789abcdef 0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "basic comment");

    t = (test_t) {
      .test_number = 21,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA"},
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "keyword before comment");

    t = (test_t) {
      .test_number = 22,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA+#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA"},
        {.type = TK_PUNCS,      .cstr = "+"},
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "punc before comment");
    
    t = (test_t) {
      .test_number = 23,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA(e x p)#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AAA"},
        {.type = TK_EXPRESSION,    .cstr = "(e x p)"},
        {.type = TK_NOT_SET,       .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "expression before comment");

    t = (test_t) {
      .test_number = 24,
      .parsing_flags = PFLAG_INCOMMENT,
      .input = "AAA(e x p)#0123456789abcdefghi\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AAA"},
        {.type = TK_EXPRESSION,    .cstr = "(e x p)"},
        {.type = TK_COMMENT,       .cstr = "#0123456789abcde"},
        {.type = TK_COMMENT,       .cstr = "fghi"},
        {.type = TK_NOT_SET,       .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "with include comment flag");
  }

  puts ("-- multi-line comment --");
  {
    t = (test_t) {
      .test_number = 25,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "/*t e \n s t*/ XXX/*t e \n s t*/YYY ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "XXX"},
        {.type = TK_KEYWORD, .cstr = "YYY"},
        {0}
      }};
    DO_TEST (&t, "basic multi-line comment");

    t = (test_t) {
      .test_number = 26,
      .parsing_flags = PFLAG_INCOMMENT,
      .input = "XXX/*aaaaaaaabbbbbbbbccccccccdddddddd*/YYY ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "XXX"},
        {.type = TK_COMMENT, .cstr = "/*aaaaaaaabbbbbb"},
        {.type = TK_COMMENT, .cstr = "bbccccccccdddddd"},
        {.type = TK_COMMENT, .cstr = "dd*/"},
        {.type = TK_KEYWORD, .cstr = "YYY"},
        {0}
      }};
    DO_TEST (&t, "with include comment flag");
  }
  

  puts ("-- end of input slice --");
  {
    END_SLICE (&src);
    t = (test_t) {
      .test_number = 0,
      .parsing_flags = PFLAG_DEFAULT,
      .input = "",
      .etk = (Milexer_Token []){
        {0}
      }};
    DO_TEST (&t, "end of lazy loading");
  }

  puts ("\n *** All tests were passed *** ");
 eo_main:
  TOKEN_FREE (&tk);
  return ret;
}
#endif /* ML_TEST_1 */
