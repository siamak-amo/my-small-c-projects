/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>
   
  codeM is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.
  
  codeM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>. 
 */

/**
 *   file: codeM.c
 *   created on: 1 Oct 2023
 *
 *   Common Iranian ID number (code-e-melli) single-file library
 *   (self test program included)
 *
 *   The term `codem' in the following refers to `code-e-melli'
 *   This library validates and generates random codems
 *
 *   To use this library in python, see `codeM_py.c'
 *   also `codeM_shell.c' is a simple shell program for this library
 *   see `example.cm', an example script for codeM shell
 *
 *   Compilation:
 *     to compile the self test program:
 *       cc -ggdb -Wall -Wextra -Werror \
 *          -D CODEM_IMPLEMENTATION \
 *          -D CODEM_TEST \
 *          -D TEST_DEBUG -o test codeM.c
 *
 *     Compilation options:
 *       `-D CODEM_NO_CITY_DATA':
 *          to compile without data of cites (ignore codeM_data.h)
 *       `-D TEST_DEBUG':
 *          to enable printing some debug information
 *       `-D CODEM_FUZZY_SEARCH_CITYNAME':
 *          to enable fuzzy search, you need to provide
 *          the `leven.h` file (available in the same repository)
 *
 *     To include in c files:
 *       ```c
 *       #define CODEM_IMPLEMENTATION
 *       #define CODEM_FUZZY_SEARCH_CITYNAME
 *       #include "codeM.c"
 *
 *       size_t my_rand_fun (void) {...}
 *
 *       int
 *       main (void)
 *       {
 *         char codem[CODEM_BUF_LEN] = {0};
 *
 *         // if you need to use any of the *_rand_* functions
 *         codem_rand_init (my_rand_fun);
 *
 *         // do something here
 *         // see the examples in the accompanying programs
 *         ...
 *
 *         return 0;
 *       }
 *       ```
 **/
#ifndef codeM__H__
#define codeM__H__
#include <ctype.h>
#include <string.h>

/* city_name and city_code data */
#include "codeM_data.h"

#ifdef CODEM_FUZZY_SEARCH_CITYNAME
#  define LEVEN_IMPLEMENTATION /* fuzzy search feature */
#  include "leven.h" /* provide leven.h */
#endif

typedef size_t(*RandFunction)(void);
/**
 *  codem_srand is the main pseudo-random number
 *  generator function used by this library
 *  initialization of codem_srand is necessary for using
 *  any of *_rand_* functions and macros
 */
RandFunction codem_srand = NULL;
/* macro to initialize codem_srand */
#define codem_rand_init(randfun) codem_srand = &(randfun)

#ifndef CODEMDEF
#  define CODEMDEF static inline
#endif

/* codem is a numeric string of length 10 */
#define CODEM_LEN 10
/* control digit of codem is the last digit of it */
#define CTRL_DIGIT_IDX 9
/**
 *  it's important to allocate your buffers for codem of
 *  length 11, 10 characters for codem and a 0-byte at the end
 *  the functions within this library that are directly
 *  related to the codem (not explicitly mentioned), operate
 *  under the assumption that their codem buffer, has been
 *  allocated with a length of at least this size
 **/
#define CODEM_BUF_LEN 11

#define char2num(c) ((c) - '0')
#define num2char(x) ((x) + '0')
#define isnumber(c) (((c) >= '0') && ((c) <= '9'))

#undef UNUSED
#define UNUSED(x) (void)(x)

/**
 *  internal macro to calculate the control-digit of the @codem
 *  only use `codem_*_ctrl_digit` functions
 */
#define ctrl_digit__H(res, codem) do {                   \
    res = 0;                                             \
    for (int __idx = CODEM_LEN - 1; __idx-- != 0;)       \
      res += (10 - __idx) * char2num ((codem)[__idx]);   \
    res %= 11;                                           \
    if ((res) >= 2)                                      \
      res = 11 - (res);                                  \
  } while (0)

/* get the name of city code @code */
#ifndef CODEM_NO_CITY_DATA
// get by index
#  define codem_get_cname(idx) city_name[idx]
// get by index with error handling
#  define codem_cname_byidx(idx)                           \
  ({ int __idx = idx;                                      \
  ((__idx) == CC_NOT_FOUND) ? CCERR_NOT_FOUND              \
    : ((__idx) < 0) ? CCERR : codem_get_cname (__idx); })
