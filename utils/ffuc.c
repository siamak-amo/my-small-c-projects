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
#define PROG_VERSION "1.0-dev"

#define SLIST_APPEND(list, val) \
  list = curl_slist_append (list, val)

#ifndef TMP_CAP
#  define TMP_CAP 1024 /* bytes */
#endif
static char tmp[TMP_CAP];

#define NOP ((void) NULL)
#define UNUSED(x) (void)(x)
#define MIN(a,b) ((a < b) ? (a) : (b))

#define FLG_SET(dst, flg) (dst |= flg)
#define FLG_UNSET(dst, flg) (dst &= ~(flg))

const struct option lopts[] =
  {
    {"word",                required_argument, NULL, 'w'},
    {"word-list",           required_argument, NULL, 'w'},

    {"url",                 required_argument, NULL, 'u'},
    {"header",              required_argument, NULL, 'H'},
    {"data",                required_argument, NULL, 'd'},

    {"help",                no_argument,       NULL, 'h'},
    {"version",             no_argument,       NULL, 'v'},

    {NULL,                  0,                 NULL,  0 },
  };

enum ffuc_flag_t
  {
    /**
     *  Initialize request template
     *
     * URL_TEMPLATE:
     *   Set the target URL (e.g. "http(s)://host:port")
     * BODY_TEMPLATE:
     *   Aappend to the request body; It handles `&` automatically
     *   (e.g. "key1=val1&key2=val2" or "&key=val")
     * HEADER_TEMPLATE:
     *   Append Http header (e.g. "Header: value")
     */
    URL_TEMPLATE      = 0,
    BODY_TEMPLATE     = 1,
    HEADER_TEMPLATE   = 2,
    
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
     *  Enable FUZZ keyword substitution flags
     */
    URL_HASFUZZ       = (1 << 1),
    BODY_HASFUZZ      = (1 << 2),
    HEADER_HASFUZZ    = (1 << 3),

    /* Internal (used in RequestContext.flag) */
    CTX_FREE          = 0,
    CTX_INUSE         = 1,
  };

struct stat_t
{
  unsigned int wcount; /* Count of words */
  unsigned int lcount; /* Count of lines */
  unsigned int size_bytes;
};

typedef struct
{
  int flag;
  CURL *easy_handle;

  /* Statistics of the request */
  struct stat_t stat;
  
  /**
   *  All 'FUZZ' keywords within @opt.fuzz_template
   *  (URL, POST body, HTTP headers respectively)
   *  will be substituted with elements of this array.
   *  It must contain @opt.fuzz_count elements.
   */
  char **FUZZ;

  struct request_t
  {
    /* As libcurl keeps a copy of the URL,
       we don't need to keep track of it */
    char *post_body;
    struct curl_slist *http_headers;
  } request;

} RequestContext;

typedef struct
{
  char *URL;
  char *post_body;
  struct curl_slist *headers;
} FuzzTemplate;

/**
 *  Fuzz word
 *
 *  We don't load the entire word-list files
 *  into the memory, instead mmap them and iterate
 *  on them using fw_next and fw_get.
 */
typedef struct
{
  /**
   *  The fw_get macro can be used to get the current word
   *  The result is *NOT* null-terminated ('\0')
   *  @len is the result's length
   *
   *  FW_FORMAT and FW_ARG macros can be used for printf
   */
  char *str;
  uint len; /* Length of the current word */

  /**
   *  Index of the current word and total words
   *
   *  Each fw_next call, increments @idx by 1,
   *  after @total_count resets to 0
   */
  uint idx, total_count;

  /* Internal */
  uint __offset; /* Offset of the current word in @str */
  size_t __str_bytes; /* length of @str in bytes */
} Fword;

/* Fword printf format and arguments */
#define FW_FORMAT "%.*s"
#define FW_ARG(fw) (int)((fw)->len), fw_get (fw)

/* Fword init, copy, duplicate and unmap */
void fw_init (Fword *fw, char *cstr, size_t cstr_len);
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
#define fw_eof(fw) ((fw)->idx + 1 == (fw)->total_count)
#define fw_bof(fw) ((fw)->idx == 0)

