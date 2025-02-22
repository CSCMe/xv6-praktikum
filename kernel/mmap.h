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

#define SHARED_MAPPING_ENTRIES_NUM 512  // Can share up to 512*4096 = 2MB, should be enough

uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, struct file* f, uint64 offset) ;
uint64 __intern_munmap(void* addr, uint64 length);
int64  munmap_shared(uint64 physicalAddr, uint32 doWriteBack);
int64 acquire_or_insert_table(struct buf* underlying_buf,  void* physicalAddr);
int populate_mmap_page(uint64 addr);

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