// get by code
#  define codem_cname(code)                                \
  codem_cname_byidx(codem_ccode_idx (code))
#else
#  define codem_cname_byidx(idx) CCERR_NOT_IMPLEMENTED
#  define codem_cname(code) CCERR_NOT_IMPLEMENTED
#endif

/* get the codes of city at index @idx */
#ifndef CODEM_NO_CITY_DATA
#  define codem_ccode(idx)                                 \
  ({ int __idx = idx;                                      \
    (__idx == CC_NOT_FOUND) ? CCERR_NOT_FOUND              \
      : (__idx < 0) ? CCERR                                \
      : city_code[idx]; })
#else
#  define codem_ccode(idx) CCERR_NOT_IMPLEMENTED
#endif

/* validate only city code of @codem */
#define codem_ccode_isvalid(codem)                         \
  (codem_ccode_idx (codem) != CC_NOT_FOUND)

/* validate codem and it's city code */
#define codem_isvalid2(codem)                              \
  (codem_ccode_isvalid (codem) && codem_isvalid (codem))

/**
 *  internal macro to make random city indexes
 *  only use `codem_rand_ccode` function
 */
#define city_rand_idx__H() (int)((codem_srand ()) % CITY_COUNT)


/* function definitions */

/**
 *  return the correct control digit of codem
 *  ignore the current one
 */
CODEMDEF int codem_find_ctrl_digit (const char *codem);

/* set the control digit of @codem to the correct value */
CODEMDEF void codem_set_ctrl_digit (char *codem);

/**
 *  codem memcpy, only copies numeric characters
 *  and replaces other characters with '0'
 *  @return:  @dest
 */
CODEMDEF void *
codem_memnumcpy (char *restrict dest, const char *restrict src, size_t n);

/**
 *  makes the @src buffer numeric
 */
CODEMDEF void codem_memnum (char *src, size_t n);

/**
 *  is numeric function
 *  @return:  1 when @codem is numeric otherwise 0
 */
CODEMDEF int
codem_isnumeric (const char *codem);

/**
 *  normalize @src and write the result on @dest
 *  normalized codem has exactly 10 digits
 *  @dest will be made by adding enough '0' to the left of
 *  @src and making it numeric using the memnumcpy function
 *  @return:  -1 on failure, 0 on success
 */
CODEMDEF int
codem_normcpy (char *restrict dest, const char *restrict src);

/* make @src normalized */
CODEMDEF int codem_norm (char *src);

/**
 *  validate the control digit of @codem
 *  after making it normalized,
 *  @return: 0 on normalization and validation failure
 */
CODEMDEF int codem_isvalid (const char *codem);

/**
 *  validate the control digit of @codem
 *  @codem should be normalized
 */
CODEMDEF int codem_isvalidn (const char *codem);

/**
 *  make a random 10-digit valid codem
 *  city code is not necessarily valid
 */
CODEMDEF void codem_rand (char *codem);

/* make a random valid codem with a valid city code */
CODEMDEF void codem_rand2 (char *codem);

/**
 *  generate random codem with prefix
 *  @offset is the length of the prefix
 */
CODEMDEF void codem_randp (char *codem, int offset);

/**
 *  generate random codem with suffix
 *  @len is the length of the suffix
 *  city code of the result might be invalid
 *  @return:
 *    1  -> on success
 *    0  -> on failure (it might fail only when: @len >= 9)
 */
CODEMDEF int codem_rands (char *codem, int len);

/* write a valid random city code on @dest */
CODEMDEF void codem_rand_ccode (char *dest);

/**
 *  @return: the index of @codem[0:3] in city_code
 *  only use the `codem_cname_byidx` function
 *  to get the name of the city
 */
CODEMDEF int codem_ccode_idx (const char *codem);

/**
 *  search the @search among city names
 *  returns index of the best match
 */
CODEMDEF int
codem_cname_search (const char *search);
#endif /* codeM__H__ */


/* implementation */
#ifdef CODEM_IMPLEMENTATION

CODEMDEF int
codem_isnumeric (const char *codem)
{
  for (int idx = 0; idx < CODEM_LEN; ++idx)
    if (!isdigit (codem[idx]))
      return 0;
  return 1;
}

CODEMDEF int
codem_find_ctrl_digit (const char *codem)
{
  int res;

  ctrl_digit__H (res, codem);
  return res;
}

