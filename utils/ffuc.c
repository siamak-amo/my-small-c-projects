/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>
   Copyright 2025-2026 Ahmad <edu.siamak@gmail.com>

  FFuc is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License,
  or (at your option) any later version.

  FFuc is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/** file: ffuc.c
    created on: 7 May 2025

  FFuc (read it: EF-Fuc)  -  ffuf program written in C.
  Single thread and Non-concurrent HTTP fuzzer.

  Usage:
    $ ffuc  [OPTIONS]  [ [HTTP OPTION] [-w /path/to/wordlist]... ]...
      Provide HTTP request options and word-list file paths.
      Each file path, maps to a corresponding FUZZ keyword.

  Examples of using FUZZ keywords and word-lists:
    $ ffuc -u https://x.com/FUZZ.FUZZ -w /tmp/wl1 -w /tmp/wl2
    - the first FUZZ uses: '/tmp/wl1' and the second FUZZ uses: '/tmp/wl2')

    $ ffuc -u http://x.com/FUZZ -w /tmp/wl1  -H 'X-test: FUZZ' -w /tmp/wl2
    - using '/tmp/wl1' for URL  and  '/tmp/wl2' for the X-test header)

    $ ffuc -XPOST -u https://x.com  -d 'username=FUZZ' -w /tmp/usernames \
                                    -d 'password=FUZZ' -w /tmp/rockyou.txt

  Advanced FUZZ tags:
    FFuc accepts tagged word-list file paths e.g., '/path/to/file:FUZZ_XXX'.
    only tags with FUZZ_XXX format are acceptable.
    These tags later can be used in HTTP component in plase of FUZZ keyword:

    $ ffuc -u http://x.com/FUZZ_1/FUZZ_1  -w /tmp/wl:FUZZ_1
    - this is equivalent to passing two `-w /tmp/wl` options.

    $ ffuc -u http://x.com/FUZZ_1/FUZZ_2  -w /tmp/wl2:FUZZ_2 \
                                          -w /tmp/wl1:FUZZ_1
    - Although /tmp/wl2 is given first, the first FUZZ, will use
      /tmp/wl1, and the second FUZZ, /tmp/wl2.


  Recommendation:  using the `--auto-filter` option
    The `--auto-filter` option (also known as AI mode!) automatically
    detects a proper response filtering by sending a few discovery requests.

    Some endpoints, respond with unexpected HTTP error code on encountering
    any unexpcted or absent parameter in requests; As a result, the default
    setup of FFuc (HTTP code filtering) might  *miss*  some valid results.

    The auto-filter mode can be reproduced manually as follows:
      1. Use '--all' to ignore all filters, with a small word-list.
      2. Identify a common response hook to filter (or match).
         FFuc starts by, at first, common word count, then line count,
         then response size, then HTTP status code, and otherwise,
         returns Error as it could not find a common pattern to filter.


  Filter & Match (--fX, --mX):
    To filter responses (to excluding if satisfied):
          --fs (filter size),        --fc (filter status code)
          --fw (filter word count),  --fl (filter line count)

        Ex.  '--fs 1024  --fc 400-500':
               filters out responses with a size of 1024,
               AND those with status codes in the range of [400 to 500]

    To match responses (to include only if satisfied):
          --ms (match size),         --mc (match status code)
          --mw (match word count),   --ml (match line count)

        Ex.  The default setup is equivalent to  '--mc 200-399'
        Ex.  '--fs 0  --mc 300':
               first excludes all responses of zero length,
               then only shows those with a status code of 300.

    Filter by content (--filter-regex, --match-regex):
        FFuc provides a minimal regex matching, which currently only
        supports simple expressions.
        Ex.
          --fr "xxx"   Exclude responses that contain "xxx" in their body
          --mr "yyy"   Include only responses that contain "yyy"


  Mode (-m, --mode):
    FFuc implements three different methods to work with word-lists,
    see the usage help for more details.

     - Cluster-bomb (default):  all combinations of all word-lists
           O( len(word-list #1) x ... x len(word-list #N) )

     - Pitchfork:  cycling through all word-lists simultaneously
           O( MAX( len(word-list #1), ..., len(word-list #N) ) )

     - Singular:  one word-list for all FUZZ keywords
           O( len(word-list #1) )


    Compilation:
      cc -O2 -Wall -Wextra -Werror -I ../libs/ \
         ffuc.c -o ffuc -lcurl

    Options:
      -D_DEBUG:  print debug information
      -D SKIP_FREE:  skip freeing heap memory in cleanup function
      -D NO_DEFAULT_COLOR:  disable output colors
      -D MAX_REQ_RATE:  maximum allowed req/sec (default is 1000)
 **/
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <termios.h>

#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <curl/curl.h>

/* Only used in interactive mode */
#include <pthread.h>
#include <signal.h>

/** From local libs **/
#define DYNA_IMPLEMENTATION
#include "dyna.h" /* dynamic array */

#define UNESCAPE_IMPLEMENTATION
#include "unescape.h" /* used for URL decoding */

#define CLI_IMPLEMENTATION
#include "clistd.h"
#include <getopt.h>

#define PROG_NAME "FFuc"
#define PROG_VERSION "3.1"

#ifndef FUZZ_STR
# define FUZZ_STR "FUZZ"
# define FUZZ_STR_LEN 4
#endif

#ifndef SV_MAX_CAP
# define SV_MAX_CAP (4 * 1024)
#endif
#ifndef SV_MIN_CAP
#define SV_MIN_CAP 8
#endif

struct strv /* string view */
{
  char *buff;
  int len, cap;
};
static struct strv tmp = {0};

/* Default concurrent requests count */
#ifndef DEFAULT_REQ_COUNT
# define DEFAULT_REQ_COUNT 40
#endif

/* Maximum request rate (req/sec) */
#ifndef MAX_REQ_RATE
# define MAX_REQ_RATE 1000
#endif

/* Count of samples to calculate request rate */
#ifndef RATE_SAMPLE_COUNT
# define RATE_SAMPLE_COUNT 4
#endif

/* Default connection/read timeuot */
#ifndef CONN_TTL_MS
# define CONN_TTL_MS 10000L
#endif
#ifndef DEFAULT_TTL_MS
# define DEFAULT_TTL_MS 10000
#endif

#ifndef MIN_DT_US
# define MIN_DT_US 100
#endif

/* Poll timeout */
#ifndef POLL_TTL_MS
# define POLL_TTL_MS 1000
#endif

/* Maximim number of errors to print */
#ifndef MAX_ERROR_REPS
#define MAX_ERROR_REPS 16
#endif

#ifndef PRINT_MARGIN
# ifndef __ANDROID__
#   define PRINT_MARGIN 31
# else /* smaller screen width on Android */
#   define PRINT_MARGIN 4
# endif
#endif

#ifndef DISCOVERY_REQ_COUNT
# define DISCOVERY_REQ_COUNT 4
#endif
#ifndef MAX_RETRY_DISCOVERY
# define MAX_RETRY_DISCOVERY 3
#endif

#define NOP() ((void) NULL)
#define UNUSED(x) (void)(x)
#define __TO_STR(x) #x
#define STR(x) __TO_STR(x)
#define MIN(a,b) ((a < b) ? (a) : (b))
#define MAX(a,b) ((a > b) ? (a) : (b))
#define lstrlen(lstr) (sizeof (lstr) - 1) /* only for string literals */
#define lastcharof(cstr) \
  cstr[ (cstr[0]=='\0') ? 0 : strlen(cstr)-1 ] /* last character of string */

#define FLG_SET(dst, flg) (dst |= flg)
#define HAS_FLAG(val, flg) (val & flg)
#define FLG_UNSET(dst, flg) (dst &= ~(flg))

#define Fprintarr(stream, element_format, arr, len) do {    \
    int __idx = 0;                                          \
    for (; __idx < (len)-1; __idx++)                        \
      fprintf (stream, element_format ", ", (arr)[__idx]);  \
    fprintf (stream, element_format, (arr)[__idx]);         \
  } while (0)

const char *lopt_str = "m:w:" "T:R:t:p:A" "u:H:d:X:x:" "ivhc";
const struct option lopts[] =
  {
    /* We call it `thread` (-t) for compatibility with ffuf,
       even though we don't use threads */
    {"thread",              required_argument, NULL, 't'},

    /* HTTP options */
    {"url",                 required_argument, NULL, 'u'},
    {"header",              required_argument, NULL, 'H'},
    {"data",                required_argument, NULL, 'd'},
    {"data-ascii",          required_argument, NULL, 'd'},
    {"data-binary",         required_argument, NULL, 'd'},
    /* HTTP verb, does NOT accepts FUZZ keyword */
    {"request",             required_argument, NULL, 'X'},
    {"method",              required_argument, NULL, 'X'},
    {"request-method",      required_argument, NULL, 'X'},

    /* Filter options */
    {"fc",                  required_argument, NULL, '0'},
    {"fcode",               required_argument, NULL, '0'},
    {"filter-code",         required_argument, NULL, '0'},
    {"fs",                  required_argument, NULL, '1'},
    {"fsize",               required_argument, NULL, '1'},
    {"filter-size",         required_argument, NULL, '1'},
    {"fw",                  required_argument, NULL, '2'},
    {"fword",               required_argument, NULL, '2'},
    {"filter-word",         required_argument, NULL, '2'},
    {"fl",                  required_argument, NULL, '3'},
    {"fline",               required_argument, NULL, '3'},
    {"filter-line",         required_argument, NULL, '3'},
    {"fr",                  required_argument, NULL, '4'},
    {"fregex",              required_argument, NULL, '4'},
    {"filter-regex",        required_argument, NULL, '4'},
    /* Match options */
    {"mc",                  required_argument, NULL, '9'},
    {"mcode",               required_argument, NULL, '9'},
    {"match-code",          required_argument, NULL, '9'},
    {"ms",                  required_argument, NULL, '8'},
    {"msize",               required_argument, NULL, '8'},
    {"match-size",          required_argument, NULL, '8'},
    {"mw",                  required_argument, NULL, '7'},
    {"mword",               required_argument, NULL, '7'},
    {"match-word",          required_argument, NULL, '7'},
    {"ml",                  required_argument, NULL, '6'},
    {"mline",               required_argument, NULL, '6'},
    {"match-line",          required_argument, NULL, '6'},
    {"mr",                  required_argument, NULL, '5'},
    {"mregex",              required_argument, NULL, '5'},
    {"match-regex",         required_argument, NULL, '5'},
    /* Filter and Match disabled */
    {"all",                 no_argument,       NULL, 'A'},
    {"no-filter",           no_argument,       NULL, 'A'},
    {"filter-no",           no_argument,       NULL, 'A'},
    /* Auto filter discovery mode */
    {"auto-filter",         no_argument,       NULL, '*'},
    {"af",                  no_argument,       NULL, '*'},
    {"AI",                  no_argument,       NULL, '*'},

    /* Common options */
    {"mode",                required_argument, NULL, 'm'},
    {"word",                required_argument, NULL, 'w'},
    {"wordlist",            required_argument, NULL, 'w'},
    {"word-list",           required_argument, NULL, 'w'},
    {"rate",                required_argument, NULL, 'R'},
    {"max_rate",            required_argument, NULL, 'R'},
    {"timeout",             required_argument, NULL, 'T'},
    {"ttl",                 required_argument, NULL, 'T'},
    {"delay",               required_argument, NULL, 'p'},
    {"verbose",             no_argument,       NULL, 'v'},
    {"color",               no_argument,       NULL, 'c'},
    {"it",                  no_argument,       NULL, 'i'},
    {"interactive",         no_argument,       NULL, 'i'},
    {"proxy",               required_argument, NULL, 'x'},
    {"http-proxy",          required_argument, NULL, 'x'},
    {"help",                no_argument,       NULL, 'h'},
    /* End of options */
    {NULL,                  0,                 NULL,  0 }
  };

