/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>
   Copyright 2026 Ahmad <edu.siamak@gmail.com>

  This software is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  This software is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: fuzz_sqlite.c
    created on: 7 May 2026

    Fuzzy `LIKE' function for the sqlite3 database.

    This file implements a function similar to LIKE in sqlite3,
    but using Levenshtein Distance to compare strings.

    Usage:
      1- Loading the shared object of this extension (fuzz_like.so),
         in the sqlite3 environment:
      sqlite> .load ./fuzz_like
      sqlite>  SELECT FUZZ_LIKE(1,1);    -- - smoke test

      2- Including this file in C programs that link with sqlite3:
      ```{c}
        #define FUZZ_SQLITE_INCLIB
        #include "fuzz_sqlite.c"

        int main ( void ) {
            sqlite3 *db = NULL;

            // After initializing @db
            fuzz_sqlite_load (db);
        }
      ```

      Now, the `FUZZ_LIKE' function is available:
        SELECT v FROM T t WHERE FUZZ_LIKE('%xxx%', v);

      see the <FUZZ_SQLITE_TEST_1> program for more details.

    Compilation:
      - shared object extension:
      $ cc -Wall -Wextra -Werror -ggdb -O3 \
           -shared -fPIC fuzz_sqlite.c -o sqlite_fuzz.so

      - the example program:
      $ cc -Wall -Wextra -Werror -ggdb fuzz_sqlite.c \
           -D FUZZ_SQLITE_TEST_1 -D FUZZ_SQLITE_INCLIB -lsqlite3
 */
#ifndef FUZZ_SQLITE__H__
#define FUZZ_SQLITE__H__
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef FUZZ_SQLITE_INCLIB
#  include <sqlite3ext.h>
SQLITE_EXTENSION_INIT1
#else
#  include <sqlite3.h>
#endif

typedef unsigned int u32;
typedef unsigned char u8;
typedef short int i16;

/** used by: is_leven_exceeded()
 *  If the current Levenshtein Distance exceeds this value,
 *  there is no point with reading more characters in patternCompare()
 */
#ifndef LEVEN_THRESHOLD
#define LEVEN_THRESHOLD 3
#endif

/**
 *  Maximum temporary buffer for calculating Levenshtein Distance
 *  For longer input, calculate_leven_imm() resets the calculated
 *  distance and starts from zero.
 */
#ifndef LEVEN_TMP_CAP
#define LEVEN_TMP_CAP 128
#endif

static int __leven_idx = 0;
#define LEVEN_RESET() (__leven_idx = 0)

#define sqlite3Toupper(x)  toupper ((unsigned char)(x))
#define sqlite3Tolower(x)  tolower ((unsigned char)(x))


static inline int
calculate_leven_imm (u32 c, u32 c2)
{
  static int tmp[LEVEN_TMP_CAP];

  if (++__leven_idx == LEVEN_TMP_CAP)
    __leven_idx = 1;
  tmp[0] = __leven_idx;
  tmp[__leven_idx] = c2;

  for (int y=1, diag = __leven_idx-1; y <= __leven_idx; ++y)
    {
      int p_diag = tmp[y];
      int test = tmp[y] + 1;
      tmp[y] = test;

      test = tmp[y-1] + 1;
      if (test < tmp[y])
        tmp[y] = test;

      test = diag + ((c == c2) ? 0 : 1);
      if (test < tmp[y])
        tmp[y] = test;

      diag = p_diag;
    }
  return tmp[__leven_idx];
}

static int
is_leven_exceeded (u32 c, u32 c2, int noCaseSensitive)
{
  if (noCaseSensitive && c<0x80 && c2<0x80)
    {
      c = sqlite3Tolower (c);
      c2 = sqlite3Tolower (c2);
    }

  int leven = calculate_leven_imm (c, c2);
  // fprintf (stderr, "(%d)", leven);
  if (leven >= LEVEN_THRESHOLD)
    {
      // fprintf (stderr, ".\n");
      __leven_idx = 0;
      return true;
    }
  return 0;
}


/********************************************************************
 *  The most of the following code is taken from sqlite3.c
 *  of the sqlite3 project.
 *
 *  The LIKE function in sqlite3, is implemented using the
 *  patternCompare() call; we used the same approach with the
 *  difference that, instead of comparing strings byte-wise,
 *  we calculate Levenshtein Distance, and if it exceeds a threshold
 *  it fails the match.
 ********************************************************************/
struct compareInfo
{
  u8 matchAll;          /* "*" or "%" */
  u8 matchOne;          /* "?" or "_" */
  u8 matchSet;          /* "[" or 0 */
  u8 noCase;            /* true to ignore case differences */
};

