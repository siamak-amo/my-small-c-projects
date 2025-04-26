#ifndef __TINYSTD_H
#define __TINYSTD_H

#include "def.h"
#include "x86_syscall.h"

/* R/W */
ssize_t read (int fd, const void *buf, size_t count);
ssize_t write (int fd, const void *buf, size_t count);

/* exit */
_Noreturn void _exit (int status);

#endif /* __TINYSTD_H */
