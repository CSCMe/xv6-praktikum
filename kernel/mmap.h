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
#include "kernel/buf.h"

#define SHARED_MAPPING_ENTRIES_NUM 256

/* Error codes */
#define EPERM  0x1    // Permission error
#define EINVAL 0x2    // Invalid input
#define ENIMPL 0x3    // Not implemented
#define EEXIST 0x4    // Mapping exists, but we don't want to overwrite
#define ENOMEM 0x194  // Memory not found
#define EACCESS 0x5   // 
#define EBADF   0x6

uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, struct file* f, uint64 offset) ;
uint64 __intern_munmap(void* addr, uint64 length);
int64  munmap_shared(uint64 physicalAddr, uint32 doWriteBack);
int64 acquire_or_insert_table(struct buf* underlying_buf,  void* physicalAddr);

typedef struct __map_shared_entry {
    // How many procs have mapped this page
    uint64 refCount;
    // Buffer is NULL if not file backed
    struct buf* underlying_buf;
    // Entry is invalid when NULL
    void* physicalAddr;
} MapSharedEntry;

#ifdef __cplusplus
}
#endif

#endif