/*
** Possible error returns from patternCompare()
*/
#define SQLITE_MATCH             0
#define SQLITE_NOMATCH           1
#define SQLITE_NOWILDCARDMATCH   2

#ifndef ESQLITE_LIMIT_LIKE_PATTLEN
#define ESQLITE_LIMIT_LIKE_PATTLEN 100
#endif

/*
** For LIKE and GLOB matching on EBCDIC machines, assume that every
** character is exactly one byte in size.  Also, provide the Utf8Read()
** macro for fast reading of the next character in the common case where
** the next character is ASCII.
*/
#if defined(SQLITE_EBCDIC)
# define sqlite3Utf8Read(A)        (*((*A)++))
# define Utf8Read(A)               (*(A++))
#else
# define Utf8Read(A)               (A[0]<0x80?*(A++):sqlite3Utf8Read(&A))
#endif

/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SQLITE_SKIP_UTF8(zIn) {                        \
  if( (*(zIn++))>=0xc0 ){                              \
    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \
  }                                                    \
}

/*
** pZ is a UTF-8 encoded unicode string. If nByte is less than zero,
** return the number of unicode characters in pZ up to (but not including)
** the first 0x00 byte. If nByte is not less than zero, return the
** number of unicode characters in the first nByte of pZ (or up to
** the first 0x00, whichever comes first).
*/
static int
sqlite3Utf8CharLen (const char *zIn, int nByte)
{
  int r = 0;
  const u8 *z = (const u8*)zIn;
  const u8 *zTerm;
  if( nByte>=0 )
    zTerm = &z[nByte];
  else
    zTerm = (const u8*)(-1);

  assert ( z<=zTerm );
  while ( *z!=0 && z<zTerm )
    {
      SQLITE_SKIP_UTF8(z);
      r++;
    }
  return r;
}


/*
** Translate a single UTF-8 character.  Return the unicode value.
**
** During translation, assume that the byte that zTerm points
** is a 0x00.
**
** Write a pointer to the next unread byte back into *pzNext.
**
** Notes On Invalid UTF-8:
**
**  *  This routine never allows a 7-bit character (0x00 through 0x7f) to
**     be encoded as a multi-byte character.  Any multi-byte character that
**     attempts to encode a value between 0x00 and 0x7f is rendered as 0xfffd.
**
**  *  This routine never allows a UTF16 surrogate value to be encoded.
**     If a multi-byte character attempts to encode a value between
**     0xd800 and 0xe000 then it is rendered as 0xfffd.
**
**  *  Bytes in the range of 0x80 through 0xbf which occur as the first
**     byte of a character are interpreted as single-byte characters
**     and rendered as themselves even though they are technically
**     invalid characters.
**
**  *  This routine accepts over-length UTF8 encodings
**     for unicode values 0x80 and greater.  It does not change over-length
**     encodings to 0xfffd as some systems recommend.
*/
#define READ_UTF8(zIn, zTerm, c)                           \
  c = *(zIn++);                                            \
  if( c>=0xc0 ){                                           \
    c = sqlite3Utf8Trans1[c-0xc0];                         \
    while( zIn<zTerm && (*zIn & 0xc0)==0x80 ){             \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }        \
  }

static u32
sqlite3Utf8Read (const unsigned char **pz)
{
  static const u8 sqlite3Utf8Trans1[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
  };
  unsigned int c;
  /* Same as READ_UTF8() above but without the zTerm parameter.
  ** For this routine, we assume the UTF8 string is always zero-terminated.
  */
  c = *((*pz)++);
  if ( c>=0xc0 ){
    c = sqlite3Utf8Trans1[c-0xc0];
    while ( (*(*pz) & 0xc0)==0x80 )
      {
        c = (c<<6) + (0x3f & *((*pz)++));
      }
    if ( c<0x80
         || (c&0xFFFFF800)==0xD800
         || (c&0xFFFFFFFE)==0xFFFE ){  c = 0xFFFD; }
  }
  return c;
}

