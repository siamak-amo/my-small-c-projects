#ifndef STDDEF__H__
#define STDDEF__H__

/* NULL pointer */
#define NULL ((void *) 0)
#define _Nullable
#define _Noreturn __attribute__((__noreturn__))
#define _Naked    __attribute__((naked))
#define UNUSED(x) ((void)(x))

/* std int */
typedef long int ssize_t;
typedef long unsigned int size_t;
typedef unsigned long int uintptr_t;

typedef char int8;
typedef unsigned char uint8;

typedef unsigned int uint;
typedef unsigned int uint32;

/* std bool */
typedef char bool;
#define true  1
#define false 0

/* std in/out/err file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* exit codes */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* std args */
typedef __builtin_va_list va_list;
#define va_start(v,l)   __builtin_va_start (v,l)
#define va_end(v)       __builtin_va_end (v)
#define va_arg(v,l)     __builtin_va_arg (v,l)
#define va_copy(d,s)    __builtin_va_copy (d,s)

/* alias */
#define weak_alias(old, new) \
  extern typeof (old) new __attribute__((__weak__, __alias__(#old)))
#endif /* STDDEF__H__ */