CODEMDEF void
codem_set_ctrl_digit (char *codem)
{
  int res;
  
  ctrl_digit__H (res, codem);
  codem[CTRL_DIGIT_IDX] = num2char (res);
}

CODEMDEF void *
codem_memnumcpy (char *restrict dest,
                 const char *restrict src, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    {
      if (isdigit (*src))
        dest[i] = src[i];
      else
        dest[i] = '0';
    }

  return dest;
}

CODEMDEF void
codem_memnum (char *src, size_t n)
{
  for (size_t i = 0; i < n; ++i)
    {
      if (!isdigit (src[i]))
        src[i] = '0';
    }
}

CODEMDEF int
codem_normcpy (char *restrict dest, const char *restrict src)
{
  int l = strlen (src);

  if (l > CODEM_BUF_LEN-1)
    return -1; // error
  
  memset (dest, '0', CODEM_LEN - l);

  char *__p = dest + (CODEM_LEN - l);
  for (l--; l >= 0; --l)
    {
      if (isnumber (src[l]))
        __p[l] = src[l];
      else
        return -1; /* cannot be normalized */
    }
  dest[CODEM_LEN] = '\0';
  return 0;
}

CODEMDEF int
codem_norm (char *src)
{
  int ret;
  char res[CODEM_BUF_LEN];

  ret = codem_normcpy (res, src);
  strcpy (src, res);
  
  return ret;
}

CODEMDEF int
codem_isvalidn (const char *codem)
{
  if (!codem_isnumeric (codem))
    return 0;

  return (codem[CTRL_DIGIT_IDX] ==
          num2char (codem_find_ctrl_digit (codem)));
}

CODEMDEF int
codem_isvalid (const char *codem)
{
  char codem_n[CODEM_BUF_LEN];
  
  if (0 != codem_normcpy (codem_n, codem))
    return 0;
  
  return codem_isvalidn (codem_n);
}

/**
 *  internal function
 *  fills @len bytes of the @res with random
 *  characters '0', ..., '9'
 */
void
codem_rand_gen (char *res, int len)
{
  size_t rand = codem_srand ();
  
  while (0 != len--)
    {
      *(res++) = num2char (rand % 10);
      rand /= 10;
    }
}

CODEMDEF void
codem_rand_ccode (char *dest)
{
#ifndef CODEM_NO_CITY_DATA
  int code_count = CC_LEN;
  int idx = city_rand_idx__H ();
  size_t rand = codem_srand ();
  const char *p = city_code[idx];
  const char *q = p;

  /* make a random choice between code at idx */
  while ('\0' != *q)
    {
      p = q;
      q += CC_LEN;
      code_count += CC_LEN;
      /* randomly break the loop -- code_count >= 6 */
      if (0 == rand % code_count)
        break;
    }
  strncpy (dest, p, CC_LEN);
#else
  size_t rand = codem_srand ();
  for (int idx = CC_LEN-1; idx >= 0; --idx)
    {
      dest[idx] = (rand%10) + '0';
      rand /= 10;
    }
#endif
  dest[CC_LEN] = '\0';
}

CODEMDEF void
codem_rand (char *codem)
{
  codem_rand_gen (codem, CODEM_LEN - 1);
  codem_set_ctrl_digit (codem);
  codem[CODEM_LEN] = '\0';
}

CODEMDEF void
codem_rand2 (char *codem)
{
  /* write a random city code */
  codem_rand_ccode (codem);
  /* fill the rest by random numbers */
  codem_rand_gen (codem + CC_LEN, CODEM_LEN - CC_LEN - 1);
  codem_set_ctrl_digit (codem);
  codem[CODEM_LEN] = '\0';
}

