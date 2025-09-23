/* This file is part of my-small-c-projects <https://gitlab.com/SI.AMO/>

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

    FFuc  -  ffuf program written in C

    Usage:
      $ ffuc [OPTIONS] [[HTTP OPTION] [-w /path/to/wordlist]...]...
      Provide HTTP component option, with or without FUZZ keyword,
      then provide enough word-list file path for it.

    Examples:
      $ ffuc -u https://x.com/FUZZ.FUZZ -w /tmp/wl1 -w /tmp/wl2
      (wl1 gets used for the first FUZZ keyword, and wl2 for the second one)

      $ ffuc -u http://x.com/FUZZ -w /tmp/wl1  -H 'X-test: FUZZ' -w /tmp/wl2

      $ ffuc -u https://x.com -w /tmp/words
      (It assumes you want to fuzz the end of the URL -> https://z.com/FUZZ)

      $ ffux -XPOST -u https://x.com  -d 'username=FUZZ' -w /tmp/words \
                                      -d 'password=FUZZ' -w /tmp/rockyou.txt

    Modes:
      This concept only matters if you provide more than one word-list;
      As in this case, we have more than one way to iterate over lists.

      FFuc implements three different methods:
        cluster-bomb (default):   [--mode c],[--mode clusterbomb]
          Cartesian product of all word-lists.

        pitchfork:  [--mode p],[--mode pitchfork]
          Treats word-lists as circular lists, and picks words
          one by one, until the longest word-list reaches the end.

        singular:  [--mode s],[--mode singular]
          Only accepts ONE word-list and use it for all FUZZ keywords.


    Compilation:
      cc -ggdb -O3 -Wall -Wextra -Werror \
         -I ../libs/ \
         ffuc.c -o ffuc -lcurl

    Options:
      -D_DEBUG:
        Print debug information
      -D SKIP_FREE:
        Skip freeing heap memory in cleanup function
      -D DO_NOT_FIX_NO_FUZZ:
        Disables handing no FUZZ keyword provided scenarios
      -D NO_DEFAULT_COLOR:
        Disables output colors by default
 **/
#undef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <errno.h>
#include <string.h>
#include <stdbool.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <curl/curl.h>

#define DYNA_IMPLEMENTATION
#include "dyna.h"

#define CLI_IMPLEMENTATION
#include "clistd.h"
#include <getopt.h>

#define PROG_NAME "FFuc"
#define PROG_VERSION "2.0"

#define SLIST_APPEND(list, val) \
  list = curl_slist_append (list, val)

#ifndef TMP_CAP
#  define TMP_CAP 1024 /* bytes */
#endif
static char tmp[TMP_CAP];

/* Maximum concurrent requests */
#ifndef DEFAULT_REQ_COUNT
# define DEFAULT_REQ_COUNT 10
#endif

/* Maximum request rate (req/sec) */
#ifndef MAX_REQ_RATE
# define MAX_REQ_RATE 1000
#endif

/* Request rate measure interval (ms) - Not accurate */
#ifndef MAX_RATE_MEASURE_DUR
# define MAX_RATE_MEASURE_DUR 1000
#endif
#ifndef MIN_RATE_MEASURE_DUR
# define MIN_RATE_MEASURE_DUR 200
#endif

/**
 *  Minimum deltatime to call gettimeofday again.
 *  In some systems, calling gettimeofday might introduce
 *  overhead; when LIMIT_TIMEOFDAY_CALLS macro is defined,
 *  we skip this calls SKIP_TIMEOFDAY_N times if encounter
 *  any measured delta time less then MIN_TIMEOFDAY_DUR.
 */
#ifndef MIN_TIMEOFDAY_DUR
# define MIN_TIMEOFDAY_DUR 10 // 10 ms
#endif
#ifndef SKIP_TIMEOFDAY_N
# define SKIP_TIMEOFDAY_N 2  // 2 times skipping
#endif

/* Default connection/read timeuot */
#ifndef CONN_TTL_MS
# define CONN_TTL_MS 10000L
#endif
#ifndef DEFAULT_TTL_MS
# define DEFAULT_TTL_MS 10000
#endif

/* Poll timeout */
#ifndef POLL_TTL_MS
# define POLL_TTL_MS 1000
#endif

#ifndef PRINT_MARGIN
# define PRINT_MARGIN 25
#endif

#define NOP ((void) NULL)
#define UNUSED(x) (void)(x)
#define MIN(a,b) ((a < b) ? (a) : (b))

#define FLG_SET(dst, flg) (dst |= flg)
#define HAS_FLAG(val, flg) (val & flg)
#define FLG_UNSET(dst, flg) (dst &= ~(flg))

#define Fprintarr(stream, element_format, arr, len) do {    \
    int __idx = 0;                                          \
    for (; __idx < (len)-1; __idx++)                        \
      fprintf (stream, element_format ", ", (arr)[__idx]);  \
    fprintf (stream, element_format, (arr)[__idx]);         \
  } while (0)

