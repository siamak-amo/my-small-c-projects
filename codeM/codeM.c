/* This file is part of my-small-c-projects
   
  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
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
 *   common Iranian ID number (code-e-melli) single file library
 *
 *   `codem` in the following is referred to code-e-melli.
 *   this library performs validating and making random codem's.
 *
 *   compilation:
 *     to compile the CLI program:
 *       cc -Wall -Wextra -D CODEM_IMPLEMENTATION
 *                        -D CODEM_CLI -o codeM codeM.c
 *
 *     to compile the test program:
 *       cc -Wall -Wextra -D CODEM_IMPLEMENTATION
 *          -D CODEM_TEST -D CODEM_DEBUG -o test codeM.c
 *
 *     other compilation options:
 *       `-D CODEM_NO_CITY_DATA`:
 *          to compile without data of cites (ignore codeM_data.h)
 *       `-D CODEM_DEBUG`:
 *          to enable printing some debug information
 *
 *     to include in c files:
 *       `
 *         #define CODEM_IMPLEMENTATION
 *         #include "codeM.c"
 *
 *         size_t my_rand_fun (void) {...}
 *         int main(...)
 *         {
 *           char codem[CODEM_BUF_LEN] = {0};
 *           // if you need to use any of the *_rand_* functions
 *           codem_rand_init (my_rand_fun);
 *         }
 *       `
 **/
#ifndef codeM__H__
#define codeM__H__
#include <string.h>

#ifndef CODEM_NO_CITY_DATA
#include "codeM_data.h" /* city_name and city_code data */
#else
/* city code length */
/* it's needed for some functions even without codeM_data.h */
#define CC_LEN 3
#endif

typedef size_t(*RandFunction)(void);
/**
 *  prand is the main pseudo-random number
 *  generator function used by this library
 *  initialization of prand is necessary for using
 *  any of *_rand_* functions and macros
 */
static RandFunction prand;

#ifndef CODEMDEF
#define CODEMDEF static inline
#endif

/* codem is a numeric string of length 10 */
#define CODEM_LEN 10
/* control digit of codem is the last digit of it */
#define CTRL_DIGIT_IDX 9
/* it's important to allocate your buffers for codem of */
/* length 11, 10 char for codem and a 0-byte at the end */
#define CODEM_BUF_LEN 11
/* city code errors */
#define CC_NOT_FOUND -1
#define CC_NOT_IMPLEMENTED -2

/* macro to initialize prand */
#define codem_rand_init(randfun) prand = &(randfun)

#define char2num(c) ((c) - '0')
#define num2char(x) ((x) + '0')

/**
 *  internal macro to check @codem buffer
 *  is only contains numeric characters,
 *  only use `codem_isvalid*` functions
 *  @codem should be normalized
 */
#define is_numeric(codem) ({int __res = 1;                 \
      for (int __i=CODEM_LEN-1; __i--!=0;){                \
        __res &= ('0'<=codem[__i] && '9'>=codem[__i]);     \
        if (!__res) break;                                 \
      }; __res;})

/**
 *  internal macro to calculate the control digit
 *  only use `codem_*_ctrl_digit` functions
 */
#define ctrl_digit__H(res, codem) do{                      \
    (res) = 0;                                             \
    for (int __i=CODEM_LEN-1; __i--!=0;)                   \
      (res) += (10 - __i) * char2num ((codem)[__i]);       \
    (res) %= 11;                                           \
    if ((res) >= 2) (res) = 11 - (res); } while(0)

/* get the name of city code @code */
#ifndef CODEM_NO_CITY_DATA
#define codem_cname(code)                                  \
  ({ int __city_idx = codem_ccode_idx (code);              \
    (__city_idx == CC_NOT_FOUND) ? "Not Found"             \
      : city_name[__city_idx]; })
#else
#define codem_cname(code) "Not Implemented"
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
#define city_rand_idx__H() (int)((prand ()) % CITY_COUNT)

/**
 *  return the correct control digit of codem
 *  ignore the current one
 */
CODEMDEF int codem_find_ctrl_digit (const char *codem);

/* set the control digit of @codem to the correct value */
CODEMDEF void codem_set_ctrl_digit (char *codem);

/**
 *  normalize @src and write the result on @dest
 *  normalized codem has exactly 10 digits
 *  return -1 when not possible
 */
CODEMDEF int codem_normcpy (char *dest, const char *src);

/* make @src normalized */
CODEMDEF int codem_norm (char *src);

/**
 *  validate the control digit of @codem
 *  after making it normalized,
 *  return 0 on normalization and validation failure
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
 *  generate random codem with suffix
 *  @offset is the length of the suffix
 */
