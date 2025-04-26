/** syscall **/
#include "syscall.h"

static inline long
__syscall1 (long number, long arg1)
{
  long result;
  ASM_SYSCALL (result, number,
               SYSCALL_ARG1 (arg1));
  return result;
}

static inline long
__syscall2 (long number, long arg1, long arg2)
{
  long result;
  ASM_SYSCALL (result, number,
               SYSCALL_ARG1 (arg1),
               SYSCALL_ARG2 (arg2));
  return result;
}

static inline long
__syscall3 (long number, long arg1, long arg2, long arg3)
{
  long result;
  ASM_SYSCALL (result, number,
               SYSCALL_ARG1 (arg1),
               SYSCALL_ARG2 (arg2),
               SYSCALL_ARG3 (arg3));
  return result;
}


/* syscalls */
ssize_t
__read (int fd, const void *buf, size_t count)
{
  return __syscall3 (__NR_read, fd, (long)buf, count);
}

ssize_t
__write (int fd, const void *buf, size_t count)
{
  return __syscall3 (__NR_write, fd, (long)buf, count);
}

_Noreturn void
_exit__ (int status)
{
  __syscall1 (__NR_exit, status);
  __builtin_unreachable ();
}

weak_alias (__read, read);
weak_alias (__write, write);
weak_alias (_exit__, _exit);
