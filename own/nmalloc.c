#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

// Despite the numerous mentions of "Block", this is not Block allocation

#define NULL 0
#define BLOCKSIZE sizeof(Header)
#define BLOCKALIGN (BLOCKSIZE - 1)
//#define DEBUG_BDMALLOC
//#define DEBUG_BDFREE
//#define DEBUG_ANCHOR
//#define DEBUG_TRAVERSAL
//#define DEBUG_GROWMEM
//#define DEBUG_ERRORS

typedef enum __dir{
    NONE = 0,
    LEFT = 1,
    RIGHT  = 2
} dir;

/**
 * Struct describing the headers used in tree
 * size: 16 byte
*/
typedef struct __header {
    // Splits on the left.
    // No free space if 0.
    // If 1 at least 1 free space of that level in that region
    // If entire side is free = level
    uint32 freeLeft;
    // Splits on the right
    // No free space if 0
    // If Bit=1 at least 1 free space of that level in that region.
    // If entire side is free = level
    uint32 freeRight;
    // Level in Blocks. Lowest Level is 2 x 16 Byte Regions aka 2 x 1 Block
    // (Meaning a Level 0001 is residing over 2x 16 Byte Regions, one left, one right)
    uint32 level;
    // Path required to get to header
    // Needed for free
    uint16 dirToParent;
    // Bitmap = LEFT | RIGHT when both sides directly allocated, LEFT when only left, RIGHT when only right
    // Needed for free
    uint16 allocDirs;
} Header;

// Prints all the information needed to debug a particular header
#define DEBUGHEADER(header) printf("ptr:%p lvl:%b, fl:%b, fr:%b, dir:%b, allocDirs:%b\n", (header), (header)->level, (header)->freeLeft, (header)->freeRight, (header)->dirToParent, (header)->allocDirs)


Header* base;
Header* anchor = NULL;

/**
 * Returns integer with only first set bit
 * Stolen from https://stackoverflow.com/a/3065433
*/
uint32 only_fs(uint32 num) {
    uint32 l = 0;
    while (num >>= 1) { ++l; }
    return 1 << l;
}

/**
 * Goes up the structure to update free flags
 * uHeader: previously updated Header
*/
void updateFreeFlags(Header* uHeader){
    #ifdef DEBUG_BDMALLOC
    printf("Start flag update\n");
    #endif
    // uHeader is never anchor, because there is no parent of anchor
    while (uHeader != anchor) {
        // Flag for upper level is the OR of left and right
        uint32 nFlag = uHeader->freeLeft | uHeader->freeRight;
        // Pointer to parent Header
        Header* parHeader;

        // parHeader is set according to direction the parent is
        // and proper free flags are set
        if (uHeader->dirToParent == RIGHT) {
            parHeader = uHeader + (uHeader->level << 1);
            parHeader->freeLeft = nFlag;
        } else {
            parHeader = uHeader - (uHeader->level << 1);
            parHeader->freeRight = nFlag;
        }
        // Debugging stuff
        #ifdef DEBUG_BDMALLOC
        printf("uHeader: ");
        DEBUGHEADER(uHeader);
        printf("parHeader: ");
        DEBUGHEADER(parHeader);
        #endif
        // Go up one level by setting the last updated Header to the parent
        uHeader = parHeader;
    }
}

void mergeRegion(Header* toMerge) {
    // Is the entire region free?
    #ifdef DEBUG_BDFREE
        printf("BDFREE-Merging: ");
        DEBUGHEADER(toMerge);
    #endif
    while (toMerge != anchor && toMerge->freeLeft == toMerge->freeRight && toMerge->freeLeft == toMerge->level) {
        #ifdef DEBUG_BDFREE
            printf("BDFREE-Merging-Cont: ");
            DEBUGHEADER(toMerge);
        #endif
        if (toMerge->dirToParent == LEFT) {
            toMerge = toMerge - (toMerge->level << 1);
            toMerge->freeRight = toMerge->level;
        } else {
            toMerge = toMerge + (toMerge->level << 1);
            toMerge->freeLeft = toMerge->level;
        }
    }
    updateFreeFlags(toMerge);
}