/*
** Compare two UTF-8 strings for equality where the first string is
** a GLOB or LIKE expression.  Return values:
**
**    SQLITE_MATCH:            Match
**    SQLITE_NOMATCH:          No match
**    SQLITE_NOWILDCARDMATCH:  No match in spite of having * or % wildcards.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** Like matching rules:
**
**      '%'       Matches any sequence of zero or more characters
**
***     '_'       Matches any one character
**
**      Ec        Where E is the "esc" character and c is any other
**                character, including '%', '_', and esc, match exactly c.
**
** The comments within this routine usually assume glob matching.
**
** This routine is usually quick, but can be N**2 in the worst case.
*/
static int patternCompare(
  const u8 *zPattern,              /* The glob pattern */
  const u8 *zString,               /* The string to compare against the glob */
  const struct compareInfo *pInfo, /* Information about how to do the compare */
  u32 matchOther)                  /* The escape char (LIKE) or '[' (GLOB) */
{
  u32 c, c2;                       /* Next pattern and input string chars */
  u32 matchOne = pInfo->matchOne;  /* "?" or "_" */
  u32 matchAll = pInfo->matchAll;  /* "*" or "%" */
  u8 noCase = pInfo->noCase;       /* True if uppercase==lowercase */
  const u8 *zEscaped = 0;          /* One past the last escaped input char */
  LEVEN_RESET ();

  while ( (c = Utf8Read(zPattern)) != 0 )
    {
      if ( c==matchAll )
        {  /* Match "*" */
          /* Skip over multiple "*" characters in the pattern.  If there
          ** are also "?" characters, skip those as well, but consume a
          ** single character of the input string for each "?" skipped */
          while ( (c=Utf8Read(zPattern)) == matchAll
                  || (c == matchOne && matchOne!=0) )
            {
              if ( c==matchOne && sqlite3Utf8Read(&zString)==0 )
                return SQLITE_NOWILDCARDMATCH;
            }
          if ( c==0 )
            return SQLITE_MATCH;   /* "*" at the end of the pattern matches */
          else if ( c==matchOther )
            {
              if ( pInfo->matchSet==0 )
                {
                  c = sqlite3Utf8Read (&zPattern);
                  if ( c==0 )
                    return SQLITE_NOWILDCARDMATCH;
                }
              else
                {
                  /* "[...]" immediately follows the "*".  We have to do a slow
                  ** recursive search in this case, but it is an unusual case. */
                  assert ( matchOther<0x80 );  /* '[' is a single-byte character */
                  while ( *zString )
                    {
                      int bMatch = patternCompare (&zPattern[-1], zString, pInfo, matchOther);
                      if ( bMatch!=SQLITE_NOMATCH )
                        return bMatch;
                      SQLITE_SKIP_UTF8 (zString);
                    }
                  return SQLITE_NOWILDCARDMATCH;
                }
            }

          /* At this point variable c contains the first character of the
          ** pattern string past the "*".  Search in the input string for the
          ** first matching character and recursively continue the match from
          ** that point.
          **
          ** For a case-insensitive search, set variable cx to be the same as
          ** c but in the other case and search the input string for either
          ** c or cx.
          */
          if ( c<0x80 )
            {
              char zStop[3];
              int bMatch;
              if ( noCase )
                {
                  zStop[0] = sqlite3Toupper(c);
                  zStop[1] = sqlite3Tolower(c);
                  zStop[2] = 0;
                }
              else
                {
                  zStop[0] = c;
                  zStop[1] = 0;
                }
              while (1)
                {
                  zString += strcspn ((const char*)zString, zStop);
                  if ( zString[0]==0 ) break;
                  zString++;
                  bMatch = patternCompare (zPattern,zString,pInfo,matchOther);
                  if ( bMatch!=SQLITE_NOMATCH ) return bMatch;
                }
            }
          else
            {
              int bMatch;
              while ( (c2 = Utf8Read (zString)) != 0 )
                {
                  if ( c2!=c )
                    continue;
                  bMatch = patternCompare (zPattern,zString,pInfo,matchOther);
                  if ( bMatch!=SQLITE_NOMATCH )
                    return bMatch;
                }
            }
          return SQLITE_NOWILDCARDMATCH;
        }
      if ( c==matchOther )
        {
          if ( pInfo->matchSet==0 )
            {
              if (0 == (c = sqlite3Utf8Read (&zPattern)))
                return SQLITE_NOMATCH;
              zEscaped = zPattern;
            }
          else
            {
              u32 prior_c = 0;
              int seen = 0;
              int invert = 0;
              if (0 == (c = sqlite3Utf8Read (&zString)))
                return SQLITE_NOMATCH;
              c2 = sqlite3Utf8Read (&zPattern);
              if ( c2=='^' )
                {
                  invert = 1;
                  c2 = sqlite3Utf8Read (&zPattern);
                }
              if ( c2==']' )
                {
                  if ( c==']' ) seen = 1;
                  c2 = sqlite3Utf8Read (&zPattern);
                }
              while ( c2 && c2!=']' )
                {
                  if ( c2=='-' && zPattern[0]!=']' && zPattern[0]!=0 && prior_c>0 )
                    {
                      c2 = sqlite3Utf8Read (&zPattern);
                      if ( c>=prior_c && c<=c2 ) seen = 1;
                      prior_c = 0;
                    }
                  else
                    {
                      if ( c==c2 )
                        seen = 1;
                      prior_c = c2;
                    }
                  c2 = sqlite3Utf8Read (&zPattern);
                }
              if ( c2==0 || (seen ^ invert)==0 )
                return SQLITE_NOMATCH;
              continue;
            }
        }
      c2 = Utf8Read (zString);
      if( ! is_leven_exceeded (c, c2, noCase) )
        continue;
      if( c==matchOne && zPattern!=zEscaped && c2!=0 )
        continue;
      return SQLITE_NOMATCH;
    }
  return *zString==0 ? SQLITE_MATCH : SQLITE_NOMATCH;
}

