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

/** file: mini-lexer.h
    created on: 16 Dec 2024

    Mini-Lexer
    A minimal lexer library.

  * DISCLAIMER *
    This library was developed for my personal use,
    it may not be suitable for general purposes.

    This library comes with a self-test program, and two other
    example programs, for further information see ML_EXAMPLE_ macros
    and the compilation section.

    For adding new features or fixing bugs, it's more convenient to
    use a debugger along with the provided test program,
    as this is how the library was originally developed!


 -- Usage Example ---------------------------------------------------
    ```{c}
      #include <stlib.h>
      #define ML_IMPLEMENTATION
      #include "mini-lexer.h"
  
      //-- Defining the language -------//
      enum LANG
        {
          // keywords
          KEY_IF = 0,
          KEY_ELSE,
          ...
          // punctuations
          PUNC_COMMA = 0,
          ...
          // expressions
          EXP_STR = 0,
          ...
          // single-line & multi-line comment
          SL_SL_COMM_1 = 0,
          ...
          ML_SL_COMM_1 = 0,
          ...
        };
  
      static const char *Keywords[] = {
        [KEY_IF] = "if",
        ...
      };
      static const char *SL_Comments[] = {
        [SL_SL_COMM_1] = "#",
        ...
      };
  
      static struct Milexer_exp_ Puncs[] = {
        [PUNC_COMMA] = {","},
        ...
      };
      static struct Milexer_exp_ Expressions[] = {
        [EXP_STR] = { "'", "'" },
        ...
      };
      static struct Milexer_exp_ ML_Comments[] = {
        [ML_SL_COMM_1] = { "-(", ")-" },
        ...
      };
  
      // Define delimiter ranges if deeded
      static const char *Delimiters = {
        "\x00\x19",  // the range [0x00, 0x19]
        ";",         // the `;` character
        ...
      };
  
      // The main configuration of Mini-Lexer
      static Milexer ml = {
        .puncs         = GEN_MLCFG (Puncs),
        .keywords      = GEN_MLCFG (Keywords),
        .expression    = GEN_MLCFG (Expressions),
        .b_comment     = GEN_MLCFG (SL_Comments),
        .a_comment     = GEN_MLCFG (ML_Comments),
        .delim_ranges  = GEN_MLCFG (Delimiters),
      };
  
      const int flg = PFLAG_DEFAULT;
      Milexer_Slice src = {.lazy = true};

      // To allocate tokens on stack use the TOKEN_DECLARE macro,
      // but here we use TOKEN_ALLOC which works with malloc:
      Milexer_Token tk = TOKEN_ALLOC (32);
  
      //-- Actual Parsing --------------//
      for (int ret = 0; !NEXT_SHOULD_END (ret); )
        {
          ret = ml_next (&ml, &src, &tk, flg);
          switch (ret)
            {
            case NEXT_NEED_LOAD:
              // Load the data to parse
              if ( has_more_data )
                SET_ML_SLICE (&src, buffer, buffer_length);
              // If your input data is completed
              if ( end_of_data )
                END_ML_SLICE (&src);
              break; 
  
            case NEXT_MATCH:   // match
            case NEXT_CHUNK:   // you are receiving a chunk of the result
            case NEXT_ZTERM:   // the parser has encountered a null-byte
              {
                // If the token is recognized correctly, Milexer_Token->id
                // is set to the index of the token within the corresponding
                // language entry, for example:
                if ( tk.type == TK_KEYWORD  &&  tk.id == KEY_IF ) {
                    puts ("[IF keyword]");
                } else if ( tk.type == TK_PUNCS  &&  tk.id == PUNC_COMMA ) {
                    puts ("[Comma]");
                }
                // some other else if ...

                if ( tk.id == TK_NOT_SET ) {
                    printf ("[Unrecognized] `%s` of type %s\n",
                            tk.cstr,
                            milexer_token_type_cstr[tk.type]);
                }
              }
              break;
  
            default: break;
            }
        }
      TOKEN_FREE (&tk);
      ```

 -- Lexer in Lexer --------------------------------------------------
      Tokenizing contents of a token again; for instance, lexing an
      expression token to get it's comma separated values.
      For a complete example, see: ML_EXAMPLE_1

      ```{c}
      // You *MUST* pass the INEXP flag to the parser; Otherwise,
      // the result will contain the expression prefix and suffix,
      // which may potentially cause an infinite loop
      ret = ml_next (&ml, &src, &tk, PFLAG_INEXP);

      if (tk.type == TK_EXPRESSION)
      {
        int _ret;
        Milexer_Slice new_src = {0};
  
        SET_ML_SLICE (&new_src, tk.cstr, strlen (tk.cstr));
        // As we are using @tk.cstr in new_src, we cannot also
        // store the result data into the @tk itself, so
        // you *MUST* also allocate a new token, @new_token
        Milexer_Token new_token = TOKEN_ALLOC (32);
  
        do {
          // Indicates to the parser when it should stop
          new_src.eof_lazy = (ret != NEXT_CHUNK);

          // Prepare the new parsing data source, @new_src
          SET_ML_SLICE (&new_src, tk.cstr, strlen (tk.cstr));
  
          for (int _ret = 0; !NEXT_SHOULD_END (_ret); )
            {
              _ret = ml_next (&ml, &s, &new_token, PFLAG_IGSPACE);
              if (NEXT_SHOULD_LOAD (_ret))
                break;
  
              // do something here
              ...
            }
  
          // Load the remaining chunk(s)
          if (ret != NEXT_CHUNK)
            break;
          ret = ml_next (&ml, &src, &tk, PFLAG_INEXP);
  
        } while (!NEXT_SHOULD_LOAD (ret));
      ```

 -- Extending chunked tokens ----------------------------------------
    If Milexer_Token->cstr is allocated via malloc, for example:
    TOKEN_ALLOC macro is used, it can be extended to receive
    the actual intended data when it's chunked.
    The TOKEN_EXTEND macro can be used to extend the capacity:
    ```{c}
      while (ret == NEXT_CHUNK)
        {
          // Grow the token @tk, 12 bytes using realloc
          TOKEN_EXTEND (&tk, 12);

          // Continue filling the token
          ret = ml_next (&ml, &src, &tk, ...);
        }
    ```

 -- Disabling some features -----------------------------------------
    As your parser proceeds, it might be useful to disable/enable
    some components of the language dynamically; for instance,
    comma is a separator in lists, but it should be treated as a
    normal character in file paths or strings.
    ML_DISABLE and ML_ENABLE macros can be used for this purpose.

    Disabling the whole feature:
      {
        ML_DISABLE( &ml.puncs );        // Disbale all punctuation's
        ML_DISABLE( &ml.expression );   // Disable all expressions
      }

    Disabling features more specifically:
      {
        ML_DISABLE( Expressions[EXP_STR] );   // Disable strings
        ML_DISABLE( Puncs[PUNC_COMMA] );      // Disable comma
      }

 -- Changing the language -------------------------------------------
    Milexer supports changing language dynamically at runtime,
    users can simply assign new item to the main struct:
      {
        static struct Milexer_exp_ NewPuncs[] = {
           [xxx] = {"x"}, ...
        };
        ml.puncs = GEN_MLCFG (NewPuncs);
      }

    This assignment clears the @clean field of _exp_t struct,
    and `ml_next` will update the necessary Milexer internals.
    Changing the language in middle of yielding tokens chunks
    (ret: NEXT_CHUNK) has no effect until the token is ready.

    To manually change the language (without using GEN_MLCFG),
    users must set the @clean field to false:
      {
        ml.puncs.exp[0].begin = "??";     // changing the language
        ml.puncs[0].clean = false;        // make it dirty
      }

 -- Flex compatibility ----------------------------------------------
    Although Milixer provides low-level access to it's internal
    tokens and configurations, some users might prefer to use
    a higher level API like Flex.
    see the Example_2 for a quick example.

    Milixer implements a subset of Flex API which can be enabled,
    using ML_FLEX macro:
      ```
      #define ML_FLEX
      #define ML_IMPLEMENTATION
      #include "mini-lexer.h"
      ```
    After include, you have access to these global variables:
      yytext:   C string of the current token
      yyleng:   strlen of yytext
      yyin:     the current input FILE pointer
      yyline:   line number of the current token
      yycolumn: column number of the current token
      yyid:     equivalent to Milexer_Token->id      (Not standard)
      yyml:     to set the global Milexer language   (Not standard)

    Example:
      ```{c}
      Milexer ml = { ... }; // The language

      int main( void )
      {
         int type;
         yyml = &ml; // set the global language
         YY_BUFFER_STATE *buffer;

         buffer = yy_scan_string( "if flex > milexer" );
         while ( (type = yylex()) != -1 )
           {
             printf( "token:'%s'\n", yytext );

             // maybe disable/enable some features here, or
             // create another lexer with a different behavior
           }
         yy_delete_buffer( buffer );
         yylex_destroy(); // global destroy
      }
      ```

      There are other variants of yy functions which make memory
      managements automatic:
      {
        // yy_restart return pointers, do not need to be freed
        FILE *f = fopen( "/path/to/file", "r" );
        yy_restart( f );

        // but, yy_create_buffer requires yy_delete_buffer
        YY_BUFFER_STATE *b yy_create_buffer( f, 4096 );

        yy_delete_buffer( b );
        fclose (f);
      }
      yylex_destroy call's steal required to free global memories.
      see headers for more information.


 -- Known Issues ----------------------------------------------------
    1. Unicode/UTF8 is NOT supported.
    2. Token fragmentation (when the buffer overflows) breaks
       the logic of punctuation and expression detection.

 -- Compilation -----------------------------------------------------
    The test program:
      cc -x c -O0 -ggdb -Wall -Wextra -Werror \
          -D ML_IMPLEMENTATION -D ML_TEST_1 \
          mini-lexer.h -o test.out
    The example program:
      pass `-D ML_EXAMPLE_1` instead of `ML_TEST_1`
  
    Compilation Options:
      Debug Info:  define `-D_ML_DEBUG`
 **/