enum ffuc_flag_t
  {
    /**
     *  Fuzz Modes (default is CLUSTERBOMB)
     *
     * Singular:
     *   Using only one wordlist for all FUZZ keywords
     * Pitchfork:
     *   Each FUZZ, uses it's own word-list
     *   word-lists are not necessarily the same size
     * Clusterbomb:
     *   All combinations of word-list(s)
     */
    MODE_CLUSTERBOMB  = 0,
    MODE_PITCHFORK    = 1,
    MODE_SINGULAR     = 2,
    MODE_DEFAULT = MODE_CLUSTERBOMB,

    /**
     *  FUZZ keyword substitution enabled flags
     *  Used in opt.fuzz_flag
     */
    URL_HASFUZZ       = (1 << 1),
    BODY_HASFUZZ      = (1 << 2),
    HEADER_HASFUZZ    = (1 << 3),

    /**
     *  Request context flags
     *  Used in RequestContext.flag
     */
    CTX_FREE          = 0,
    CTX_INUSE         = 1,

    /* Exit codes */
    EOFUZZ            = 1,
    SHOULD_EXIT       = -1,
  };

/* Output filter & match, Fits in unsigned char,
   Positive values are filter, negatives are match */
enum filter_flag_t
  {
    FILTER_CODE       = 1,
    FILTER_WCOUNT     = 2,
    FILTER_LCOUNT     = 3,
    FILTER_SIZE       = 4,
    FILTER_TIME       = 5,
    FILTER_REGEX      = 6,

    MATCH_CODE        = 255,
    MATCH_WCOUNT      = 254,
    MATCH_LCOUNT      = 253,
    MATCH_SIZE        = 252,
    MATCH_TIME        = 251,
    MATCH_REGEX       = 250,
  };
#define IS_FILTER(fft) ((char)(fft) > 0)
#define IS_MATCH(fft) (! IS_FILTER (fft))
#define __ABS__(x) ((x > 0) ? (x) : -1 * (x))

#define FILTER_T_CSTR(fft) (IS_FILTER (fft) ? "Filter" : "Match")
#define FILTER_CSTR(fft) __filter_cstr[ __ABS__((char) fft) ]
const char *__filter_cstr[] =
  {
    [FILTER_CODE]     = "status code",
    [FILTER_WCOUNT]   = "word count",
    [FILTER_LCOUNT]   = "line count",
    [FILTER_SIZE]     = "size",
    [FILTER_TIME]     = "time",
    [FILTER_REGEX]    = "regex",
  };
const char *mode_name_cstr[] =
  {
    [MODE_CLUSTERBOMB]  = "Clusterbomb",
    [MODE_PITCHFORK]    = "Pitchfork",
    [MODE_SINGULAR]     = "Singular",
  };

enum template_op
  {
    /**
     * URL_TEMPLATE:
     *   Set the target URL (e.g. "http(s)://host:port")
     *
     * BODY_TEMPLATE:
     *   Aappend parameter to the body of requests
     *   It handles `&` automatically:
     *     (e.g. "key=val&..." or "&key=val")
     *
     * HEADER_TEMPLATE:
     *   Append Http header (e.g. "Header: value")
     *
     * WLIST_TEMPLATE:
     *   To append a wordlist file path; It tries to
     *   match it with the latest 'FUZZ' keyword
     *   appended using the set_template function
     *
     * FINISH_TEMPLATE:
     *   set_template MUST be called with it, at the end
     */
    URL_TEMPLATE      = 0,
    BODY_TEMPLATE     = 1,
    HEADER_TEMPLATE   = 2,
    WLIST_TEMPLATE    = 3,
    FINISH_TEMPLATE   = 126,
  };

const char *template_name[] =
  {
    [URL_TEMPLATE]    = "URL",
    [BODY_TEMPLATE]   = "Body",
    [HEADER_TEMPLATE] = "Header",
  };

struct req_stat_t
{
  uint wcount; /* count of words */
  uint lcount; /* count of lines */
  uint size_bytes;

  int64_t code; /* HTTP response code */
  uint duration; /* total time to response */
  CURLcode ccode;
};

/* Always reset the zero initialized request stat type,
   we assume word count and line count start from 1 */
#define STAT_RESET(r_stat) \
  (*(r_stat) = (struct req_stat_t){.wcount=1, .lcount=1})

typedef struct progress_t
{
  uint req_total;
  uint req_sent; /* count of sent requests */
  uint err_count; /* count of errors */
  uint req_dt; /* requests in delta time (real time) */

  bool progbar_enabled;
  uint progbar_refrate; /* progress bar refresh rate */
  uint rate;

  /* Internals (to calculate request rate) */
  size_t dt_us; /* delta time */
  uint __req_dt; /* stabilized req_dt */
} Progress;

/* Percentage of progress */
#define REQ_PERC(prog) ((prog)->req_sent * 100 / (prog)->req_total)
/* Convert timespec to microseconds */
#define TS2US(tv) ((tv).tv_sec * 1000000LL + (tv).tv_sec)
/* Request rate and real time rate */
#define REQ_RATE(prog) (((prog)->__req_dt * 1000000) / (prog)->dt_us)
#define RT_REQ_RATE(prog) (((prog)->req_dt * 1000000) / (prog)->dt_us)

struct res_filter_t
{
  enum filter_flag_t type; /* FILTER_XXX  or  MATCH_XXX */
  union {
    struct /* for numeric filters */
    {
      int start, end;
    } range;
    struct /* for regex filters */
    {
      char *pattern;
    } regex;
  } filter;
};

const struct res_filter_t default_filter =
  { /* The default filter to match only success HTTP codes */
    .type = MATCH_CODE,
    .filter.range = {200, 399}
  };
/* To disable the default filter */
#define NO_FILTER ((void *) -1)

struct request_t
{
  char *URL;
  char *body;
  struct curl_slist *headers;
};

struct ffuc_regex /* only supports string matching! */
{
  int match;
  int __idx; // internal
  const struct res_filter_t *fl; // length = opt.regex_count
};
/* match a regex, character by character */
static inline int
regex_match_c (struct ffuc_regex *rx, u_char c);
#define REGEX_RESET(freg) ((freg)->match = false, (freg)->__idx = 0)

typedef struct
{
  int flag;
  CURL *easy_handle;

  /* Statistics of the request */
  struct req_stat_t stat;
  struct request_t request;
  struct ffuc_regex *matches;
  
  /**
   *  All 'FUZZ' keywords within @opt.fuzz_template
   *  (URL, POST body, HTTP headers respectively)
   *  will be substituted with elements of this array.
   *  It must contain opt.words_len elements.
   */
  char **FUZZ;
} RequestContext;

/* used by fuzz_snprintf(), generated by gen_fuzz_cache(). */
struct template_cache
{
  /** flag:
   *  To determine the type of cache
   *  'u' for URL, 'b' for body, 'h','H' for headers periodically
   */
  int flag;
  int stridx; /* index of FUZZ keywords */
  int widx; /* corresponding opt.words index */
  int taglen; /* strlen of tag */
};

typedef struct
{
  char *URL;
  char *body;
  struct curl_slist *headers;

  struct template_cache *cache;

  /* internal. */
  int local_fuzz_count;
  int local_off, local_cap; // offset of opt.words
} FuzzTemplate;

/**
 *  FFuc word (fw)
 *
 *  Instead of loading the entire word-list files
 *  into memory, we mmap them; To iterate over them,
 *  we use functions fw_next and fw_get.
 */
typedef struct
{
  /**
   *  `fw_get` returns the current word; The result is *NOT*
   *  null-terminated, and it's length is @len
   *
   *  FW_FORMAT and FW_ARG macros can be used with printf
   */
  char *str;
  uint len; /* Length of the current word */

  /**
   *  Index of the current word and total words
   *
   *  Each fw_next call, increments @index by 1,
   *  after @total_count resets to 0
   */
  uint index, total_count;

  /* Internal */
  uint __offset; /* Offset of the current word in @str */
  size_t __str_bytes; /* length of @str in bytes */

  const char *path; /* path to filename, or NULL if not using file */
  const char *tag; /* NULL means the default FUZZ_STR */
  int type; /* which HTTP component in which is bind to */
} Fword;

const Fword dummy_fword = {
  .str=(FUZZ_STR "\n"),  .len = lstrlen (FUZZ_STR),
  .total_count=1,  .__str_bytes = sizeof(FUZZ_STR)
};

/* Fword printf format and arguments */
#define FW_FORMAT "%.*s"
#define FW_ARG(fw) (int)((fw)->len), fw_get (fw)

/* To initialize Fword from opened file @fd */
int fw_map (Fword *dst, int fd);
/* To initialize Fword manually */
void fw_init (Fword *fw, char *cstr, size_t cstr_len);

/**
 *  Open file @path and create Fword of it,
 *  returns dummy_fword on failure.
 *  The result always must be freed via fw_free
 */
static inline Fword *make_fw_from_path (const char *path);

/* Fword copy, duplicate and unmap */
#define fw_cpy(dst, src) Memcpy (dst, src, sizeof (Fword))
Fword *fw_dup (const Fword *src);
#define fw_unmap(fw) \
  if ((fw) && (fw)->str) { ffuc_munmap ((fw)->str, (fw)->__str_bytes); }

/* Fword free, Only needed if @fw is created via fw_dup */
#define fw_free(fw) do {                        \
    fw_unmap (fw);                              \
    safe_free (fw);                             \
  } while (0)

/* Moves to the next word in the word-list, and returns it */
char *fw_next (Fword *fw);
/* Returns the current word of the word-list */
#define fw_get(fw) ((fw)->str + (fw)->__offset)
#define fw_len(fw) ((fw)->len)

/* Fword end of file & beginning of file */
#define fw_eof(fw) ((fw)->index + 1 == (fw)->total_count)
#define fw_bof(fw) ((fw)->index == 0)

/**
 *  Fword functions
 *  fw_next should be called after fw_init
 */
void fw_init (Fword *fw, char *cstr, size_t cstr_len);
Fword *fw_dup (const Fword *src);
char *fw_next (Fword *fw);

