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

    -- Not Completed --

    Compilation:
      cc -ggdb -O3 -Wall -Wextra -Werror \
         -I ../libs/ \
         ffuc.c -o ffuc -lcurl

    Options:
      -D_DEBUG:  print debug information
 **/
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <curl/curl.h>

#define DYNA_IMPLEMENTATION
#include "dyna.h"

#define CLI_IMPLEMENTATION
#include "clistd.h"
#include <getopt.h>

#define PROG_NAME "FFuc"
#define PROG_VERSION "1.2-dev"

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
# define MAX_REQ_RATE 2048
#endif

/* Default delta time, for measuring speed */
#ifndef DELTA_T
# define DELTA_T 1
#endif

/* Default connection/read timeuot */
#ifndef CONN_TTL_MS
# define CONN_TTL_MS 10000
#endif
#ifndef DEFAULT_TTL_MS
# define DEFAULT_TTL_MS 10000
#endif

/* Poll timeout */
#ifndef POLL_TTL_MS
# define POLL_TTL_MS 1000
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

const char *lopt_cstr = "m:w:" "T:R:t:p:" "u:H:d:" "vh";
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

    /* Common options */
    {"word",                required_argument, NULL, 'w'},
    {"word-list",           required_argument, NULL, 'w'},
    {"mode",                required_argument, NULL, 'm'},
    {"rate",                required_argument, NULL, 'R'},
    {"max_rate",            required_argument, NULL, 'R'},
    {"timeout",             required_argument, NULL, 'T'},
    {"delay",               required_argument, NULL, 'p'},
    {"help",                no_argument,       NULL, 'h'},
    {"verbose",             no_argument,       NULL, 'v'},
    /* End of options */
    {NULL,                  0,                 NULL,  0 },
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
     *     O( Max( len(wlist 1), ..., len(wlist N) ) )
     * Clusterbomb:
     *   All combinations of word-list(s)
     *     O( len(wlist 1) x ... x len(wlist N) )
     */
    MODE_CLUSTERBOMB  = 0,
    MODE_PITCHFORK    = 1,
    MODE_SINGULAR     = 2,
    MODE_DEFAULT = MODE_CLUSTERBOMB,

    /**
     *  Internal (used in opt.fuzz_flag)
     *  FUZZ keyword substitution enabled flags
     */
    URL_HASFUZZ       = (1 << 1),
    BODY_HASFUZZ      = (1 << 2),
    HEADER_HASFUZZ    = (1 << 3),

    /* Internal (used in RequestContext.flag) */
    CTX_FREE          = 0,
    CTX_INUSE         = 1,
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
     *   Aappend to the request body; It handles `&` automatically
     *   (e.g. "key1=val1&key2=val2" or "&key=val")
     *
     * HEADER_TEMPLATE:
     *   Append Http header (e.g. "Header: value")
     *
     * WLIST_TEMPLATE:
     *   To append a wordlist file path
     *
     * FINISH_TEMPLATE:
     *   set_template MUST be called with this at the end
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
  uint wcount; /* Count of words */
  uint lcount; /* Count of lines */
  uint size_bytes;

  int code; /* HTTP response code */
  int duration; /* Total time to response */
};

typedef struct progress_t
{
  uint req_total;
  uint req_count; /* Count of sent requests */
  uint err_count; /* Count of errors */

  uint req_count_dt; /* Requests in delta time */
  time_t t0;
} Progress;

/* Calculate the current request rate (req/second) uint_t */
#define REQ_RATE(prog) ((prog)->req_count_dt / DELTA_T)

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
   *  It must contain opt.total_fuzz_count elements.
   */
  char **FUZZ;

  struct request_t
  {
    /* As libcurl keeps a copy of the URL,
       we don't need to keep track of it */
    char *body;
    struct curl_slist *headers;
  } request;

} RequestContext;

typedef struct
{
  /* xxx_wlists means wordlist file paths */
  char *URL;
  char **URL_wlists;

  char *body;
  char **body_wlists;

  struct curl_slist *headers;
  char **header_wlists;
} FuzzTemplate;