CODEMDEF int
codem_rands (char *codem, int len)
{
  if (len > 9)
    return codem_isvalidn (codem);

  int sum11 = 0;
  int exp_sum = char2num (codem[CTRL_DIGIT_IDX]);
  if (exp_sum >= 2)
    exp_sum = 11 - exp_sum; // in {0,1, 9,8,7,6,5,4,3,2}

  /* fill codem from index 1 until the suffix with random numbers */
  if (len < CODEM_LEN - 1)
    codem_rand_gen (codem + 1, CODEM_LEN - 1 - len);

  /* calculate codem's CTRL_DIGIT algorithm for index 2 to 9 */
  for (int idx = 1; idx < 9; ++idx)
    sum11 += (10 - idx) * char2num (codem[idx]);

  /**
   *  now we need to solve the equation below:
   *  10*c_10 + sum11 = exp_sum   (mod 11)
   *  => c_10 = sum11 - exp_sum   (mod 11)
   *  which always has a unique solution c_10 in {0, ..., 9}
   *  except for the case c_10 = 10 (mod 11), so we handle it first
   */
  sum11 -= exp_sum;
  sum11 %= 11; // in {0, ..., 10}
  if (sum11 == 10)
    {
      if (len >= 9)
        {
          // we cannot fix it, as it overwrites the suffix
          return 0;
        }
      else
        {
          int __c = char2num (codem[1]);
          if (__c == 0)
            {
              codem[1] = '1';
              sum11 = 8;
            }
          else
            {
              codem[1] = num2char (__c - 1);
              sum11 = 1;
            }
        }
    }
  // solve the equation when c_10 = 0, ..., 9   (mod 11)
  if (sum11 <= 9 && sum11 >= 0)
    {
      *codem = num2char (sum11);
      return 1;
    }
  else
    {
      *codem = '*'; /* unreachable */
      return 0; /* unknown error */
    }
}

CODEMDEF void
codem_randp (char *codem, int offset)
{
  if (offset < 9)
    codem_rand_gen (codem + offset, 9 - offset);
  codem_set_ctrl_digit (codem);
  codem[CODEM_LEN] = '\0';
}

CODEMDEF int
codem_ccode_idx (const char *codem)
{
#ifndef CODEM_NO_CITY_DATA
  int idx = 0;
  const char *p;
  
  while (idx < CITY_COUNT)
    {
      p = city_code[idx];
      do{
        if (0 == strncmp (p, codem, CC_LEN))
          return idx;
        p += CC_LEN;
      } while (*p != '\0');
      p++;
      idx++;
    }

  return CC_NOT_FOUND;
#else
  UNUSED (codem);
  return CC_NOT_IMPLEMENTED;
#endif
}

/**
 *  internal functions
 *  `__cname_fuzzy_search` and `__cname_normal_search`
 *  based on CODEM_NO_CITY_DATA and CODEM_FUZZY_SEARCH_CITYNAME
 *  to be used by the `codem_cname_search` function
 **/
#ifndef CODEM_NO_CITY_DATA
#  define MAX_TMP 64 /* max tmp buffer size */
#  define SAFE_LEN(x) MIN(x, MAX_TMP)

#  ifdef CODEM_FUZZY_SEARCH_CITYNAME
/* fuzzy search */
static inline int
__cname_fuzzy_search (const char *search)
{
  const char *p;
  char *tmp = malloc (MAX_TMP);
  size_t n = SAFE_LEN(strlen (search));
  size_t min_dist = -1; // size_t MAX value
  size_t min_dist_idx = CC_NOT_FOUND; // -1

  for (size_t idx = 0; idx < CITY_COUNT; ++idx)
    {
      p = city_name[idx];
      strncpy (tmp, p, n);

      size_t LD = leven_imm (tmp, search);
      if (LD < min_dist)
        {
          min_dist = LD;
          min_dist_idx = idx;
        }
    }

  free (tmp);
  if (min_dist > leven_strlen (search) / 2)
    return CC_NOT_FOUND;
  return min_dist_idx;
}
#  else /* ! CODEM_FUZZY_SEARCH_CITYNAME */
/* normal search */
static inline int
__cname_normal_search (const char *search)
{
  const char *p;
  size_t n = strlen (search);

  for (size_t idx = 0; idx < CITY_COUNT; ++idx)
    {
      p = city_name[idx];
      if (strncmp (search, p, n) == 0)
        return idx;
    }
  return CC_NOT_FOUND;
}
#  endif /* CODEM_FUZZY_SEARCH_CITYNAM */
#endif /* CODEM_NO_CITY_DATA */

CODEMDEF int
codem_cname_search (const char *search)
{
#ifdef CODEM_NO_CITY_DATA
  UNUSED (search);
  return CC_NOT_IMPLEMENTED;
#else
#  ifdef CODEM_FUZZY_SEARCH_CITYNAME
  return __cname_fuzzy_search (search);
#  else
  return __cname_normal_search (search);
#  endif
#endif
}

#endif /* CODEM_IMPLEMENTATION */



/*-----------------------*/
/* the self test program */
/*-----------------------*/
#ifdef CODEM_TEST
#include <stdio.h>
#include <assert.h>

