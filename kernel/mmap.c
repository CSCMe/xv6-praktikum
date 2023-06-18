#include "kernel/defs.h"
#include "kernel/memlayout.h"
#include "kernel/printk.h"
#include "kernel/buf.h"
// There are important things in riscv.h

//#define DEBUG_ERRORS
//#define DEBUG_PRECON
//#define DEBUG_SHARED_TABLE

struct {
    MapSharedEntry entries[SHARED_MAPPING_ENTRIES_NUM];
    struct spinlock tableLock;
} shared_mappings_table;

void init_mmap() {
    initlock(&shared_mappings_table.tableLock, "mmap");
    for (int i = 0; i < SHARED_MAPPING_ENTRIES_NUM; i++) {
        shared_mappings_table.entries[i] = (MapSharedEntry){.physicalAddr=NULL, .refCount=0,.underlying_buf=NULL};
    }
}

/**
 * Hash function for shared_mappings_map
*/
uint64 hash_function(uint64 key) {
    key ^= (key >> 33);
    key *= 0xff51afd7ed558ccd;
    key ^= (key >> 33);
    key *= 0xc4ceb9fe1a85ec53;
    key ^= (key >> 33);

    return key % SHARED_MAPPING_ENTRIES_NUM;
}


void debug_shared_mappings_table() {
    pr_debug("Printing Map-Shared-Table\n");
    for (int i = 0; i < SHARED_MAPPING_ENTRIES_NUM; i++) {
        pr_debug("%d: k:%d,  pA:%p, uB:%p, refs:%d\n", i, hash_function((uint64)shared_mappings_table.entries[i].physicalAddr),shared_mappings_table.entries[i].physicalAddr, shared_mappings_table.entries[i].underlying_buf, shared_mappings_table.entries[i].refCount);
    }
}


/**
 * Search the table for an entry with the specified targetKey, uses key in the hash function
 * Returns -1 on failure, index on success
*/
int64 table_lookup(uint64 key, void* targetKey) {
    uint64 result = hash_function(key);
    uint64 count = 0;
    while (shared_mappings_table.entries[result].physicalAddr != targetKey) {
        count++;
        // If we either iterated over the entire table or met a NULL entry (and aren't looking for a NULL entry) we bail
        if (count == SHARED_MAPPING_ENTRIES_NUM || shared_mappings_table.entries[result].physicalAddr == NULL) {
            return -1;
        }
        result = (result + 1) % SHARED_MAPPING_ENTRIES_NUM;
    }
    return result;
}

/**
 * Inserts an entry into the hash table
 * return -1 on error, index on success
*/
int64 acquire_or_insert_table(struct buf* underlying_buf,  void* physicalAddr) {
    acquire(&shared_mappings_table.tableLock);
    int64 pos = table_lookup((uint64)physicalAddr, physicalAddr);

    // If entry does not exist, make new one
    if (pos < 0) {
        pos = table_lookup((uint64)physicalAddr, NULL);
        if (pos < 0) {
            release(&shared_mappings_table.tableLock);
            return -1;
        }
    }

    MapSharedEntry* myEntry = shared_mappings_table.entries + pos;
    if (myEntry->refCount == 0) {
        myEntry->physicalAddr = physicalAddr;
        myEntry->underlying_buf = underlying_buf;
    }
    myEntry->refCount++;

    release(&shared_mappings_table.tableLock);
    return pos;
}