static void
like_func (sqlite3_context *ctx, int argc, sqlite3_value **argv)
{
  const unsigned char *zA, *zB;
  u32 escape;
  int nPat;
  // sqlite3 *db = sqlite3_context_db_handle (ctx);
  struct compareInfo *pInfo = sqlite3_user_data (ctx);
  struct compareInfo backupInfo;

#ifdef SQLITE_LIKE_DOESNT_MATCH_BLOBS
  if( sqlite3_value_type (argv[0]) == SQLITE_BLOB
   || sqlite3_value_type (argv[1]) == SQLITE_BLOB
  ){
#ifdef SQLITE_TEST
    sqlite3_like_count++;
#endif
    sqlite3_result_int (ctx, 0);
    return;
  }
#endif

  /* Limit the length of the LIKE or GLOB pattern to avoid problems
  ** of deep recursion and N*N behavior in patternCompare().
  */
  nPat = sqlite3_value_bytes (argv[0]);
  if( nPat > ESQLITE_LIMIT_LIKE_PATTLEN ){
    sqlite3_result_error (ctx, "LIKE or GLOB pattern too complex", -1);
    return;
  }
  if( argc==3 ){
    /* The escape character string must consist of a single UTF-8 character.
    ** Otherwise, return an error.
    */
    const unsigned char *zEsc = sqlite3_value_text (argv[2]);
    if( zEsc==0 ) return;
    if( sqlite3Utf8CharLen ((char*)zEsc, -1) != 1 ){
      sqlite3_result_error (ctx,
         "ESCAPE expression must be a single character", -1);
      return;
    }
    escape = sqlite3Utf8Read (&zEsc);
    if ( escape==pInfo->matchAll || escape==pInfo->matchOne )
      {
        memcpy (&backupInfo, pInfo, sizeof (backupInfo));
        pInfo = &backupInfo;
        if ( escape==pInfo->matchAll ) pInfo->matchAll = 0;
        if ( escape==pInfo->matchOne ) pInfo->matchOne = 0;
      }
  }
  else
    escape = pInfo->matchSet;

  zB = sqlite3_value_text (argv[0]);
  zA = sqlite3_value_text (argv[1]);
  if ( zA && zB ){
#ifdef SQLITE_TEST
    sqlite3_like_count++;
#endif
    sqlite3_result_int (ctx,
       patternCompare (zB, zA, pInfo, escape) == SQLITE_MATCH
    );
  }
}

static inline int
esqlite_register_fuzz_like (sqlite3 *db, const char *cmd)
{
  /* The correct SQL-92 behavior is for the LIKE operator to ignore
  ** case.  Thus  'a' LIKE 'A' would be true. */
  static const struct compareInfo likeInfoNorm = { '%', '_',   0, 1 };

  int rc = 0;
  struct compareInfo *pInfo = (struct compareInfo*) &likeInfoNorm;
  for (int nArg=2; nArg<=3; nArg++)
    {
      rc |= sqlite3_create_function
        (db, cmd, nArg, SQLITE_UTF8, pInfo, like_func,
         NULL, NULL
        );
      if (SQLITE_OK != rc)
        {
          fprintf (stderr, "WARNING: could not register `%s'.\n", cmd);
        }
    }

  return rc;
}

int
fuzz_sqlite_load (sqlite3 *db)
{
  int rc = 0;
  rc |= esqlite_register_fuzz_like (db, "fuzz_like");
  return rc;
}


#ifndef FUZZ_SQLITE_INCLIB
/**
 *  Entry point for shared library
 *  It gets called by sqlite3, if you load it (.load)
 *  Do not call it directly from your C programs.
 */