#ifdef TEST_DEBUG
#  define DEBUG(fmt, ...) printf (fmt, ##__VA_ARGS__)
#else
#  define DEBUG(fmt, ...) do{} while (0)
#endif

/* assert char x is a number '0', ..., '9' */
#define assert_isnumber(x) assert (isnumber (x))

/* assert x is a 10-digit numeric string */
#define assert_10numeric(x) do{                             \
    char *__tmp = x; size_t __count=0;                      \
    while ('\0' != *__tmp) {                                \
      assert_isnumber (*__tmp);                             \
      __tmp++; __count++;                                   \
    } assert (10 == __count); } while (0);

#define FUN_TEST(fun, comment)                              \
  printf (" * "#fun" -- %s", comment);                      \
  fun ();                                                   \
  puts (" * PASS");


static inline int
validate (const char *codem)
{
  int r = codem_isvalidn (codem);

  if (r)
    DEBUG ("code %s is valid.\n", codem);
  else
    DEBUG ("code %s is not valid.\n", codem);
  
  return r;
}

/**
 *    Test Type 1
 *    
 *    tests:  codem_isvalidn,
 *            codem_set_ctrl_digit,
 *            codem_norm
 **/
static void
test_1_1 ()
{
  /* code doesn't need to be normalized */
  char code[CODEM_BUF_LEN] = "1234567890";
  /* code is not valid */
  assert (!validate (code));

  codem_set_ctrl_digit (code);
  /* code must be 1234567891 */
  DEBUG ("codem_set_ctrl_digit: %s\n", code);

  /* code must be valid */
  assert (validate (code));
}

static void
test_1_2 ()
{
  /* code needs to be normalized */
  char code[CODEM_BUF_LEN] = "567890";
  DEBUG ("before codem_norm: %s\n", code);
  
  codem_norm (code);
  DEBUG ("codem_norm: %s\n", code);
  /* code must be 0000567890 */
  assert (0 == strncmp (code, "0000567890", 10));
  
  /* code is not valid */
  assert (!validate (code));
  
  codem_set_ctrl_digit (code);
  /* code must be 0000567892 */
  DEBUG ("set_ctrl_digit: %s\n", code);

  /* code must be valid */
  assert (validate (code));
}

/**
 *    Test Type 2
 *    
 *    tests:  codem_rand, codem_rand2
 *            codem_randp
 *            codem_ccode_idx
 **/
static void
test_2_1 ()
{
  char code[CODEM_BUF_LEN] = {0};

  codem_rand (code);
  DEBUG ("codem_rand: %s\n", code);

  /* code must be a 10-digit numeric string */
  assert_10numeric (code);
  
  /* code must be valid */
  assert (validate (code));
}

static void
test_2_2 ()
{
  char code[CODEM_BUF_LEN] = "666";
  DEBUG ("prefix: %s, ", code);
  
  codem_randp (code, 3);
  DEBUG ("codem_randp: %s\n", code);

  /* check the prefix is intact */
  assert (0 == strncmp (code, "666", 3));

  /* code must be a 10-digit numeric string */
  assert_10numeric (code);

  /* code must be valid */
  assert (validate (code));
}

static void
test_2_3 ()
{
  char code[CODEM_BUF_LEN] = "";

  codem_rand2 (code);
  DEBUG ("codem_rand2: %s\n", code);

  int idx = codem_ccode_idx (code);

  /* check city code is valid */
  assert (idx != CC_NOT_FOUND);

  DEBUG ("city name: %s\n", city_name[idx]);
  /* idx must be 462 */
  assert (idx == 462);

  /* codem_isvalid2 must return 1 */
  assert (codem_isvalid2 (code));
}

/* pseudo random number generator function */
/* which always returns const 4242424242UL */
static inline size_t 
test_rand ()
{
  return 4242424242UL;
}

int
main (void)
{
  codem_rand_init (test_rand);
  
  /**   test type 1  **/
  puts ("/* Running test type 1 *******************/");
  FUN_TEST (test_1_1, "\n")
  FUN_TEST (test_1_2, "normalize\n");

  /**   test type 2  **/
  puts ("\n/* Running test type 2 *******************/");
  FUN_TEST (test_2_1, "random code generator\n");
  FUN_TEST (test_2_2, "random code with prefix\n");
  FUN_TEST (test_2_3, "random code with city code\n");
  
  return 0;
}
#endif /* CODEM_TEST */