/**
 * Returns 0 if memory should not be freed
*/
int64 munmap_shared(uint64 physicalAddr, uint32 doWriteBack) {
    acquire(&shared_mappings_table.tableLock);
    int64 holeIndex = table_lookup(physicalAddr, (void*) physicalAddr);
    // Panic if entry not in table (required, if not: table broke)
    if (holeIndex < 0) {
        #ifdef DEBUG_ERRORS
        pr_emerg("PANIC: %p\n", physicalAddr);
        debug_shared_mappings_table();
        #endif
        panic("munmap-shared");
    }
    // Remove current entry
    MapSharedEntry copy = shared_mappings_table.entries[holeIndex];
    MapSharedEntry* currentEntry = shared_mappings_table.entries + holeIndex;
    #ifdef DEBUG_SHARED_TABLE
    pr_info("Unmapping: %p\n", physicalAddr);
    debug_shared_mappings_table();
    #endif
    if ((--(currentEntry->refCount)) == 0) {
        currentEntry->physicalAddr = NULL;
        struct buf* entryBuf = currentEntry->underlying_buf;
        // Loop through all successive entries until there's an empty one, and move the entries to their new position
        // There is always an empty entry, so (theoretically) no infinite loop here
        // uint64 so distance is not negative => wraparound case works. Might still be bugged in unknown ways
        uint64 curIndex = (holeIndex + 1) % SHARED_MAPPING_ENTRIES_NUM;
        uint64 holeDistance = 1;
        // See for wraparound case explanation: https://cs.stackexchange.com/a/60198
        while ((currentEntry = shared_mappings_table.entries + curIndex)->physicalAddr != NULL) {
            int64 shouldBeIndex = hash_function((uint64)currentEntry->physicalAddr);
            int64 distance = (curIndex - shouldBeIndex) % SHARED_MAPPING_ENTRIES_NUM;

            // Inspired by: https://github.com/leventov/Koloboke/blob/68515672575208e68b61fadfabdf68fda599ed5a/benchmarks/research/src/main/javaTemplates/com/koloboke/collect/research/hash/NoStatesLHashCharSet.java#L194-L212
            if (distance >= holeDistance) {
                shared_mappings_table.entries[holeIndex] = shared_mappings_table.entries[curIndex];
                // Actually NULL our entry so our break condition works
                shared_mappings_table.entries[curIndex] = (MapSharedEntry){.physicalAddr=NULL,.refCount=0,.underlying_buf=NULL};
                holeIndex = curIndex;
                holeDistance = 1;
            } else {
                holeDistance++;
            }
            // Actually increasing curIndex now. Should fix inifinite loop
            curIndex = (curIndex + 1) % SHARED_MAPPING_ENTRIES_NUM;
        }

        shared_mappings_table.entries[holeIndex] = (MapSharedEntry){.physicalAddr=NULL, .refCount=0, .underlying_buf=NULL};

       
        release(&shared_mappings_table.tableLock);
        if (entryBuf != NULL) {
            // doWriteBack is true if called from munmap and false if called from zombie cleanup (wait)
            if (doWriteBack) {
                bwrite(entryBuf);
            }
            brelse(entryBuf);
        }
        // Never free a buffer! 
        return copy.underlying_buf == NULL;
    }
    release(&shared_mappings_table.tableLock);
    return 0;
}


