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

#define SLIST_APPEND(list, val) \
  list = curl_slist_append (list, val)

#ifndef TMP_CAP
#  define TMP_CAP 1024 /* bytes */
#endif
static char tmp[TMP_CAP];

#define UNUSED(x) (void)(x)
#define lenof(arr) (sizeof (arr) / sizeof (arr[0]))

#define FLG_SET(dst, flg) (dst |= flg)
#define FLG_UNSET(dst, flg) (dst &= ~(flg))

enum ffuc_flag_t
  {
    /* Context flag */
    CTX_FREE          = 0,
    CTX_INUSE         = 1,

    /**
     *  Initialize requests template
     *
     *  URL_TEMPLATE: set the url only one time
     *  BODY_TEMPLATE: append to the request body
     *                 it adds `&` automatically
     *  HEADER_TEMPLATE: append header
     */
    URL_TEMPLATE      = 0,
    BODY_TEMPLATE     = 1,
    HEADER_TEMPLATE   = 2,
    
    /**
     *  Fuzzing modes
     *
     *  Singular: using only one wordlist for all FUZZ keywords
     *
     *  Pitchfork: each FUZZ, uses it's own word-list
     *             word-lists are not necessarily the same size
     *             O( Max(len(wlist 1), ..., len(wlist N)) )
     *
     *  Clusterbomb: All combinations of word-list(s)
     *               O(len(wlist 1) x ... x len(wlist N))
     */
    MODE_CLUSTERBOMB  = 0,
    MODE_PITCHFORK    = 1,
    MODE_SINGULAR     = 2,

    /**
     *  Internal
     *  Without these flags, the generated request
     *  will use the raw templates without FUZZ
     *  keyword substitution
     */
    URL_HASFUZZ       = (1 << 1),
    BODY_HASFUZZ      = (1 << 2),
    HEADER_HASFUZZ    = (1 << 3),
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
   *  The number of elements within FUZZ, should be at least
   *  the count of FUZZ keywords used in request template
   *  *OR* set the latest element to NULL to indicate the end
   */
  char **FUZZ;

  struct request_t
  {
    /* As libcurl automatically keeps a copy of
     the URL, we don't need to keep track of it */
    char *post_body;
    struct curl_slist *http_headers;
  } request;

} RequestContext;

struct FuzzTemplate
{
  char *URL;
  char *post_body;
  struct curl_slist *headers;
};

/* Fuzz word */
typedef struct
{
  /**
   *  Use fw_get macro and @len together
   *  to retrieve the current word
   *
   *  @str is a C string, but words are not,
   *  they contain newline at their end.
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
} Fword;

#define fw_get(fw) ((fw)->str + (fw)->__offset)

#define fw_eof(fw) ((fw)->idx + 1 == (fw)->total_count)
#define fw_bof(fw) ((fw)->idx == 0)

/**
 *  As the real bottleneck is the network,
 *  any more efficient way to handle string memories,
 *  is not probably going to improve the performance.
 */
#define safe_free(ptr) if (ptr) free (ptr)
#define Strlen(s) ((NULL != s) ? strlen (s) : 0)

#define Strrealloc(dst, src) do {               \
    safe_free (dst);                            \
    dst = strdup (src);                         \
  } while (0)

#define Realloc(ptr, len) \
  if (ptr) ptr = realloc (ptr, len)

