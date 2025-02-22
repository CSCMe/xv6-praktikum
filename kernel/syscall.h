/*! \file syscall.h
 * \brief system call list
 */

#ifndef INCLUDED_kernel_syscall_h
#define INCLUDED_kernel_syscall_h

#ifdef __cplusplus
extern "C" {
#endif


// System call numbers
#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_pipe    4
#define SYS_read    5
#define SYS_kill    6
#define SYS_exec    7
#define SYS_fstat   8
#define SYS_chdir   9
#define SYS_dup    10
#define SYS_getpid 11
#define SYS_sbrk   12
#define SYS_sleep  13
#define SYS_uptime 14
#define SYS_open   15
#define SYS_write  16
#define SYS_mknod  17
#define SYS_unlink 18
#define SYS_link   19
#define SYS_mkdir  20
#define SYS_close  21
#define SYS_mmap   22
#define SYS_munmap 23
#define SYS_futex_init 24
#define SYS_futex_wait 25
#define SYS_futex_wake 26
#define SYS_net_test 27
#define SYS_net_bind 28
#define SYS_net_send_listen 29
#define SYS_net_unbind 30
#define SYS_hello_kernel 50
#define SYS_printPT 51
#define SYS_cxx    100
#define SYS_term   101



#ifdef __cplusplus
}
#endif

#endif