CODEMDEF void codem_rands (char *codem, int offset);

/* write a valid random city code on @dest */
CODEMDEF void codem_rand_ccode (char *dest);

/**
 *  return the index of @codem[0:3] in city_code
 *  only use the `codem_cname_byidx` function
 *  to get the name of the city
 */
CODEMDEF int codem_ccode_idx (const char *codem);
#endif /* codeM__H__ */


/* implementation */
#ifdef CODEM_IMPLEMENTATION

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

CODEMDEF int
codem_normcpy (char *dest, const char *src)
{
  size_t l = strlen (src);

  if (l > CODEM_BUF_LEN-1)
    return -1; // error
  
  memset (dest, '0', CODEM_LEN - l);
  memcpy (dest + (CODEM_LEN - l), src, l);
  /* make dest null-terminated */
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
  if (!is_numeric (codem))
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

CODEMDEF void
codem_rand_gen (char *res, int len)
{
  unsigned long long rand = prand ();
  
  while (0 != len--)
    {
      res[len] = num2char (rand % 10);
      rand /= 10;
    }
}

CODEMDEF void
codem_rand_ccode (char *dest)
{
#ifndef CODEM_NO_CITY_DATA
  int code_count = CC_LEN;
  int idx = city_rand_idx__H ();
  size_t rand = prand ();
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
  size_t rand = prand ();
  for (int idx = CC_LEN-1; idx >= 0; --idx)
    {
      dest[idx] = (rand%10) + '0';
      rand /= 10;
    }
#endif
}

CODEMDEF void
codem_rand (char *codem)
{
  codem_rand_gen (codem, CODEM_LEN - 1);
  codem_set_ctrl_digit (codem);
}

CODEMDEF void
codem_rand2 (char *codem)
{
  /* write a random city code */
  codem_rand_ccode (codem);
  /* fill the rest by random numbers */
  codem_rand_gen (codem + CC_LEN, CODEM_LEN - CC_LEN - 1);
  codem_set_ctrl_digit (codem);
}

CODEMDEF void
codem_rands (char *codem, int offset)
{
  if (offset < 9)
    codem_rand_gen (codem + offset, 9 - offset);
  codem_set_ctrl_digit (codem);
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
  (void) codem; /* prevent compiler warning */
  return CC_NOT_IMPLEMENTED;
#endif
}
#endif /* CODEM_IMPLEMENTATION */



/*--------------*/
/* test program */
/*--------------*/
#ifdef CODEM_TEST
#include <stdio.h>
#include <assert.h>

#ifdef CODEM_DEBUG
#define DEBUG(fmt, ...) printf (fmt, ##__VA_ARGS__)
#else
#define DEBUG(fmt, ...) do{} while (0)
#endif

/* assert char x is a number '0', ..., '9' */
#define assert_isnumber(x) assert ((x)>='0' && (x)<='9')

/* assert x is a 10-digit numeric string */
#define assert_10numeric(x) do{                             \
    char *__tmp = x; size_t __count=0;                      \
    while ('\0' != *__tmp) {                                \
      assert_isnumber (*__tmp);                             \
      __tmp++; __count++;                                   \
    } assert (10 == __count); } while(0);

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
 *            codem_rands
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
  DEBUG ("suffix: %s, ", code);
  
  codem_rands (code, 3);
  DEBUG ("codem_rands: %s\n", code);

  /* check the suffix is intact */
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
rand ()
{
  return 4242424242UL;
}


int
main (void)
{
  codem_rand_init (rand);
  
  /**   test type 1  **/
  puts ("/* Running test type 1 *******************/");
  FUN_TEST (test_1_1, "\n")
  FUN_TEST (test_1_2, "normalize\n");

  /**   test type 2  **/
  puts ("\n/* Running test type 2 *******************/");
  FUN_TEST (test_2_1, "random code generator\n");
  FUN_TEST (test_2_2, "random code with suffix\n");
  FUN_TEST (test_2_3, "random code with city code\n");
  
  return 0;
}
#endif /* CODEM_TEST */



/*-------------*/
/* CLI program */
/*-------------*/
#ifdef CODEM_CLI
#ifdef CODEM_TEST
#error "cannot make CLI and test programs together"
#else

#include <stdio.h>
#include <time.h>
#include <assert.h>

