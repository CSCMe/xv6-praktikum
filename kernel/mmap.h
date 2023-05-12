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
#define EPERM  0x1    // Permission error
#define EINVAL 0x2    // Invalid input
#define ENIMPL 0x3    // Not implemented
#define EEXIST 0x4    // Mapping exists, but we don't want to overwrite
#define ENOMEM 0x194  // Memory not found


uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset) ;
uint64 __intern_munmap(void* addr, uint64 length);

#ifdef __cplusplus
}
#endif

#endif