/**
 * Frees memory allocated with BDMALLOC
 * And merges sides if both are free (TODO)
*/
void free(void* address) {
    // Check the left side first
    // Header potentially responsible for the address
    Header* respHeader = ((Header*) address) - 1;

    if (address == NULL || respHeader < (base - 1)) {
        #ifdef DEBUG_BDFREE
        printf("BDFREE-NOTFREED: anch:%p, base:%p, ptr:%p\n", anchor, base, address);
        #endif
        return;
    }
    
    // Are we to the right of the left header?
    if (respHeader->allocDirs & RIGHT) {
        // Congrats, we are done.
        // Merging after the branch
        respHeader->allocDirs ^= RIGHT;
        respHeader->freeRight = respHeader->level;
        #ifdef DEBUG_BDFREE
        printf("BDFREE: leftH: ");
        DEBUGHEADER(respHeader);
        #endif
    } else {
        // Are we at the very left? In that case our left side is (base - 1), so we go to anchor and left
        // Else we go to the right of the header on our left
        #ifdef DEBUG_BDFREE
        printf("BDFREE: respHeader:%p, base-1:%p\n", respHeader, base -1);
        #endif
        respHeader = respHeader != (base - 1) ? respHeader + respHeader->level : anchor;
        #ifdef DEBUG_BDFREE
        printf("BDFREE: respHeader: ");
        DEBUGHEADER(respHeader);
        #endif
        while(!(respHeader->allocDirs & LEFT)) {
            // Go down one level to the left
            #ifdef DEBUG_BDFREE
            printf("BDFREE-searchLeft: cHeader: ");
            DEBUGHEADER(respHeader);
            #endif
            if (respHeader->level == 0) {
                #ifdef DEBUG_ERRORS
                printf("BDFREE-NOTFREED-Left: NotFound\n");
                #endif
                return;
            }
            respHeader = respHeader - respHeader->level;
        }
        // Congrats! We found our header.
        respHeader->allocDirs ^= LEFT;
        respHeader->freeLeft = respHeader->level;
        #ifdef DEBUG_BDFREE
            printf("BDFREE: rightH: ");
            DEBUGHEADER(respHeader);
        #endif
    }
    if (respHeader->level == 0) {
        #ifdef DEBUG_ERRORS
        printf("BDFREE-NOTFREED-Left: NotFound\n");
        #endif
        return;
    }
    mergeRegion(respHeader);
    #ifdef DEBUG_ANCHOR
    printf("BDFREE-End: ");
    DEBUGHEADER(anchor);
    #endif
}

/**
 * Creates headers needed for consistent data structure
 * Returns pointer to new anchor
*/
Header* fixDatastructure(uint32 highestLevel) {
    // Previous header in loop
    // Usually starts at anchor
    Header* prevHeader = anchor;

    #ifdef DEBUG_GROWMEM
    printf("FIXDS: Anchor_lvl: %b, required_lvl: %b\n", prevHeader->level, highestLevel);
    #endif
    // Goes through all the levels
    // highestLevel will be the new anchor and the anchor never has a parent -> break condition
    while(prevHeader->level < highestLevel) {
        #ifdef DEBUG_GROWMEM
        printf("FIXDS: prevHeader: ");
        DEBUGHEADER(prevHeader);
        #endif
        // The level of the new Header
        uint32 nLevel = (prevHeader->level << 1);
        // new Header. Like in updateFreeFlags we need to get to the next Header by adding the level of the new Header
        // Since newly allocated space is always "to the right" there's not problem here
        Header* nHeader = prevHeader + nLevel;
        nHeader->allocDirs = NONE;
        nHeader->dirToParent = NONE;
        nHeader->freeLeft = prevHeader->freeLeft | prevHeader->freeRight;
        nHeader->freeRight = nLevel;
        nHeader->level = nLevel;

        // Update dir to Parent of prevHeader
        prevHeader->dirToParent = RIGHT;
        #ifdef DEBUG_GROWMEM
        printf("FIXDS: nHeader: ");
        DEBUGHEADER(nHeader);
        #endif
        // Start next iteration
        prevHeader = nHeader;
    }
    return prevHeader;
}