#define CONSTRUCT_VIRT_FROM_PT_INDICES(high, mid, low) (( (((((high) << 9) + (mid)) << 9) + (low))) << 12)
/**
 * Goes through the page table starting at addr baseAddr
 * fixed is true if MAP_FIXED was passed (meaning we deffo allocate at baseAddr unless there's a kernel page here)
*/
uint64 mmap_find_free_area(pagetable_t table, uint64 required_pages, uint64 baseAddr, int fixed, int noOverwrite) {

    uint64 contig_pages = 0;
    uint64 upBase = PX(2, baseAddr);
    uint64 midBase = PX(1, baseAddr);
    uint64 lowBase = PX(0, baseAddr);
    uint64 returnAddr = baseAddr;

    // Recursion was also an option. Why not choose that?
    // Top level only has 256 entries
    for (uint64 u = upBase; u < 256 && contig_pages < required_pages; u++) {
        pte_t upper = table[u];
        if (!(upper & PTE_V)) {
            contig_pages += 512*512;
            if (returnAddr == 0)
                returnAddr = CONSTRUCT_VIRT_FROM_PT_INDICES(u, 0, 0);
            continue;
        }

        for (uint64 m = (u == upBase) ? midBase : 0; m < 512 && contig_pages < required_pages; m++) {
            pte_t mid = ((pagetable_t)PTE2PA(upper))[m];
            if (!(mid & PTE_V)) {
                contig_pages += 512;
                if (returnAddr == 0)
                    returnAddr =  CONSTRUCT_VIRT_FROM_PT_INDICES(u,m,0);
                continue;
            }

            for (uint64 l = (u == upBase && m == midBase) ? lowBase : 0; l < 512 && contig_pages < required_pages; l++) {
                pte_t low = ((pagetable_t)PTE2PA(mid))[l];
                if (low & PTE_V || low & PTE_MM) { // Either a valid entry or one that is soon to be valid
                    if ((fixed && noOverwrite) || (fixed && !(low & PTE_U))) {
                        #ifdef DEBUG_ERRORS
                        pr_debug("ERROR-EEXIST: f:%d, nO:%d, base:%p, cur:%p\n", fixed, noOverwrite, baseAddr, CONSTRUCT_VIRT_FROM_PT_INDICES(u,m,l));
                        #endif
                        // There is already an existing mapping we don't want to overwrite
                        return EEXIST;
                    } else if (!fixed){
                        contig_pages = 0;
                        returnAddr = 0;
                        continue;
                    }
                    // Case fixed && PTE_U, fall through
                } 
                contig_pages++;
                if (returnAddr == 0)
                    returnAddr = CONSTRUCT_VIRT_FROM_PT_INDICES(u,m,l);
            }
        }
    }

    if (contig_pages >= required_pages) {
        return returnAddr;
    }

    return ENOMEM;
}

/**
 * Precondition checking
*/
uint64 is_valid_addr(uint64 addr) {
    #ifdef DEBUG_PRECON
    pr_debug("%d && (%d || (%d && %d)\n", (addr % PGSIZE == 0), (void*)addr == NULL, (addr >= (uint64)MMAP_MIN_ADDR), (addr < MAXVA));
    #endif
    return (addr % PGSIZE == 0) && ((void*)addr == NULL || ((addr >= (uint64)MMAP_MIN_ADDR) && (addr < MAXVA)));
}

struct buf* get_nth_buffer_from_file(uint64 n, struct file* f) {
    ilock(f->ip);
    uint addr = bmap(f->ip, n);
    if(addr == 0)
      panic("mmap should not allocate new disk blocks (and should not fail)");
    struct buf* bp = bread(f->ip->dev, addr);
    iunlock(f->ip);
    return bp;
}

// Returns 0 on success, != 0 on fail
int populate_mmap_page(uint64 addr) {
    if (addr % 4096 != 0 || addr < (uint64)MMAP_MIN_ADDR || addr >= MAXVA)
        return 2;

    // Validates mapping
    pte_t* entry = walk(myproc()->pagetable, addr, 1);
    // If entry is MM and not already valid
    if (*entry & PTE_MM && !(*entry & PTE_V)) {
        // Bits other than permission should be 0, are now address
        *entry |= PA2PTE(kalloc_zero());
        // Mark entry as valid
        *entry |= PTE_V;
        return 0;
    } else {
        return 1;
    }

}