#ifndef MINI_LEXER__H
#define MINI_LEXER__H

#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define MILEXER_VERSION "2.3"

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

#ifndef ml_malloc
#define ml_malloc malloc
#endif
#ifndef ml_realloc
#define ml_realloc realloc
#endif
#ifndef ml_free
#define ml_free free
#endif

#define ML_STRLEN(cstr) ((cstr) ? strlen (cstr) : 0)

typedef struct Milexer_exp_
{
  const char *begin;
  const char *end;
  struct
  {
    int begin, end;
  } len;

  // int __tag; // internal nesting
  bool disabled;
} _exp_t;

/**
 *  Basic expression
 **/
typedef struct
{
  int len;
  const char **exp;
  bool disabled;
} Milexer_BEXP;

/**
 *  Advanced expression
 */
typedef struct
{
  int len;
  struct Milexer_exp_ *exp;
  bool disabled;
  bool clean; /* Not dirty */
} Milexer_AEXP;

#define EXP_SET_STRLEN(exp)                               \
  ((exp)->len.begin = ML_STRLEN ((exp)->begin),           \
   (exp)->len.end   = ML_STRLEN ((exp)->end))

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

enum milexer_next_t
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
  (ret == NEXT_NEED_LOAD && !NEXT_SHOULD_END (ret))

const char *milexer_next_cstr[] =
  {
    [NEXT_MATCH]                   = "Match",
    [NEXT_CHUNK]                   = "Chunk",
    [NEXT_ZTERM]                   = "zero-byte",
    [NEXT_NEED_LOAD]               = "Load",
    [NEXT_END]                     = "END",
    [NEXT_ERR]                     = "Error"
  };

#define __flag__(n) (1 << (n))
#define HAS_FLAG(n, flag) (n & (flag))
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

const char *milexer_token_type_cstr[] =
  {
    [TK_NOT_SET]         = "NAN",
    [TK_PUNCS]           = "Punctuation",
    [TK_KEYWORD]         = "Keyword",
    [TK_EXPRESSION]      = "Expression",
    [TK_COMMENT]         = "Comment",
  };
#define ml_token_type_cstr milexer_token_type_cstr

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
   *  We guarantee that @cstr is always null-terminated
   *  The user of this library is responsible for allocating
   *  and freeing the buffer @cstr of length `@cap + 1`
   *
   *  The `TOKEN_ALLOC` macro allocates a token using malloc
   */
  char *cstr;
  size_t cap, occ;  /* capacity and occupied (=strlen) */

  /* line/column number of the token in the input */
  size_t line, col;

  /* Internal */
  size_t __idx;
  size_t __line_idx; /* start of the current line's index */
} Milexer_Token;

/* Users only use these macros */
#define TK_DECLARE    TOKEN_DECLARE
#define TK_ALLOC      TOKEN_ALLOC   /* needs stdlib malloc */
#define TK_FREE       TOKEN_FREE    /* needs stdlib free */
#define TK_EXTEND     TOKEN_EXTEND  /* needs stdlib realloc */
#define TK_EXTEND2    TOKEN_EXTEND2
#define TK_STRLEN     TOKEN_STRLEN
#define TK_FREE_SPACE TOKEN_FREE_SPCAE

/* To allocate/free tokens using malloc */
#define TOKEN_DECLARE(mem, len) (Milexer_Token){.cstr=mem, .cap=len}
#define TOKEN_ALLOC(n) TOKEN_DECLARE (ml_malloc(n+1), n)
#define TOKEN_FREE(t) if ((t)->cstr) {ml_free ((t)->cstr);}

/**
 *  Extends a token when it runs out of memory so
 *  ml_next can continue using it.
 *  Only use these macros after ml_next call, and use it
 *  until ml_next returns NEXT_CHUNK.
 * @grow: how many bytes to extend
 */
#define TOKEN_EXTEND(tk, grow) \
  TOKEN_EXTEND2(tk, ml_realloc((tk)->cstr, (tk)->cap + grow +1), grow)
/* To extend tokens manually */
#define TOKEN_EXTEND2(tk, new_mem, delta_size)  \
  ((tk)->__idx = (tk)->occ,                     \
   (tk)->cap  += delta_size,                    \
   (tk)->cstr  = new_mem)

/* strlen and free space of tokens */
#define TOKEN_STRLEN(tk) ((tk)->occ)
#define TOKEN_LEFT_CAP(tk) ((tk)->cap - ((tk)->occ))
#define TOKEN_FREE_SPCAE TOKEN_LEFT_CAP

/* Internal macros */
#define TOKEN_IS_KNOWN(t) ((t)->id >= 0)
#define TOKEN_FINISH(t) \
  ((t)->cstr[(t)->__idx] = 0 , (t)->occ=(t)->__idx, (t)->__idx=0)
/* realloc token buffer */
#define _REALLOC(ptr, len) ptr = ml_realloc(ptr, len)
#define TOKEN_REALLOC(tk, new_cap) \
  (_REALLOC( (tk)->cstr, new_cap), (tk)->cap=new_cap)
/* line/column number marker macros */
#define TK_MARK_COLUMN(src, tk) \
  ((tk)->col = (src)->idx - (tk)->__line_idx)
#define TK_RESET_LINE(tk) ((tk)->line = 1, (tk)->__line_idx = 0)
#define TK_MARK_NEWLINE(src, tk)                \
  ((src)->__last_newline = 0,                   \
   (tk)->line++,                                \
   (tk)->__line_idx = (src)->idx)
/* Reset the token @t, NO free */
#define TOKEN_RESET(t) \
  ((t)->__idx = 0, (t)->len = 0, \
   (t)->type = TK_NOT_SET, (t)->cstr[0] = '\0')


typedef struct
{
  /* lazy mode */
  bool lazy;
  /* End of lazy loading */
  bool eof_lazy;

  /* Internal state of the buffer, SYN_xxx */
  enum __buffer_state_t state, prev_state;

  /* buffer & index & capacity */
  const char *buffer;
  size_t cap, idx;

  /* Internal */
  /* TODO: for nesting, these should be arrays */
  int __last_exp_idx;
  int __last_punc_idx;
  int __last_newline;
  const char *__last_comm;
} Milexer_Slice;

/**
 *  SET_ML_SLICE:
 *    Initializes a Mini-Lexer slice 
 *    using a pre-allocated buffer
 *  END_ML_SLICE:
 *    Ends a slice, after this, the parser
 *    will always return NEXT_END
 *  GETLINE_SLICE:
 *   Uses getline to fill and manage slices,
 *   slice->buffer MUST be malloc pointer or NULL
 */
#define SET_ML_SLICE(src, buf, n) \
  ((src)->buffer = buf, (src)->cap = n, (src)->idx = 0)
/* to indicate that lazy loading is over */
#define END_ML_SLICE(src) ((src)->cap = 0, (src)->eof_lazy = 1)
#define RESET_ML_SLICE(src) ((src)->idx = 0)
#define GETLINE_SLICE(src, stream) \
  getline ((char **)&(src)->buffer, &(src)->cap, stream)

/** Internal macros **/
/* set new state, load the previous state */
#define LD_STATE(src) (src)->state = (src)->prev_state
#define ST_STATE(slice, new_state) \
  ((slice)->prev_state = (slice)->state, (slice)->state = new_state)
/* get the latest exp/punc prefix */
#define __get_last_exp(ml, src) \
  ((ml)->expression.exp + (src)->__last_exp_idx)
#define __get_last_punc(ml, src) \
  ((ml)->puncs.exp + (src)->__last_punc_idx)