/**
 *  Fword functions
 */
void fw_init (Fword *fw, char *cstr, size_t cstr_len);
Fword *fw_dup (const Fword *src);
char *fw_next (Fword *fw);

/**
 *  Strline - StrlineNull functions
 *
 *  These functions return a pointer to the first character
 *  of @cstr in the range [0x00, 0x1F], and NULL on '\0'.
 *
 *  StrlineNull function is like Strline() except that,
 *  at the null-byte(s) and characters in the above range,
 *  it returns a pointer to them, rather than NULL.
 */
static inline char *Strline (const char *cstr);
static inline char *StrlineNull (const char *cstr);

/**
 *  Libcurl handle lookup functions
 *
 * lookup_handle:
 *   Finds @handle in @ctxs array, NULL is not found
 * lookup_free_handle:
 *   Finds the first free to use request context in @ctxs
 */
static inline RequestContext *
lookup_handle (CURL *handle, RequestContext *ctxs, size_t len);
static inline RequestContext *
lookup_free_handle (RequestContext *ctxs, size_t len);

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
#ifndef FFUC_NO_FREE
# define ffuc_free(x) free (x)
# define ffuc_munmap(ptr, len) munmap (ptr, len)
#else
# define ffuc_free NOP
# define ffuc_munmap NOP
#endif

#define safe_free(ptr) \
  if (NULL != ptr) { ffuc_free (ptr); ptr = NULL; }

/**
 *  As the primary bottleneck is the network,
 *  improving memory allocation is not probably
 *  going to enhance performance.
 *  To prevent memory leak, do NOT use ffuc_free & safe_free here
 *  because, Strrealloc is used in reading from word-lists.
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
  size_t concurrent; /* Max number of concurrent requests */
  FuzzTemplate fuzz_template;

  /* Internals */
  int fuzz_flag;
  size_t waiting_reqs;
  size_t fuzz_count; /* Total count of FUZZ keywords */
  CURLM *multi_handle;

  Fword **wlists; /* Dynamic array */
  char **wlist_paths; /* Dynamic array, path of word-lists */
  RequestContext *ctxs; /* Static array, Request contexts */

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