/**
 *  String View functions.
 *  sv_appd_str:  append a null-terminated string.
 *    after this call, string view will contain @str and a tailing null-byte.
 *
 *  sv_appd_buf:  append character buffer
 *    is @nullterm is true, it will append a tailing null-byte, if the @buf
 *    is already null-terminated, do *not* set this parameter true.
 *    sv_appd_str(v, buf)  :equivalent:  sv_appd_buf(v, buf, sizeof(buf), true)
 *
 *  sv_drop:  makes the string view fresh. just sets the length to 0, no free.
 */
#ifndef SV_DEF
#define SV_DEF static inline
#endif
SV_DEF int sv_appd_str (struct strv *, const char *str);
SV_DEF int sv_appd_buf (struct strv *, const void *buf, int size_bytes, bool nullterm);
SV_DEF void sv_free (struct strv *);
#define sv_drop(v) do {                         \
    (v)->len = 0;                               \
    if ((v)->buff) *(v)->buff = '\0';           \
  } while (0)
#define sv_get(v) ((v)->buff)

/**
 *  FUZZ sprintf function
 *  substitutes the FUZZ keyword of @format
 *  with the appropriate value from the @FUZZ array.
 * Return:
 *  The number of elements consumed from @FUZZ.
 *  @FUZZ must have enough element(s).
 */
void fuzz_snprintf (struct strv *dst, const char *format, char **src,
                    const struct template_cache *cache, int *cache_ss);

/**
 *  Strline and StrlineNull Functions
 *
 *  These functions return a pointer to the first character
 *  in the string @cstr that falls within the range (0x00, 0x1F].
 *
 *  Strline returns NULL, if a null-byte is encountered, but
 *  StrlineNull, returns a pointer to null-byte.
 */
static inline char *Strline (const char *cstr);
static inline char *StrlineNull (const char *cstr);

/**
 *  Libcurl handle lookup functions
 *
 * lookup_handle:
 *   Finds @handle in @ctxs array, returns NULL is not found
 * lookup_free_handle:
 *   Finds the first free request context in @ctxs,
 *   returns null if couldn't find any
 */
static inline RequestContext *
lookup_handle (CURL *handle, RequestContext *ctxs, size_t len);
static inline RequestContext *
lookup_free_handle (RequestContext *ctxs, size_t len);

/**
 *  Progress functions
 *
 * init_progress:
 *   Initializes the progress struct, sets the current time t0
 *
 * tick_progress:
 *   It should be called in the main loop to update @prog fields:
 *   delta time (dt_us) and stable request in delta time (req_dt)
 *
 * update_progress_bar:
 *   Prints a simple progress-bar if is not disabled,
 *   a newline is needed at the end by end_progress_bar()
 *
 * update_req_rate:
 *   Returns the current request rate and updates @prog->rate
 * rt_req_rate:
 *   Real time request rate
 */
static void init_progress (Progress *prog);
static void tick_progress (Progress *prog);
static inline size_t update_req_rate (Progress *prog);
static inline size_t rt_req_rate (Progress *prog);

static void __update_progress_bar (const Progress *prog);
#define update_progress_bar(prog) \
  if ((prog)->progbar_enabled) __update_progress_bar (prog)
#define end_progress_bar(prog) \
  if ((prog)->progbar_enabled) fprintf (stderr, CLEAN_LINE("\n"));

/**
 *  Interactive mode functions
 * interact:
 *   This function reads user input, from another thread,
 *   and prints appropriate response.
 */
void goto_raw_mode (struct termios *original);
#define disable_raw_mode(original) \
  tcsetattr (STDIN_FILENO, TCSANOW, original)
void * interact (void *);

/**
 *  Sleeps for a random duration in microseconds
 *  within the specified range @range
 *
 *  If range[1] is less than or equal to range[0],
 *  then it will sleep for range[0] microseconds
 */
static inline void range_usleep (useconds_t range[2]);

/**
 *  Template functions
 *
 * set_template:
 *   To set HTTP options and wordlist templates
 *   It keeps track of FUZZ keywords, and appends
 *   wordlist file path to the most recently modified HTTP option
 * set_template_wlist:
 *   To manually append wordlist file path
 */
int set_template (FuzzTemplate *t, enum template_op op, void *param);
int set_template_wlist (FuzzTemplate *t, enum template_op op,
                        Fword **dst, void *param);

/* curl helper macros */
#define curl_setopt(...) curl_easy_setopt (__VA_ARGS__)
#define curl_slist_appd(list, element) \
  (list = curl_slist_append (list, element))
#define curl_slist_foreach(list, element)                       \
  for (struct curl_slist *element = list;                       \
       NULL != element; element = element->next)
#define curl_slist_safe_free(ptr)                               \
  if (NULL != ptr) { curl_slist_free_all (ptr);  ptr = NULL; }

#ifndef ffuc_malloc
#define ffuc_malloc(len) malloc (len)
#endif
#ifndef ffuc_calloc
#define ffuc_calloc(count, len) calloc (count, len)
#endif
#ifndef ffuc_mmap
#define ffuc_mmap(addr, len, prot, flg, fd, off) \
  mmap (addr, len, prot, flg, fd, off)
#endif

#undef ffuc_free
#ifndef SKIP_FREE
# define ffuc_free(x) free (x)
# define ffuc_munmap(ptr, len) munmap (ptr, len)
#else
# define ffuc_free(...) NOP()
# define ffuc_munmap(...) NOP()
#endif /* SKIP_FREE */

#define safe_free(ptr) \
  if (NULL != ptr) { ffuc_free (ptr); ptr = NULL; }

/**
 **  The primary bottleneck is the network, so improving
 **  memory allocation is unlikely to enhance performance.
 **/

/**
 *  Returns a malloc pointer which contains @src
 *  If @malloced is not null, uses realloc instead
 */
static inline void * strrealloc (void *malloced, const char *src);

/**
 *  After running these macros, @dst will contain a copy
 *  of @src, Estrrealloc also URL encodes the result.
 *  If @dst is not NULL, it will get reallocated.
 *
 *  To prevent memory leaks, DO *NOT* use
 *  ffuc_free or safe_free here, as Strrealloc is used for
 *  reading from word-lists,   and Estrrealloc is used for
 *  generating requests, so they may be called thousands of times!
 */
#define Strrealloc(dst, src) (dst = strrealloc (dst, src))
#define Estrrealloc(dst, src) do {              \
    if (dst) free (dst);                        \
    dst = curl_escape (src, 0);                 \
  } while (0)

#define Realloc(ptr, len) \
  if (ptr) { ptr = realloc (ptr, len); }
#define Memzero(ptr, len) \
  if (ptr) { memset (ptr, 0, len); }
#define Strlen(s) \
  ((NULL != s) ? strlen (s) : 0)
#define Memcpy(dst, src, len) \
  if (NULL != dst) { memcpy (dst, src, len); }
#define Strcmp(s1, s2) \
  ((s1 == NULL || s2 == NULL) ? 1 : (0 == strcmp (s1, s2)))
#define url_unescape_safe(cstr) \
  if (NULL != cstr) { url_unescape (cstr); }