/**
 *  Fuzz word
 *
 *  We don't load the entire word-list files
 *  into memory, instead mmap them and iterate
 *  through them using fw_next and fw_get.
 */
typedef struct
{
  /**
   *  `fw_get` returns the current word. The result is NOT
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
static inline Fword *make_fw_from_path (char *path);

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

/* Find the next word in the word-list */
char *fw_next (Fword *fw);
/* Fword get the current word of word-list */
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
 * tick_progress:
 *   Advances the timer and resets the progress if needed
 *   It MUST be called at the end of the main loop
 */
void init_progress (Progress *prog);
static inline void tick_progress (Progress *prog);

/**
 *  Sleeps for a random duration in microseconds
 *  within the specified range @range
 *
 *  If range[1] is less than or equal to range[0], the functio
 *  it will sleep for exactly range[0] microseconds
 */
static inline void range_usleep (useconds_t range[2]);

/**
 *  Template setter functions
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
  ((s1 == NULL || s2 == NULL) ? -1 : strcmp (s1, s2))

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
  int verbose;
  int max_rate; /* Max request rate (req/sec) */
  FILE *streamout;
  FuzzTemplate fuzz_template;
  struct res_filter_t *filters; /* Dynamic array */

  /* Internals */
  int fuzz_flag;
  CURLM *multi_handle;
  struct progress_t progress;

  Fword **words; /* Dynamic array */
  int total_fuzz_count; /* Length of @words */

  struct request_queue_t
  {
    RequestContext *ctxs; /* Static array */
    size_t len; /* Length of @ctxs */
    size_t waiting; /* number of used elements */
    useconds_t delay_us[2]; /* delay range, microseconds */
  } Rqueue;

  /* Next FUZZ loader and */
  void (*load_next_fuzz) (RequestContext *ctx);
  bool should_end; /* End of load_next_fuzz */
  union fuzz_context_t
    {
      /**
       *  Pitchfork needs to know which word-list
       *  is the longest one.
       */
      Fword *longest;
      void *__dummy; /* Unused */
    } fuzz_ctx;
};
struct Opt opt = {0};

#define REGISTER_WLIST(path) do {               \
    Fword *fw = make_fw_from_path (path);       \
    da_appd (opt.words, fw);                    \
  } while (0)

/**
 *  We only require request statistics, so we pass
 *  this function as CURLOPT_WRITEFUNCTION to libcurl
 *  Libcurl sets @optr to the corresponding request context
 */
