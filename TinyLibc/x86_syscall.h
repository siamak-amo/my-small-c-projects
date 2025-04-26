/**
 **  Internal syscall macro
 **  For linux - x86_64
 **/

/* syscall numbers */
#define __NR_read     0
#define __NR_write    1
#define __NR_open     2 /* not implemented */
#define __NR_close    3 /* not implemented */
#define __NR_exit     60

/**
 *  ASM syscall block
 *  \res:  8byte to store the result
 *  \num:  syscall number
 *  ...    pointers to syscall arguments, like:
 *         SYSCALL_ARG1(p1), SYSCALL_ARG(p2), ...
 */
#define ASM_SYSCALL(res, num, ...)    \
  asm volatile (                      \
    SYSCALL_PROLOG (res, num),        \
      __VA_ARGS__                     \
    SYSCALL_EPILOG ()                 \
  )

/* syscall arguments */
#define SYSCALL_ARG1(__arg__) "D" (__arg__) // rdi
#define SYSCALL_ARG2(__arg__) "S" (__arg__) // rsi
#define SYSCALL_ARG3(__arg__) "d" (__arg__) // rdx
/* header & footer */
#define SYSCALL_PROLOG(res, num) "syscall" : "=a" (res) : "a" (num)
#define SYSCALL_EPILOG() : "rcx", "r11", "memory"