/**
 * Internal version of mmap, called by system call
*/
uint64 __intern_mmap(void *addr, uint64 length, int prot, int flags, struct file *f, uint64 offset) 
{
    // Check preconditions:
    if ( !is_valid_addr((uint64)addr)
        || length == 0 || !(length % PGSIZE == 0)
        || !(offset % PGSIZE == 0)
        || !((flags & MAP_PRIVATE) | (flags & MAP_SHARED) )) {
        #ifdef DEBUG_ERRORS
        pr_debug("INTERN-MMAP-EINVAL");
        #endif
        return EINVAL;
    }

    // Check if file perm are valid
    if (!(flags & MAP_ANONYMOUS)) {
        if (!(f->readable)) {
            return EPERM;
        } 
        if (!(f->writable) && prot & PROT_WRITE) {
            return EACCESS;
        }
        if (f->type != FD_INODE) {
            return EACCESS;
        }
        if (f->ip->valid == 0) {
            return EBADF;
        }
        if (length + offset > f->ip->size) {
            return EINVAL;
        }
    }

    // Only used by user
    // Holds flags for mapping
    int entryProt = PTE_U | PTE_MM;
    if (prot & PROT_READ)
        entryProt |= PTE_R;
    if (prot & PROT_WRITE)
        // PROT_WRITE implies PROT_READ
        entryProt |= PTE_W | PTE_R;
    if (prot & PROT_EXEC)
        entryProt |= PTE_X;
    
    // An unset MAP_ANON also implies MAP_POPULATE
    if (!(flags & MAP_ANON)) {
        flags |= MAP_POPULATE;
    }

    // MAP_SHARED sets PTE_SH page bit & IMPLIES MAP_POPULATE
    if (flags & MAP_SHARED) {
        flags |= MAP_POPULATE;
        entryProt |= PTE_SH;
    }

    struct proc* proc = myproc();

    if (addr < MMAP_MIN_ADDR) {
        addr = proc->last_mmap;
    }

    // Here begins actual mapping

    pagetable_t curTable = proc->pagetable;

    #ifdef DEBUG_PT
    pr_debug("Start: ");
    print_pt(curTable);
    #endif

    // Contiguous pages required
    uint64 required_pages = length / PGSIZE;

    // We find contig pages of the required length
    uint64 base = mmap_find_free_area(curTable, required_pages, (uint64) addr, flags & MAP_FIXED || flags & MAP_FIXED_NOREPLACE, flags & MAP_FIXED_NOREPLACE);

    // Happens when various errors are returned
    if (base < (uint64)MMAP_MIN_ADDR) {
        // Couldn't find memory. Try again with addr = MIN_MMAP
        if (base == ENOMEM && !(flags & MAP_FIXED || flags & MAP_FIXED_NOREPLACE)) {
            base = mmap_find_free_area(curTable, required_pages, (uint64) MMAP_MIN_ADDR, 0, 0);
        }
        if (base < (uint64)MMAP_MIN_ADDR) {
            return base;
        }
    }

    // go through the pages and map them
    // Since physical memory comes in PGSIZE chunks we need to loop here
    for (int i = 0; i < required_pages; i++) {
        
        void* curAlloc;
        struct buf* curBuf = NULL;

        // We need to populate the page and it's not anonymous
        if (flags & MAP_POPULATE && !(flags & MAP_ANON)) {
            // Get file backed mapping
            curBuf = get_nth_buffer_from_file(i + (offset / PGSIZE), f);
            // private file backed mappings are simple copies that aren't reflected on the actual file
            if (flags & MAP_PRIVATE) {
                curAlloc = kalloc();

                if (curAlloc == NULL) {
                    // kernel malloc error
                    #ifdef DEBUG_ERRORS
                    pr_debug("INTERN-MMAP-ENOMEM: Kernel alloc failed\n");
                    #endif
                    return ENOMEM;
                }
                memmove(curAlloc, curBuf->data, PGSIZE);
                // Private case, release buffer
                brelse(curBuf);

            } else { // Shared case
                curAlloc = curBuf->data;
            }
        } else if (flags & MAP_POPULATE && flags & MAP_ANON){ // page is anon and populated
            curAlloc = kalloc_zero();
            // MAP_POPULATE ignores kalloc erros (for some reason)
        } else { // Not populate && anon. Demand paging time
            curAlloc = NULL;
            //pr_debug("virt demand:%p\n", base + (i * PGSIZE));
        }

        // Insert into shared map
        if (flags & MAP_SHARED) {    
            // curBuf->data is never null
            if(acquire_or_insert_table(curBuf, curAlloc) == -1) {
                #ifdef DEBUG_ERRORS
                pr_notice("Table insert failed for %p with key %d\n", curAlloc, hash_function((uint64) curAlloc));
                #if defined(DEBUG_SHARED_TABLE) || defined(DEBUG_ERRORS)
                debug_shared_mappings_table();
                #endif
                #endif
                return ENOMEM;
            }
        }
        
        // VA of our current page
        uint64 curVA = base + (i * PGSIZE);


        // Gets an old entry at this location and frees & invalidates it
        pte_t* entry = walk(curTable, curVA, 1);
        if (entry == 0) {
            #ifdef DEBUG_ERRORS
            pr_debug("INTERN-MMAP-ENOMEM: Walk kalloc failed");
            #endif
            return ENOMEM;
        }
        
        if (*entry != 0) {
            // Shared entry -> check and unmap
            if (*entry & PTE_SH) {
                int should_free = munmap_shared(PTE2PA(*entry), 0);
                uvmunmap(curTable, curVA, 1, should_free);
            } else if (*entry & PTE_V) { // Private valid entry
                // Free private entry
                kfree((void*) PTE2PA(*entry));
            }
            // Now that we've freed. Set entry to 0
            *entry = 0;
        }

        // entry is now a pte we want. Set its flags (and address if MAP_POPULATE is set)
        *entry |= entryProt;
        if (flags & MAP_POPULATE) {
            if (curAlloc != NULL) {
                *entry |= PTE_V;
                *entry |= PA2PTE(curAlloc);
            } else {
                // Failure. manpage explicitely states we should not fail in this case but that won't stop us cuz we can't read 
                #ifdef DEBUG_ERRORS
                pr_debug("INTERN-MMAP-ENOMEM: POPULATE kalloc failed");
                #endif
                return ENOMEM;
            }
        }
        proc->last_mmap = (void*) curVA;
    }

    #ifdef DEBUG_PT
    pr_debug("Result: %p: \n", base);
    print_pt(curTable);
    #endif
    return (uint64)base;
}