/**
 * Buddy allocator memory grower.
 * Allocates correct amount of memory needed to place all headers and validates data structure
 * Returns Pointer to new Anchor OR NULL on error
*/
Header* bgrowmemory(uint32 requiredLevel) {
    // Some power of 2 that is passed to sbrk
    uint32 requiredBlocks;
    // If we couldn't find enough space in the anchor, we double our size
    if (anchor->level >= requiredLevel) {
        requiredLevel = anchor->level << 1;
    }
    // This formula is ver important and I would like to thank 
    // https://math.stackexchange.com/questions/355963/ for their addition to the internet
    // i.e the formula blocks/level = (level << 2) -1
    // formula for new blocks with existing anchor = ((requiredLevel << 2) - 1) - ((anchor->level << 2) - 1)
    // Simplified to the following
    requiredBlocks = (requiredLevel << 2) - (anchor->level << 2);
    #ifdef DEBUG_GROWMEM
    printf("GROMEMORY: nLevel: %b, newBlocks:%d, existing:%d, total:%d, bytes:%d\n", requiredLevel, requiredBlocks, ((anchor->level << 2) - 1), ((requiredLevel << 2) - 1), requiredBlocks * BLOCKSIZE);
    #endif
    // This entire allocator lives on the assumption that sbrk returns contiguous memory
    // But here's error handling in case it doesn't provide new memory at all#
    // We don't need to fix alignment here, cuz we assume there are no sbrk calls in between
    long rVal = (uint64) sbrk((requiredBlocks * BLOCKSIZE));
    if (rVal == -1) {
        return NULL;
    }

    return fixDatastructure(requiredLevel);
}

/**
 * Assumption:
 * We always have more free blocks than nBlock
*/
Header* traverseBestFit(uint32 requiredLevel, dir* rDir) {
    // Travel down the tree
    // We need to find a region of the correct size
    uint32 nextBestLevel = requiredLevel;
    Header* uHeader = anchor;
    // If such a region does not exist, we'll take the next best
    while (!((uHeader->freeLeft | uHeader->freeRight) & nextBestLevel)) {
        nextBestLevel <<= 1;
    }
    #ifdef DEBUG_TRAVERSAL
    printf("BDMALLOC-TRAVERSAL-NBL: nbl: %b, req: %b\n", nextBestLevel, requiredLevel);
    #endif
    // We found the next best level:
    // Now we traverse the tree to this location
    while (uHeader->level != nextBestLevel){
        if (uHeader->freeLeft & nextBestLevel) {
            uHeader = uHeader - uHeader->level;
        } else if (uHeader->freeRight & nextBestLevel){
            uHeader = uHeader + uHeader->level;
        } 
    }

    // We found the last occupied node
    // Now we check if we can directly allocate here
    // or if we need to split
    #ifdef DEBUG_TRAVERSAL
    printf("BDMALLOC-TRAVERSAL-LON: req:%b, node: ",requiredLevel);
    DEBUGHEADER(uHeader);
    #endif

    // We can allocate here?
    if (requiredLevel == nextBestLevel) {
        // Set return vars and return
        if (uHeader->freeLeft & requiredLevel) {
            *rDir = LEFT;
        } else if (uHeader->freeRight & requiredLevel) {
            *rDir = RIGHT;
        } else {
            #ifdef DEBUG_ERRORS
            printf("BDMALLOC-TRAVERSAL-ERROR\n");
            #endif
        }
        return uHeader;
    // We need to split
    } 
    else {
        // Check if we need to  branch right on last node
        if (!(uHeader->freeLeft & nextBestLevel) && uHeader->freeRight & nextBestLevel) {
            nextBestLevel >>= 1;
            uHeader = uHeader + uHeader->level;
            uHeader->freeLeft    = nextBestLevel;
            uHeader->freeRight   = nextBestLevel;
            uHeader->level       = nextBestLevel;
            uHeader->dirToParent = LEFT;
            uHeader->allocDirs   = NONE;
            #ifdef DEBUG_TRAVERSAL
            printf("BDMALLOC-TRAVERSAL-RSplit: ");
            DEBUGHEADER(uHeader);
            #endif
        }

        // Build new left nodes
        while (requiredLevel != nextBestLevel) {
            nextBestLevel >>= 1;
            uHeader = uHeader - uHeader->level;
            uHeader->freeLeft    = nextBestLevel;
            uHeader->freeRight   = nextBestLevel;
            uHeader->level       = nextBestLevel;
            uHeader->dirToParent = RIGHT;
            uHeader->allocDirs   = NONE;
            #ifdef DEBUG_TRAVERSAL
            printf("BDMALLOC-TRAVERSAL-Splits: ");
            DEBUGHEADER(uHeader);
            #endif
        }
    }
    *rDir = LEFT;
    // At the end of this loop uHeader is the header we want
    // We'll set freeLeft to its proper value once we've actually allocated the memory
    return uHeader;
}