#ifdef _DEBUG
# define fprintd(format, ...) \
  fprintf (stderr, CLEAN_LINE(format), ##__VA_ARGS__)
# define printd(format, ...) \
  fprintd ("%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__);
#else
# define fprintd(...) NOP()
# define printd(...) NOP()
#endif /* _DEBUG */


/**
 *  FFuc main configuration
 */
struct Opt
{
  /* User options */
  int mode;
  int ttl; /* Timeout in milliseconds */
  bool verbose;
  bool color_enabled;
  bool AI; /* AI mode! (auto filter mode) */
  bool interactive;
  uint max_rate; /* Max request rate (req/sec) */
  char *verb; /* HTTP verb */
  char *proxy;
  FuzzTemplate fuzz_template;

  int regex_count; /* count of regex filters */
  struct res_filter_t *filters; /* Dynamic array */

  /* Internals */
  int fuzz_flag;
  CURLM *multi_handle;
  struct progress_t progress;
  pthread_t interact_th; /* interactive mode thread */

  Fword **words; /* Dynamic array */
  int words_len; /* Length of @words */

  struct request_queue_t
  {
    RequestContext *ctxs; /* Static array */
    size_t len; /* Length of @ctxs */
    size_t waiting; /* number of in-use contexts */
    useconds_t delay_us[2]; /* delay range, microseconds */
  } Rqueue;

  struct printf_t
  {
    FILE *streamout;
    bool isatty;
    bool lineclear; /* Should clear terminal */
    bool color; /* Color enabled */
  } Printf;
  struct termios orig_termios; /* original setup of terminal */

  /* Next word loader */
  void (*load_next_fuzz) (RequestContext *ctx);
  bool eofuzz; /* End of load_next_fuzz */
};
struct Opt opt;

enum http_color_code
  {
    HTTP_NOCOLOR = 0,
    HTTP_1xx = 1,
    HTTP_2xx = 2,
    HTTP_3xx = 3,
    HTTP_4xx = 4,
    HTTP_5xx = 5,
    HTTP_ERR = 5
  };

const char *HttpPallet[] =
  {
    [HTTP_NOCOLOR] = "", // no color
    [HTTP_1xx]     = F_BLUE(),
    [HTTP_2xx]     = F_GREEN(),
    [HTTP_3xx]     = F_YELLOW(),
    [HTTP_4xx]     = F_PURPLE(),
    [HTTP_5xx]     = F_RED(),
  };

/* COLOR_ARG color value based on HTTP code */
static inline const char *http_pallet_of (int resp_code);
#define colorof_ctx(ctx) http_pallet_of ((ctx)->stat.code)

/** @ptr: The corresponding request context, passed by libcurl **/
size_t
curl_fwrite (void *ptr, size_t size, size_t nmemb, void *__req_ctx)
{
  size_t len = (size_t)(size * nmemb);
  unsigned char *data = (unsigned char *) ptr;
  RequestContext *ctx = (RequestContext *) __req_ctx;

  ctx->stat.size_bytes += len; /* Update size */
  for (size_t i=0; i<len; ++i)
    {
      /* Update word count and line count */
      unsigned char c = data[i];
      if (c == ' ')
        ctx->stat.wcount++;
      else if (c < ' ')
        ctx->stat.lcount++;

      for (int i=0; i < opt.regex_count; ++i)
        {
          if (! ctx->matches[i].match)
            regex_match_c (&ctx->matches[i], c);
        }
    }
  return len; 
}

//-- Strline and StrlineNull functions --//
static inline char *
Strline (const char *cstr)
{
  for (; '\0' != *cstr; ++cstr)
    {
      if (*cstr > 0 && *cstr < 0x20)
        return (char *) cstr;
    }
  return NULL;
}

static inline char *
StrlineNull (const char *cstr)
{
  for (;; ++cstr)
    {
      if (*cstr >= 0 && *cstr < 0x20)
        return (char *) cstr;
    }
}

//-- string view functions --//
static inline void
__extend_cap (struct strv *view)
{
  int newcap = view->len + SV_MIN_CAP;
  assert (newcap < SV_MAX_CAP && "maximum string view capacity");
  if (newcap <= view->cap)
    return;
  view->cap = newcap;
  view->buff = realloc (view->buff, newcap);
}

SV_DEF int
sv_appd_str (struct strv *view, const char *str)
{
  int strl = Strlen (str);

  int strbl = strl + 1; /* null-termination */
  if (view->len + strbl >= view->cap)
    __extend_cap (view);

  memcpy (view->buff + view->len, (str) ? str : "", strbl);
  view->len += strl;
  return view->len;
}

SV_DEF int
sv_appd_buf (struct strv *view, const void *_src, int _len, bool nullterm)
{
  const char *buf = (const char *) _src;
  int len = (nullterm) ? _len+1 : _len;
  if (view->len + len >= view->cap)
    __extend_cap (view);
  memcpy (view->buff + view->len, buf, _len);
  view->len += _len;
  if (nullterm)
    view->buff[view->len] = '\0';
  return view->len;
}

SV_DEF void
sv_free (struct strv *view)
{
  view->cap = view->len = 0;
  free (view->buff);
  view->buff = NULL;
}

//-- Fword functions --//
void
fw_init (Fword *fw, char *cstr, size_t cstr_len)
{
  fw->index = 0;
  fw->__offset = 0;
  fw->total_count = 0;

  fw->str = cstr;
  fw->len = (uint) (StrlineNull (cstr) - cstr);
  fw->__str_bytes = cstr_len;

  /* Calculating count of words within the wordlist */
  for (char *p = cstr;; fw->total_count++, p++)
    {
      p = Strline (p);
      if (!p)
        break;
    }
}

int
fw_map (Fword *dst, int fd)
{
  struct stat sb;
  if (-1 == fstat (fd, &sb))
    return 1;

  char *mapped = ffuc_mmap (NULL,
                       sb.st_size,
                       PROT_READ, MAP_PRIVATE,
                       fd, 0);
  if (mapped == MAP_FAILED)
    {
      close (fd);
      return fd;
    }

  /* Success */
  fw_init (dst, mapped, sb.st_size);
  return 0;
}

Fword *
fw_dup (const Fword *src)
{
  Fword *tmp = ffuc_malloc (sizeof (Fword));
  fw_cpy (tmp, src);
  return tmp;
}

char *
fw_next (Fword *fw)
{
  char *p = fw_get (fw) + fw->len + 1;
  char *next = Strline (p);
  if (NULL == next || '\0' == *next)
    {
      p = fw->str;
      next = Strline (p);
      fw->len = next - p;
      fw->__offset = 0;
      fw->index = 0;
      return fw->str;
    }
  fw->__offset = p - fw->str;
  fw->len = next - p;
  fw->index++;
  if (fw->index == fw->total_count)
    fw->index = 0;
  return p;
}

void
fw_export (char **dst, const Fword *src)
{
  sv_drop (&tmp);
  sv_appd_buf (&tmp, fw_get(src), fw_len(src), true);
  Estrrealloc (*dst, sv_get(&tmp));
}

//-- Logger functions --//
#define KEY_MARGIN 26 // for alignment, set it: (PRINT_MARGIN - 5)
#define KEY_FMT "%-" STR(KEY_MARGIN) "s"

static inline void
log_filter (FILE *stream, const struct res_filter_t *fl)
{
  fprintf (stream, ": "KEY_FMT" : %s",
           FILTER_T_CSTR (fl->type), FILTER_CSTR (fl->type));
  switch (fl->type)
    {
    case FILTER_REGEX:
    case MATCH_REGEX:
      fprintf (stream, "('%s')\n", fl->filter.regex.pattern);
      break;

    default:
      if (fl->filter.range.start != fl->filter.range.end)
        fprintf (stream, " range [%d-%d]\n",
                 fl->filter.range.start, fl->filter.range.end);
      else
        fprintf (stream, " == %d\n", fl->filter.range.start);
      break;
    }
}

void
log_current_config ()
{
  FILE *stream = opt.Printf.streamout;
  fprintf (stream, "\
------------------------------\n\
-  FFUC v%-4s Configuration  -\n\
------------------------------\n", PROG_VERSION);
  fprintf (stream, ": "KEY_FMT" : %s\n", "URL", opt.fuzz_template.URL);
  if (opt.fuzz_template.body)
    fprintf (stream, ": "KEY_FMT" : %s\n", "Body", opt.fuzz_template.body);
  curl_slist_foreach (opt.fuzz_template.headers, header) {
    fprintf (stream, ": "KEY_FMT" : [%s]\n", "Header", header->data);
  }
  fprintf (stream, ": "KEY_FMT" : %s\n", "Fuzz mode", mode_name_cstr[opt.mode]);
  if (opt.max_rate != MAX_REQ_RATE)
    fprintf (stream, ": "KEY_FMT" : %d req/sec\n", "Maximum request rate", opt.max_rate);
  fprintf (stream, ": "KEY_FMT" : %ld req\n", "Concurrency", opt.Rqueue.len);
  if (opt.Rqueue.delay_us[0])
    {
      fprintf (stream, ": "KEY_FMT" : ", "Delay");
      if (opt.Rqueue.delay_us[0] != opt.Rqueue.delay_us[1])
        fprintf (stream, "%d-%d (ms)\n",
                 opt.Rqueue.delay_us[0] / 1000,
                 opt.Rqueue.delay_us[1] / 1000);
      else
        fprintf (stream, "%d (ms)\n", opt.Rqueue.delay_us[0]/1000);
    }
  da_foreach (opt.filters, i) {
    log_filter (stream, &opt.filters[i]); /* log: Filter / Match */
  }
  fprintf (stream, "------------------------------\n\n");
  fflush (stream);
}
#undef KEY_FMT
#undef KEY_MARGIN

void
print_stats_fuzz (RequestContext *ctx)
{
  /* Wiping the line out of the progress-bar stuff */
  if (opt.Printf.lineclear)
    fprintf (opt.Printf.streamout, CLEAN_LINE ());
  /* Undo URL encoding */
  for (int i=0; i < opt.words_len; ++i)
    url_unescape_safe (ctx->FUZZ[i]);

  FILE *stream = opt.Printf.streamout;
  if (1 >= opt.words_len)
    {
      int ss, to, margin;
#ifndef __ANDROID__
      #define __FMT__ "%n" "%s" "%n"
      #define __ARG__ &ss, ctx->FUZZ[0], &to
#else /* on Android, printf does not support %n */
      #define __FMT__ "%s"
      #define __ARG__ ctx->FUZZ[0]
      ss = 0, to = PRINT_MARGIN; /* forces newline */
#endif /* __ANDROID__ */

      if (opt.Printf.color)
        fprintf (stream,
                 COLOR_FMT( __FMT__ ),
                 COLOR_ARG( colorof_ctx(ctx), __ARG__ ));
      else
        fprintf ( stream, __FMT__, __ARG__ );

#undef __FMT__
#undef __ARG__
      if ((margin = PRINT_MARGIN + ss - to) > 0)
        fprintf (stream, "%*s", margin, "");
      else
        fprintf (stream, "\n%*s", PRINT_MARGIN, "");
    }
  else /* Multiple FUZZ keywords provided */
    {
      if (opt.Printf.color)
        fprintf (stream, "\n " COLOR_FMT( "* FUZZ" ) " = [",
                 COLOR_ARG( colorof_ctx (ctx) ));
      else
        fprintf (stream, "\n * FUZZ = [");
      Fprintarr (stream, "'%s'", ctx->FUZZ, opt.words_len);
      fprintf (stream, "]:\n");
    }
}

static inline void
print_stats_context (RequestContext *ctx)
{
  static int prev_error_count = 0;
  static CURLcode prev_error_code = 0;
  if (CURLE_OK != ctx->stat.ccode)
    {
      if (! opt.verbose)
        { /* prevent printing of similar error */
          if (prev_error_code != ctx->stat.ccode)
            { /* got a new error, resetting the counter */
              prev_error_code = ctx->stat.ccode;
              prev_error_count = 0;
            }
          else if (++prev_error_count > MAX_ERROR_REPS)
            return;
          else if (prev_error_count  == MAX_ERROR_REPS)
            Strrealloc (ctx->FUZZ[0], "too many errors...");
        }
      print_stats_fuzz (ctx);
      fprintf (opt.Printf.streamout, "[Error: %s, Duration: %dms]\n",
               curl_easy_strerror (ctx->stat.ccode),
               ctx->stat.duration);
      return;
    }
  else /* reset the error counting */
    prev_error_code = prev_error_count = 0;

  print_stats_fuzz (ctx);
  fprintf (opt.Printf.streamout, "\
[Status: %-3d,  Size: %d,  Words: %d,  Lines: %d,  Duration: %dms]\n",
           (int) ctx->stat.code,
           ctx->stat.size_bytes,
           ctx->stat.wcount,
           ctx->stat.lcount,
           ctx->stat.duration
           );
}

//-- Load next FUZZ functions --//
static void
__next_fuzz_rand (RequestContext *ctx)
{
#define N 13
  char randstr[N];
  randstr[N-1] = '\0';
  for (ssize_t i = 0; i < opt.words_len; ++i)
    {
      for (size_t j=0, rnd; j < N-1; ++j)
        {
          switch ((rnd = rand ()) % 3)
            {
            case 0:
              randstr[j] = (rnd % 26) + 'a';  break;
            case 1:
              randstr[j] = (rnd % 26) + 'A';  break;
            case 2:
              randstr[j] = (rnd % 10) + '0';  break;
            }
        }
      Strrealloc (ctx->FUZZ[i], randstr);
    }
#undef N
}

static void
__next_fuzz_singular (RequestContext *ctx)
{
  Fword *fw = opt.words[0];
  fw_export (ctx->FUZZ, fw);

  da_idx i=1, N = opt.words_len;
  for (; i < N; ++i)
    ctx->FUZZ[i] = ctx->FUZZ[0];
  ctx->FUZZ[i] = NULL;

  if (fw_eof (fw))
    opt.eofuzz = true;

  fprintd ("singular:  [%ld-%ld]->`%s`\n", i, N, tmp);
  fw_next (fw);
}

static void
__next_fuzz_pitchfork (RequestContext *ctx)
{
  Fword *fw;
  size_t N = opt.words_len;

  static Fword *longest = NULL;
  /* Find the longest wordlist */
  if (! longest)
    {
      longest = opt.words[0];
      for (size_t i=0; i < N; ++i)
        {
          if (opt.words[i]->total_count > longest->total_count)
            longest = opt.words[i];
        }
    }

  fprintd ("pitchfork:  ");
  for (size_t i = 0; i < N; ++i)
    {
      fw = opt.words[i];
      fw_export (ctx->FUZZ+i, fw);
      fprintd ("[%d]->`%s`\t", fw->index, tmp);
      fw_next (fw);
    }
  fprintd ("\n");

  if (fw_bof (longest))
    opt.eofuzz = true;
}

static void
__next_fuzz_clusterbomb (RequestContext *ctx)
{
  Fword *fw = NULL;
  size_t N = opt.words_len;

  size_t next = 0;
  bool go_next = true;
  fprintd ("clusterbomb:  ");
  for (size_t i=0; i < N; ++i)
    {
      fw = opt.words[i];
      fw_export (ctx->FUZZ+i, fw);
      fprintd ("[%d]->`%s`\t", fw->index, tmp);

      if (go_next)
        {
          fw_next (fw);
          next++;
        }
      if (! fw_bof (fw))
        go_next = false;
    }
  fprintd ("\n");

  if (next == N && fw_bof (fw))
    opt.eofuzz = true;
}

//-- RequestContext functions --//
RequestContext *
lookup_handle (CURL *needle,
               RequestContext *haystack, size_t len)
{
  for (size_t i=0; i < len; ++i)
    {
      if (haystack[i].easy_handle == needle)
        return &haystack[i];
    }
  return NULL;
}

RequestContext *
lookup_free_handle (RequestContext *ctxs, size_t len)
{
  for (size_t i=0; i < len; ++i)
    {
      if (ctxs[i].flag == CTX_FREE)
        return &ctxs[i];
    }
  return NULL;
}

void
context_reset (RequestContext *ctx)
{
  ctx->flag = CTX_FREE;
  curl_multi_remove_handle (opt.multi_handle, ctx->easy_handle);
  STAT_RESET (&ctx->stat);
  for (int i=0; i < opt.regex_count; ++i)
    REGEX_RESET (&ctx->matches[i]);
}

static inline const char *
http_pallet_of (int resp_code)
{
  resp_code /= 100;
  if (resp_code <= 0 || resp_code >= 6)
      return HttpPallet[ HTTP_ERR ];
  else
    return HttpPallet[ resp_code ];
}

static inline bool
filter_pass (RequestContext *ctx, struct res_filter_t *filters)
{
  struct req_stat_t *stat = &ctx->stat;
#define RANGE(x, rng) ((rng).start <= (int)(x) && (int)(x) <= (rng).end)
#define EXCLUDE(cond) if (cond) return false; break;

  da_foreach (filters, i)
    {
      struct res_filter_t *fl = &filters[i];
      switch (fl->type)
        {
        case FILTER_CODE:
          EXCLUDE (RANGE (stat->code, fl->filter.range));
        case FILTER_WCOUNT:
          EXCLUDE (RANGE (stat->wcount, fl->filter.range));
        case FILTER_LCOUNT:
          EXCLUDE (RANGE (stat->lcount, fl->filter.range));
        case FILTER_SIZE:
          EXCLUDE (RANGE (stat->size_bytes, fl->filter.range));
        case FILTER_TIME:
          EXCLUDE (RANGE (stat->duration, fl->filter.range));

        case MATCH_CODE:
          EXCLUDE (! RANGE (stat->code, fl->filter.range));
        case MATCH_WCOUNT:
          EXCLUDE (! RANGE (stat->wcount, fl->filter.range));
        case MATCH_LCOUNT:
          EXCLUDE (! RANGE (stat->lcount, fl->filter.range));
        case MATCH_SIZE:
          EXCLUDE (! RANGE (stat->size_bytes, fl->filter.range));
        case MATCH_TIME:
          EXCLUDE (! RANGE (stat->duration, fl->filter.range));

        default:
          break;
        }
    }

  for (int i=0; i < opt.regex_count; ++i)
    {
      switch (ctx->matches[i].fl->type)
        {
        case MATCH_REGEX:
          EXCLUDE (ctx->matches[i].match == 0);
        case FILTER_REGEX:
          EXCLUDE (ctx->matches[i].match != 0);
        default:
          assert (0 && "Expecting only regex filter");
        }
    }

  return true;
#undef EXCLUDE
#undef RANGE
}

static void
handle_response_context (RequestContext *ctx)
{
  double duration;
  struct req_stat_t *stat = &ctx->stat;
  struct progress_t *prog = &opt.progress;

  if (CURLE_OK == ctx->stat.ccode)
    curl_easy_getinfo (ctx->easy_handle, CURLINFO_HTTP_CODE, &ctx->stat.code);
  else
    prog->err_count++;
  curl_easy_getinfo (ctx->easy_handle, CURLINFO_TOTAL_TIME, &duration);
  stat->duration = (uint) (duration * 1000.f);
  /* Print stats and progress-bar if necessary */
  if (CURLE_OK != ctx->stat.ccode || filter_pass (ctx, opt.filters))
    {
      print_stats_context (ctx);
      update_progress_bar (prog);
    }
  else if (prog->req_sent % prog->progbar_refrate == 0)
    update_progress_bar (prog);
}

static inline int
__do_fuzz_cache (Fword **fw, int ss, int fw_len,
                 struct template_cache *cache,
                 const char *start, int flg)
{
#define GET_TAG(fw) ( ((fw) && (fw)->tag) ? (fw)->tag : FUZZ_STR )
  static int i = 0;
  const char *tag;

  fw += ss; /* apply @fw start offset */
  int count = 0;
  for (const char *p = start, *end = start; end && count+ss < fw_len; ++count)
    {
      tag = GET_TAG (*fw);
      if ((end = strstr (p, tag)))
        {
          int _taglen = strlen (tag);
          cache[i] = (struct template_cache) {
            .widx = ss,  .flag = flg,  .taglen = _taglen,
            .stridx = (int) (end - start),
          };
          ++i, ++fw, p = end + _taglen;
        }
    }
  return i;
#undef GET_TAG
}

static int
lookup_fw_idx (Fword **haystack, int ss, int to, int needle_type)
{
  for (int i = ss; i < to; ++i)
    {
      if (haystack[i]->type == needle_type)
        return i;
    }
  return -1;
}

static void
gen_fuzz_cache (FuzzTemplate *template)
{
  Fword **fw = opt.words;
  int fw_len = opt.words_len, off = 0;
  int __off = 0;
  template->cache = ffuc_calloc (fw_len, sizeof (struct template_cache));

  const char *start;
  if ((start = template->URL)  &&  (opt.fuzz_flag & URL_HASFUZZ))
    {
      __off = lookup_fw_idx (fw, 0, fw_len, URL_TEMPLATE);
      assert (-1 != __off && "URL with no Fword");
      off = __do_fuzz_cache (fw, __off, fw_len, template->cache, start, 'u');
    }
  if ((start = template->body)  &&  (opt.fuzz_flag & BODY_HASFUZZ))
    {
      __off = lookup_fw_idx (fw, 0, fw_len, BODY_TEMPLATE);
      assert (-1 != __off && "body with no Fword");
      off = __do_fuzz_cache (fw, __off, fw_len, template->cache, start, 'b');
    }
  if (template->headers  &&  (opt.fuzz_flag & HEADER_HASFUZZ))
    {
      int i = 0;
      curl_slist_foreach (template->headers, h) {
        start = h->data;
        __off = lookup_fw_idx (fw, i, fw_len, HEADER_TEMPLATE);
        assert (-1 != __off && "header with no Fword");
        off =
          __do_fuzz_cache (fw, __off, fw_len, template->cache, start,
                           (i % 2) ? 'H' : 'h');
        ++i;
      }
    }
  assert (off == fw_len && "len(opt.words) != #FUZZ, broken logic.");
}

void
__register_context (RequestContext *dst)
{
  char **FUZZ = dst->FUZZ;
  int ss = 0; /* appropriate cache index */
  FuzzTemplate *template = &opt.fuzz_template;
  struct request_t *req = &dst->request;

  /**
   *  Generating URL
   *  based on opt.fuzz_template->URL
   */
  if (opt.fuzz_flag & URL_HASFUZZ)
    {
      sv_drop (&tmp);
      fuzz_snprintf (&tmp, template->URL, FUZZ, template->cache, &ss);
      Strrealloc (req->URL, sv_get(&tmp));
    }
  else if (! dst->request.URL)
    Strrealloc (req->URL, template->URL);
  curl_setopt (dst->easy_handle, CURLOPT_URL, req->URL);

  /**
   *  Generating POST body
   *  based on opt.fuzz_template->body
   */
  if (template->body)
    {
      if (opt.fuzz_flag & BODY_HASFUZZ)
        {
          sv_drop (&tmp);
          fuzz_snprintf (&tmp, template->body, FUZZ, template->cache, &ss);
          Strrealloc (req->body, sv_get(&tmp));
        }
      else if (! req->body)
        Strrealloc (req->body, template->body);
      curl_setopt (dst->easy_handle, CURLOPT_POSTFIELDS, req->body);
    }

  /**
   *  Generating HTTP headers
   *  based on opt.fuzz_template->headers
   */
  if (template->headers)
    {
      if (opt.fuzz_flag & HEADER_HASFUZZ)
        {
          curl_slist_safe_free (req->headers);
          curl_slist_foreach (template->headers, h)
            {
              sv_drop (&tmp);
              fuzz_snprintf (&tmp, h->data, FUZZ, template->cache, &ss);
              curl_slist_appd (req->headers, sv_get(&tmp));
            }
          curl_setopt (dst->easy_handle, CURLOPT_HTTPHEADER, req->headers);
        }
      else
        curl_setopt (dst->easy_handle, CURLOPT_HTTPHEADER, template->headers);
    }
}

int
register_context (RequestContext *ctx, bool sync)
{
  CURL *curl = ctx->easy_handle;
  curl_easy_reset (ctx->easy_handle);
  {
    FLG_SET (ctx->flag, CTX_INUSE);
    /* HTTP verb */
    if (opt.verb)
      curl_setopt (curl, CURLOPT_CUSTOMREQUEST, opt.verb);
    /* Ignore certification check */
    curl_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
    /* Deliver @ctx to curl_fwrite (custom fwrite function) */
    curl_setopt (curl, CURLOPT_WRITEDATA, ctx);
    curl_setopt (curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
    /* Timeout */
    curl_setopt (curl, CURLOPT_TIMEOUT_MS, (size_t) opt.ttl);
    curl_setopt (curl, CURLOPT_CONNECTTIMEOUT_MS, CONN_TTL_MS);
    /* Proxy */
    if (opt.proxy)
      curl_setopt (curl, CURLOPT_PROXY, opt.proxy);
  }
  __register_context (ctx);

  if (sync) /* blocking */
    return curl_easy_perform (curl);
  else /* none blocking */
    curl_multi_add_handle (opt.multi_handle, curl);
  return 0;
}

//-- Progress statistics & Regex functions --//
static inline size_t
update_req_rate (Progress *prog)
{
  if (prog->dt_us < MIN_DT_US)
    return prog->rate;
  size_t rate = REQ_RATE (prog);
  return prog->rate = rate;
}

static inline size_t
rt_req_rate (Progress *prog)
{
  if (prog->dt_us < MIN_DT_US)
    return prog->rate;
  return RT_REQ_RATE (prog);
}

static inline int
regex_match_c (struct ffuc_regex *rx, unsigned char c)
{
#ifdef _DEBUG
  assert ((rx->fl->type == FILTER_REGEX ||
           rx->fl->type == MATCH_REGEX) && "Not a regex filter.");
#endif
  const char *patt = rx->fl->filter.regex.pattern;
  if ('\0' == c)
    return rx->match ? true : false;

  unsigned char _c = patt[rx->__idx];
  if ('\0' == _c)
    goto found;
  if (_c != c)
    return (rx->match = rx->__idx = 0);
  else if ('\0' == patt[++rx->__idx])
    goto found;

  return (rx->match = 0);
 found:
  rx->match = rx->__idx;
  rx->__idx = 0;
  return true;
}

/* find common value at offset @foff in @stat array
   Ex:  req_stat_find_common(stats, N, wcount); */
int
__req_stat_find_common (struct req_stat_t *stat, int len, int foff)
{
#define S(x, offset) *(int *)((char *)(&(x)) + offset)
  int common = S(stat[0], foff);
  for (int i=1; i < len; ++i)
    {
      if (S(stat[i], foff) != common)
        return -1;
    }
  return common;
#undef S
}

#define req_stat_find_common(arr, len, name) \
  __req_stat_find_common (arr, len, offsetof(struct req_stat_t, name))

static void
__update_progress_bar (const Progress *prog)
{
  fprintf (stderr, CLEAN_LINE ("\
::.   Progress: %d%% [%d/%d]  ::  %-3d req/sec  ::   Errors: %d   .::"),
           REQ_PERC (prog),
           prog->req_sent, prog->req_total,
           prog->rate,
           prog->err_count
  );
}

static void
init_progress (Progress *prog)
{
  prog->req_sent = 0;
  /* This makes progress-bar refresh at every 1% of progress */
  prog->progbar_refrate = MAX(1, prog->req_total / 100);
  if (opt.Printf.isatty) /* makes it dirty with pipes */
    update_progress_bar (prog);
}

static void
tick_progress (Progress *prog)
{
  static size_t t0=0;  // NO thread
  struct timespec ts;

  clock_gettime (CLOCK_MONOTONIC, &ts);
  size_t t = TS2US (ts);
  size_t dt = t-t0;

  if (dt > MIN_DT_US)
    {
      prog->__req_dt = prog->req_dt; /* stable req_dt */
      prog->req_dt = 0;
      prog->dt_us = dt; /* update delta time */
      t0 = t;
    }
  else
    usleep (MIN_DT_US);
}

//-- Utility functions --//
void
goto_raw_mode (struct termios *original)
{
  struct termios raw;
  tcgetattr (STDIN_FILENO, original);
  raw = *original;
  {
    raw.c_lflag &= ~(ICANON | ECHO); /* Disable canonical mode and echo */
    raw.c_iflag &= ~(IXON);          /* Disable software flow control */
  }
  tcsetattr (STDIN_FILENO, TCSANOW, &raw);
}

static inline void *
strrealloc (void *malloced, const char *src)
{
  void * res;
  size_t n = strlen (src);
  res = realloc (malloced, n+1);
  memcpy (res, src, n+1);
  return res;
}

void
fuzz_snprintf (struct strv *dst, const char *format, char **src,
               const struct template_cache *cache, int *cache_ss)
{
  int ss = 0, to = 0;
  cache += *cache_ss; /* apply cache start offset */

  for (int i=0, flg = 0; i < opt.words_len;)
    {
      const char *key = format + ss;
      const char *val = src[cache->widx];
      to = cache->stridx;
      flg = cache->flag;
      sv_appd_buf (dst, key, to - ss, false);
      sv_appd_buf (dst, val, Strlen (val), false);

      ss = to + cache->taglen;
      ++i, ++(*cache_ss), ++src, ++cache;
      if (flg != cache->flag)
        break; // END.
    }
  if (format[ss])
    sv_appd_str (dst, format+ss);
  else
    sv_appd_buf (dst, "", 0, true); /* just to null-terminate */

  fprintd ("fuzz_snprintf:  output->'%s'\n", tmp);
}

static int
da_fw_lookup (Fword **haystack, const char *needle)
{
  if (! needle)
    return -1;
  da_foreach (haystack, i)
    {
      if (! haystack[i] || !haystack[i]->path)
        continue;
      if (0 == strcmp (haystack[i]->path, needle))
        return i;
    }
  return -1;
}

static Fword *
register_wlist (FuzzTemplate *t, const char *path, const char *tag)
{
  Fword *fw;
  (void) t;
  int idx = da_fw_lookup (opt.words, path);
  if (-1 == idx) /* New file */
    {
      fw = make_fw_from_path (path);
      if (! fw) /* could not use file, use dummy wordlist */
        fw = fw_dup (&dummy_fword);
    }
  else /* We have already opened this file */
    {
      fw = fw_dup (opt.words[idx]);
    }
  fw->tag = tag;
  return fw;
}

static void
range_usleep (useconds_t range[2])
{
  int interval = range[1] - range[0];
  if (0 >= interval)
    {
      if (range[0] > 0)
        usleep (range[0]);
    }
  else
    {
      useconds_t r = rand () % (interval + 1);
      usleep (range[0] + r);
    }
}

size_t
fuzz_count (const char *s)
{
  size_t n = 0;
  if (!s)
    return 0;
  while ((s = strstr (s, FUZZ_STR)))
    n++, s += lstrlen (FUZZ_STR);
  return n;
}

/* Only for numeric filters */
static inline void
opt_filter (int type, int start, int end)
{
  struct res_filter_t fl = {
    .type  = type,
    .filter.range = {start, end},
  };
  da_appd (opt.filters, fl);
}
#define opt_filter_val(typ, v) opt_filter (typ, v, v)

static inline void
opt_append_filter (int typ, const char *patt)
{
  struct res_filter_t fl = { .type = typ };

  switch (typ)
    {
    case FILTER_REGEX:
    case MATCH_REGEX:
      fl.filter.regex.pattern = strdup (patt);
      break;

    default: /* numeric filtering */
      const char *p = patt;
      fl.filter.range.start = atoi (p);
      p = strchr (p, '-');
      if (p)
        fl.filter.range.end = atoi (p + 1);
      else
        fl.filter.range.end = fl.filter.range.start;
    }

  da_appd (opt.filters, fl);
}

static inline void
free_filter (struct res_filter_t *fl)
{
  switch (fl->type)
    {
    case FILTER_REGEX:
    case MATCH_REGEX:
      safe_free (fl->filter.regex.pattern);
      break;
    default:
      break;
    }
}

static Fword *
make_fw_from_path (const char *path)
{
  int fd;
  Fword tmp = {0};
  if (! path)
    return NULL;

  fd = open (path, O_RDONLY);
  if (fd < 0)
    {
      warnln ("could not open file '%s' -- %s.", path, strerror (errno));
      return NULL;
    }

  if (0 == fw_map (&tmp, fd))
    {
      tmp.path = path;
      return fw_dup (&tmp);
    }
  else
    {
      warnln ("could not mmap file '%s'.", path);
      close (fd);
      return NULL;
    }

  return NULL;
}

/* In interactive mode, this runs in a separate thread */
void *
interact (void *__ptr)
{
  int c;
  struct Opt *opt = (struct Opt *)__ptr;
  while ((c = getchar ()))
    {
      switch (c)
        {
        case '\n':
          if (opt->Rqueue.waiting)
            update_progress_bar (&opt->progress);
          break;

        case EOF:
        case CEOT:
          goto end_of_interactive;
        }
    }
 end_of_interactive:
  disable_raw_mode (&opt->orig_termios);
  return NULL;
}

/* Initializes the global Opt, after parsing user options */
static int
init_opt ()
{
  /* Finalizing the HTTP request template */
  set_template (&opt.fuzz_template, FINISH_TEMPLATE, NULL);
  opt.words_len = da_sizeof (opt.words);
  if (opt.words_len == 0)
    {
      opt.eofuzz = true;
      warnln ("cannot continue with no word-list.");
      return EXIT_FAILURE;
    }

  /* Set the default filters if not disabled */
  if (NO_FILTER == opt.filters)
    opt.filters = NULL;
  else if (NULL == opt.filters && !opt.AI)
    da_appd (opt.filters, default_filter);

  /* find regex filters, and give each request context a copy of them */
  struct ffuc_regex *tmp_regs = NULL;
  opt.regex_count = 0;
  da_foreach (opt.filters, i) {
    switch (opt.filters[i].type)
      {
      case FILTER_REGEX:
      case MATCH_REGEX:
        da_appd (tmp_regs, (struct ffuc_regex){.fl = &opt.filters[i]});
        opt.regex_count++;
        break;
      default:
        break;
      }
  }
  /* Initializing request contexts */
  if (opt.Rqueue.len > opt.max_rate) /* prevent exceeding max rate */
    opt.Rqueue.len = MAX (opt.max_rate, 1);
  opt.Rqueue.ctxs = ffuc_calloc (opt.Rqueue.len, sizeof (RequestContext));
  for (size_t i = 0; i < opt.Rqueue.len; i++)
    {
      int cap_bytes;
      RequestContext *ctx = &opt.Rqueue.ctxs[i];
      STAT_RESET (&ctx->stat);
      ctx->easy_handle = curl_easy_init();
      /* init FUZZ array */
      cap_bytes = (opt.words_len + 1) * sizeof (char *);
      ctx->FUZZ = ffuc_malloc (cap_bytes);
      Memzero (ctx->FUZZ, cap_bytes);
      /* init regex match array */
      if (opt.regex_count)
        {
          cap_bytes = opt.regex_count * sizeof(struct ffuc_regex);
          ctx->matches = ffuc_malloc (cap_bytes);
          Memcpy (ctx->matches, tmp_regs, cap_bytes);
        }
      else ctx->matches = NULL;
    }
  da_free (tmp_regs);

  /* Initialize libcurl & context of requests */
  curl_global_init (CURL_GLOBAL_DEFAULT);
  opt.multi_handle = curl_multi_init ();

  if (!opt.verb || Strcmp (opt.verb, "GET"))
     {
       if (opt.fuzz_template.body)
         warnln ("sending body in 'GET' request.");
    }

  /* Improve performance, sets mode=singular for one wordlist */
  if (opt.words_len <= 1)
    opt.mode = MODE_SINGULAR;
  /* Mode depended initializations */
  switch (opt.mode)
    {
    case MODE_PITCHFORK:
      opt.progress.req_total = 0;
      for (da_idx i=0; i < opt.words_len; ++i)
        if (opt.words[i]->total_count > opt.progress.req_total)
          opt.progress.req_total = opt.words[i]->total_count;
      opt.load_next_fuzz = __next_fuzz_pitchfork;
      break;

    case MODE_SINGULAR:
      opt.progress.req_total = opt.words[0]->total_count;
      opt.load_next_fuzz = __next_fuzz_singular;
      break;

    case MODE_CLUSTERBOMB:
      opt.progress.req_total = 1;
      for (da_idx i=0; i < opt.words_len; ++i)
        opt.progress.req_total *= opt.words[i]->total_count;
      opt.load_next_fuzz = __next_fuzz_clusterbomb;
      break;
    }

  if (! isatty (fileno (stderr)))
    opt.progress.progbar_enabled = false;
  opt.Printf.isatty = isatty (fileno (opt.Printf.streamout));
  if (! opt.Printf.isatty)
    {
      opt.Printf.color = false;
      opt.Printf.lineclear = false;
    }
  else
    {
      opt.Printf.color = opt.color_enabled;
      if (opt.progress.progbar_enabled)
        opt.Printf.lineclear = true;
    }
  if (! isatty (STDIN_FILENO))
    opt.interactive = false;
  return EXIT_SUCCESS;
}

int
set_template_wlist (FuzzTemplate *t, enum template_op op,
                    Fword **dst, void *param)
{
  const char *tag = NULL;
  char *path = (char *) param, *p = path;
  if (path  &&  (p = strchr (path, ':')))
    { /* has tag */
      *p++ = '\0';
      if (0 == strncmp (p, FUZZ_STR, FUZZ_STR_LEN))
        tag = p;
      else
        warnln ("tag '%s' was ignored, only FUZZ_XXX pattern is acceptable", p);
    }

  dst += t->local_off; /* apply start offset */
  if (! tag  ||  opt.mode == MODE_SINGULAR)
    { /* just find a free cell */
    no_tag:
      for (int i=0; i < t->local_cap; ++i)
        {
          if (NULL == dst[i])
            {
              dst[i] = register_wlist (t, path, NULL);
              dst[i]->type = op;
              t->local_fuzz_count--;
              break;
            }
        }
      return 0;
    }

  char *tag_candidate = NULL;
  switch (op)
    {
    case URL_TEMPLATE:
      tag_candidate = t->URL;
      break;
    case BODY_TEMPLATE:
      tag_candidate = t->body;
      break;
    case HEADER_TEMPLATE:
      struct curl_slist *last_header = t->headers;
      for (; last_header->next; last_header = last_header->next);
      tag_candidate = last_header->data;
      break;

    default:
      break;
    }

  int resolved = 0;
  for (int i=0; t->local_fuzz_count > 0; ++i)
    {
      tag_candidate = strstr (tag_candidate, FUZZ_STR);
      if (! tag_candidate)
        break;
      if (0 == strncmp (tag_candidate, tag, strlen (tag)))
        {
          if (NULL != dst[i])
            { /* we reached here, only because in the previous call,
                 we didn't know a tagged word-list will come later */
              for (int j=0; j < t->local_cap; ++j)
                { /* so fix it by swapping. it MUST success. */
                  if (NULL == dst[j])
                    {
                      dst[j] = dst[i];
                      if (dst[j]->tag)
                        dst[j]->tag = NULL;
                      dst[i] = NULL;
                      break;
                    }
                }
              assert (NULL == dst[i] && "tag swapping must success");
            }
          dst[i] = register_wlist (t, path, tag);
          dst[i]->type = op;
          t->local_fuzz_count--, resolved++;
        }
      tag_candidate++;
    }
  if (0 == t->local_fuzz_count)
    return 0;
  if (0 == resolved)
    {
      warnln ("could not find tag: '%s', retrying without tag.", tag);
      tag = NULL;
      goto no_tag;
    }
  return 0;
}

/* While this may sound like OOP, it simplifies the
   logic for parsing user options a lot.
   Users might provide wrong number of word-lists
   or forget the `&` in their HTTP body options;
   This function will fix all of them */
int
set_template (FuzzTemplate *t, enum template_op op, void *_param)
{
  char *param = (char *) _param;
  static int prev_op = -1;

  switch (prev_op)
    {
    case URL_TEMPLATE:
    case BODY_TEMPLATE:
    case HEADER_TEMPLATE:
      if (opt.mode == MODE_SINGULAR)
        break;
      if (WLIST_TEMPLATE != op  &&  0 < t->local_fuzz_count)
        {
          /* We are not adding a new word-list and the latest
             modified HTTP template doesn't have enough word-lists */
          warnln ("not enough worlists provided for the "
                  "HTTP option '%s', ignoring %d FUZZ keyword%s",
                  template_name[prev_op],
                  t->local_fuzz_count,
                  (t->local_fuzz_count == 1) ? "." : "s.");
          /* Fill the previous template with dummy word-list */
          for (int i=0; 0 != t->local_fuzz_count; ++i)
            set_template_wlist (t, prev_op, opt.words, NULL);
        }
      break;

    default:
      break;
    }

  switch (op)
    {
    case URL_TEMPLATE:
      {
        prev_op = URL_TEMPLATE;
        Strrealloc (t->URL, param);
        t->local_fuzz_count = fuzz_count (t->URL);
        if (t->local_fuzz_count > 0)
          {
            FLG_SET (opt.fuzz_flag, URL_HASFUZZ);
            t->local_off = da_zallocate (opt.words, t->local_fuzz_count);
            t->local_cap = t->local_fuzz_count;
          }
      }
      return t->local_fuzz_count;

    case HEADER_TEMPLATE:
      {
        prev_op = HEADER_TEMPLATE;
        curl_slist_appd (t->headers, param);
        t->local_fuzz_count = fuzz_count (param);
        if (t->local_fuzz_count > 0)
          {
            FLG_SET (opt.fuzz_flag, HEADER_HASFUZZ);
            t->local_off = da_zallocate (opt.words, t->local_fuzz_count);
            t->local_cap = t->local_fuzz_count;
          }
      }
      return t->local_fuzz_count;

    case BODY_TEMPLATE:
      {
        prev_op = BODY_TEMPLATE;
        if (NULL == t->body)
          t->body = strdup (param);
        else
          {
            size_t len = Strlen (t->body);
            if ('&' != param[0] && '&' != t->body[len - 1])
              {
                /* We need an extra `&` */
                Realloc (t->body, len + Strlen (param) + 2);
                char *p = t->body + len;
                *p = '&';
                strcpy (p + 1, param);
              }
            else
              {
                Realloc (t->body, len + Strlen (param) + 1);
                strcpy (t->body + len , param);
              }
          }
        t->local_fuzz_count = fuzz_count (param);
        if (t->local_fuzz_count > 0)
          {
            FLG_SET (opt.fuzz_flag, BODY_HASFUZZ);
            t->local_off = da_zallocate (opt.words, t->local_fuzz_count);
            t->local_cap = t->local_fuzz_count;
          }
      }
      return t->local_fuzz_count;

    case WLIST_TEMPLATE:
      {
        if (opt.mode == MODE_SINGULAR)
          {
            static int single = 0;
            if (0 == single++) /* the very only singular mode word-list */
              set_template_wlist (t, -1, opt.words, param);
            else
              {
                warnln ("word-list '%s' was ignored  --  \
singular mode only accepts one word-list", param);
                return -1;
              }
          }
        else
          {
            if (t->local_fuzz_count <= 0)
              warnln ("unexpected word-list '%s' was ignored.", param);
            else /* prev_op indicates the latest modified HTTP option */
              set_template_wlist (t, prev_op, opt.words, param);
          }
      }
      return 0;

    case FINISH_TEMPLATE:
      if (0 != t->local_fuzz_count)
        return 1;
      return 0;
    }

  return 0;
}

void
cleanup (int c, void *p)
{
  UNUSED (c), UNUSED (p);

  if (opt.interactive)
    {
      if (pthread_kill (opt.interact_th, 0) == 0)
        {
          pthread_cancel (opt.interact_th);
          disable_raw_mode (&opt.orig_termios);
          end_progress_bar (&opt.progress);
        }
    }

#ifndef SKIP_FREE
  /* Libcurl cleanup */
  if (NULL != opt.Rqueue.ctxs)
    {
      for (size_t i = 0; i < opt.Rqueue.len; i++)
        {
          RequestContext *ctx = &opt.Rqueue.ctxs[i];
          curl_easy_cleanup (ctx->easy_handle);
          safe_free (ctx->request.URL);
          safe_free (ctx->request.body);
          curl_slist_free_all (ctx->request.headers);
          safe_free (ctx->matches);
          for (int j = 0; j < opt.words_len; j++) {
            safe_free (ctx->FUZZ[j]);
            if (opt.mode == MODE_SINGULAR) break;
          }
          safe_free (ctx->FUZZ);
        }
    }
  curl_multi_cleanup (opt.multi_handle);
  curl_global_cleanup ();
  /* Opt cleanup */
  safe_free (opt.Rqueue.ctxs);
  da_foreach (opt.filters, i) {
    free_filter (&opt.filters[i]);
  }
  da_free (opt.filters);
  da_foreach (opt.words, i) {
    fw_free (opt.words[i]);
    if (opt.mode == MODE_SINGULAR) break;
  }
  da_free (opt.words);
  /* Template cleanup */
  safe_free (opt.fuzz_template.URL);
  safe_free (opt.fuzz_template.body);
  safe_free (opt.fuzz_template.cache);
#endif /* SKIP_FREE */
}

void
help ()
{
  fprintf (stdout, "\
%s v%s - ffuf written in C\n\
Usage:  ffuc [OPTIONS] [ HTTP_OPTION [WORDLIST]... ]...\n\
\n\
HTTP_OPTIONS:\n\
  -u, --url         URL (mandatory)\n\
  -d, --data        request bodt (only in POST requests)\n\
  -H, --header      HTTP header\n\
  -w, --wordlist    path to word-list(s)\n\
                    if the previous HTTP component has FUZZ keyword\n\
\n\
OPTIONS:\n\
    -R, --rate      maximum request rate (req/second)\n\
  --auto-filter     apply filters automatically\n\
                    it's recommended to use this instead of default settings\n\
  --fc, --mc        filter and match HTTP response code\n\
                      e.g.  [--fc 429]  or  [--fc 400-500]\n\
  --fs, --ms        filter and match size of response\n\
  --fw, --mw        filter and match word count of response\n\
  --fl, --ml        filter and match line count of response\n\
    -p, --delay     add delay between requests (in seconds)\n\
                      e.g.  [-p 1]  or  [-p 100-500ms] (random range)\n\
    -T, --timeout   requests timeout\n\
    -c, --color     toggle output color\n\
    -v, --verbose   verbose\n\
\n\
MODE:\n\
    -m, --mode        when more than one FUZZ keyword is provided\n\
   Clusterbomb (default):\n\
     All combinations of all word-lists\n\
   Pitchfork:\n\
     Picks up words from word-lists one by one, until longest one ends\n\
   Singular:\n\
     Accepts only one word-list and uses it for all FUZZ keywords\n\
     It's equivalent to clusterbomb for only one FUZZ keyword\n\
", PROG_NAME, PROG_VERSION);
}

int
parse_args (int argc, char **argv)
{
  int idx, flag;
  char *last_wlist = NULL;

  idx = 0, optind = 1;
  for (int i=1; i<argc; ++i)
    {
      char *arg = argv[i];
      if ('-' != *arg)
        continue;
      if (Strcmp (arg, "-m") || Strcmp (arg, "--mode")) /* -m, --mode */
        {
          optarg = argv[i+1];
          if (strcasestr ("pitchfork", optarg))
            opt.mode = MODE_PITCHFORK;
          else if (strcasestr ("singular", optarg))
            opt.mode = MODE_SINGULAR;
          else if (strcasestr ("clusterbomb", optarg) || strcasestr ("cluster-bomb", optarg))
            opt.mode = MODE_CLUSTERBOMB;
          else
            warnln ("invalid mode `%s` was ignored", optarg);
        }
    }

  idx = 0, optind = 1;
  while ((flag = getopt_long (argc,argv, lopt_str,lopts, &idx)) != -1)
    {
      switch (flag)
        {
        case 'h':
          help ();
          return SHOULD_EXIT;
        case 'v':
          opt.verbose = true;
          break;
        case 'c':
          opt.color_enabled ^= true;
          break;

        case 't':
          opt.Rqueue.len = atol (optarg);
          if (opt.Rqueue.len <= 0)
            {
              opt.Rqueue.len = DEFAULT_REQ_COUNT;
              warnln ("invalid thread number was ignored.");
            }
          break;
        case 'T':
          if ((opt.ttl = atoi (optarg)) <= 0)
            {
              opt.ttl = DEFAULT_TTL_MS;
              warnln ("invalid TTL was ignored.");
            }
          break;
        case 'p':
          {
            int d1 = 0, d2 = 0;
            sscanf (optarg, "%d-%d", &d1, &d2);

            if (d1 < 0 || d2 < 0 || (0 != d2 && d2 < d1))
              {
                warnln ("invalid delay interval was ignored.");
                break;
              }
            if (0 == d2)
              d2 = d1; /* constant delay */

            if (strstr (optarg, "ms"))
              d1 *= 1000, d2 *= 1000;  /* convert to millisecond */
            else
              d1 *= 1000000, d2 *= 1000000; /* convert to second */
            opt.Rqueue.delay_us[0] = d1;
            opt.Rqueue.delay_us[1] = d2;
          }
          break;
        case 'R':
          opt.max_rate = atoi (optarg);
          break;

          /* HTTP options */
        case 'u':
          if (opt.fuzz_template.URL)
            warnln ("unexpected URL option was ignored.");
          else
            set_template (&opt.fuzz_template, URL_TEMPLATE, optarg);
          break;
        case 'H':
          set_template (&opt.fuzz_template, HEADER_TEMPLATE, optarg);
          break;
        case 'd':
          set_template (&opt.fuzz_template, BODY_TEMPLATE, optarg);
          break;
        case 'X':
          opt.verb = optarg;
          break;
        case 'w':
          last_wlist = optarg;
          set_template (&opt.fuzz_template, WLIST_TEMPLATE, optarg);
          break;
        case 'x':
          opt.proxy = optarg;
          break;

#define AddFilter(ftype)                                    \
          if (NO_FILTER != opt.filters)                     \
            opt_append_filter (ftype, optarg);              \
          else warnln ("filter and match is disabled.");    \
          break;

        case '0': AddFilter (FILTER_CODE);
        case '1': AddFilter (FILTER_SIZE);
        case '2': AddFilter (FILTER_WCOUNT);
        case '3': AddFilter (FILTER_LCOUNT);
        case '4': AddFilter (FILTER_REGEX);

        case '9': AddFilter (MATCH_CODE);
        case '8': AddFilter (MATCH_SIZE);
        case '7': AddFilter (MATCH_WCOUNT);
        case '6': AddFilter (MATCH_LCOUNT);
        case '5': AddFilter (MATCH_REGEX);

#undef AddFilter

        case '*':
          opt.AI = true;
          break;
        case 'i':
          opt.interactive = true;
          break;
        case 'A':
          if (opt.filters && NO_FILTER != opt.filters)
            warnln ("disable filters along with filter options.");
          else
            opt.filters = NO_FILTER;
          break;

        default:
          break;
        }
    }

  if (!opt.fuzz_template.URL)
    {
      warnln ("no URL provided (use -u <URL>).");
      return EXIT_FAILURE;
    }
#ifndef DO_NOT_FIX_NO_FUZZ /* no FUZZ keyword is provided handling */
  if (! HAS_FLAG (opt.fuzz_flag, URL_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, BODY_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, HEADER_HASFUZZ))
    {
      const char *url = opt.fuzz_template.URL;
      if (last_wlist && url)
        {
          if (opt.verbose)
            warnln ("\
No FUZZ keyword found, assuming to use '%s' with the given URL.", last_wlist);
          sv_drop (&tmp);
          sv_appd_str (&tmp, url);
          sv_appd_str (&tmp, ('/' == lastcharof (url)) ? "FUZZ" : "/FUZZ");
          set_template (&opt.fuzz_template, URL_TEMPLATE, sv_get(&tmp));
          set_template (&opt.fuzz_template, WLIST_TEMPLATE, last_wlist);
        }
      else
        {
          warnln ("nothing to do  --  exiting.");
          return SHOULD_EXIT;
        }
    }
#endif /* DO_NOT_FIX_NO_FUZZ */
  return EXIT_SUCCESS;
}

/* Sets filters automatically based on endpoint's behavior */
int __attribute__ ((optimize ("O0")))
do_filter_discovery (void)
{
#define N DISCOVERY_REQ_COUNT
#if N <= 0
# error "DISCOVERY_REQ_COUNT must be grater than zero."
#endif
  int ret;
  struct req_stat_t disc_stat[N];
  { /* Sending discovery (probe) requests */
    for (int i=0; i<N; ++i)
      {
        int retry_c = 0;
        RequestContext *ctx = opt.Rqueue.ctxs;
        __next_fuzz_rand (ctx); /* loading a random FUZZ string */
      retry:
        ret = register_context (ctx, true); /* blocking */
        if (CURLE_OK != ret)
          {
            bool should_retry =
              (retry_c++ < MAX_RETRY_DISCOVERY) & /* max retry */
              (ret > CURLE_COULDNT_CONNECT); /* avoid pointless retry */
            warnln ("discovery request failed  --  %s%s",
                    curl_easy_strerror (ret),
                    (should_retry) ? "  (retrying)" : "");
            context_reset (ctx);
            if (should_retry)
              goto retry;
            return 1;
          }
        curl_easy_getinfo (ctx->easy_handle, CURLINFO_HTTP_CODE, &ctx->stat.code);
        disc_stat[i] = ctx->stat;
        if (opt.verbose)
          {
            warnln ("probe #%d | W: %-4d | L: %-4d | S: %-5d | C: %-3d |", i+1,
                    ctx->stat.wcount,
                    ctx->stat.lcount,
                    ctx->stat.size_bytes,
                    (int) ctx->stat.code);
          }
        context_reset (ctx);
      }
  }

    int common;
         if ((common = req_stat_find_common (disc_stat, N,  wcount))      != -1)
      opt_filter_val (FILTER_WCOUNT,  common);  /* filter by word count */
    else if ((common = req_stat_find_common (disc_stat, N,  lcount))      != -1)
      opt_filter_val (FILTER_LCOUNT,  common);  /* filter by line count */
    else if ((common = req_stat_find_common (disc_stat, N,  size_bytes))  != -1)
      opt_filter_val (FILTER_SIZE,    common);  /* filter by response length */
    else if ((common = req_stat_find_common (disc_stat, N,  code))        != -1)
      opt_filter_val (FILTER_CODE,    common);  /* filter by HTTP code */
    else
      warnln ("auto-filter failed - endpoint is not stable");
  return 0;
#undef N
}

static inline RequestContext *
handle_response_curl (const CURLMsg *msg)
{
  RequestContext *ctx;
  CURL *curl = msg->easy_handle;

  ctx = lookup_handle (curl, opt.Rqueue.ctxs, opt.Rqueue.len);
  assert (NULL != ctx && "Broken Logic!!  -  \
Completed easy_handle doesn't have request context.\n");

  if (CURLMSG_DONE == msg->msg)
    {
      ctx->stat.ccode = msg->data.result;
      handle_response_context (ctx);
    }
  return ctx;
}

void
on_sigint (int signo)
{
  if (SIGINT != signo)
    return;
  /* This will trigger cleanup function
     It's important to disable terminal raw mode */
  exit (0);
}

int
main (int argc, char **argv)
{
#define Return(n) return (n == SHOULD_EXIT) ? EXIT_SUCCESS : EXIT_FAILURE

  int ret;
  set_program_name (PROG_NAME);
  on_exit (cleanup, NULL);
  srand (time (NULL));

  opt = (struct Opt) {
    .ttl = DEFAULT_TTL_MS,
    .mode = MODE_DEFAULT,
    .Rqueue.len = DEFAULT_REQ_COUNT,
    .max_rate = MAX_REQ_RATE,
    .Printf = { .streamout = stdout },
    .progress.progbar_enabled = true,
#ifndef NO_DEFAULT_COLOR
    .color_enabled = true,
#endif /* NO_DEFAULT_COLOR */
  };

  /* Print on stdout if it's not a tty (user's creating log file) */
  opt.Printf.streamout = (! isatty (fileno (stdout))) ? stdout : stderr;
  /* Parse cmdline arguments & Initialize opt */
  if ((ret = parse_args (argc, argv)))
    Return (ret);
  if ((ret = init_opt ()))
    Return (ret);

  if (opt.AI)
    {
      warnln ("sending discovery requests");
      if ((ret = do_filter_discovery ()))
        Return (ret);
    }
  /* Interactive mode */
  if (opt.interactive)
    {
      goto_raw_mode (&opt.orig_termios);
      pthread_create (&opt.interact_th, NULL, interact, &opt);
      signal (SIGINT, on_sigint); /* to disable raw mode on SIGINT */
    }
  log_current_config ();
  init_progress (&opt.progress);
  gen_fuzz_cache (&opt.fuzz_template);

  /**
   *  The main Loop
   */
  CURLMsg *msg;
  size_t avg_rate=0;
  RequestContext *ctx = NULL;
  int numfds, res, still_running;
  do {
    /* Find a free context (If there is any) and register it */
    while (!opt.eofuzz && opt.Rqueue.waiting < opt.Rqueue.len)
      {
        if (opt.max_rate <= rt_req_rate (&opt.progress))
          break;

        if ((ctx = lookup_free_handle (opt.Rqueue.ctxs, opt.Rqueue.len)))
          { /* Registering the context */
            opt.load_next_fuzz (ctx);
            register_context (ctx, false); /* none blocking */
            opt.Rqueue.waiting++;
            opt.progress.req_dt++;
          }
      }

    range_usleep (opt.Rqueue.delay_us);
    curl_multi_perform (opt.multi_handle, &still_running);
    curl_multi_wait (opt.multi_handle, NULL, 0, POLL_TTL_MS, &numfds);
    while ((msg = curl_multi_info_read (opt.multi_handle, &res)))
      {
        RequestContext *completed = handle_response_curl (msg);
        context_reset (completed);
        opt.progress.req_sent++;
        opt.Rqueue.waiting--;
      }

    tick_progress (&opt.progress);
    update_req_rate (&opt.progress);
    if (opt.verbose)
      { /* update average request rate */
        avg_rate += opt.progress.rate;
        avg_rate /= 2;
      }
  }
  while (still_running > 0 || !opt.eofuzz);

  end_progress_bar (&opt.progress);
  if (! opt.Printf.lineclear)
    fprintf (opt.Printf.streamout, "\n");
  if (opt.verbose)
    {
      warnln ("Total requests: %d, Errors: %d, at ~%zu req/sec.",
              opt.progress.req_total, opt.progress.err_count, avg_rate);
    }
  return EXIT_SUCCESS;
}
