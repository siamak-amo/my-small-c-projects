#include "def.h"
#include "syscall.h"

/* env */
char *getenv (const char *name);
_Noreturn void _exit (int status);
#define exit(n) _exit (n)

/* std read */
char getc ();
/* std write & format print */
int dputc (int fd, void *c);
int dprintf (int fd, char *format, ...);

#define printf(format, ...) \
  dprintf (STDOUT_FILENO, format, ##__VA_ARGS__)
#define println(format, ...) \
  printf (format "\n", ##__VA_ARGS__)
#define puts(cstr) do { \
    printf ("%s\n", cstr); \
  } while (0)

/* cstr stuff */
size_t strlen (const char *buf);
int strcmp (const char *s1, const char *s);
int strncmp (const char *s1, const char *s, size_t n);
char *strchr (const char *cstr, int delim);
char *strchrnul (const char *cstr, int delim);

/* mem */
void *memset (void *dst, int c, size_t n);
void *mempset (void *dst, int c, size_t n);
void *memcpy (void *dest, const void *src, size_t n);
void *mempcpy (void *dest, const void *src, size_t n);
int memcmp (const void *s1, const void *s2, size_t n);