int
sqlite3_fuzzlike_init (sqlite3 *db, char **pzErrMsg,
                       const sqlite3_api_routines *pApi)
{
  (void) pzErrMsg;
  SQLITE_EXTENSION_INIT2 (pApi);
  return fuzz_sqlite_load (db);
}
#endif /* FUZZ_SQLITE_INCLIB */

#endif /* FUZZ_SQLITE__H__ */


/******************************************************
 *  Test 1 program.
 *  A minimal program to demonstrate FUZZ_LIKE.
 *
 *  The `FUZZ_SQLITE_INCLIB' macro should be defined.
 ******************************************************/
#ifdef FUZZ_SQLITE_TEST_1

#define lenof(arr) (sizeof (arr) / sizeof (arr[0]))
#define warnln(fmt, ...) \
  fprintf (stderr, "%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

static sqlite3 *db = NULL;
#define sqlite3_safe_close(_db) (sqlite3_close (_db), _db = NULL)


int
add_dummy_data2db (sqlite3 *db)
{
  int rc = 0;
  char *errMsg;
  const char *createTableSQL = "\
CREATE TABLE text ( \
    id INTEGER PRIMARY KEY AUTOINCREMENT, \
    value TEXT NOT NULL \
);";
  rc = sqlite3_exec (db, createTableSQL, NULL, NULL, &errMsg);
  if (rc != SQLITE_OK)
    {
      warnln ("SQL error (CREATE TABLE): %s", errMsg);
      sqlite3_free (errMsg);
      return 1;
    }
  printf ("Table 'text' created successfully.\n");
    
  /* Insert some data using sqlite3_exec (simple method) */
  const char *insertSQL[] =
    {
      "INSERT INTO text (value) VALUES ('1');",
      "INSERT INTO text (value) VALUES ('12');",
      "INSERT INTO text (value) VALUES ('123');",
      "INSERT INTO text (value) VALUES ('1234');",
      "INSERT INTO text (value) VALUES ('123456');"
      "INSERT INTO text (value) VALUES ('1234567');"
    };
    
  int i;
  for (i = 0; i < (int) lenof (insertSQL); i++)
    {
      rc = sqlite3_exec (db, insertSQL[i], NULL, NULL, &errMsg);
      if (rc != SQLITE_OK)
        {
          warnln ("SQL error (INSERT): %s", errMsg);
          sqlite3_free (errMsg);
          break;
        }
    }
  printf ("%d records inserted successfully.\n\n", i);
  return rc;
}

void
lookup (sqlite3 *db, const char *name)
{
  sqlite3_stmt *stmt;

#define TMP_CAP 256
  char tmp[TMP_CAP];
  snprintf (tmp, TMP_CAP,
            "SELECT id,value FROM text WHERE FUZZ_LIKE('%s', value)", name);

  int rc = sqlite3_prepare_v2 (db, tmp, -1, &stmt, NULL);
  if (rc != SQLITE_OK)
    {
      warnln ("Failed to prepare SELECT statement: %s", sqlite3_errmsg (db));
    }
  printf ("-----------------------------------------------\n"
          "Query: '%s'\n"
          "Results: \n", tmp);
  printf ("%-4s %-15s\n" "%-4s %-15s\n",
          "ID", "Value",
          "--", "-----");
  while (sqlite3_step (stmt) == SQLITE_ROW)
    {
      int id = sqlite3_column_int (stmt, 0);
      const unsigned char *value = sqlite3_column_text (stmt, 1);
      printf ("%-4d %-15s\n", id, value);
    }
  puts ("");
  sqlite3_finalize (stmt);
}

void
cleanup (int rc, void *__ptr)
{
  (void) rc;
  sqlite3 *db = (sqlite3 *) __ptr;
  if (db)
    {
      sqlite3_safe_close (db);
      printf ("\nDatabase closed, memory freed.\n");
    }
}

int
main (void)
{
  int rc;
  on_exit (cleanup, db);

  rc = sqlite3_open (":memory:", &db);
  if (rc != SQLITE_OK)
    {
      warnln ("Cannot open in-memory database: %s", sqlite3_errmsg (db));
      return 1;
    }

  /**
   *  Loading fuzz_sqlite extension.
   */
  fuzz_sqlite_load (db);

  /* Create dummy data for testing. */
  rc = add_dummy_data2db (db);
  if (0 != rc)
    return rc;
 
  /**
   *  Query and display the data
   *  Now we want to test our extension: `FUZZ_LIKE'
   */
  {
    lookup (db, "x234xx%");
    lookup (db, "xx345%");
    lookup (db, "%111x%");
  }

  return 0;
}

#endif /* FUZZ_SQLITE_TEST_1 */
