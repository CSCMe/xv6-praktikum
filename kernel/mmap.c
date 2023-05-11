#include "kernel/proc.h"
#include "kernel/mmap.h"

/**
 * Checks the validity of the protection flags
 * returns 0 on valid, != 0 on invalid
*/
int check_prot_validity(int prot) {
    return prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC);
}


// There are important things in riscv.h

void* mmap_find_free_area(uint64 required_pages) {
    //TODO: Implement
    return 0;
}


/**
 * Internal version of mmap, called by system call
*/
uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset) 
{
    // Check preconditions:
    if ((uint64)addr % PGSIZE || (addr != 0 && (uint64)addr < MMAP_MIN_ADDR) || length == 0 || length % PGSIZE || offset % PGSIZE || !(flags & MAP_PRIVATE || flags & MAP_SHARED ) ) {
        return EINVAL;
    }

    // Not implemented
    if (flags & MAP_SHARED || flags & MAP_SHARED_VALIDATE) {
        return ENIMPL;
    }

    // Only used by user
    // Holds flags for mapping
    int entryFlags = PTE_U;
    if (prot & PROT_READ)
        entryFlags |= PTE_R;
    if (prot & PROT_WRITE)
        entryFlags |= PTE_W;
    if (prot & PROT_EXEC)
        entryFlags |= PTE_X;

    if (!(flags & MAP_POPULATE)) {
        // We shall ignore this for now
    }

    // Here begins actual mapping

    uint64 required_pages = length % PGSIZE;
    void* wip_mappings[required_pages];

    void* va = va;
    if (va == 0)
        va = mmap_find_free_area(required_pages);
    //void* mapped_area;
    for (int i = 0; i < required_pages; i++) {
        void* lastMap =  0;
        wip_mappings[i] = lastMap; 
    }
    (void) wip_mappings;
    // Init to 0s if set
    if (flags & MAP_ANONYMOUS) {

    }
    return 0;
}

uint64 __intern_munmap(void* addr, uint64 length) {
    return 0;
}