#include "own/alloc.h"
#include "user/bmalloc.h"

//#define DEBUG_BMALLOC_ALIGN
#define DEBUG_BMALLOC_FREE

void setup_balloc() 
{
    setup_malloc();
}

void block_free(struct block block)
{
    // Do calculation to get pointer from regular malloc back
    if (block.begin == NULL) {
        return;
    }
    void** back = (void*)((uint64)block.begin - sizeof(void*) - ((uint64)block.begin % sizeof(void*)));

    free(*back);
}

/*
 * Allocates
 * size: Size in bytes
 * align: Alignment in bytes
 */
struct block block_alloc(uint32 size, uint32 align)
{   
    // Create empty block
    struct block output = {.align=0, .begin=NULL,.size=0};

    uint64 newSize = size + align + sizeof(void**);
    // If required space too large: Abort
    if (newSize > (uint)-1 || align == 0) {
        return output;
    }

    Header* start = malloc(newSize);
    if (start == NULL) {
        return output;
    }

    // Modulo align to get bytes over alignment
    uint64 alignmentError = ((uint64)start + sizeof(void**)) % align;

    // Second % align so we have 0 in case we're already aligned, gets us bytes we have to add to get to desired alignment
    uint64 alignmentFix = (align - alignmentError) % align;
    
    output.begin = (void**)((uint64) start + sizeof(void**) + alignmentFix);
    output.size = size;
    output.align = align;

    void** back = (void**)((uint64)output.begin - sizeof(void**) - ((uint64)output.begin % sizeof(void**)));
    *back = start;

    return output;

}