/**
 *  We only require request statistics, so we pass
 *  this function as CURLOPT_WRITEFUNCTION to libcurl.
 *  Libcurl sets @optr to the corresponding request context *
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
  fw->str = cstr;
  fw->__offset = 0, fw->total_count = 0;
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

Fword *
fw_dup (const Fword *src)
{
  Fword *tmp = ffuc_malloc (sizeof (Fword));
  memcpy (tmp, src, sizeof (Fword));
  return tmp;
}

char *
fw_next (Fword *fw)
{
  char *p = fw_get (fw);
  if ('\n' == *p)
    p++;

  char *next = Strline (p);
  if (NULL == next || '\0' == next[1])
    {
      p = fw->str;
      next = Strline (p);
      fw->len = next - p;
      fw->__offset = 0;
      fw->idx = 0;
      return fw->str;
    }
  fw->__offset += (next - p) + 1;
  fw->len = next - p;
  fw->idx++;
  if (fw->idx == fw->total_count)
    fw->idx = 0;
  return p;
}

/**
 *  FUZZ sprintf, substitutes the FUZZ keyword
 *  with the appropriate value from @FUZZ
 *  The tailing element of @FUZZ MUST be NULL
 *
 *  @return: number of consumed elements from @FUZZ
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
void
__next_fuzz_singular (RequestContext *ctx)
{
  Fword *fw = opt.wlists[0];
  snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
  Strrealloc (ctx->FUZZ[0], tmp);

  da_idx i=1, N = da_sizeof (opt.wlists);
  for (; i < N; ++i)
    ctx->FUZZ[i] = ctx->FUZZ[0];
  ctx->FUZZ[i] = NULL;

  if (fw_eof (fw))
    opt.should_end = true;

  fprintd ("singular:  [0-%ld]->`%s`\n", N, tmp);
  fw_next (fw);
}

void
__next_fuzz_pitchfork (RequestContext *ctx)
{
  Fword *fw;
  size_t N = da_sizeof (opt.wlists);
  fprintd ("pitchfork:  ");

  for (size_t i = 0; i < N; ++i)
    {
      fw = opt.wlists[i];
      snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
      Strrealloc (ctx->FUZZ[i], tmp);
      fprintd ("[%d]->`%s`\t", fw->idx, tmp);
    }
  fprintd ("\n");

  for (size_t i = 0; i < N; ++i)
    {
      fw = opt.wlists[i];
      fw_next (fw);
    }
  if (fw_bof (opt.fuzz_ctx.longest))
    opt.should_end = 1;
}

void
__next_fuzz_clusterbomb (RequestContext *ctx)
{
  Fword *fw;
  fprintd ("clusterbomb:  ");
  size_t N = da_sizeof (opt.wlists);

  for (size_t i=0; i<N; ++i)
    {
      fw = opt.wlists[i];
      snprintf (tmp, TMP_CAP, FW_FORMAT, FW_ARG (fw));
      Strrealloc (ctx->FUZZ[i], tmp);
      fprintd ("[%d]->`%s`\t", fw->idx, tmp);
    }
  fprintd ("\n");

  size_t i = 0;
  for (; i < N; )
    {
      fw = opt.wlists[i];
      fw_next (fw);

      if (fw_bof (fw))
        i++;
      else
        break;
    }

  if (i == N)
    opt.should_end = true;
}

//-- RequestContext functions --//
static inline RequestContext *
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

static inline RequestContext *
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
  memset (&ctx->stat, 0, sizeof (struct stat_t));
}

void
print_stats (RequestContext *c)
{
  long http_code = 0;
  double total_time;

  curl_easy_getinfo (c->easy_handle, CURLINFO_HTTP_CODE, &http_code);
  curl_easy_getinfo (c->easy_handle, CURLINFO_TOTAL_TIME, &total_time);
  
  fprintf (stderr,
           "%s \t\t\t [Status: %-3ld,  "
           "Size: %d,  " "Words: %d,  " "Lines: %d,  "
           "Duration: %.0fms]\n",
           c->FUZZ[0], /* TODO: what about the others FUZZs? */
           http_code,
           c->stat.size_bytes,
           c->stat.wcount,
           (c->stat.size_bytes > 0) ? c->stat.lcount + 1 : 0,
           total_time * 1000
           );
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
   *  based on opt.fuzz_template.post_body
   */
  if (template.post_body)
    {
      if (opt.fuzz_flag & BODY_HASFUZZ)
        {
          FUZZ += fuzz_snprintf (tmp, TMP_CAP, template.post_body, FUZZ);
          Strrealloc (dst->request.post_body, tmp);
          curl_easy_setopt (dst->easy_handle, CURLOPT_POSTFIELDS,
                            dst->request.post_body);
        }
      else
        {
          curl_easy_setopt (dst->easy_handle, CURLOPT_POSTFIELDS,
                            template.post_body);
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
          struct curl_slist **headers = &dst->request.http_headers;
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
  opt.load_next_fuzz (ctx);
  curl_easy_reset (ctx->easy_handle);
  {
    FLG_SET (ctx->flag, CTX_INUSE);
    curl_easy_setopt (ctx->easy_handle, /* deliver ctx to curl_fwrite */
                      CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt (ctx->easy_handle, /* custom fwrite function */
                      CURLOPT_WRITEFUNCTION, curl_fwrite);
  }
  __register_contex (ctx);
  curl_multi_add_handle (opt.multi_handle, ctx->easy_handle);
}

//-- Utility functions --//
static inline size_t
fuzz_count (const char *s)
{
  size_t n = 0;
  if (!s)
    return 0;
  while ((s = strstr (s, "FUZZ")))
    n++, s += 4;
  return n;
}

int
is_wlist_open (const char *filepath)
{
  da_foreach (opt.wlist_paths, i)
    {
      if (0 == strcmp (filepath, opt.wlist_paths[i]))
        return i;
    }
  return -1;
}

int
wlmap (int fd, Fword *dst)
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

int
register_wordlist (char *pathname)
{
#define WL_APPD(filepath, fword) do {           \
      da_appd (opt.wlists, fword);              \
      da_appd (opt.wlist_paths, filepath);      \
    } while (0)

  Fword fw = {0};
  int wl_idx = is_wlist_open (pathname);
  if (-1 != wl_idx)
    {
      /* File is already open */
      fw_cpy (&fw, opt.wlists[wl_idx]);
      WL_APPD (pathname, fw_dup (&fw));
    }
  else
    /* New wordlist file, needs fopen and mmap */
    {
      int fd = open (pathname, O_RDONLY);
      if (fd < 0)
        return -fd;
      if (0 == wlmap (fd, &fw))
        {
          WL_APPD (pathname, fw_dup (&fw));
        }
      else
        {
          warnln ("failed to map file `%s`\n", pathname);
          return 1;
        }
    }
  return 0;
#undef WL_APPD
}

int
init_opt ()
{
  if (NULL == opt.fuzz_template.URL)
    {
      warnln ("no URL provided.");
      return -1;
    }

   size_t n =  da_sizeof (opt.wlists);
   if (n == 0)
     {
       opt.should_end = 1;
       warnln ("cannot continue with no word-list");
       return 1;
     }

   /* Initialize requests context */
  opt.ctxs = ffuc_calloc (opt.concurrent, sizeof (RequestContext));
   for (size_t i = 0; i < opt.concurrent; i++)
     {
       opt.ctxs[i].easy_handle = curl_easy_init();
       int cap_bytes = (opt.fuzz_count + 1) * sizeof (char *);
       opt.ctxs[i].FUZZ = ffuc_malloc (cap_bytes);
       Memzero (opt.ctxs[i].FUZZ, cap_bytes);
     }

   switch (opt.mode)
    {
    case MODE_PITCHFORK:
      if (opt.fuzz_count != n)
        {
          warnln ("expected %ld word-list(s), provided %ld", opt.fuzz_count, n);
          opt.fuzz_count = MIN (opt.fuzz_count, n);
        }
      /* Find the longest wordlist, needed by pitchfork */
      opt.fuzz_ctx.longest = opt.wlists[0];
      for (size_t i=0; i<n; ++i)
        {
          if (opt.wlists[i]->total_count > opt.fuzz_ctx.longest->total_count)
            opt.fuzz_ctx.longest = opt.wlists[i];
        }
      opt.load_next_fuzz = __next_fuzz_pitchfork;
      break;

    case MODE_SINGULAR:
      if (1 != n)
        warnln ("expected 1 word-list, provided %ld", n);
      opt.load_next_fuzz = __next_fuzz_singular;
      break;

    case MODE_CLUSTERBOMB:
      if (opt.fuzz_count != n)
        {
          warnln ("expected %ld word-list(s), provided %ld", opt.fuzz_count, n);
          opt.fuzz_count = MIN (opt.fuzz_count, n);
        }
      opt.load_next_fuzz = __next_fuzz_clusterbomb;
      break;
    } 

  return 0;
}

int
set_template (FuzzTemplate *t, int op, void *param)
{
  int n;
  char *s = (char *) param;
  switch (op)
    {
    case URL_TEMPLATE:
      {
        if (t->URL)
          {
            /* Resetting the URL, Ignores the previous URL */
            if ((n = fuzz_count (t->URL)))
              {
                FLG_UNSET (opt.fuzz_flag, URL_HASFUZZ);
                opt.fuzz_count -= n;
              }
          }
        Strrealloc (t->URL, s);
        if ((n = fuzz_count (t->URL)))
          {
            FLG_SET (opt.fuzz_flag, URL_HASFUZZ);
            opt.fuzz_count += n;
          }
      }
      break;

    case HEADER_TEMPLATE:
      {
        SLIST_APPEND (t->headers, s);
        if ((n = fuzz_count (s)))
          {
            FLG_SET (opt.fuzz_flag, HEADER_HASFUZZ);
            opt.fuzz_count += n;
          }
      }
      break;

    case BODY_TEMPLATE:
      {
        if (NULL == t->post_body)
          {
            t->post_body = strdup (s);
          }
        else
          {
            size_t len = Strlen (t->post_body);
            if ('&' != s[0] && '&' != t->post_body[len - 1])
              {
                /* We need an extra `&` */
                Realloc (t->post_body, len + Strlen (s) + 2);
                char *p = t->post_body + len;
                *p = '&';
                strcpy (p + 1, s);
              }
            else
              {
                Realloc (t->post_body, len + Strlen (s) + 1);
                strcpy (t->post_body + len , s);
              }
          }
        if ((n = fuzz_count (s)))
          {
            FLG_SET (opt.fuzz_flag, BODY_HASFUZZ);
            opt.fuzz_count += n;
          }
      }
      break;
    }
  return 0;
}

void
cleanup (int c, void *p)
{
  UNUSED (c), UNUSED (p);
  /* Libcurl cleanup */
  for (size_t i = 0; i < opt.concurrent; i++)
    {
      RequestContext *ctx = &opt.ctxs[i];
      curl_multi_remove_handle (opt.multi_handle, ctx->easy_handle);
      curl_easy_cleanup (ctx->easy_handle);
      safe_free (ctx->request.post_body);
      curl_slist_free_all (ctx->request.http_headers);
    }
  curl_multi_cleanup (opt.multi_handle);
  curl_global_cleanup ();
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
  const char *lopt_cstr = "u:H:d:w:vh";
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
          fprintf (stdout, "%s - version: %s\n", PROG_NAME, PROG_VERSION);
          return 1;

        case 'w':
          register_wordlist (optarg);
          break;

        case 'u':
          set_template (&opt.fuzz_template, URL_TEMPLATE, optarg);
          break;
        case 'H':
          set_template (&opt.fuzz_template, HEADER_TEMPLATE, optarg);
          break;
        case 'd':
          set_template (&opt.fuzz_template, BODY_TEMPLATE, optarg);
          break;

        default:
          break;
        }
    }
  return 0;
}

