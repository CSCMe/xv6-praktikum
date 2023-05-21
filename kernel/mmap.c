#include "kernel/defs.h"
#include "kernel/memlayout.h"
#include "kernel/printk.h"
#include "kernel/buf.h"

// There are important things in riscv.h

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


/**
 * Search the table for an entry with the specified targetKey, uses key in the hash function
 * Returns -1 on failure, index on success
*/
int64 table_lookup(uint64 key, void* targetKey) {
    uint64 result = hash_function(key);
    uint64 count = 0;
    while (shared_mappings_table.entries[result].physicalAddr != targetKey) {
        result = (result + 1) % SHARED_MAPPING_ENTRIES_NUM;
        count++;
        if (count == SHARED_MAPPING_ENTRIES_NUM) {
            return -1;
        }
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
            return -1;
        }
    }

    MapSharedEntry* myEntry = shared_mappings_table.entries + pos;
    myEntry->refCount++;
    myEntry->physicalAddr = physicalAddr;
    myEntry->underlying_buf = underlying_buf;

    release(&shared_mappings_table.tableLock);
    return pos;
}

/**
 * Returns 0 if memory should not be freed
*/
int64 munmap_shared(uint64 physicalAddr) {
    acquire(&shared_mappings_table.tableLock);
    int64 holeIndex = table_lookup(physicalAddr, (void*) physicalAddr);
    // Panic if entry not in table (required, if not: table broke)
    if (holeIndex < 0) {
        panic("munmap-shared");
    }
    // Remove current entry
    MapSharedEntry copy = shared_mappings_table.entries[holeIndex];
    MapSharedEntry* currentEntry = shared_mappings_table.entries + holeIndex;
    if ((--(currentEntry->refCount)) == 0) {
        currentEntry->physicalAddr = NULL;
        struct buf* entryBuf = currentEntry->underlying_buf;
        if (entryBuf != NULL) {
            brelse(entryBuf);
        }

        // Loop through all successive entries until there's an empty one, and move the entries to their new position
        // There is always an empty entry, so (theoretically) no infinite loop here
        int64 curIndex = (holeIndex + 1) % SHARED_MAPPING_ENTRIES_NUM;
        int64 holeDistance = 1;
        // See for wraparound case explanation: https://cs.stackexchange.com/a/60198
        while ((currentEntry = shared_mappings_table.entries + curIndex)->physicalAddr != NULL) {
            int64 shouldBeIndex = hash_function((uint64)currentEntry->physicalAddr);
            int64 distance = (curIndex - shouldBeIndex) % SHARED_MAPPING_ENTRIES_NUM;
            // Inspired by: https://github.com/leventov/Koloboke/blob/68515672575208e68b61fadfabdf68fda599ed5a/benchmarks/research/src/main/javaTemplates/com/koloboke/collect/research/hash/NoStatesLHashCharSet.java#L194-L212
            if (distance >= holeDistance) {
                shared_mappings_table.entries[holeIndex] = shared_mappings_table.entries[curIndex];
                holeIndex = curIndex;
                holeDistance = 1;
            } else {
                holeDistance++;
            }
        }

        shared_mappings_table.entries[holeIndex] = (MapSharedEntry){.physicalAddr=NULL, .refCount=0, .underlying_buf=NULL};

        // Never free a buffer!
        release(&shared_mappings_table.tableLock);
        return copy.underlying_buf == NULL;
    }
    release(&shared_mappings_table.tableLock);
    return 0;
}

/**
 * Goes through the page table starting at addr baseAddr
 * fixed is true if MAP_FIXED was passed (meaning we deffo allocate at baseAddr unless there's a kernel page here)
*/
uint64 mmap_find_free_area(pagetable_t table, uint64 required_pages, uint64 baseAddr, int fixed, int noOverwrite) {

    int working = 1;
    int pageIssue = 0;
    while (working) {
        pageIssue = 0;
        for(int i = 0; i < required_pages; i++) {
            if (baseAddr + (i * PGSIZE) > MAXVA) {
                if (fixed) {
                    return ENOMEM;
                }
                // If we reach max, just go to min again?
                baseAddr = (uint64)MMAP_MIN_ADDR;
            }
            // Gets entry
            pte_t* entry = walk(table, baseAddr + (i * PGSIZE), 0);
            
            // This entry is a valid entry
            if (entry && *entry & PTE_V) {
                // We don't want to use this specific address anyways, check next best
                if (!fixed) {
                    baseAddr += (i + 1) * PGSIZE;
                    pageIssue = 1;
                    break;
                }  
                // But wait, we don't want to overwrite
                else if(noOverwrite) {
                    return EEXIST;
                }
                // We want to overwrite, addr is not a hint, if it's not a user page return ERROR
                else if (!(*entry & PTE_U)){
                    return ENOMEM;
                }
                
            }
        }
        working = pageIssue;
    }
    // We get here after somehow exiting the while loop
    return baseAddr;
}