const char *lopt_str = "m:w:" "T:R:t:p:A" "u:H:d:X:x:" "vhc";
const struct option lopts[] =
  {
    /* We call it `thread` (-t) for compatibility with ffuf,
       even though we don't use threads */
    {"thread",              required_argument, NULL, 't'},
    {"concurrent",          required_argument, NULL, 't'},

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
    /* Filter and Match disabled */
    {"all",                 no_argument,       NULL, 'A'},
    {"no-filter",           no_argument,       NULL, 'A'},
    {"filter-no",           no_argument,       NULL, 'A'},

    /* Common options */
    {"mode",                required_argument, NULL, 'm'},
    {"word",                required_argument, NULL, 'w'},
    {"wordlist",            required_argument, NULL, 'w'},
    {"word-list",           required_argument, NULL, 'w'},
    {"mode",                required_argument, NULL, 'm'},
    {"rate",                required_argument, NULL, 'R'},
    {"max_rate",            required_argument, NULL, 'R'},
    {"timeout",             required_argument, NULL, 'T'},
    {"ttl",                 required_argument, NULL, 'T'},
    {"delay",               required_argument, NULL, 'p'},
    {"verbose",             no_argument,       NULL, 'v'},
    {"color",               no_argument,       NULL, 'c'},
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
     *
     * Pitchfork:
     *   Each FUZZ, uses it's own word-list
     *   word-lists are not necessarily the same size
     *     O( Max( len(wlist #1), ..., len(wlist #N) ) )
     *
     * Clusterbomb:
     *   All combinations of word-list(s)
     *     O( len(wlist #1) x ... x len(wlist #N) )
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
     *  Request context in use flag
     *  Used in RequestContext.flag
     */
    CTX_FREE          = 0,
    CTX_INUSE         = 1,

    /* Exit codes */
    EOFUZZ            = 1,
    SHOULD_EXIT       = -1,
  };

/* Output filter & match, fits in `unsigned char` */
enum filter_flag_t
  {
    FILTER_CODE       = 1,
    FILTER_WCOUNT     = 2,
    FILTER_LCOUNT     = 3,
    FILTER_SIZE       = 4,
    FILTER_TIME       = 5,

    MATCH_CODE        = 127,
    MATCH_WCOUNT      = 126,
    MATCH_LCOUNT      = 125,
    MATCH_SIZE        = 124,
    MATCH_TIME        = 123,
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

  int code; /* HTTP response code */
  uint duration; /* total time to response */
  CURLcode ccode;
};

typedef struct progress_t
{
  uint req_total;
  uint req_count; /* count of sent requests */
  uint err_count; /* count of errors */

  bool progbar_enabled;
  uint progbar_refrate; /* progress bar refresh rate */

  uint req_count_dt; /* requests in delta time */
  size_t dt; /* delta time in milliseconds */
  struct timeval __t0; /* internal */
} Progress;

#define MAX(x,y) (((x) > (y)) ? (x) : (y))
/* Percentage of progress */
#define REQ_PERC(prog) ((prog)->req_count * 100 / (prog)->req_total)
/* Convert timeval struct to milliseconds */
#define TV2MS(tv) (tv.tv_sec * 1000LL  +  tv.tv_usec / 1000)

struct res_filter_t
{
  char type; /* FILTER_XXX  or  MATCH_XXX */
  struct
  {
    int start, end;
  } range;
};

/* The default filter to match success status codes */
const struct res_filter_t default_filter = {MATCH_CODE, {200, 399}};
/* To disable the default filter */
#define NO_FILTER ((void *) -1)

typedef struct
{
  int flag;
  CURL *easy_handle;

  /* Statistics of the request */
  struct req_stat_t stat;
  
  /**
   *  All 'FUZZ' keywords within @opt.fuzz_template
   *  (URL, POST body, HTTP headers respectively)
   *  will be substituted with elements of this array.
   *  It must contain opt.words_len elements.
   */
  char **FUZZ;

  struct request_t
  {
    char *body;
    struct curl_slist *headers;
  } request;

} RequestContext;

typedef struct
{
  char *URL;
  char *body;
  struct curl_slist *headers;

  char **wlists;
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
} Fword;

const Fword dummy_fword = {
  .str="FUZZ\n", .len=4, .total_count=1, .__str_bytes=5
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
 *   Advances the timer and resets the progress if needed
 *   It MUST be called at the end of the main loop
 *
 * update_progress_bar:
 *   Prints a simple progress-bar if is not disabled,
 *   It needs a newline at the end, use end_progress_bar()
 */
static void init_progress (Progress *prog);
static inline void tick_progress (Progress *prog);
static void __update_progress_bar (const Progress *prog);
static inline size_t req_rate (const Progress *prog);
#define update_progress_bar(prog) \
  if ((prog)->progbar_enabled) __update_progress_bar (prog)
#define end_progress_bar(prog) \
  if ((prog)->progbar_enabled) fprintf (stderr, "\n");

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
static inline int
set_template_wlist (FuzzTemplate *t, enum template_op op, void *param);


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
#define curl_setopt(...) curl_easy_setopt (__VA_ARGS__)

#undef ffuc_free
#ifndef SKIP_FREE
# define ffuc_free(x) free (x)
# define ffuc_munmap(ptr, len) munmap (ptr, len)
#else
# define ffuc_free NOP
# define ffuc_munmap NOP
#endif /* SKIP_FREE */

#define safe_free(ptr) \
  if (NULL != ptr) { ffuc_free (ptr); ptr = NULL; }

/**
 **  The primary bottleneck is the network, so improving
 **  memory allocation is unlikely to enhance performance.
 **/

/**
 *  To prevent memory leaks, DO *NOT* use
 *  `ffuc_free` or `safe_free` here, as Strrealloc is used for
 *  reading from word-lists, and may be called thousands of time!
 */
#define Strrealloc(dst, src) do {               \
    if (dst) free (dst);                        \
    dst = strdup (src);                         \
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

#ifdef _DEBUG
# define fprintd(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)
# define printd(format, ...) \
  fprintd ("%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__);
#else
# define fprintd(...) NOP
# define printd(...) NOP
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
  uint max_rate; /* Max request rate (req/sec) */
  char *verb; /* HTTP verb */
  FILE *streamout;
  char *proxy;
  FuzzTemplate fuzz_template;
  struct res_filter_t *filters; /* Dynamic array */

  /* Internals */
  int fuzz_flag;
  CURLM *multi_handle;
  struct progress_t progress;

  Fword **words; /* Dynamic array */
  int words_len; /* Length of @words */

  struct request_queue_t
  {
    RequestContext *ctxs; /* Static array */
    size_t len; /* Length of @ctxs */
    size_t waiting; /* number of used elements */
    useconds_t delay_us[2]; /* delay range, microseconds */
  } Rqueue;

  struct printf_t
  {
    bool lineclear; /* Should clear terminal */
    bool color; /* Color enabled */
  } Printf;

  /* Next FUZZ loader and */
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

/** @optr: The corresponding request context, passed by libcurl **/
size_t
curl_fwrite (void *ptr, size_t size, size_t nmemb, void *optr)
{
  size_t len = (size_t)(size * nmemb);
  unsigned char *data = (unsigned char *) ptr;
  RequestContext *ctx = (RequestContext *) optr;

  ctx->stat.size_bytes += len; /* Update size */
  for (size_t i=0; i<len; ++i)
    {
      /* Update word count and line count */
      unsigned char c = data[i];
      if (c == ' ')
        ctx->stat.wcount++;
      else if (c < ' ')
        ctx->stat.lcount++;
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

/**
 *  FUZZ sprintf substitutes the FUZZ keyword of @format
 *  with the appropriate value from the @FUZZ array
 *  The last element of @FUZZ MUST be NULL.
 * Return:
 *  The number of elements consumed from @FUZZ.
 */
int
fuzz_snprintf (char *restrict dst, size_t dst_cap,
              const char *restrict format, char **FUZZ)
{
  int fuzz_used = 0;
  const char *start = format;

  for (const char *end = start, *__dst = dst;
       NULL != end && (size_t)(dst - __dst) < dst_cap; )
    {
      if ((end = strstr (start, "FUZZ")))
        {
          if (end != start)
            dst = mempcpy (dst, start, (size_t)(end - start));
          start = end + 4;

          if (opt.mode == MODE_SINGULAR)
            dst = stpcpy (dst, *FUZZ); // replacing all FUZZs with FUZZ[0]
          else if (*FUZZ)
            {
              dst = stpcpy (dst, *FUZZ);
              FUZZ++, fuzz_used++;
            }
          else
            dst = stpcpy (dst, "FUZZ");
        }
    }

  if ('\0' != *start)
    strcpy (dst, start);
  else
    *dst = '\0';
  return fuzz_used;
}

//-- Load next FUZZ functions --//
static void
__next_fuzz_singular (RequestContext *ctx)
{
  Fword *fw = opt.words[0];
  snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
  Strrealloc (ctx->FUZZ[0], tmp);

  da_idx i=1, N = opt.words_len;
  for (; i < N; ++i)
    ctx->FUZZ[i] = ctx->FUZZ[0];
  ctx->FUZZ[i] = NULL;

  if (fw_eof (fw))
    opt.eofuzz = true;

  fprintd ("singular:  [0-%ld]->`%s`\n", N, tmp);
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
      snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
      Strrealloc (ctx->FUZZ[i], tmp);
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
      snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
      Strrealloc (ctx->FUZZ[i], tmp);
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
lookup_handle (CURL *handle,
               RequestContext *ctxs, size_t len)
{
  for (size_t i=0; i < len; ++i)
    {
      if (ctxs[i].easy_handle == handle)
        return &ctxs[i];
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
  memset (&ctx->stat, 0, sizeof (struct req_stat_t));
}

static inline const char *
http_pallet_of (int resp_code)
{
  if (resp_code < 100 || resp_code >= 600)
    return HttpPallet[ HTTP_ERR ];
  if (resp_code >= 200 && resp_code < 300)
    return HttpPallet[ HTTP_2xx ];
  if (resp_code >= 300 && resp_code < 400)
    return HttpPallet[ HTTP_3xx ];
  if (resp_code >= 400 && resp_code < 500)
    return HttpPallet[ HTTP_4xx ];
  if (resp_code >= 500)
    return HttpPallet[ HTTP_5xx ];
  return HttpPallet[ HTTP_NOCOLOR ];
}

static inline void
__print_stats_fuzz (RequestContext *ctx)
{
  /* Wiping the line out of the progress-bar stuff */
  if (opt.Printf.lineclear)
    fprintf (opt.streamout, CLEAN_LINE ());

  if (1 >= opt.words_len)
    {
      int m, n, margin;
#ifndef __ANDROID__ /* Android does not implement %n printf */
      #define __FMT__ "%n%s%n"
      #define __ARG__ &m, ctx->FUZZ[0], &n
#else
      #define __FMT__ "%s"
      #define __ARG__ ctx->FUZZ[0]
      m = 0, n = 0; /* just ignore margins */
#endif /* __ANDROID__ */

      if (opt.Printf.color)
        fprintf (opt.streamout,
                 COLOR_FMT( __FMT__ ),
                 COLOR_ARG( colorof_ctx(ctx), __ARG__ ));
      else
        fprintf ( opt.streamout, __FMT__, __ARG__ );

#undef __FMT__
#undef __ARG__
      if ((margin = PRINT_MARGIN - n + m) > 0)
        fprintf (opt.streamout, "%*s", margin, "");
      else
        fprintf (opt.streamout, "\n%*s", PRINT_MARGIN, "");
    }
  else /* Multiple FUZZ keywords provided */
    {
      if (opt.Printf.color)
        fprintf (opt.streamout, "\n " COLOR_FMT("* FUZZ") " = [",
                 COLOR_ARG(colorof_ctx (ctx)));
      else
        fprintf (opt.streamout, "\n * FUZZ = [");
      Fprintarr (opt.streamout, "'%s'", ctx->FUZZ, opt.words_len);
      fprintf (opt.streamout, "]:\n");
    }
}

static inline void
print_stats_context (RequestContext *ctx)
{
  if (CURLE_OK != ctx->stat.ccode)
    {
      __print_stats_fuzz (ctx);
      fprintf (opt.streamout, "[Error: %s, Duration: %dms]\n",
               curl_easy_strerror (ctx->stat.ccode),
               ctx->stat.duration);
      return;
    }

  __print_stats_fuzz (ctx);
  fprintf (opt.streamout, "\
[Status: %-3d,  Size: %d,  Words: %d,  Lines: %d,  Duration: %dms]\n",
           ctx->stat.code,
           ctx->stat.size_bytes,
           ctx->stat.wcount,
           ctx->stat.lcount,
           ctx->stat.duration
           );
}

static inline bool
filter_pass (struct req_stat_t *stat, struct res_filter_t *filters)
{
#define RANGE(x, rng) ((rng).start <= (int)(x) && (int)(x) <= (rng).end)
#define EXCLUDE(cond) if (cond) return false; break;
  da_foreach (filters, i)
    {
      struct res_filter_t *filter = filters + i;
      switch (filter->type)
        {
        case FILTER_CODE:
          EXCLUDE (RANGE (stat->code, filter->range));
        case FILTER_WCOUNT:
          EXCLUDE (RANGE (stat->wcount, filter->range));
        case FILTER_LCOUNT:
          EXCLUDE (RANGE (stat->lcount, filter->range));
        case FILTER_SIZE:
          EXCLUDE (RANGE (stat->size_bytes, filter->range));
        case FILTER_TIME:
          EXCLUDE (RANGE (stat->duration, filter->range));

        case MATCH_CODE:
          EXCLUDE (! RANGE (stat->code, filter->range));
        case MATCH_WCOUNT:
          EXCLUDE (! RANGE (stat->wcount, filter->range));
        case MATCH_LCOUNT:
          EXCLUDE (! RANGE (stat->lcount, filter->range));
        case MATCH_SIZE:
          EXCLUDE (! RANGE (stat->size_bytes, filter->range));
        case MATCH_TIME:
          EXCLUDE (! RANGE (stat->duration, filter->range));
        }
    }
  return true;
#undef EXCLUDE
#undef RANGE
}

static void
handle_response_context (RequestContext *ctx)
{
  long result = 0;
  double duration;
  struct req_stat_t *stat = &ctx->stat;
  struct progress_t *prog = &opt.progress;

  prog->req_count++;
  prog->req_count_dt++;
  if (0 == prog->req_count % prog->progbar_refrate)
    update_progress_bar (prog);

  if (CURLE_OK == ctx->stat.ccode)
    {
      curl_easy_getinfo (ctx->easy_handle, CURLINFO_HTTP_CODE, &result);
      stat->code = (int) result;
      if (stat->size_bytes)
        { /* Starting from 1 */
          stat->lcount++;
          stat->wcount++;
        }
    }
  else
    prog->err_count++;

  curl_easy_getinfo (ctx->easy_handle, CURLINFO_TOTAL_TIME, &duration);
  stat->duration = (uint) (duration * 1000.f);

  if (CURLE_OK != ctx->stat.ccode || filter_pass (stat, opt.filters))
    {
      print_stats_context (ctx);
      update_progress_bar (prog);
    }
}

static inline void
__register_contex (RequestContext *dst)
{
  char **FUZZ = dst->FUZZ;
  FuzzTemplate template = opt.fuzz_template;

  /**
   *  Generating URL
   *  based on opt.fuzz_template.URL
   *
   *  Libcurl automatically copies the URL (on CURLOPT_URL),
   *  so we don't need to duplicate @tmp
   */
  if (opt.fuzz_flag & URL_HASFUZZ)
    {
      FUZZ += fuzz_snprintf (tmp, TMP_CAP, template.URL, FUZZ);
      curl_setopt (dst->easy_handle, CURLOPT_URL, tmp);
    }
  else
    curl_setopt (dst->easy_handle, CURLOPT_URL, template.URL);

  /**
   *  Generating POST body
   *  based on opt.fuzz_template.body
   */
  if (template.body)
    {
      if (opt.fuzz_flag & BODY_HASFUZZ)
        {
          FUZZ += fuzz_snprintf (tmp, TMP_CAP, template.body, FUZZ);
          Strrealloc (dst->request.body, tmp);
          curl_setopt (dst->easy_handle, CURLOPT_POSTFIELDS, dst->request.body);
        }
      else
        curl_setopt (dst->easy_handle, CURLOPT_POSTFIELDS, template.body);
    }

  /**
   *  Generating HTTP headers
   *  based on opt.fuzz_template.headers
   */
  if (template.headers)
    {
      if (opt.fuzz_flag & HEADER_HASFUZZ)
        {
          struct curl_slist **headers = &dst->request.headers;
          if (*headers)
            {
              curl_slist_free_all (*headers);
              *headers = NULL;
            }
          for (struct curl_slist *h = template.headers;
               NULL != h; h = h->next)
            {
              FUZZ += fuzz_snprintf (tmp, TMP_CAP, h->data, FUZZ);
              *headers = curl_slist_append (*headers, tmp);
            }
          curl_setopt (dst->easy_handle, CURLOPT_HTTPHEADER, *headers);
        }
      else
        curl_setopt (dst->easy_handle, CURLOPT_HTTPHEADER, template.headers);
    }
}

void
register_contex (RequestContext *ctx)
{
  CURL *curl = ctx->easy_handle;
  opt.load_next_fuzz (ctx);
  curl_easy_reset (ctx->easy_handle);
  {
    FLG_SET (ctx->flag, CTX_INUSE);
    /* HTTP verb */
    if (opt.verb)
      curl_easy_setopt (curl, CURLOPT_CUSTOMREQUEST, opt.verb);
    /* Ignore certification check */
    curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
    /* Deliver @ctx to curl_fwrite (custom fwrite function) */
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
    /* Timeout */
    curl_easy_setopt (curl, CURLOPT_TIMEOUT_MS, (size_t) opt.ttl);
    curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT_MS, CONN_TTL_MS);
    /* Proxy */
    curl_easy_setopt (curl, CURLOPT_PROXY, opt.proxy);
  }
  __register_contex (ctx);
  curl_multi_add_handle (opt.multi_handle, curl);
}

//-- Progress statistics functions --//
static inline size_t
req_rate (const Progress *prog)
{
  static size_t rate = 0;
  if (0 == prog->dt)
    return rate;
  rate = prog->req_count_dt * 1000 / prog->dt;

  if (0 == rate && prog->req_count_dt)
    return (rate = 1);
  return rate;
}

static void
__update_progress_bar (const Progress *prog)
{
  uint percentage = REQ_PERC (prog);
  uint rate = req_rate (prog);

  fprintf (stderr, CLEAN_LINE ("\
::.   Progress: %d%% [%d/%d]  ::  %d req/sec  ::   Errors: %d   .::"),
           percentage,
           prog->req_count, prog->req_total,
           rate, prog->err_count
  );
}

static void
init_progress (Progress *prog)
{
  prog->req_count = 0;
  prog->req_count_dt = 0;
  gettimeofday (&prog->__t0, NULL);

  /* This makes progress-bar refresh at every 1% of progress */
  prog->progbar_refrate = MAX (1, prog->req_total / 100);
}

static inline void
tick_progress (Progress *prog)
{
#ifdef LIMIT_TIMEOFDAY_CALLS
  static int n = 0;
  if (0 != n) {
    --n;
    return;
  }
#endif /* LIMIT_TIMEOFDAY_CALLS */

  struct timeval t1 = {0};
  gettimeofday (&t1, NULL);

  size_t DT = TV2MS (t1) - TV2MS (prog->__t0);
  if (DT < MIN_RATE_MEASURE_DUR)
    return;

#ifdef LIMIT_TIMEOFDAY_CALLS
  if (DT < MIN_TIMEOFDAY_DUR) {
    n += SKIP_TIMEOFDAY_N;
    return;
  }
#endif /* LIMIT_TIMEOFDAY_CALLS */

  prog->dt = DT;
  if (DT > MAX_RATE_MEASURE_DUR)
    {
      update_progress_bar (prog);
      prog->req_count_dt = 0;
      prog->__t0 = t1;
    }
}

//-- Utility functions --//
static inline int
da_locate_str (char **haystack, const char *needle)
{
  if (! needle)
    return -1;
  da_foreach (haystack, i)
    {
      if (Strcmp (haystack[i], needle))
        return i;
    }
  return -1;
}

static int
register_wlist (const char *path)
{
  Fword *fw = make_fw_from_path (path);
  if (! fw)
    fw = fw_dup (&dummy_fword);
  int idx = da_locate_str (opt.fuzz_template.wlists, path);
  if (-1 == idx)
    da_appd (opt.words, fw);
  else /* We have already opened this file */
    da_appd (opt.words, fw_dup (opt.words[idx]));
  return 0;
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
  while ((s = strstr (s, "FUZZ")))
    n++, s += 4;
  return n;
}

static inline void
opt_append_filter (int type, const char *range)
{
  struct res_filter_t fl;
  const char *p = range;

  fl.type = type;
  fl.range.start = atoi (p);
  p = strchr (p, '-');
  if (p)
    fl.range.end = atoi (p + 1);
  else
    fl.range.end = fl.range.start;

  da_appd (opt.filters, fl);
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
      warnln ("could not open file (%s) -- %s.", path, strerror (errno));
      return NULL;
    }

  if (0 == fw_map (&tmp, fd))
    return fw_dup (&tmp);
  else
    {
      warnln ("could not mmap file (%s).", path);
      close (fd);
      return NULL;
    }

  return NULL;
}

/* Initializes the global Opt, after parsing user options */
static int
init_opt ()
{
  /* Finalizing the HTTP request template */
  set_template (&opt.fuzz_template, FINISH_TEMPLATE, NULL);

  da_idx n = da_sizeof (opt.words);
  if (n == 0)
    {
      opt.eofuzz = true;
      warnln ("cannot continue with no word-list.");
      return EXIT_FAILURE;
    }
  opt.words_len = n;

  /* Initializing request contexts */
  opt.Rqueue.ctxs = ffuc_calloc (opt.Rqueue.len, sizeof (RequestContext));
  for (size_t i = 0; i < opt.Rqueue.len; i++)
    {
      opt.Rqueue.ctxs[i].easy_handle = curl_easy_init();
      int cap_bytes = (n + 1) * sizeof (char *);
      opt.Rqueue.ctxs[i].FUZZ = ffuc_malloc (cap_bytes);
      Memzero (opt.Rqueue.ctxs[i].FUZZ, cap_bytes);
    }
  /* Initialize libcurl & context of requests */
  curl_global_init (CURL_GLOBAL_DEFAULT);
  opt.multi_handle = curl_multi_init ();

  /* Set the default filters if not disabled */
  if (NO_FILTER == opt.filters)
    opt.filters = NULL;
  else if (NULL == opt.filters)
    da_appd (opt.filters, default_filter);

  if (!opt.verb || Strcmp (opt.verb, "GET"))
     {
       if (opt.fuzz_template.body)
         warnln ("sending body in 'GET' request.");
    }

  /* Mode depended initializations */
  switch (opt.mode)
    {
    case MODE_PITCHFORK:
      opt.progress.req_total = 0;
      for (da_idx i=0; i < n; ++i)
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
      for (da_idx i=0; i < n; ++i)
        opt.progress.req_total *= opt.words[i]->total_count;
      opt.load_next_fuzz = __next_fuzz_clusterbomb;
      break;
    }

  if (! isatty (fileno (stderr)))
    opt.progress.progbar_enabled = false;
  if (! isatty (fileno (opt.streamout)))
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

  return EXIT_SUCCESS;
}

static inline int
set_template_wlist (FuzzTemplate *t, enum template_op op, void *param)
{
  char *path = (char *) param;
  switch (op)
    {
    case URL_TEMPLATE:
    case BODY_TEMPLATE:
    case HEADER_TEMPLATE:
      register_wlist (path);
      da_appd (t->wlists, param);
      return 0;

    default:
      break;
    }
  return 1;
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
  static int local_fuzz_count = -1;

  switch (prev_op)
    {
    case URL_TEMPLATE:
    case BODY_TEMPLATE:
    case HEADER_TEMPLATE:
      if (opt.mode == MODE_SINGULAR)
        break;
      if (WLIST_TEMPLATE != op && 0 != local_fuzz_count)
        {
          /* We are not adding a new word-list and the latest
             modified HTTP template doesn't have enough word-lists */
          warnln ("not enough worlists provided for the "
                  "HTTP option '%s', ignoring %d FUZZ keyword%c.",
                  template_name[prev_op],
                  local_fuzz_count,
                  (local_fuzz_count == 1) ? 0 : 's');
          /* Fill the previous template with dummy word-list */
          for (int i=0; i < local_fuzz_count; ++i)
            set_template_wlist (&opt.fuzz_template, prev_op, NULL);
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
        local_fuzz_count = fuzz_count (t->URL);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, URL_HASFUZZ);
      }
      return local_fuzz_count;

    case HEADER_TEMPLATE:
      {
        prev_op = HEADER_TEMPLATE;
        SLIST_APPEND (t->headers, param);
        local_fuzz_count = fuzz_count (param);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, HEADER_HASFUZZ);
      }
      return local_fuzz_count;

    case BODY_TEMPLATE:
      {
        prev_op = BODY_TEMPLATE;
        if (NULL == t->body)
          {
            t->body = strdup (param);
          }
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
        local_fuzz_count = fuzz_count (param);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, BODY_HASFUZZ);
      }
      return local_fuzz_count;

    case WLIST_TEMPLATE:
      {
        if (opt.mode == MODE_SINGULAR)
          {
            if (0 == da_sizeof (opt.words))
              register_wlist (param);
            else
              {
                warnln ("word-list (%s) was ignored -- \
singular mode, only accepts one word-list", param);
                return -1;
              }
          }
        else
          {
            if (local_fuzz_count <= 0)
              warnln ("unexpected word-list (%s) was ignored.", param);
            else
              {
                /* prev_op indicates the latest modified HTTP option */
                set_template_wlist (&opt.fuzz_template, prev_op, param);
                local_fuzz_count--;
              }
          }
      }
      return 0;

    case FINISH_TEMPLATE:
      if (0 != local_fuzz_count)
        return 1;
      return 0;
    }

  return 0;
}

void
cleanup (int c, void *p)
{
  UNUSED (c), UNUSED (p);
#ifndef SKIP_FREE
  /* Libcurl cleanup */
  if (NULL != opt.Rqueue.ctxs)
    {
      for (size_t i = 0; i < opt.Rqueue.len; i++)
        {
          RequestContext *ctx = &opt.Rqueue.ctxs[i];
          curl_easy_cleanup (ctx->easy_handle);
          safe_free (ctx->request.body);
          curl_slist_free_all (ctx->request.headers);
        }
    }
  curl_multi_cleanup (opt.multi_handle);
  curl_global_cleanup ();
  /* Opt cleanup */
  safe_free (opt.Rqueue.ctxs);
  da_free (opt.filters);
  da_foreach (opt.words, i)
    {
      fw_free (opt.words[i]);
    }
  /* Template cleanup */
  da_free (opt.fuzz_template.wlists);
#endif /* SKIP_FREE */
}

void
help ()
{
  fprintf (stdout, "\
%s v%s - ffuf written in C\n\
Usage:  ffuc [OPTIONS] [HTTP_OPTION [WORDLIST]...]...\n\
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
     clusterbomb  (default),  pitchfork,  singular\n\
     see the documentation for more details.\n\
", PROG_NAME, PROG_VERSION);
}

int
parse_args (int argc, char **argv)
{
  int idx, flag;
  char *last_wlist = NULL;

  idx = 0, optind = 1;
  while ((flag = getopt_long (argc,argv, lopt_str, lopts, &idx)) != -1)
    {
      switch (flag)
        {
        case 'h':
          help ();
          return SHOULD_EXIT;
        case 'm':
          if (strcasestr ("pitchfork", optarg))
            opt.mode = MODE_PITCHFORK;
          else if (strcasestr ("singular", optarg))
            opt.mode = MODE_SINGULAR;
          else if (strcasestr ("clusterbomb", optarg) || strcasestr ("cluster-bomb", optarg))
            opt.mode = MODE_CLUSTERBOMB;
          else
            warnln ("invalid mode `%s` was ignored", optarg);
          break;
        }
    }

  idx = 0, optind = 1;
  while ((flag = getopt_long (argc,argv, lopt_str,lopts, &idx)) != -1)
    {
      switch (flag)
        {
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
            if (0 != d2)
              srand (time (NULL)); /* random delay */
            else
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
          else warnln ("filter and match is disabled.");
        case '0':
          AddFilter (FILTER_CODE);
          break;
        case '1':
          AddFilter (FILTER_SIZE);
          break;
        case '2':
          AddFilter (FILTER_WCOUNT);
          break;
        case '3':
          AddFilter (FILTER_LCOUNT);
          break;
        case '9':
          AddFilter (MATCH_CODE);
          break;
        case '8':
          AddFilter (MATCH_SIZE);
          break;
        case '7':
          AddFilter (MATCH_WCOUNT);
          break;
        case '6':
          AddFilter (MATCH_LCOUNT);
          break;
        case 'A':
          if (opt.filters && NO_FILTER != opt.filters)
            warnln ("disable filters along with filter options.");
          else
            opt.filters = NO_FILTER;
          break;
#undef AddFilter

        default:
          break;
        }
    }

  if (!opt.fuzz_template.URL)
    {
      warnln ("no URL provided (use -u <URL>).");
      return EXIT_FAILURE;
    }
#ifndef DO_NOT_FIX_NO_FUZZ
  /* No FUZZ keyword */
  if (! HAS_FLAG (opt.fuzz_flag, URL_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, BODY_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, HEADER_HASFUZZ))
    {
      if (last_wlist)
        {
          warnln ("\
no FUZZ keyword found, fuzzing tail of URL (with %s).", last_wlist);
          /* TODO: prevent double slash in URL before FUZZ */
          snprintf (tmp, TMP_CAP, "%s/FUZZ", opt.fuzz_template.URL);
          set_template (&opt.fuzz_template, URL_TEMPLATE, tmp);
          set_template (&opt.fuzz_template, WLIST_TEMPLATE, last_wlist);
        }
      else
        {
          warnln ("nothing to do, exiting.");
          return SHOULD_EXIT;
        }
    }
#endif /* DO_NOT_FIX_NO_FUZZ */
  return EXIT_SUCCESS;
}


int
main (int argc, char **argv)
{
#define Return(n) return (n == SHOULD_EXIT) ? EXIT_SUCCESS : EXIT_FAILURE

  int ret;
  set_program_name (PROG_NAME);
  on_exit (cleanup, NULL);

  opt = (struct Opt) {
    .ttl = DEFAULT_TTL_MS,
    .mode = MODE_DEFAULT,
    .Rqueue.len = DEFAULT_REQ_COUNT,
    .max_rate = MAX_REQ_RATE,
    .streamout = stdout,
    .words = da_new (Fword *),
    .progress.progbar_enabled = true,
#ifndef NO_DEFAULT_COLOR
    .color_enabled = true,
#endif /* NO_DEFAULT_COLOR */
  };

  /* Parse cmdline arguments & Initialize opt */
  if ((ret = parse_args (argc, argv)))
    Return (ret);
  if ((ret = init_opt ()))
    Return (ret);

  /**
   *  Main Loop
   */
  CURLMsg *msg;
  RequestContext *ctx;
  size_t rate=0, avg_rate=0;
  int numfds, res, still_running;

  init_progress (&opt.progress);
  do {
    /* Find a free context (If there is any) and register it */
    while (!opt.eofuzz && opt.Rqueue.waiting < opt.Rqueue.len)
      {
        rate = req_rate (&opt.progress);
        if (opt.max_rate <= rate)
          break;

        if ((ctx = lookup_free_handle (opt.Rqueue.ctxs, opt.Rqueue.len)))
          { /* Registering the context */
            opt.Rqueue.waiting++;
            register_contex (ctx);
            range_usleep (opt.Rqueue.delay_us);
          }
      }

    curl_multi_perform (opt.multi_handle, &still_running);
    curl_multi_wait (opt.multi_handle, NULL, 0, POLL_TTL_MS, &numfds);

    while ((msg = curl_multi_info_read (opt.multi_handle, &res)))
      {
        CURL *completed_handle = msg->easy_handle;
        RequestContext *ctx = lookup_handle (completed_handle,
                                             opt.Rqueue.ctxs, opt.Rqueue.len);
        assert (NULL != ctx && "Broken Logic!!\n\
Completed easy_handle doesn't have request context.\n");

        if (CURLMSG_DONE == msg->msg)
          {
            ctx->stat.ccode = msg->data.result;
            handle_response_context (ctx);
            /* Release the completed context */
            context_reset (ctx);
            opt.Rqueue.waiting--;
            avg_rate += rate, avg_rate /= 2;
          }
      }

    tick_progress (&opt.progress);
  }
  while (still_running > 0 || !opt.eofuzz);

  end_progress_bar (&opt.progress);
  if (opt.verbose)
    {
      warnln ("Total requests: %d, Errors: %d, at ~%zu req/sec.",
              opt.progress.req_total, opt.progress.err_count, avg_rate);
    }
  return EXIT_SUCCESS;
}
