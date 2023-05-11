/*! \file mmap.h
 * \brief Contains definitions needed for mmap and co
 */

#ifndef INCLUDED_kernel_mmap_h
#define INCLUDED_kernel_mmap_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/riscv.h"
#include "kernel/printk.h"
// Flags and stuff is in here
#include "uk-shared/mmap_defs.h"

/* Error codes */
#define EPERM  0x0b1
#define EINVAL 0x0b10
#define ENIMPL 0x0b100
#define ENOMEM 404 // Memory not found


#define MMAP_MIN_ADDR ((uint64)1 << 34)

uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset) ;
uint64 __intern_munmap(void* addr, uint64 length);

#ifdef __cplusplus
}
#endif

#endif