/**
 * Precondition checking
*/
uint64 is_valid_addr(uint64 addr) {
    return (addr % PGSIZE == 0) && ((void*)addr == NULL || addr >= (uint64)MMAP_MIN_ADDR) && (addr < MAXVA);
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

    if (addr == NULL) {
        addr = MMAP_MIN_ADDR;
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

    if (!(flags & MAP_POPULATE)) {
        // We shall ignore this for now
    }

    // MAP_SHARED sets PTE_SH page bit
    if (flags & MAP_SHARED) {
        entryProt |= PTE_SH;
    }

    // Here begins actual mapping

    pagetable_t curTable = mycpu()->proc->pagetable;

    #ifdef DEBUG_PT
    pr_debug("Start: ");
    print_pt(curTable, 3);
    #endif

    // Contiguous pages required
    uint64 required_pages = length / PGSIZE;

    // We find contig pages of the required length
    uint64 base = mmap_find_free_area(curTable, required_pages, (uint64) addr, flags & MAP_FIXED || flags & MAP_FIXED_NOREPLACE, flags & MAP_FIXED_NOREPLACE);

    // Happens when various errors are returned
    if (base < (uint64)MMAP_MIN_ADDR) {
        return base;
    }

    // go through the pages and map them
    // Since physical memory comes in PGSIZE chunks we need to loop here
    for (int i = 0; i < required_pages; i++) {
        
        void* curAlloc;
        struct buf* curBuf = NULL;
        // We might need to pre-0 the pages, kernel memset uses physical addresses
        if (flags & MAP_ANONYMOUS) {
            curAlloc = kalloc();

            if (curAlloc == NULL) {
                // kernel malloc error
                #ifdef DEBUG_ERRORS
                pr_debug("INTERN-MMAP-ENOMEM: Kernel alloc failed\n");
                #endif
                return ENOMEM;
            }
            memset((void*)curAlloc, 0, PGSIZE);
        }
        else {
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

        }

        // Insert into shared map
        if (flags & MAP_SHARED) {    
            // curBuf->data is never null
            if(acquire_or_insert_table(curBuf, curAlloc) == -1) {
                return ENOMEM;
            }
        }
        
        // VA of our current page
        uint64 curVA = base + (i * PGSIZE);

        // Goes through the table and gets old entries
        pte_t* prevEntry = walk(curTable, curVA, 1);
        if (*prevEntry != 0) {
            // dofree for now, MAP_SHARED might be a problem
            uvmunmap(curTable, curVA, 1, 1);
        }

        uint64 mapR = mappages(curTable, curVA, PGSIZE, (uint64)curAlloc, entryProt);
        // Return if mappages failed
        if (mapR) {
            #ifdef DEBUG_ERRORS
            pr_debug("INTERN-MMAP-ENOMEM: mappages fail\n");
            #endif
            return ENOMEM;
        }

    }

    #ifdef DEBUG_PT
    pr_debug("Result: %p: \n", base);
    print_pt(curTable, 3);
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

    uint64 nPages = PGROUNDUP(length) / PGSIZE;

    pagetable_t curTable = mycpu()->proc->pagetable;

    // Find the appropriate entry
    pte_t* tableEntry = walk(curTable, (uint64)addr, 0);
    uint64 intEntry = (uint64) *tableEntry;

    if (intEntry != 0 && (intEntry & PTE_V) && (intEntry & PTE_U) && (intEntry & PTE_MM)) {
        if (intEntry & PTE_SH) {
            uvmunmap(curTable, (uint64)addr, nPages, munmap_shared(PTE2PA(intEntry)));
        } else {
            // Invalidate mapping, and free memory
            uvmunmap(curTable, (uint64)addr, nPages, 1);
        }
    } else {
        return EEXIST;
    }

    return 0;
}