static inline void
pre_init_opts ()
{
  /* Set default values */
  opt.concurrent = 5;
  opt.mode = MODE_DEFAULT;
  /* Initialize opt */
  opt.wlists = da_new (Fword *);
  opt.wlist_paths = da_new (char *);

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

  pre_init_opts ();

  if (0 != (ret = parse_args (argc, argv)))
    return (ret < 0) ? EXIT_FAILURE : EXIT_SUCCESS;

  if (0 != (ret = init_opt ()))
    return EXIT_FAILURE;

  /**
   *  Main Loop
   */ 
  CURLMsg *msg;
  RequestContext *ctx;
  int numfds, res, still_running;
  do {
    /* Find a free context (If there is any) and register it */
    while (!opt.should_end && opt.waiting_reqs < opt.concurrent)
      {
        if ((ctx = lookup_free_handle (opt.ctxs, opt.concurrent)))
          {
            opt.waiting_reqs++;
            register_contex (ctx);
          }
      }

    curl_multi_perform (opt.multi_handle, &still_running);
    curl_multi_wait (opt.multi_handle, NULL, 0, 1000, &numfds);

    while ((msg = curl_multi_info_read (opt.multi_handle, &res)))
      {
        CURL *completed_handle = msg->easy_handle;
        RequestContext *ctx = lookup_handle (completed_handle,
                                             opt.ctxs, opt.concurrent);
        assert (NULL != ctx && "Broken Logic!!\n"
                "Completed easy_handle doesn't have request context.\n");

        if (msg->msg == CURLMSG_DONE)
          {
            if (msg->data.result == CURLE_OK)
              print_stats (ctx);
            /* Release the completed context */
            context_reset (ctx);
            opt.waiting_reqs--;
          }
      }
  }
  while (still_running > 0 || !opt.should_end);

  return 0;
}