uint64 __intern_munmap(void* addr, uint64 length) {
    if ((uint64)addr % PGSIZE != 0) {
        return EINVAL;
    }

    if (addr < MMAP_MIN_ADDR) {
        return 0;
    }

    // >= or >? Who knows, won't be relevant as highest pages are reserved by kernel
    if (((uint64)addr + length) >= MAXVA) {
        return EINVAL;
    }

    pagetable_t curTable = myproc()->pagetable;

    // Every single page in this range will be unmapped
    // munmap docs: It is not an error if the indicated range does not contain any mapped pages.
    for (int i = 0; i < PGROUNDUP(length) / PGSIZE; i++) {
        uint64 curAddr = (uint64)addr + (i * PGSIZE);
        pte_t* tableEntry = walk(curTable, curAddr, 0);
        uint64 flags = PTE_FLAGS(*tableEntry);
        // Entry does not exist or is already 0. Cool, continue;
        if (tableEntry == 0 || *tableEntry == 0) {
            continue;
        }
        // Entry is valid and mmaped
        if(flags & PTE_MM && flags & PTE_U) {
            if (flags & PTE_V) {
                if (flags & PTE_SH) {
                    int should_free = munmap_shared(PTE2PA(*tableEntry), 1);
                    uvmunmap(curTable, curAddr, 1, should_free);
                } else {
                    // Invalidate mapping, and free memory
                    uvmunmap(curTable, curAddr, 1, 1);
                }
            } else {
                // Just set flags to 0 if it's not valid
                *tableEntry = 0;
            }
        }
    }
    // Sometimes we need to free levels. Let's do so here and quite frequently at first (sloooow)
    // Iterate over entire pagetable and check from bottom to top if entire thing empty
    uvmfreelevels(curTable);

    return 0;
}