#ifdef CODEM_DEBUG
/* debug macro to print codeM buffer */
#define printd_code(code)                       \
  printf ("[debug %s:%d] %s=[%s]\n",            \
          __func__, __LINE__, #code, code);
#else
#define printd_code(code) do{} while (0)
#endif

static inline void
help (void)
{
  puts (
  "v: validate            -  V: make my code valid\n"
  "c: randon city code    -  C: find my city name\n"
  "r: make random codem   -  R: make random codem with suffix\n"
  "q: quit                -  h: help\n");
}

/* super simple pseudo random number generator */
static size_t
ssrand ()
{
  unsigned long r = time (NULL);

  for (int i=7; i>0; --i)
    {
      r *= 0x666;
      r += 0x42;
    }

  return r;
}

/**
 *  internal function to be used by the exec_command function
 *  pass @inp NULL to print @message and read from stdin
 *  otherwise scan number from @inp into @dest
 */
int
numscanf(const char *restrict inp,
         const char *restrict message, char *dest)
{
  int n;

  if (inp == NULL)
    {
      printf (message);
      scanf ("%10s%n", dest, &n);
      n--; /* ignore newline */
    }
  else
    {
      int __tmp_num;
      sscanf (inp, "%d", &__tmp_num);
      snprintf (dest, CODEM_BUF_LEN, "%d%n", __tmp_num, &n);
    }

  return n;
}

/**
 *  returns 1 when program should be exited otherwise 0
 *  use @argv for commands that have argument otherwise
 *  pass it NULL to read from stdin
 */
int
exec_command (char prev_comm, char comm, char *argv)
{
  char tmp[CODEM_BUF_LEN] = {0};

  switch (comm)
    {
      /* validation */
    case 'v':
      numscanf (argv, "enter code: ", tmp);
      if (0 != codem_norm (tmp))
        {
          puts ("cannot be normalized");
          assert ( 0 && "unreachable code" );
        }
      printd_code(tmp);
      if (codem_isvalidn (tmp))
        {
          if (codem_ccode_isvalid (tmp))
            puts ("OK.");
          else
            puts ("city code was not found - code is valid");
        }
      else puts ("Not Valid.");
      break;

      /* make a code valid */
    case 'V':
      numscanf (argv, "enter code: ", tmp);
      if (0 != codem_norm (tmp))
        {
          puts ("cannot be normalized");
          assert ( 0 && "unreachable code" );
        }
      printd_code(tmp);
      codem_set_ctrl_digit (tmp);
      puts (tmp);
      break;

      /* make a random city code */
    case 'c':
      codem_rand_ccode (tmp);
      printf ("city code: %.3s -- city name: %s\n",
              tmp, codem_cname (tmp));
      break;
        
      /* find city name */
    case 'C':
      numscanf (argv, "enter code: ", tmp);
      printd_code(tmp);
      puts (codem_cname (tmp));
      break;
        
      /* make a random code */
    case 'r':
      codem_rand2 (tmp);
      puts (tmp);
      break;

      /* make a random code by suffix */
    case 'R':
      int off = numscanf (argv, "enter suffix: ", tmp);
      printd_code(tmp);
      if (off > CODEM_LEN)
        puts ("suffix is too long");
      else
        {
          codem_rands (tmp, off);
          puts (tmp);
        }
      break;

      /* print help */
    case 'h':
      help ();
      break;

    case 'q':
      return 1;

      /* empty command */
    case '\n':
    case ';':
    case '#':
    case ' ':
      return 0;

      /* invalid command */
    default:
      if (prev_comm == '\n' || prev_comm == '\0')
        printf ("invalid command -- %c\n", comm);
    }

  return 0;
}

int
main (int argc, char **argv)
{
  char comm = '\0', prev_comm = comm;
  /* initialize codeM random number generator function */
  codem_rand_init (ssrand);

  if (argc == 3 && strcmp (argv[1], "-c") == 0)
    {
      /* run commands from argv[2] */
      char *comm_ptr = argv[2];
      while (*comm_ptr != '\0')
        {
          prev_comm = comm;
          comm = *comm_ptr;
          comm_ptr++;
          if (exec_command (prev_comm, comm, comm_ptr))
            return 0;
        }
    }
  else /* shell mode */
    {
      /* check silent mode to not the print help message */
      if (!(argc > 1 && strcmp (argv[1], "-s") == 0))
        help ();

      while (1)
      {
        if ('\0' == comm || '\n' == comm)
          printf ("> ");

        prev_comm = comm;
        if (EOF == scanf ("%c", &comm))
          {
            puts("");
            return 0;
          }

        if (exec_command (prev_comm, comm, NULL))
          return 0;
      }
    }

  return 0;
}

#endif /* CODEM_TEST */
#endif /* CODEM_CLI */