size_t
curl_fwrite (void *ptr, size_t size, size_t nmemb, void *optr)
{
  char *data = (char *) ptr;
  int len = (int)(size * nmemb);
  RequestContext *ctx = (RequestContext *) optr;

  ctx->stat.size_bytes += len; /* Update size */
  for (int i=0; i < len; ++i)
    {
      /* Update word count and line count */
      if (data[i] == ' ')
        ctx->stat.wcount++;
      else if (data[i] < ' ')
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
 * return:
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
          if (*FUZZ)
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

  da_idx i=1, N = opt.total_fuzz_count;
  for (; i < N; ++i)
    ctx->FUZZ[i] = ctx->FUZZ[0];
  ctx->FUZZ[i] = NULL;

  if (fw_eof (fw))
    opt.should_end = true;

  fprintd ("singular:  [0-%ld]->`%s`\n", N, tmp);
  fw_next (fw);
}

static void
__next_fuzz_pitchfork (RequestContext *ctx)
{
  Fword *fw;
  size_t N = opt.total_fuzz_count;

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

  if (fw_bof (opt.fuzz_ctx.longest))
    opt.should_end = true;
}

static void
__next_fuzz_clusterbomb (RequestContext *ctx)
{
  Fword *fw = NULL;
  size_t N = opt.total_fuzz_count;

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
    opt.should_end = true;
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

static inline void
__print_stats_fuzz (RequestContext *ctx)
{
  if (opt.total_fuzz_count == 1)
    {
      fprintf (opt.streamout, "%s \t\t\t ", ctx->FUZZ[0]);
    }
  else
    {
      fprintf (opt.streamout, "\n* FUZZ = [");
      Fprintarr (opt.streamout, "'%s'", ctx->FUZZ, opt.total_fuzz_count);
      fprintf (opt.streamout, "]:\n  ");
    }
}

static inline void
print_stats_context (RequestContext *ctx, CURLcode res_code)
{
  if (CURLE_OK != res_code)
    {
      if (opt.verbose)
        {
          __print_stats_fuzz (ctx);
          fprintf (opt.streamout, "\
[Error: %s, Size: %d, Words: %d, Lines: %d, Duration: %dms]\n",
                   curl_easy_strerror (res_code),
                   ctx->stat.size_bytes,
                   ctx->stat.wcount,
                   ctx->stat.lcount,
                   ctx->stat.duration);
        }
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
handle_response_context (RequestContext *ctx, CURLcode res_code)
{
  long result = 0;
  double duration;
  struct req_stat_t *stat = &ctx->stat;

  opt.progress.req_count++;
  opt.progress.req_count_dt++;
  if (res_code == CURLE_OK)
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
    opt.progress.err_count++;

  curl_easy_getinfo (ctx->easy_handle, CURLINFO_TOTAL_TIME, &duration);
  stat->duration = (int) (duration * 1000.f);

  if (CURLE_OK != res_code || filter_pass (stat, opt.filters))
    print_stats_context (ctx, res_code);
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
      curl_easy_setopt (dst->easy_handle, CURLOPT_URL, tmp);
    }
  else
    {
      curl_easy_setopt (dst->easy_handle, CURLOPT_URL, template.URL);
    }

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
          curl_easy_setopt (dst->easy_handle, CURLOPT_POSTFIELDS,
                            dst->request.body);
        }
      else
        {
          curl_easy_setopt (dst->easy_handle, CURLOPT_POSTFIELDS,
                            template.body);
        }
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
          curl_easy_setopt (dst->easy_handle, CURLOPT_HTTPHEADER, *headers);
        }
      else
        {
          curl_easy_setopt (dst->easy_handle, CURLOPT_HTTPHEADER,
                            template.headers);
        }
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
    /* Ignore certification check */
    curl_easy_setopt (curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt (curl, CURLOPT_SSL_VERIFYHOST, 0L);
    /* Deliver @ctx to curl_fwrite (custom fwrite function) */
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, curl_fwrite);
    /* Timeout */
    curl_easy_setopt (curl, CURLOPT_TIMEOUT_MS, opt.ttl);
    curl_easy_setopt (curl, CURLOPT_CONNECTTIMEOUT_MS, CONN_TTL_MS);
  }
  __register_contex (ctx);
  curl_multi_add_handle (opt.multi_handle, curl);
}

//-- Progress statistics functions --//
void
init_progress (Progress *prog)
{
  time (&prog->t0); // initialize t0
  prog->req_count = 0;
  prog->req_count_dt = 0;
}

static inline void
tick_progress (Progress *prog)
{
  time_t t = time (NULL);
  if (t - prog->t0 >= DELTA_T)
    {
      prog->req_count_dt = 0;
      prog->t0 = t;
    }
}