typedef struct Milexer_t
{
  /* Configurations */
  Milexer_BEXP escape;    // Not implemented
  /* puncs only uses begin from _exp_t, end is empty string */
  Milexer_AEXP puncs;
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
#define GEN_MLCFG(exp_ptr) {.exp = exp_ptr, .len = GEN_LENOF (exp_ptr)}

#define MLCFG_A_(s) (Milexer_AEXP) GEN_MLCFG (s)
#define MLCFG_B_(s) (Milexer_BEXP) GEN_MLCFG (s)

/**
 *  To simply initialize Milexer internals, outside
 *  of the body of the struct, for example:
 *    ML.expression = MKEXP (Expressions);
 *    ML.punctuation = MKPUNC (Punctuations);
 */
#define MKEXP(exp) MLCFG_A_ (exp)
#define MKPUNC(pun) MLCFG_B_ (pun)
#define MKKEYWORD(keyw) MLCFG_B_ (key)
#define MKCOMMENT_SL(c) MLCFG_B_ (c)
#define MKCOMMENT_ML(c) MLCFG_A_ (c)

#define ML_UNSET(field) (field = (typeof (field)) {0})

/**
 *  Disable/Enable feature macros
 *    for basic expressions:
 *      ML_DISABLE(ml.keywords)
 *    for advanced expressions:
 *      ML_DISABLE(ml.expressions.exp[.n])
 *    Or disable all of them at once:
 *      ML_DISABLE(ml.expressions)
 */
#define ML_DISABLE(field, ...) __SET_DISABLE (field, true)
#define ML_ENABLE(field, ...) __SET_DISABLE (field, false)

#define __SET_DISABLE(field, v, ...) ((field)->disabled = v)

/**
 *  Retrieves the next token
 *
 *  @src is the input buffer source and
 *  The result will be stored in @tk
 *
 *  Memory management for @src and @tk is up to
 *  the user of this library
 *  @src and @tk buffers *MUST* be distinct
 */
int ml_next (Milexer *ml, Milexer_Slice *src,
             Milexer_Token *tk, int flg);


#ifdef ML_FLEX
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef __UNUSED__
#define __UNUSED__ __attribute__((unused))
#endif /* __UNUSED__ */

static char    *yytext  __UNUSED__;   /* Milexer_Token->cstr */
static size_t   yyleng  __UNUSED__;   /* strlen of Milexer_Token->cstr */
static FILE    *yyin    __UNUSED__;     /* stream input file */

static Milexer *yyml    __UNUSED__;     /* Milexer Language */
static int      yyid    __UNUSED__;     /* Milexer_Token->id */

static int    yyline    __UNUSED__;     /* Milexer_Token->line */
static int    yycolumn  __UNUSED__;     /* Milexer_Token->col */

enum yy_memflag
  {
    USER_MEM = 0, /* Allocated by the user */
    ITS_OUR_STATE_BUF = (1<<1),  /* yy_buffer_state is allocated by us */
    ITS_OUR_BASE_BUF =  (1<<2),  /* yy_buffer_state->base is allocated by us */
  };

typedef struct yy_buffer_state
{
  int ret;
  FILE *yy_input_file;
  int   yy_is_interactive;

  int memflgs;
  char *base;
  int   base_cap;

  Milexer_Slice src;
  Milexer_Token tk;

} YY_BUFFER_STATE;

/**
 *  Stack of input buffers (Internal)
 *  Must be allocated and freed using `yyensure_buffer_stack`
 */
static YY_BUFFER_STATE **yy_buffer_stack = NULL;   /* Stack as an array  */
static size_t            yy_buffer_stack_top = 0;  /* index of top of stack */
static size_t            yy_buffer_stack_max = 0;  /* capacity of stack */

/* To retrieve the top of the stack */
#define YY_CURRENT_BUFFER \
  ((yy_buffer_stack) ? *YY_CURRENT_BUFFER_LVALUE : NULL)
/* top of the stack without NULL check */
#define YY_CURRENT_BUFFER_LVALUE \
  ((YY_BUFFER_STATE **)(yy_buffer_stack + yy_buffer_stack_top))

#ifndef YY_BUFFER_CAP
#define YY_BUFFER_CAP 4096  /* disk pgae size, to read from disk */
#endif
#ifndef YY_TOKEN_CAP
#define YY_TOKEN_CAP 512
#endif
#ifndef YY_TOKEN_GROW
#define YY_TOKEN_GROW 128  /* token realloc grow factor */
#endif

#ifdef ML_DEBUG
# define ml_dprintf(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)
#else
#define ml_dprintf(...)
#endif /* ML_DEBUG */

#ifndef ML_DEBUG
# ifndef yy_free
#   define yy_free free
# endif
# ifndef yy_malloc
#   define yy_malloc malloc
# endif
# ifndef yy_realloc
#   define yy_realloc realloc
# endif
#else /* ML_DEBUG */
# ifndef yy_free
# define yy_free(ptr) __yyfree (ptr, __FILE__, __LINE__)
void
__yyfree (void *ptr, const char *src_name, int _line_)
{
  ml_dprintf ("%s:%d:  free(%p)\n", src_name, _line_, ptr);
  free (ptr);
}
# endif /* yy_free */
# ifndef yy_malloc
# define yy_malloc(n) __yymalloc (n, __FILE__, __LINE__)
void *
__yymalloc (size_t size, const char *src_name, int _line_)
{
  void *ptr = malloc (size);
  ml_dprintf ("%s:%d:  malloc(%ld) -> %p\n",
              src_name, _line_, size, ptr);
  return ptr;
}
# endif /* yy_alloc */
# ifndef yy_realloc
# define yy_realloc(ptr, n) __yyrealloc (ptr, n, __FILE__, __LINE__)
void *
__yyrealloc (void *ptr, size_t size, const char *src_name, int _line_)
{
  intptr_t __ptr = (intptr_t) ptr; // just to shut up the compiler
  void *res = realloc (ptr, size);
  ml_dprintf ("%s:%d:  realloc(0x%lx, %ld) -> %p\n",
              src_name, _line_, __ptr, size, res);
  return res;
}
# endif /* yy_realloc */

#undef ml_malloc
#define ml_malloc yy_malloc
#undef ml_realloc
#define ml_realloc yy_realloc
#undef ml_free
#define ml_free yy_free

#endif /* ML_DEBUG */

/**
 *  The main  lex  function, to retrieve the next token
 *  Sets the global variables to the corresponding values
 *
 *  The return valuse of this function is one of `TK_XXX`
 * Return:
 *   On the end of tokens:  0  (=TK_NOT_SET)
 *   Successful lex:   Milixer_Token->type
 */
int yylex (void);

/**
 *  Getline function
 *  Reads from @stream at most @len bytes, writes on @buffer,
 *  @buffer should have a newline character at the end, to be
 *  distinguished from the rest of the input
 *
 *  Minilexer has implemented a minimal getline function,
 *  and it can be overridden using YY_CUSTOM_GETLINE macro
 */
size_t yy_getline (FILE *stream, char *buffer, size_t len);

/**
 *  To delete a buffer from the stack
 *  This should be called by the users, if they
 *  are handling the lexer memory themselves
 */
void yy_delete_buffer (YY_BUFFER_STATE *b);

/**
 *  Allocates new state buffer from @base
 *  It should be freed by `yy_delete_buffer`
 */
YY_BUFFER_STATE * yy_scan_buffer (char *base, size_t size);

/**
 *  Like yy_scan_buffer, but keep a malloc copy of @bytes
 *  yy_delete_buffer will free it and it's copy of @bytes
 */
YY_BUFFER_STATE * yy_scan_bytes (const char *bytes, size_t len);

/* C string version of yy_scan_bytes */
YY_BUFFER_STATE * yy_scan_string (const char *str);

/**
 *  Create state buffer from file
 *  @size is size of the internal buffer to read from @file
 *  If @file is NULL, it read from stdin
 */
YY_BUFFER_STATE * yy_create_buffer (FILE *file, int size);

/**
 *  To ensure all the allocated memory are freed,
 *  call it at the end of parsing
 */
int yylex_destroy (void);


/**
 **  No `yy_delete_buffer` needed functions
 **  These functions return value should NOT
 **  be freed by the users
 **/
/* Switch to reading from @file */
YY_BUFFER_STATE * yy_restart (FILE *file);


#endif /* ML_FLEX */
#endif /* MINI_LEXER__H */

#ifdef ML_IMPLEMENTATION

/** Internal
 *  Some punctuations may have common prefixes with other
 *  components of the Milexer language;
 *  The `ml_next` function sets `_exp_t->end` to this value
 *  to indicate that the punctuation requires additional checking.
 *  This helps avoid expensive __is_exp_or_comment call each time
 *  a punctuation is encountered.
 */
#define PUNC_DOUBLE_CHECK ((void *) -1)

/**
 **  Internal functions
 **/
static void
__set_strlens (Milexer_AEXP *adv_exp)
{
  for (int i=0; i < adv_exp->len; ++i)
    {
      EXP_SET_STRLEN (&adv_exp->exp[i]);
    }
}
/* update strlen of Milexer_AEXP if needed */
#define UPDATE_STRLEN(exp) \
  if (! (exp)->clean) { __set_strlens (exp); (exp)->clean = true; }

/**
 *  To set the ID of keyword tokens
 *  @return 0 on success, -1 if not detected
 */
int ml_set_keyword_id (const Milexer *ml, Milexer_Token *tk);

/* returns @p when @p is a delimiter, and -1 on null-byte */
static inline int
__detect_delim (const Milexer *ml, unsigned char p, int flags)
{
  if (p == 0)
    return -1;
  if (ml->delim_ranges.len == 0 || HAS_FLAG (flags, PFLAG_ALLDELIMS))
    {
      /* default delimiters */
      if (p < ' ' ||
          (p == ' ' && !HAS_FLAG (flags, PFLAG_IGSPACE)))
        {
          return p;
        }
    }
  if (ml->delim_ranges.len > 0 || HAS_FLAG (flags, PFLAG_ALLDELIMS))
    {
      for (int i=0; i < ml->delim_ranges.len; ++i)
        {
          const char *__p = ml->delim_ranges.exp[i];
          if (__p[1] != '\0')
            {
              if (p >= (unsigned char)__p[0] && p <= (unsigned char)__p[1])
                return p;
            }
          else if (p == (unsigned char)__p[0])
            return p;
        }
    }
  return 0;
}

/** Expensive internal function: O(#expression + #a_comment + #b_comment)
 *  This function detects whether @cstr is a:
 *   expression prefix  OR  a multi-line/simple-line comment prefix or not
 *  It should be only used if it's necessary to distinguish between
 *  a punctuation and the rest of the language.
 */
static int
__is_exp_or_comment (const Milexer *ml, Milexer_Slice *src,
                     const char *cstr)
{
  for (int i=0; i < ml->expression.len; ++i)
    {
      _exp_t *e = ml->expression.exp + i;
      if (e->disabled)
        continue;
      if (strncmp (cstr, e->begin, e->len.begin) == 0)
        {
          src->__last_exp_idx = i;
          return 1;
        }
    }
 for (int i=0; i < ml->a_comment.len; ++i)
    {
      _exp_t *e = &ml->a_comment.exp[i];
      if (e->disabled)
        continue;
      if (strncmp (cstr, e->begin, e->len.begin) == 0)
        {
          src->__last_comm = e->begin;
          return 1;
        }
    }
 for (int i=0; i < ml->b_comment.len; ++i)
    {
      const char *pref = ml->b_comment.exp[i];
      size_t len = strlen (pref);
      if (strncmp (pref, cstr, len) == 0)
        {
          src->__last_comm = pref;
          return 1;
        }
    }
 return 0;
}

static inline bool
__should_double_check (const Milexer *ml, _exp_t *punc)
{
  const char *pref = punc->begin;
  size_t pref_len = punc->len.begin;
  for (int i=0; i < ml->expression.len; ++i)
    if (0 == strncmp (ml->expression.exp[i].begin, pref, pref_len))
      return true;
  for (int i=0; i < ml->a_comment.len; ++i)
    if (0 == strncmp (ml->a_comment.exp[i].begin, pref, pref_len))
      return true;
  for (int i=0; i < ml->b_comment.len; ++i)
    if (0 == strncmp (ml->b_comment.exp[i], pref, pref_len))
      return true;
  return false;
}

static inline char *
__detect_puncs (const Milexer *ml,
                Milexer_Slice *src, Milexer_Token *res)
{
  int longest_match_idx = -1;
  size_t longest_match_len = 0;
  res->cstr[res->__idx] = '\0';
  if (ml->puncs.disabled)
    return NULL;
  for (int i=0; i < ml->puncs.len; ++i)
    {
      _exp_t *punc = &ml->puncs.exp[i];
      if (punc->disabled)
        continue;
      size_t len = punc->len.begin;
      if (res->__idx < len)
        continue;
      if (strstr (res->cstr, punc->begin) != NULL)
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
      if (PUNC_DOUBLE_CHECK == ml->puncs.exp[longest_match_idx].end)
        {
          const char *punc_start = &src->buffer[src->idx - longest_match_len];
          if (__is_exp_or_comment (ml, src, punc_start))
            {
              return NULL;
            }
        }
      src->__last_punc_idx = longest_match_idx;
      res->id = longest_match_idx;
      return res->cstr + (res->__idx - longest_match_len);
    }
  return NULL;
}

static inline char *
__is_expression_suff (const Milexer *ml,
                      Milexer_Slice *src, Milexer_Token *tk)
{
  /* If we reach here, expressions cannot be totally disabled */
  assert (ml->expression.disabled == false && "Broken logic!!");

  /* looking for closing, O(1) */
  _exp_t *e = __get_last_exp (ml, src);
  size_t len = e->len.end;

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
__is_mline_commented_suff (const Milexer *ml,
                           Milexer_Slice *src, Milexer_Token *tk)
{
  if (ml->a_comment.disabled)
    return NULL;
  for (int i=0; i < ml->a_comment.len; ++i)
    {
      _exp_t *a_comment = &ml->a_comment.exp[i];
      if (a_comment->disabled)
        continue;
      const char *pref = a_comment->end;
      size_t len = a_comment->len.end;
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
__is_sline_commented_pref (const Milexer *ml,
                           Milexer_Slice *src, Milexer_Token *tk)
{
  if (ml->b_comment.disabled)
    return NULL;
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
__is_mline_commented_pref (const Milexer *ml,
                           Milexer_Slice *src, Milexer_Token *tk)
{
  if (ml->a_comment.disabled)
    return NULL;
  for (int i=0; i < ml->a_comment.len; ++i)
    {
      _exp_t *a_comment = &ml->a_comment.exp[i];
      if (a_comment->disabled)
        continue;
      const char *pref = a_comment->begin;
      size_t len = a_comment->len.begin;
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
__is_expression_pref (const Milexer *ml,
                      Milexer_Slice *src, Milexer_Token *tk)
{
  if (ml->expression.disabled)
    return NULL;
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
      if (e->disabled)
        continue;
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
  if (ml->keywords.len > 0 && !ml->keywords.disabled)
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

static inline bool
tk_set_defaults (const Milexer *ml, Milexer_Token *tk)
{
  if (tk->type == TK_NOT_SET)
    {
      tk->type = TK_KEYWORD;
      ml_set_keyword_id (ml, tk);
      TOKEN_FINISH (tk);
      return true;
    }
  TOKEN_FINISH (tk);
  return false;
}

int
ml_next (Milexer *ml, Milexer_Slice *src,
         Milexer_Token *tk, int flags)
{
  struct Milexer_exp_ *last_exp;
  if (tk->cstr == NULL || tk->cap <= 0 || tk->cstr == src->buffer)
    return NEXT_ERR;

  /* pre parsing */
  tk->type = TK_NOT_SET;
  switch (src->state)
    {
    case SYN_NO_DUMMY__:
      if (HAS_FLAG (flags, PFLAG_INEXP))
        TK_MARK_COLUMN (src, tk);
      if (!HAS_FLAG (flags, PFLAG_INEXP) && src->__last_exp_idx != -1)
        {
          /* certainly the token type is expression */
          tk->type = TK_EXPRESSION;
          last_exp = __get_last_exp (ml, src);
          char *__p = mempcpy (tk->cstr, last_exp->begin, last_exp->len.begin);
          tk->__idx += __p - tk->cstr;
          TK_MARK_COLUMN (src, tk);
          tk->col -= last_exp->len.begin;
        }
      src->state = SYN_NO_DUMMY;
      break;

    case SYN_PUNC__:
      TK_MARK_COLUMN (src, tk);
      last_exp = __get_last_punc (ml, src);
      memcpy (tk->cstr, last_exp->begin, last_exp->len.begin);
      LD_STATE (src);
      /* just to make to null-terminated */
      tk->__idx = last_exp->len.begin;
      tk->type = TK_PUNCS;
      tk->id = src->__last_punc_idx;
      tk->col -= last_exp->len.begin;
      TOKEN_FINISH (tk);
      return NEXT_MATCH;
      break;

    case SYN_CHUNK:
      LD_STATE (src);
      break;

    case SYN_ML_COMM:
      TK_MARK_COLUMN (src, tk);
      if (src->__last_comm && HAS_FLAG (flags, PFLAG_INCOMMENT))
        {
          tk->type = TK_COMMENT;
          tk->__idx = strlen (src->__last_comm);
          strncpy (tk->cstr, src->__last_comm, tk->__idx);
          src->__last_comm = NULL;
        }
      break;

    case SYN_DUMMY:
    case SYN_DONE:
      TK_MARK_COLUMN (src, tk);
      if (src->__last_newline)
        {
          TK_MARK_NEWLINE (src, tk);
        }
      UPDATE_STRLEN (&ml->expression);
      UPDATE_STRLEN (&ml->a_comment);
      if (! ml->puncs.clean)
        { /* Update puncs strlens and double check */
          for (int i=0; i < ml->puncs.len; ++i)
            {
              _exp_t *punc = &ml->puncs.exp[i];
              EXP_SET_STRLEN (punc);
              if (__should_double_check (ml, punc))
                punc->end = PUNC_DOUBLE_CHECK;
            }
          ml->puncs.clean = true;
        }
      break;

    default:
      TK_MARK_COLUMN (src, tk);
      break;
    }

  /* check end of src slice */
  if (src->idx >= src->cap)
    {
      src->idx = 0;
      if (tk->__idx == 0)
        {
          *tk->cstr = '\0';
          tk->type = TK_NOT_SET;
        }
      if (src->eof_lazy || src->lazy == 0)
        return NEXT_END;
      return NEXT_NEED_LOAD;
    }

  /**
   *  The main loop
   */
  const char *p;
  char *dst = tk->cstr;
  if (src->idx == 0)
    TK_RESET_LINE (tk);
  for (; src->idx < src->cap; )
    {
      p = src->buffer + (src->idx++);
      dst = tk->cstr + (tk->__idx++);
      *dst = *p;
      
      //-- detect & reset chunks -------//
      if (tk->__idx == tk->cap)
        {
          if ((src->state != SYN_COMM && src->state != SYN_ML_COMM)
              || HAS_FLAG (flags, PFLAG_INCOMMENT))
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
              /**
               *  max token len reached, this will be
               *  the next chunk unless this is end of src
               */
              TOKEN_FINISH (tk);
              if (src->idx == src->cap)
                {
                  *dst = '\0';
                  ST_STATE (src, SYN_DUMMY);
                  return NEXT_MATCH;
                }
              else
                {
                  ST_STATE (src, SYN_CHUNK);
                  return NEXT_CHUNK;
                }
            }
          else
            TOKEN_FINISH (tk);
        }
      //--------------------------------//
      int c;
      char *__ptr;
      /* logf ("'%c' - %s, %s", *p,
            milexer_state_cstr[src->state],
            milexer_token_type_cstr[tk->type]); */
      switch (src->state)
        {
        case SYN_ESCAPE:
          LD_STATE (src);
          break;

        case SYN_COMM: {
          if (*p == '\n')
            TK_MARK_NEWLINE (src, tk);
          if (*p == '\n' || *p == '\r')
            {
              TK_MARK_COLUMN (src, tk);
              ST_STATE (src, SYN_DUMMY);
              tk->type = TK_COMMENT;
              TOKEN_FINISH (tk);
              if (HAS_FLAG (flags, PFLAG_INCOMMENT))
                {
                  *(dst) = '\0';
                  return NEXT_MATCH;
                }
              else
                {
                  *tk->cstr = '\0';
                }
            }
        } break;

        case SYN_ML_COMM: {
          if (*p == '\n')
            TK_MARK_NEWLINE (src, tk);
          if ((__ptr = __is_mline_commented_suff (ml, src, tk)))
            {
              TK_MARK_COLUMN (src, tk);
              ST_STATE (src, SYN_DUMMY);
              TOKEN_FINISH (tk);
              if (HAS_FLAG (flags, PFLAG_INCOMMENT))
                {
                  tk->type = TK_COMMENT;
                  return NEXT_MATCH;
                }
            }
        } break;

        case SYN_DUMMY: {
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
                  if (HAS_FLAG (flags, PFLAG_INEXP))
                    {
                      TK_MARK_COLUMN (src, tk);
                      TOKEN_FINISH (tk);
                    }
                }
              else
                {
                  ST_STATE (src, SYN_NO_DUMMY__);
                  TOKEN_FINISH (tk);
                  return NEXT_MATCH;
                }
            }
          else if ((__ptr = __detect_puncs (ml, src, tk)))
            {
              tk->type = TK_PUNCS;
              TOKEN_FINISH (tk);
              return NEXT_MATCH;
            }
          else if ((c = __detect_delim (ml, *p, flags)) == 0)
            {
              if (c == -1)
                {
                  tk_set_defaults (ml, tk);
                  return NEXT_ZTERM;
                }
              src->state = SYN_MIDDLE;
            }
          else
            {
              if (c == '\n')
                TK_MARK_NEWLINE (src, tk);
              TK_MARK_COLUMN (src, tk);
              TOKEN_FINISH (tk);
            }
        } break;

        case SYN_MIDDLE: {
          if ((__ptr = __is_sline_commented_pref (ml, src, tk)))
            {
              ST_STATE (src, SYN_COMM);
              if (__ptr == tk->cstr)
                {
                  break;
                }
              else
                {
                  tk_set_defaults (ml, tk);
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
                  *__ptr = 0;
                  tk_set_defaults (ml, tk);
                  return NEXT_MATCH;
                }
            }
          else if ((c = __detect_delim (ml, *p, flags)) != 0)
            {
              if (c == -1)
                {
                  tk_set_defaults (ml, tk);
                  return NEXT_ZTERM;
                }
              if (tk->__idx > 1)
                {
                  if (c == '\n')
                    ++src->__last_newline;
                  *dst = '\0';
                  tk_set_defaults (ml, tk);
                  ST_STATE (src, SYN_DUMMY);
                  return NEXT_MATCH;
                }
              else
                {
                  TOKEN_FINISH (tk);
                  TK_MARK_COLUMN (src, tk);
                }
            }
          else if (__detect_puncs (ml, src, tk))
            {
              const char *_punc = __get_last_punc (ml, src)->begin;
              size_t n = strlen (_punc);
              if (n == tk->__idx)
                {
                  tk->type = TK_PUNCS;
                  TOKEN_FINISH (tk);
                  return NEXT_MATCH;
                }
              else
                {
                  tk->type = TK_KEYWORD;
                  ml_set_keyword_id (ml, tk);
                  *(dst - n + 1) = '\0';
                  TOKEN_FINISH (tk);
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
                  TOKEN_FINISH (tk);
                  tk->type = TK_KEYWORD;
                  ml_set_keyword_id (ml, tk);
                  return NEXT_MATCH;
                }
              else
                {
                  tk->type = TK_EXPRESSION;
                  if (HAS_FLAG (flags, PFLAG_INEXP))
                    {
                      TOKEN_FINISH (tk);
                      TK_MARK_COLUMN (src, tk);
                    }
                  ST_STATE (src, SYN_NO_DUMMY);
                }
            }
        } break;

        case SYN_NO_DUMMY: {
          if ((__ptr = __is_expression_suff (ml, src, tk)))
            {
              tk->type = TK_EXPRESSION;
              if (HAS_FLAG (flags, PFLAG_INEXP))
                *__ptr = '\0';
              ST_STATE (src, SYN_DUMMY);
              TOKEN_FINISH (tk);
              return NEXT_MATCH;
            }
        } break;

        default:
          break;
        }

      /* check for escape */
      if (*p == '\\')
        ST_STATE (src, SYN_ESCAPE);
    }

  /* check for end of lazy loading */
  if (src->eof_lazy || src->lazy == 0)
    {
      if (tk->__idx >= 1)
        {
          tk_set_defaults (ml, tk);
          return NEXT_MATCH;
        }
       else
        {
          tk->type = TK_NOT_SET;
          TOKEN_FINISH (tk);
          return NEXT_END;
        }
    }
  /* end of src slice */
  src->idx = 0;
  if (tk->__idx == 0)
    {
      *dst = '\0';
      return NEXT_NEED_LOAD;
    }
  *(dst + 1) = '\0';
  if (tk->type == TK_NOT_SET)
    {
      tk->type = TK_KEYWORD; 
      ml_set_keyword_id (ml, tk);
    }
  return NEXT_NEED_LOAD;
}

/* Flex compatibility implementation */
#ifdef ML_FLEX

/**
 *  Allocates the stack if it does not exist.
 *  Guarantees space for at least one push.
 *  Just a simple dynamic array for yy_buffer_stack
 */
static void yyensure_buffer_stack (void);

/* Pushes @new_buffer and update top of the stack */
static void yypush_buffer (YY_BUFFER_STATE *new_buffer);

/* Only pops the top of the stack, NO FREE */
static void yypop_buffer_state (void);
/* Internal malloc/memset to allocate YY_BUFFER_STATE struct */
static YY_BUFFER_STATE * yy_alloc_buffer (void);
/* Internal global values setter */
static inline void yy_set_global (YY_BUFFER_STATE *b);

/**
 *  It deletes and pops the previous state buffer, so
 *  it should *NOT* be freed again
 */
static void yy_switch_to_buffer (YY_BUFFER_STATE *new_buffer) __UNUSED__;

/* If FILE is provided, this will read the next chunk */
static int yy_get_next_input (YY_BUFFER_STATE *b);

YY_BUFFER_STATE *
yy_alloc_buffer (void)
{
  YY_BUFFER_STATE *b = (YY_BUFFER_STATE *)
    yy_malloc (sizeof (struct yy_buffer_state));
  memset (b, 0, sizeof (struct yy_buffer_state));
  return b;
}

void yy_delete_buffer (YY_BUFFER_STATE *b)
{
  if (! b)
    return;

  TOKEN_FREE (&b->tk);
  if (b->memflgs & ITS_OUR_BASE_BUF)
    yy_free (b->base);
  yy_free (b);
}

static void
yyensure_buffer_stack (void)
{
  int entry2alloc, new_size_b;
  if (! yy_buffer_stack)
    {
      entry2alloc = 1;
      yy_buffer_stack_top = 0;
      yy_buffer_stack_max = entry2alloc;
      new_size_b = entry2alloc * sizeof (YY_BUFFER_STATE *);

      yy_buffer_stack = (YY_BUFFER_STATE **) yy_malloc (new_size_b);
      memset (yy_buffer_stack, 0, new_size_b);
      return;
    }
  if (yy_buffer_stack_top >= yy_buffer_stack_max -1)
    {
      int grow = 8;
      entry2alloc = yy_buffer_stack_max + grow;
      new_size_b = entry2alloc * sizeof (YY_BUFFER_STATE *);
      yy_buffer_stack = (YY_BUFFER_STATE **)
        yy_realloc (yy_buffer_stack, new_size_b);
      memset (yy_buffer_stack + yy_buffer_stack_max, 0,
              grow * sizeof (YY_BUFFER_STATE *));
      yy_buffer_stack_max = entry2alloc;
      return;
    }
}

void
yypush_buffer (YY_BUFFER_STATE *new_buffer)
{
  if (new_buffer == NULL)
    return;
  yyensure_buffer_stack();

  /* Only push if top exists. Otherwise, replace top. */
  if (YY_CURRENT_BUFFER)
    yy_buffer_stack_top++;

  *YY_CURRENT_BUFFER_LVALUE = new_buffer;
}
void yypop_buffer_state (void)
{
  if (!YY_CURRENT_BUFFER)
    return;

  *YY_CURRENT_BUFFER_LVALUE = NULL;
  if (yy_buffer_stack_top > 0)
    --yy_buffer_stack_top;
}

static void
yy_set_global (YY_BUFFER_STATE *b)
{
  if (! b)
    {
      yyin = NULL, yytext = NULL;
      yyleng = 0;
      yyid = TK_NOT_SET;
      yyline = 1, yycolumn = 0;
      return;
    }
  yyin = b->yy_input_file;
  yyid = b->tk.id;
  yyline = b->tk.line;
  yycolumn = b->tk.col;
  if (TK_NOT_SET == b->tk.type)
    {
      yytext = NULL, yyleng = 0;
    }
  else
    {
      yytext = b->tk.cstr;
      yyleng = b->tk.occ;
    }
}

inline YY_BUFFER_STATE *
yy_scan_buffer (char *base, size_t size)
{
  YY_BUFFER_STATE *b = yy_alloc_buffer ();
  b->src.lazy = false;
  b->base = base;
  b->base_cap = size;
  SET_ML_SLICE (&b->src, base, size);
  b->tk = TOKEN_ALLOC (YY_TOKEN_CAP);
  yy_set_global (b);
  yypush_buffer (b);
  return b;
}

YY_BUFFER_STATE *
yy_scan_bytes (const char *bytes, size_t len)
{
  char *buf = yy_malloc (len + 1);
  memcpy (buf, bytes, len);
  YY_BUFFER_STATE *res = yy_scan_buffer (buf, len);
  res->memflgs |= ITS_OUR_BASE_BUF;
  return res;
}
YY_BUFFER_STATE *
yy_scan_string (const char *str)
{
  return yy_scan_bytes (str, strlen (str));
}

YY_BUFFER_STATE *
yy_create_buffer (FILE *file, int size)
{
  if (! file)
    return NULL;
  yyin = file;
  YY_BUFFER_STATE *b = yy_alloc_buffer ();
  b->yy_input_file = yyin;

  b->src.lazy = true;
  int tty = isatty (fileno (yyin));
  b->yy_is_interactive = (tty > 0);

  b->base_cap = size;
  b->base = yy_malloc (size);
  b->memflgs |= ITS_OUR_BASE_BUF;

  b->tk = TOKEN_ALLOC (YY_TOKEN_CAP);
  yypush_buffer (b);
  return b;
}

YY_BUFFER_STATE *
yy_restart (FILE *file)
{
  YY_BUFFER_STATE *b;
  if (! file)
    file = stdin;
  b = yy_create_buffer (file, YY_BUFFER_CAP);
  b->memflgs |= ITS_OUR_STATE_BUF;
  return b;
}

static void
yy_switch_to_buffer (YY_BUFFER_STATE *new_buffer)
{
  if (! new_buffer)
    return;
  yyensure_buffer_stack ();
  YY_BUFFER_STATE *b = YY_CURRENT_BUFFER;
  if (b == new_buffer)
    return;
  yy_delete_buffer (b);
  yypush_buffer (new_buffer);
}

#ifndef YY_CUSTOM_GETLINE
size_t
yy_getline (FILE *stream, char *buffer, size_t len)
{
  size_t rw = 0;
  for (int c = '*'; c >= ' ' && rw < len-1; ++rw)
    {
      c = getc (stream);
      buffer[rw] = (char) c;
      if (c == EOF)
        return rw;
    }
  return rw;
}
#endif /* YY_CUSTOM_GETLINE */

static int
yy_get_next_input (YY_BUFFER_STATE *b)
{
  ssize_t rw = 0;

  if (b->yy_is_interactive)
    {
      rw = yy_getline (yyin, b->base, b->base_cap);
      if (0 != rw)
        {
          SET_ML_SLICE (&b->src, b->base, rw);
          return 0;
        }
    }
  else
    {
      rw = fread (b->base, 1, b->base_cap, yyin);
      if (feof (yyin) || ferror (yyin)) /* EOF */
        b->src.lazy = false;
      if (rw > 0)
        {
          SET_ML_SLICE (&b->src, b->base, rw);
          return 0;
        }
    }
  END_ML_SLICE (&b->src);
  return 1;
}

int
yylex_destroy (void)
{
  while (YY_CURRENT_BUFFER)
    {
      yy_delete_buffer( YY_CURRENT_BUFFER  );
      *YY_CURRENT_BUFFER_LVALUE = NULL;
    }
  /* Destroy the stack itself. */
  if (yy_buffer_stack)
    {
      yy_free (yy_buffer_stack);
      yy_buffer_stack = NULL;
    }
    return 0;
}

int
yylex (void)
{
  YY_BUFFER_STATE *b = YY_CURRENT_BUFFER;
  if (! b)
    b = yy_restart (stdin);
  yy_set_global (b);

 beginning_of_lex:
  b->ret = ml_next (yyml, &b->src, &b->tk, PFLAG_INEXP);
  switch (b->ret)
    {
    case NEXT_MATCH:
    case NEXT_ZTERM:
      yy_set_global (b);
      return b->tk.type;

    case NEXT_CHUNK:
      TOKEN_EXTEND (&b->tk, YY_TOKEN_GROW);
      goto beginning_of_lex;

    case NEXT_NEED_LOAD:
      if (yy_get_next_input (b))
        goto next_end;
      goto beginning_of_lex;

    case NEXT_END:
    next_end:
      if (b->memflgs & ITS_OUR_STATE_BUF)
        yy_delete_buffer (b);
      yypop_buffer_state ();
      return -1;
    }
  return -1;
}
#endif /* ML_FLEX */


#undef logf
#undef fprintd
#endif /* ML_IMPLEMENTATION */


/**
 **  Common headers for both
 **  example_n and test_1 programs
 **/
#if defined (ML_EXAMPLE_1) || \
    defined (ML_EXAMPLE_2) || \
    defined (ML_TEST_1)
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
    LANG_FILE,
    /* punctuations */
    PUNC_PLUS = 0,
    PUNC_MINUS,
    PUNC_MULT,
    PUNC_DIV,
    PUNC_COMMA,
    PUNC_EQUAL,
    PUNC_NEQUAL,
    /* expressions */
    EXP_PAREN = 0,
    EXP_CURLY,
    EXP_STR,
    EXP_STR2,
    EXP_LONG,
    /* single-line comments */
    SL_COMM_1 = 0,
    SL_COMM_2,
    /* multi-line comments */
    ML_COMM_1 = 0,
  };
static const char *Keywords[] = {
  [LANG_IF]         = "if",
  [LANG_ELSE]       = "else",
  [LANG_FI]         = "fi",
  [LANG_FILE]       = "file",
};
static struct Milexer_exp_ Puncs[] = {
  [PUNC_PLUS]       = {"+"},
  [PUNC_MINUS]      = {"-"},
  [PUNC_MULT]       = {"*"},
  [PUNC_DIV]        = {"/"},
  [PUNC_COMMA]      = {","},
  [PUNC_EQUAL]      = {"="}, /* you cannot have "==" */
  [PUNC_NEQUAL]     = {"!="}, /* also "!===" */
};
static struct Milexer_exp_ Expressions[] = {
  [EXP_PAREN]       = {"(", ")"},
  [EXP_CURLY]       = {"{", "}"},
  [EXP_STR]         = {"\"", "\""},
  [EXP_STR2]        = {"'", "'"},
  [EXP_LONG]        = {"<<", ">>"},
};
static const char *SL_Comments[] = {
  [SL_COMM_1]          = "#",
  [SL_COMM_2]          = "//",
};
static struct Milexer_exp_ ML_Comments[] = {
  [ML_COMM_1]         = {"/*", "*/"},
};
//-- Milexer main configuration --------//
static Milexer ml = {
    .puncs       = GEN_MLCFG (Puncs),
    .keywords    = GEN_MLCFG (Keywords),
    .expression  = GEN_MLCFG (Expressions),
    .b_comment   = GEN_MLCFG (SL_Comments),
    .a_comment   = GEN_MLCFG (ML_Comments),
  };
//--------------------------------------//
#endif /* defined (ML_EXAMPLE_*) || defined (ML_TEST_*) */



/**
 **  Example 2 program
 **  Demonstrates Milexer flex API compatibility
 **/
#ifdef ML_EXAMPLE_2
#include <errno.h>

/* To redefine yy_getline,
   For this example, pass it to the compiler */
#ifdef YY_CUSTOM_GETLINE
#include <readline/readline.h>
#include <readline/history.h>
size_t
yy_getline (FILE *stream, char *buffer, size_t len)
{
  (void) stream;
  static char *tmp = NULL;
  if (tmp)
    free (tmp);
  tmp = readline (">>> ");
  if (tmp)
    {
      size_t n;
      if ((n = strlen (tmp)))
        add_history (tmp);
      n = (n < len) ? n : len;
      *((char *) mempcpy (buffer, tmp, n)) = '\n';
      return n+1;
    }
  else
    return 0;
}
#endif /* YY_CUSTOM_GETLINE */

/**
 *  In this example, we let the library handles
 *  all the memory managements by it's own, only
 *  calling yylex_destroy is required at the end.
 */
int
main (void)
{
  FILE *f;
  int ret;

  /* Set the language */
  yyml = &ml;

  /* Read tokens from stdin */
  while ( (ret = yylex() ) != -1)
    {
      printf ("Token[%.*s]: '%s'\n",
              3, ml_token_type_cstr[ret],
              yytext);

      if ( TK_KEYWORD == ret )
        switch (yyid)
          {
          case LANG_IF:    puts ("[IF]");   break;
          case LANG_ELSE:  puts ("[ELSE]"); break;
          case LANG_FI:    puts ("[FI]");   break;
          case LANG_FILE:
            {
              puts("-- lexing file ------------------------------");
              /* filepath might contain comma! */
              ML_DISABLE( &Puncs[PUNC_COMMA] );

              ret = yylex(); /* get the next token as filepath */
              if (-1 == ret)
                break;
              if ((f = fopen (yytext, "r")))
                {
                  yy_restart( f );
                  while ( (ret = yylex() ) != -1)
                    {
                      printf ("-- token[%.*s]: {%s}\n",
                              3, ml_token_type_cstr[ret],
                              yytext);
                    }
                  fclose (f);
                }
              else
                {
                  printf ("(%s): %s\n", yytext, strerror (errno));
                }

              ML_ENABLE( &Puncs[PUNC_COMMA] ); /* undo ML_DISABLE */
              puts ("-- EOF --------------------------------------");
            }
            break;
          }
    }

  yylex_destroy ();
  return 0;
}
#endif /* ML_EXAMPLE_2 */



/**
 **  Example 1 program
 **  By only using milexer, no flex API
 **/
#ifdef ML_EXAMPLE_1
static const char *exp_cstr[] = {
  [EXP_PAREN]       = "(*)",
  [EXP_CURLY]       = "{*}",
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

  printf ("\
Mini-Lexer Example 1\n\n\
This program parses your input and detects some pre-defined \
punctuation's, expressions, and keywords.\n\
It also parses the contents of expressions within parentheses \
separately, allowing the space character, which is a delimiter \
outside of these expressions.\n\n\
");

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
            END_ML_SLICE (&src);
          else
            SET_ML_SLICE (&src, line, n);
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
                    do {
                        /**
                         *  When the inner parentheses token is not a chunk,
                         *  the parser should not expect additional chunks
                         */
                        second_src.eof_lazy = (ret != NEXT_CHUNK);
                        /* prepare the new input source buffer */
                        SET_ML_SLICE (&second_src, tk.cstr, strlen (tk.cstr));

                        for (int _ret = 0; !NEXT_SHOULD_END (_ret); )
                          {
                            /* allow space character in tokens */
                            _ret = ml_next (&ml, &second_src, &tmp, PFLAG_IGSPACE);

                            if (tmp.type == TK_KEYWORD)
                              printf ("`%s`", tmp.cstr);
                            else if (tmp.type == TK_PUNCS)
                              printf ("%c", *tmp.cstr);
                          }

                        /* load the remaining chunks of the inner parentheses, if any */
                        if (ret != NEXT_CHUNK)
                          break;
                        ret = ml_next (&ml, &src, &tk, flg);
                      } while (!NEXT_SHOULD_LOAD (ret));

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
#include <stdarg.h>

typedef struct
{
  int parsing_flags;
  char *input;
  const Milexer_Token* etk; /* expected tokens */
} test_t;

/* testing token */
static Milexer_Token tk;

int breakp_test = -1, breakp_subtest = -1;

void
test_vlogf (int n, int _line_, int test_number,
          const char *format, ...)
{
  if (n > 0)
    {
      va_list ap;
      va_start (ap, format);
      printf ("fail!\n%s:%d:#%d:  ", __FILE__, _line_, n);
      vprintf (format, ap);
      printf ("\n  |  \
run it with debugger, and pass the first argument `%d:%d` \
for more investigating.",
              test_number, n);
      va_end (ap);
    }
  else
    puts ("pass.");
}

int
do_test__H (test_t *t, Milexer_Slice *src,
            int _line_, int test_number)
{
#define Return(n, format, ...) do {             \
    test_vlogf (n, _line_, test_number,         \
                format, ##__VA_ARGS__);         \
    if (n != -1) return n;                      \
  } while (0)

  int ret = 0, counter = 1;
  for (const Milexer_Token *tcase = t->etk;
       tcase != NULL && tcase->cstr != NULL; ++tcase, ++counter)
    {
      /* This only works on x86!! */
      if (test_number == breakp_test && counter == breakp_subtest)
        {
          printf ("\n\n***  Debugging Test %d:%d  ***\n\n",
                  test_number, counter);
          asm ("int3");
        }

      /* Retrieve the next token */
      if (NEXT_SHOULD_END (ret))
        Return (counter, "unexpected NEXT_END");
      ret = ml_next (&ml, src, &tk, t->parsing_flags);

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
      if (tcase->line && tcase->line != tk.line)
        {
          Return (counter, "token line number %ld != expected %ld",
                  tk.line, tcase->line);
        }
      if (tcase->col && tcase->col != tk.col)
        {
          Return (counter, "token column %ld != expected %ld",
                  tk.col, tcase->col);
        }
      if (ret == NEXT_NEED_LOAD && src->eof_lazy)
        {
          Return (counter, "unexpected NEXT_NEED_LOAD");
        }
    }

  Return (-1, NULL);
#undef Return
  return -1;
}

int
do_test (test_t *t, const char *msg, Milexer_Slice *src, int line_n)
{
  int ret;
  static int test_number = 0;

  ++test_number;
#ifdef _ML_DEBUG
  printf ("Test #%d: %s\n", test_number, msg);
#else
  printf ("Test #%d: %s... ", test_number, msg);
#endif

  SET_ML_SLICE (src, t->input, strlen (t->input));
  if ((ret = do_test__H (t, src, line_n, test_number)) != -1)
    return ret;
  return -1;
}

int
main (int argc, char **argv)
{
#define DO_TEST(test, msg)                         \
  if (do_test (test, msg, &src, __LINE__) != -1)   \
    { ret = 1; goto eof_main; }

  if (2 == argc)
    {
      char *p;
      breakp_test = atoi (argv[1]);
      if ((p = strchr (argv[1], ':')))
        breakp_subtest = atoi (p+1);
      else
        breakp_subtest = 1;
    }

  static Milexer_Slice src = {.lazy = 1};
  test_t t = {0};
  int ret = 0;
  tk = TOKEN_ALLOC (16);

  puts ("-- elementary tests -- ");
  {
    t = (test_t) {
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
      .parsing_flags = PFLAG_DEFAULT,
      .input = "ccc",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "ccc"},
        {0}
      }};
    DO_TEST (&t, "after delimiter");

    t = (test_t) {
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
      .parsing_flags = PFLAG_DEFAULT,
      .input = "aaaaaaaaaaaaaabcdefghi",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "aaaaaaaaaaaaaabc"},
        {.type = TK_KEYWORD, .cstr = "defghi"},
        {0}
      }};
    DO_TEST (&t, "after load recovery");

    t = (test_t) {
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
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA + BBB (te st) ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA",      .col=0},
        {.type = TK_PUNCS,      .cstr = "+",        .col=4},
        {.type = TK_KEYWORD,    .cstr = "BBB",      .col=6},
        {.type = TK_EXPRESSION, .cstr = "(te st)",  .col=10},
        {0}
      }};
    DO_TEST (&t, "basic puncs & expressions");

    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "()AAA+{a string . }(t e s t)",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION, .cstr = "()",             .col=0},
        {.type = TK_KEYWORD,    .cstr = "AAA",            .col=2},
        {.type = TK_PUNCS,      .cstr = "+",              .col=5},
        {.type = TK_EXPRESSION, .cstr = "{a string . }",  .col=6},
        {.type = TK_EXPRESSION, .cstr = "(t e s t)",      .col=19},
        {0}
      }};
    DO_TEST (&t, "adjacent puncs & expressions");

    /* the previous test didn't have tailing delimiter,
       but as it was an expression, the parser must
       treat this one as a separate token */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AA!=BB!= CC !=DD",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,      .cstr = "AA", .col=0},
        {.type = TK_PUNCS,        .cstr = "!=", .col=2},
        {.type = TK_KEYWORD,      .cstr = "BB", .col=4},
        {.type = TK_PUNCS,        .cstr = "!=", .col=6},
        {.type = TK_KEYWORD,      .cstr = "CC", .col=9},
        {.type = TK_PUNCS,        .cstr = "!=", .col=12},
        {.type = TK_KEYWORD,      .cstr = "DD", .col=14},
        {0}
      }};
    DO_TEST (&t, "multi-character puncs");
    
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "!= EEE ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,      .cstr = "DD"},
        {.type = TK_PUNCS,        .cstr = "!=",  .col=0},
        {.type = TK_KEYWORD,      .cstr = "EEE", .col=3},
        {0}
      }};
    DO_TEST (&t, "punc after load");

    /* long expression prefix & suffix */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "aa<<e x>>+<< AA>> <<BB >>",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "aa",        .col=0},
        {.type = TK_EXPRESSION,    .cstr = "<<e x>>",   .col=2},
        {.type = TK_PUNCS,         .cstr = "+",         .col=9},
        {.type = TK_EXPRESSION,    .cstr = "<< AA>>",   .col=10},
        {.type = TK_EXPRESSION,    .cstr = "<<BB >>",   .col=18},
        {0}
      }};
    DO_TEST (&t, "expressions with long prefix & suffix");
 
    /* fragmented expressions */
    t = (test_t) {
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

  puts ("-- partially disabled features --");
  {
    ML_DISABLE (&ml.puncs); /* disbale punctuation's */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA +BBB (te st) ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA",     .col=0},
        {.type = TK_KEYWORD,    .cstr = "+BBB",    .col=4},
        {.type = TK_EXPRESSION, .cstr = "(te st)", .col=9},
        {0}
      }};
    DO_TEST (&t, "disable all punctuation's");
    ML_ENABLE (&ml.puncs); /* undo ML_DISABLE */

    ML_DISABLE (&Puncs[PUNC_MINUS]); /* disbale only minus */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA+-BBB+",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA",    .col=0},
        {.type = TK_PUNCS,      .cstr = "+",      .col=3},
        {.type = TK_KEYWORD,    .cstr = "-BBB",   .col=4},
        {.type = TK_PUNCS,      .cstr = "+",      .col=8},
        {0}
      }};
    DO_TEST (&t, "disable a single punctuation");
    ML_ENABLE (&Puncs[PUNC_MINUS]); /* undo ML_DISABLE */

    ML_DISABLE (&Expressions[EXP_CURLY]); /* disable curly braces */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "()AAA+{XXX YYY }(t e s t)",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION, .cstr = "()",          .col=0},
        {.type = TK_KEYWORD,    .cstr = "AAA",         .col=2},
        {.type = TK_PUNCS,      .cstr = "+",           .col=5},
        {.type = TK_KEYWORD,    .cstr = "{XXX",        .col=6},
        {.type = TK_KEYWORD,    .cstr = "YYY",         .col=11},
        {.type = TK_KEYWORD,    .cstr = "}",           .col=15},
        {.type = TK_EXPRESSION, .cstr = "(t e s t)",   .col=16},
        {0}
      }};
    DO_TEST (&t, "disable only a single expression");
    ML_ENABLE (&Expressions[EXP_CURLY]); /* undo ML_DISABLE */

    ML_DISABLE (&ml.expression); /* disable all expressions */
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA+{XXX }( test) ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA",      .col=0},
        {.type = TK_PUNCS,      .cstr = "+",        .col=3},
        {.type = TK_KEYWORD,    .cstr = "{XXX",     .col=4},
        {.type = TK_KEYWORD,    .cstr = "}(",       .col=9},
        {.type = TK_KEYWORD,    .cstr = "test)",    .col=12},
        {0}
      }};
    DO_TEST (&t, "disable all expressions");
    ML_ENABLE (&ml.expression); /* undo ML_DISABLE */
  }

  puts ("-- parser flags --");
  {
    t = (test_t) {
      .parsing_flags = PFLAG_IGSPACE,
      .input = "a b c (x y z)  de f\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "a b c ",   .col=0},
        {.type = TK_EXPRESSION,    .cstr = "(x y z)",  .col=6},
        {.type = TK_KEYWORD,       .cstr = "  de f",   .col=13},
        {0}
      }};
    DO_TEST (&t, "ignore space flag");
    
    t = (test_t) {
      .parsing_flags = PFLAG_INEXP,
      .input = "AA'++'{ x y z}(test 2 . )",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AA",         .col=0},
        {.type = TK_EXPRESSION,    .cstr = "++",         .col=3},
        {.type = TK_EXPRESSION,    .cstr = " x y z",     .col=7},
        {.type = TK_EXPRESSION,    .cstr = "test 2 . ",  .col=15},
        {0}
      }};
    DO_TEST (&t, "inner expression flag");
    
    t = (test_t) {
      .parsing_flags = PFLAG_INEXP,
      .input = "<<o n e>><<t w o>> <<x y z >><<>>",
      .etk = (Milexer_Token []){
        {.type = TK_EXPRESSION,    .cstr = "o n e",   .col=2},
        {.type = TK_EXPRESSION,    .cstr = "t w o",   .col=11},
        {.type = TK_EXPRESSION,    .cstr = "x y z ",  .col=21},
        {.type = TK_EXPRESSION,    .cstr = ""},
        {0}
      }};
    DO_TEST (&t, "inner long expressions");
  }

  puts ("-- custom delimiters --");
  {
    /* making `.`,`@` and `0`,...,`9` delimiters */
    const char *delims[] = {".", "09", "@"};
    ml.delim_ranges = (Milexer_BEXP)GEN_MLCFG (delims);
    {
      t = (test_t) {
        .parsing_flags = PFLAG_INEXP,
        .input = "a@b cde0123 test.1xyz42",
        .etk = (Milexer_Token []){
          {.type = TK_KEYWORD,    .cstr = "a",       .col=0},
          {.type = TK_KEYWORD,    .cstr = "b cde",   .col=2},
          {.type = TK_KEYWORD,    .cstr = " test",   .col=11},
          {.type = TK_KEYWORD,    .cstr = "xyz",     .col=18},
          {0}
        }};
      DO_TEST (&t, "basic custom delimiter");

      t = (test_t) {
        .parsing_flags = PFLAG_ALLDELIMS,
        .input = "a b cde0123 test.1xyz42",
        .etk = (Milexer_Token []){
          {.type = TK_KEYWORD,    .cstr = "a",      .col=0},
          {.type = TK_KEYWORD,    .cstr = "b",      .col=2},
          {.type = TK_KEYWORD,    .cstr = "cde",    .col=4},
          {.type = TK_KEYWORD,    .cstr = "test",   .col=12},
          {.type = TK_KEYWORD,    .cstr = "xyz",    .col=18},
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
      .parsing_flags = PFLAG_DEFAULT,
      .input = "#0123456789abcdef 0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "basic comment");

    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA"},
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "keyword before comment");

    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA+#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,    .cstr = "AAA",    .col=0},
        {.type = TK_PUNCS,      .cstr = "+",      .col=3},
        {.type = TK_NOT_SET,    .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "punc before comment");
    
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "AAA(e x p)#0123456789abcdef\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AAA",      .col=0},
        {.type = TK_EXPRESSION,    .cstr = "(e x p)",  .col=3},
        {.type = TK_NOT_SET,       .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "expression before comment");

    t = (test_t) {
      .parsing_flags = PFLAG_INCOMMENT,
      .input = "AAA(e x p)#0123456789abcdefghi\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD,       .cstr = "AAA",               .col=0},
        {.type = TK_EXPRESSION,    .cstr = "(e x p)",           .col=3},
        {.type = TK_COMMENT,       .cstr = "#0123456789abcde",  .col=10},
        {.type = TK_COMMENT,       .cstr = "fghi"},
        {.type = TK_NOT_SET,       .cstr = ""}, // dry out
        {0}
      }};
    DO_TEST (&t, "with include comment flag");
  }

  puts ("-- multi-line comment --");
  {
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "/*t e \n s t*/ XXX num/ /666/*t e \n s t*/YYY ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "XXX", .col=7,  .line=2},
        {.type = TK_KEYWORD, .cstr = "num", .col=11, .line=2},
        {.type = TK_PUNCS,   .cstr = "/",   .col=14, .line=2},
        {.type = TK_PUNCS,   .cstr = "/",   .col=16, .line=2},
        {.type = TK_KEYWORD, .cstr = "666", .col=17, .line=2},
        {.type = TK_KEYWORD, .cstr = "YYY", .col=6,  .line=3},
        {0}
      }};
    DO_TEST (&t, "basic multi-line comment");

    t = (test_t) {
      .parsing_flags = PFLAG_INCOMMENT,
      .input = "XXX/*aaaaaaaabbbbbbbbccccccccdddddddd*/YYY ",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "XXX", .col=0},
        {.type = TK_COMMENT, .cstr = "/*aaaaaaaabbbbbb"},
        {.type = TK_COMMENT, .cstr = "bbccccccccdddddd"},
        {.type = TK_COMMENT, .cstr = "dd*/"},
        {.type = TK_KEYWORD, .cstr = "YYY", .col=39},
        {0}
      }};
    DO_TEST (&t, "with include comment flag");
  }
  

  puts ("-- some additional tests --");
  {
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "  AA AA \nBB CC\n \nDD\n  \n  \n\n  FF\n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "AA", .line=1, .col=2},
        {.type = TK_KEYWORD, .cstr = "AA", .line=1, .col=5},
        {.type = TK_KEYWORD, .cstr = "BB", .line=2, .col=0},
        {.type = TK_KEYWORD, .cstr = "CC", .line=2, .col=3},
        {.type = TK_KEYWORD, .cstr = "DD", .line=4, .col=0},
        {.type = TK_KEYWORD, .cstr = "FF", .line=8, .col=2},
        {0}
      }};
    DO_TEST (&t, "basic multi-line line number");

    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "  +AA (AA) \n+BB\n -CC\n \n(DD)\n",
      .etk = (Milexer_Token []){
        {.type = TK_PUNCS,      .cstr = "+",     .line=1, .col=2},
        {.type = TK_KEYWORD,    .cstr = "AA",    .line=1, .col=3},
        {.type = TK_EXPRESSION, .cstr = "(AA)",  .line=1, .col=6},
        {.type = TK_PUNCS,      .cstr = "+",     .line=2, .col=0},
        {.type = TK_KEYWORD,    .cstr = "BB",    .line=2, .col=1},
        {.type = TK_PUNCS,      .cstr = "-",     .line=3, .col=1},
        {.type = TK_KEYWORD,    .cstr = "CC",    .line=3, .col=2},
        {.type = TK_EXPRESSION, .cstr = "(DD)",  .line=5, .col=0},
        {0}
      }};
    DO_TEST (&t, "multi-line number with exp and punc");

    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = " /* comm1 */AAA \n /**/ BBB /* \n\n */ CCC \n",
      .etk = (Milexer_Token []){
        {.type = TK_KEYWORD, .cstr = "AAA", .line=1, .col=12},
        {.type = TK_KEYWORD, .cstr = "BBB", .line=2, .col=6},
        {.type = TK_KEYWORD, .cstr = "CCC", .line=4, .col=4},
        {0}
      }};
    DO_TEST (&t, "multi-line line number with comment");

    END_ML_SLICE (&src);
    t = (test_t) {
      .parsing_flags = PFLAG_DEFAULT,
      .input = "",
      .etk = (Milexer_Token []){
        {0}
      }};
    DO_TEST (&t, "end of lazy loading");
  }

  printf ("\n ***  All tests passed  ***\n");
 eof_main:
  TOKEN_FREE (&tk);
  return ret;
}
#endif /* ML_TEST_1 */