#ifdef _DEBUG
# define fprintd(format, ...) \
  fprintf (stderr, format, ##__VA_ARGS__)
# define printd(format, ...) \
  fprintd ("%s:%d: " format, __FILE__, __LINE__, ##__VA_ARGS__);
#else
# define fprintd(...) ((void) NULL)
# define printd(...) ((void) NULL)
#endif /* _DEBUG */


Fword *
fw_dup (const Fword *src)
{
  Fword *tmp = malloc (sizeof (Fword));
  memcpy (tmp, src, sizeof (Fword));
  return tmp;
}

void
fw_init (Fword *fw, char *cstr)
{
  fw->str = cstr;
  fw->__offset = 0, fw->total_count = 0;
  fw->len = (uint) (strchr (cstr, '\n') - cstr);

  /* calculating count of words withing the wordlist */
  for (char *p = cstr;; fw->total_count++, p++)
    {
      p = strchr (p, '\n');
      if (!p)
        break;
    }
}

char *
fw_next (Fword *fw)
{
  char *p = fw_get (fw);
  if ('\n' == *p)
    p++;

  char *next = strchr (p, '\n');
  if (NULL == next || '\0' == next[1])
    {
      p = fw->str;
      next = strchr (p, '\n');
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

struct Opt
{
  /* User options */
  int fuzz_flag;
  struct FuzzTemplate fuzz_template;
  

  /* Internals */
  Fword **wlists; /* Dynamic array */
  char **wlist_fpaths; /* Dynamic array */
  int mode;
  bool should_end;
  CURLM *multi_handle;
  
  size_t fuzz_count;
  size_t concurrent; /* Max number of concurrent requests */
  size_t waiting_reqs;

  void (*load_next_fuzz) (RequestContext *ctx);
};

struct Opt opt = {0};
static RequestContext *ctxs;

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

size_t
write_fun (void *ptr, size_t size, size_t nmemb, void *optr)
{
  char *cptr = (char *) ptr;
  int len =  (int)(size * nmemb);
  RequestContext *ctx = (RequestContext *) optr;

  ctx->stat.size_bytes += len;
  for (int i=0; i < len; ++i)
    {
      if (cptr[i] == ' ')
        ctx->stat.wcount++;
      else if (cptr[i] < ' ')
        ctx->stat.lcount++;
    }
  return len; 
}

RequestContext *
lookup_handle (CURL *handle)
{
  for (size_t i = 0; i < opt.concurrent; ++i)
    if (ctxs[i].easy_handle == handle)
      return ctxs + i;
  return NULL;
}

RequestContext *
lookup_free_handle ()
{
  for (size_t i = 0; i < opt.concurrent; ++i)
    if (ctxs[i].flag == CTX_FREE)
      return ctxs + i;
  return NULL;
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

static inline void
__register_contex (RequestContext *dst)
{
  struct FuzzTemplate template = opt.fuzz_template;

  char **FUZZ = dst->FUZZ;
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
__next_fuzz_singular (RequestContext *ctx)
{
  Fword *fw = opt.wlists[0];
  snprintf (tmp, TMP_CAP, "%.*s", (int)fw->len, fw_get (fw));
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
      snprintf (tmp, TMP_CAP, "%.*s", (int)fw->len, fw_get (fw));
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

      snprintf (tmp, TMP_CAP, "%.*s", (int)fw->len, p);
      Strrealloc (ctx->FUZZ[i], tmp);
      fprintd ("[%d]->`%s`\t", fw->idx, tmp);
    }
  fprintd ("\n");

  size_t i = 0;
  for (; i < N; )
    {
      fw = opt.wlists[i];
      fw_next (fw);

}

void
register_contex (RequestContext *ctx)
{
  opt.load_next_fuzz (ctx);
  curl_easy_reset (ctx->easy_handle);
  {
    FLG_SET (ctx->flag, CTX_INUSE);
    curl_easy_setopt (ctx->easy_handle, CURLOPT_WRITEDATA, ctx);
    curl_easy_setopt (ctx->easy_handle, CURLOPT_WRITEFUNCTION, write_fun);
  }
  __register_contex (ctx);
  curl_multi_add_handle (opt.multi_handle, ctx->easy_handle);
}

void
context_reset (RequestContext *ctx)
{
  ctx->flag = CTX_FREE;
  curl_multi_remove_handle (opt.multi_handle, ctx->easy_handle);
  memset (&ctx->stat, 0, sizeof (struct stat_t));
}

int
fuzz_count (const char *s)
{
  int n = 0;
  if (!s)
    return 0;
  while ((s = strstr (s, "FUZZ")))
    n++, s += 4;
  return n;
}

int
is_wlist_open (const char *filepath)
{
  da_foreach (opt.wlist_fpaths, i)
    {
      if (0 == strcmp (filepath, opt.wlist_fpaths[i]))
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

  char *mapped = mmap (NULL,
                       sb.st_size,
                       PROT_READ, MAP_PRIVATE,
                       fd, 0);
  if (mapped == MAP_FAILED)
    {
      close(fd);
      return fd;
    }

  /* Success */
  fw_init (dst, mapped);
  return 0;
}

int
register_wordlist (char *pathname)
{
#define WL_APPD(filepath, fword) do {           \
      da_appd (opt.wlists, fword);              \
      da_appd (opt.wlist_fpaths, filepath);     \
    } while (0)

  Fword fw = {0};
  int wl_idx = is_wlist_open (pathname);
  if (-1 != wl_idx)
    {
      /* File is already open */
      fw_init (&fw, opt.wlists[wl_idx]->str);
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
  opt.wlists = da_new (Fword *);
  opt.wlist_fpaths = da_new (char *);
  ctxs = calloc (opt.concurrent, sizeof (RequestContext));

  char *fps[] = {
      "/tmp/example2.txt",
      "/tmp/example1.txt",
    //   "/tmp/example3.txt",
    //   "/tmp/example2.txt",
      "/tmp/example2.txt",
  };
  for (size_t i=0; i < lenof (fps)
         && i < opt.fuzz_count; ++i)
    {
      register_wordlist (fps[i]);
    }

   size_t n =  da_sizeof (opt.wlists);
   if (n == 0)
     {
       opt.should_end = 1;
       warnln ("cannot continue with no word-list");
       return 1;
     }

   switch (opt.mode)
    {
    case MODE_PITCHFORK:
      if (opt.fuzz_count != n)
        warnln ("expected %ld word-list(s), provided %ld", opt.fuzz_count, n);
        {
        }
      opt.load_next_fuzz = __next_fuzz_pitchfork;
      break;

    case MODE_SINGULAR:
      if (1 != n)
        warnln ("expected 1 word-list, provided %ld", n);
      opt.load_next_fuzz = __next_fuzz_singular;
      break;

    case MODE_CLUSTERBOMB:
      assert (0 && "Not Implemented.");
      break;
    } 

  return 0;
}

int
set_template (int op, char *s)
{
  int n;
  struct fuzz_template_t *t = &opt.fuzz_template;
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
  UNUSED (c);
  UNUSED (p);
  /* Libcurl cleanup */
  for (size_t i = 0; i < opt.concurrent; i++)
    {
      RequestContext *ctx = ctxs + i;
      curl_multi_remove_handle (opt.multi_handle, ctx->easy_handle);
      curl_easy_cleanup (ctx->easy_handle);
      safe_free (ctx->request.post_body);
      curl_slist_free_all (ctx->request.http_headers);
    }
  curl_multi_cleanup (opt.multi_handle);
  curl_global_cleanup ();
}

int
main (void)
{
  int ret;
  set_program_name ("FFuc");
  on_exit (cleanup, NULL);

  /* TODO: use optarg */
  {
    opt.concurrent = 5;
    opt.waiting_reqs = 0;
    opt.fuzz_count = 0;

    // opt.mode = MODE_SINGULAR;
    // opt.mode = MODE_PITCHFORK;
    opt.mode = MODE_CLUSTERBOMB;

    // URL
    set_template (URL_TEMPLATE, "http://127.0.0.1:4444/1_FUZZ.txt");
    // Body
    set_template (BODY_TEMPLATE, "yourmom=FUZZ");
    set_template (BODY_TEMPLATE, "&test=FUZZ.FUZZ");
    set_template (BODY_TEMPLATE, "test=123");
    // Headers
    set_template (HEADER_TEMPLATE, "H1_test: FUZZ");
    set_template (HEADER_TEMPLATE, "Hr: FUZZ");
    set_template (HEADER_TEMPLATE, "Hs: FUZZ");

    ret = init_opt ();
    if (ret)
      return ret;
  }
  
  /* Initialize libcurl & context of requests */
  {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    opt.multi_handle = curl_multi_init();

    for (size_t i = 0; i < opt.concurrent; i++)
      {
        ctxs[i].easy_handle = curl_easy_init();

        int cap_bytes = (opt.fuzz_count + 1) * sizeof (char *);
        ctxs[i].FUZZ = malloc (cap_bytes);
        memset (ctxs[i].FUZZ, 0, cap_bytes);
      }
   }

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
        if ((ctx = lookup_free_handle ()))
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
        RequestContext *ctx = lookup_handle (completed_handle);
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