/**
 * Initializes anchor and base
*/
void binit(uint32 requiredLevel) {
    // Let's try to keep initial size at a reasonable level
    #ifndef DEBUG_GROWMEM
    // Allocate at least 2 x 2048+ Byte Regions
    //contigStorableBytes = level << 4;
    if (requiredLevel < 0x80) {
        requiredLevel = 0x80;
    }
    #endif
    uint32 alignFix = 0x10 - (BLOCKALIGN & (uint64)sbrk(0));
    // Amount of bytes requested from sbrk
    uint32 allocSize = ((requiredLevel << 2) - 1) * BLOCKSIZE;
    // Value returned by sbrk. Address of previous memory area end.
    uint64 rVal = (uint64) sbrk(allocSize + alignFix);
    #if defined(DEBUG_GROWMEM) || defined(DEBUG_ANCHOR)
    printf("BDMALLOC-binit: rs:%p af:%x, blocks:%d, byte:%d\n", rVal, alignFix, allocSize / BLOCKSIZE, allocSize);
    #endif
    if (rVal == -1) {
        #ifdef DEBUG_ERRORS
        printf("BDMALLOC-CRITICAL-ERROR: NO ANCHOR");
        #endif
        return;
    }
    anchor = (Header*) ((rVal + allocSize + alignFix) - ((requiredLevel << 1) * BLOCKSIZE));
    anchor->freeLeft    = requiredLevel; 
    anchor->freeRight   = requiredLevel;
    anchor->level       = requiredLevel;
    anchor->dirToParent = NONE; 
    anchor->allocDirs   = NONE;
    base = (Header*) (rVal + alignFix);
    #ifdef DEBUG_GROWMEM
    printf("BDMALLOC-binit: nAnchor:%p, base:%p\n", anchor, base);
    DEBUGHEADER(anchor);
    #endif
}
/**
 * Buddy malloc
*/
void* malloc(uint32 nBytes) {
    if(nBytes == 0) {
        return NULL;
    }

    uint32 requiredLevel = only_fs(nBytes / BLOCKSIZE);
    // There's no anchor yet.
    // We can rebuild him, we have the technology
    if(anchor == NULL) { 
        binit(requiredLevel);
        if (anchor == NULL) {
            #ifdef DEBUG_ERRORS
            printf("BDMALLOC-CRITICAL-ERROR: NO ANCHOR");
            #endif
            return NULL;
        }
    }
    // If we need more than we have ready
    if ((anchor->freeLeft < requiredLevel && anchor->freeRight < requiredLevel)) {
        // Grow memory and validates data structure
        Header* nAnchor = bgrowmemory(requiredLevel);
        if (nAnchor == NULL) {
            #ifdef DEBUG_ERRORS
            printf("BDMALLOC-ALLOC-ERROR: Not enough free space\n");
            #endif
            return NULL;
        }
        anchor = nAnchor;
    }

    // Now we definitely have a region that's big enough for us.
    // Just gotta find it somewhere.
    dir allocDir = NONE;
    
    // Now we find a space. We can change the strategy. (But why would we)
    Header* foundSpace = traverseBestFit(requiredLevel, &allocDir);
    // Address returned by BDMALLOC
    void* mallocSpace;

    // Calculate required address and set freeLeft and freeRight
    if (foundSpace == NULL) {
        #ifdef DEBUG_ERRORS
        printf("BDMALLOC-ALLOC-ERROR: traverseBestFit returned NULL\n");
        #endif
        return NULL;
    } else if (allocDir == RIGHT) {
        mallocSpace = foundSpace + 1;
        foundSpace->freeRight = 0;
    } else if (allocDir == LEFT){
        mallocSpace = foundSpace - ((requiredLevel << 1) - 1);
        foundSpace->freeLeft = 0;
    } else {
        #ifdef DEBUG_ERRORS
        printf("BDMALLOC-ALLOC-ERROR: traverseBestFit returned NONE dir\n");
        #endif
        return NULL;
    }

    foundSpace->allocDirs |= allocDir;
    updateFreeFlags(foundSpace);
    // Path we took to get to free space.
    // Used to correctly set bits at every level again.
    #ifdef DEBUG_BDMALLOC
    printf("BDMALLOC-Found: ");
    DEBUGHEADER(foundSpace);
    #endif
    #ifdef DEBUG_ANCHOR
    printf("BDMALLOC-End: ");
    DEBUGHEADER(anchor);
    #endif
    return mallocSpace;
}