//-- Utility functions --//
static inline void
range_usleep (useconds_t range[2])
{
  int duration;
  if (range[0])
    {
      duration = range[1] - range[0];
      if (duration > 0)
        {
          /* Sleep a random amount of time */
          int r = rand () % (duration + 1);
          usleep (range[0] + r);
        }
      else
        {
          usleep (range[0]);
        }
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

static inline Fword *
make_fw_from_path (char *path)
{
  int fd;
  Fword tmp = {0};
  /* NULL path is expected, It means,
     the user hasn't provided enough wordlists */
  if (! path)
    goto __return_dummy;
  fd = open (path, O_RDONLY);
  if (fd < 0)
    {
      warnln ("could not open file (%s) -- %s.", path, strerror (errno));
      goto __return_dummy;
    }

  if (0 == fw_map (&tmp, fd))
    return fw_dup (&tmp);
  else
    {
      warnln ("could not mmap file (%s).", path);
      close (fd);
      goto __return_dummy;
    }

 __return_dummy:
  return fw_dup (&dummy_fword);
}

/**
 *  Initializes wordlists (open and mmap)
 *  This function supports the clusterbomb and pitchfork modes
 *  For singular mode, use the REGISTER_WLIST macro
 *
 *  TODO: Can we prevent remapping already mmaped files
 */
static void
init_wordlists ()
{
  char *path;
#define DO(paths) if (paths) {                      \
    da_foreach (paths, i) {                         \
      path = paths[i];                              \
      fprintd ("register word-list: '%s' -> %s\n",  \
               (path) ? path : "(DUMMY)", #paths);  \
      REGISTER_WLIST (path);                        \
    }}
  {
    FuzzTemplate *t = &opt.fuzz_template;
    DO (t->URL_wlists);
    DO (t->body_wlists);
    DO (t->header_wlists);
  }
#undef DO
}

static int
init_opt ()
{
  /* Finalizing the HTTP request template */
  set_template (&opt.fuzz_template, FINISH_TEMPLATE, NULL);
  /* Open and map wordlists */
  init_wordlists ();
  uint n = da_sizeof (opt.words);
  if (n == 0)
    {
      opt.should_end = true;
      warnln ("cannot continue with no word-list.");
      return 1;
    }
  opt.total_fuzz_count = n;

  /* Initialize requests context */
  opt.Rqueue.ctxs = ffuc_calloc (opt.Rqueue.len, sizeof (RequestContext));
   for (size_t i = 0; i < opt.Rqueue.len; i++)
     {
       opt.Rqueue.ctxs[i].easy_handle = curl_easy_init();
       int cap_bytes = (n + 1) * sizeof (char *);
       opt.Rqueue.ctxs[i].FUZZ = ffuc_malloc (cap_bytes);
       Memzero (opt.Rqueue.ctxs[i].FUZZ, cap_bytes);
     }

   if (NULL == opt.filters)
     {
       da_appd (opt.filters, default_filter);
     }

   switch (opt.mode)
    {
    case MODE_PITCHFORK:
      /* Find the longest wordlist, needed by pitchfork */
      opt.fuzz_ctx.longest = opt.words[0];
      for (size_t i=0; i < n; ++i)
        {
          if (opt.words[i]->total_count > opt.fuzz_ctx.longest->total_count)
            {
              opt.fuzz_ctx.longest = opt.words[i];
            }
        }
      opt.progress.req_total = opt.fuzz_ctx.longest->total_count;
      opt.load_next_fuzz = __next_fuzz_pitchfork;
      break;

    case MODE_SINGULAR:
      opt.progress.req_total = opt.words[0]->total_count;
      opt.load_next_fuzz = __next_fuzz_singular;
      break;

    case MODE_CLUSTERBOMB:
      opt.progress.req_total = 1;
      for (size_t i=0; i < n; ++i)
        opt.progress.req_total *= opt.words[i]->total_count;
      opt.load_next_fuzz = __next_fuzz_clusterbomb;
      break;
    } 

  return 0;
}

static inline int
set_template_wlist (FuzzTemplate *t, enum template_op op, void *param)
{
  char *path = (char *) param;
  switch (op)
    {
    case URL_TEMPLATE:
      da_appd (t->URL_wlists, path);
      return 0;
    case BODY_TEMPLATE:
      da_appd (t->body_wlists, path);
      return 0;
    case HEADER_TEMPLATE:
      da_appd (t->header_wlists, path);
      return 0;
    default:
      return 1;
    }
}

int
set_template (FuzzTemplate *t, enum template_op op, void *param)
{
  char *s = (char *) param;
  static int prev_op = -1;
  static int local_fuzz_count = -1;

  switch (prev_op)
    {
    case URL_TEMPLATE:
    case BODY_TEMPLATE:
    case HEADER_TEMPLATE:
      if (WLIST_TEMPLATE != op && 0 != local_fuzz_count)
        {
          /* We are not adding a new wordlist and
             the previous http template doesn't have enough wordlists */
          warnln ("not enough worlists provided for the "
                  "HTTP option '%s', ignoring %d FUZZ keyword%c.",
                  template_name[prev_op],
                  local_fuzz_count,
                  (local_fuzz_count == 1) ? 0 : 's');

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
        Strrealloc (t->URL, s);
        local_fuzz_count = fuzz_count (t->URL);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, URL_HASFUZZ);
      }
      return local_fuzz_count;

    case HEADER_TEMPLATE:
      {
        prev_op = HEADER_TEMPLATE;
        SLIST_APPEND (t->headers, s);
        local_fuzz_count = fuzz_count (s);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, HEADER_HASFUZZ);
      }
      return local_fuzz_count;

    case BODY_TEMPLATE:
      {
        prev_op = BODY_TEMPLATE;
        if (NULL == t->body)
          {
            t->body = strdup (s);
          }
        else
          {
            size_t len = Strlen (t->body);
            if ('&' != s[0] && '&' != t->body[len - 1])
              {
                /* We need an extra `&` */
                Realloc (t->body, len + Strlen (s) + 2);
                char *p = t->body + len;
                *p = '&';
                strcpy (p + 1, s);
              }
            else
              {
                Realloc (t->body, len + Strlen (s) + 1);
                strcpy (t->body + len , s);
              }
          }
        local_fuzz_count = fuzz_count (s);
        if (local_fuzz_count)
          FLG_SET (opt.fuzz_flag, BODY_HASFUZZ);
      }
      return local_fuzz_count;

    case WLIST_TEMPLATE:
      {
        if (opt.mode == MODE_SINGULAR)
          {
            if (da_sizeof (opt.words) <= 0)
              REGISTER_WLIST (s);
            else
              {
                warnln ("word-list (%s) was ignored -- singular mode "
                        "only accepts one word-list", s);
                return -1;
              }
          }
        else
          {
            if (local_fuzz_count <= 0)
              warnln ("unexpected word-list (%s) was ignored.", s);
            else
              {
                /* prev_op indicates the latest http option that was set */
                set_template_wlist (&opt.fuzz_template, prev_op, s);
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
          // curl_multi_remove_handle (opt.multi_handle, ctx->easy_handle);
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
  da_free (opt.fuzz_template.URL_wlists);
  da_free (opt.fuzz_template.body_wlists);
  da_free (opt.fuzz_template.header_wlists);
#endif /* SKIP_FREE */
}

void
help ()
{
  fprintf (stdout, "\
%s v%s - ffuf written in C\n\
", PROG_NAME, PROG_VERSION);
}

/**
 * @return: positive -> should exit
 *          negative -> error
 *              zero -> success
 */
int
parse_args (int argc, char **argv)
{
  char *last_wlist = NULL;
  while (1)
    {
      int idx = 0;
      int flag = getopt_long (argc, argv, lopt_cstr, lopts, &idx);
      if (flag == -1)
        {
          /* End of Options */
          break;
        }
      switch (flag)
        {
        case 'h':
          help ();
          return 1;
        case 'v':
          opt.verbose = true;
          break;

        case 'w':
          last_wlist = optarg;
          set_template (&opt.fuzz_template, WLIST_TEMPLATE, optarg);
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
            sscanf (optarg, "%u-%u",
                    &opt.Rqueue.delay_us[0],
                    &opt.Rqueue.delay_us[1]);

            if ((int) opt.Rqueue.delay_us[0] < 0)
              {
                opt.Rqueue.delay_us[0] = 0;
                warnln ("invalid TTL was ignored.");
              }
            if (opt.Rqueue.delay_us[1] != 0)
              {
                srand (time (NULL));
                if ((int) opt.Rqueue.delay_us[1] <
                    (int) opt.Rqueue.delay_us[0])
                  {
                    opt.Rqueue.delay_us[1] = 0;
                    warnln ("invalid TTL range was fixed up.");
                  }
              }
            if (strchr (optarg, 's'))
              {
                /* Convert to second */
                opt.Rqueue.delay_us[0] *= 1000000;
                opt.Rqueue.delay_us[1] *= 1000000;
              }
            else
              {
                /* Convert to millisecond */
                opt.Rqueue.delay_us[0] *= 1000;
                opt.Rqueue.delay_us[1] *= 1000;
              }
          }
          break;

        case 'R':
          opt.max_rate = atoi (optarg);
          break;

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

        case 'm':
          /* Type the first letter OR complete name */
          if ('p' == *optarg || 0 == strcmp ("pitchfork", optarg))
            opt.mode = MODE_PITCHFORK;
          else if ('s' == *optarg || 0 == strcmp ("singular", optarg))
            opt.mode = MODE_SINGULAR;
          else if ('c' == *optarg || 0 == strcmp ("clusterbomb", optarg))
            opt.mode = MODE_CLUSTERBOMB;
          else
            warnln ("invalid mode `%s` was ignored", optarg);
          break;

#define AddFilter(type) opt_append_filter (type, optarg); break;
        case '0':
          AddFilter (FILTER_CODE);
        case '1':
          AddFilter (FILTER_SIZE);
        case '2':
          AddFilter (FILTER_WCOUNT);
        case '3':
          AddFilter (FILTER_LCOUNT);
        case '9':
          AddFilter (MATCH_CODE);
        case '8':
          AddFilter (MATCH_SIZE);
        case '7':
          AddFilter (MATCH_WCOUNT);
        case '6':
          AddFilter (MATCH_LCOUNT);
#undef AddFilter

        default:
          break;
        }
    }

  if (!opt.fuzz_template.URL)
    {
      warnln ("no URL provided (use -u <URL>).");
      return -1;
    }
  /* No FUZZ keyword */
  if (! HAS_FLAG (opt.fuzz_flag, URL_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, BODY_HASFUZZ) &&
      ! HAS_FLAG (opt.fuzz_flag, HEADER_HASFUZZ))
    {
      if (last_wlist)
        {
          warnln ("no FUZZ keyword found, fuzzing tail of URL.");
          snprintf (tmp, TMP_CAP, "%s/FUZZ", opt.fuzz_template.URL);
          set_template (&opt.fuzz_template, URL_TEMPLATE, tmp);
          set_template (&opt.fuzz_template, WLIST_TEMPLATE, last_wlist);
        }
      else
        {
          warnln ("nothing to do, exiting.");
          return 1;
        }
    }
  return 0;
}

void
pre_init_opt ()
{
  /* Set default values */
  opt.ttl = DEFAULT_TTL_MS;
  opt.mode = MODE_DEFAULT;
  opt.Rqueue.len = DEFAULT_REQ_COUNT;
  opt.max_rate = MAX_REQ_RATE;
  opt.streamout = stdout;
  opt.words = da_new (Fword *);
  /* Initialize libcurl & context of requests */
  curl_global_init (CURL_GLOBAL_DEFAULT);
  opt.multi_handle = curl_multi_init ();
}

int
main (int argc, char **argv)
{
  int ret;
  set_program_name (PROG_NAME);
  on_exit (cleanup, NULL);

  /* Parse cmdline arguments & Initialize opt */
  pre_init_opt ();
  ret = parse_args (argc, argv);
  if (0 != ret)
    return (ret < 0) ? EXIT_FAILURE : EXIT_SUCCESS;
  ret = init_opt ();
  if (0 != ret)
    return EXIT_FAILURE;
  init_progress (&opt.progress);

  /**
   *  Main Loop
   */
  CURLMsg *msg;
  RequestContext *ctx;
  int numfds, res, still_running;
  do {
    /* Find a free context (If there is any) and register it */
    while (!opt.should_end && opt.Rqueue.waiting < opt.Rqueue.len)
      {
        if ((uint) opt.max_rate <= REQ_RATE (&opt.progress))
          break;
        if ((ctx = lookup_free_handle (opt.Rqueue.ctxs, opt.Rqueue.len)))
          {
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
        assert (NULL != ctx && "Broken Logic!!\n"
                "Completed easy_handle doesn't have request context.\n");

        if (CURLMSG_DONE == msg->msg)
          {
            handle_response_context (ctx, msg->data.result);
            /* Release the completed context */
            context_reset (ctx);
            opt.Rqueue.waiting--;
          }
      }

    tick_progress (&opt.progress);
  }
  while (still_running > 0 || !opt.should_end);

  if (opt.verbose || opt.progress.err_count)
    warnln ("Total requests: %d,  Errors: %u (%u%%).",
            opt.progress.req_count,
            opt.progress.err_count,
            opt.progress.err_count * 100 / opt.progress.req_count);
  return 0;
}
