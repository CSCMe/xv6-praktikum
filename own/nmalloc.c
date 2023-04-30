#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

#define NULL 0
#define BLOCKSIZE sizeof(Header)
//#define DEBUG_BMALLOC

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
#define DEBUGHEADER(header) printf("ptr:%p fl:%b, fr:%b, lvl:%b, dir:%b, allocDirs:%b\n", header, header->freeLeft, header->freeRight, header->level, header->dirToParent, header->allocDirs)

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
    #ifdef DEBUG_BMALLOC
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
        #ifdef DEBUG_BMALLOC
        printf("uHeader: ");
        DEBUGHEADER(uHeader);
        printf("parHeader: ");
        DEBUGHEADER(parHeader);
        #endif
        // Go up one level by setting the last updated Header to the parent
        uHeader = parHeader;
    }
}

/**
 * Frees memory allocated with bmalloc
 * And merges sides if both are free
 * TBI
*/
uint32 bfree(void* address) {
    uint32 leftLevel = anchor->level;
    (void) leftLevel;
    if (address > 0) {

    }
    return 0;
}

/**
 * Creates headers needed for consistent data structure
 * Returns pointer to new anchor
*/
Header* fixDatastructure(uint32 highestLevel) {
    // Previous header in loop
    // Usually starts at anchor
    Header* prevHeader = anchor;

    #ifdef DEBUG_BMALLOC
    printf("Anchor: %b, required: %b\n", prevHeader->level, highestLevel);
    #endif
    // Goes through all the levels
    // highestLevel will be the new anchor and the anchor never has a parent -> break condition
    while(prevHeader->level < highestLevel) {
        #ifdef DEBUG_BMALLOC
        printf("fixDatastructure: prevHeader: ");
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
    #ifdef DEBUG_BMALLOC
    printf("GROMEMORY: nLevel: %b, newBlocks:%d, existing:%d, total:%d, bytes:%d\n", requiredLevel, requiredBlocks, ((anchor->level << 2) - 1), ((requiredLevel << 2) - 1), requiredBlocks * BLOCKSIZE);
    #endif
    // This entire allocator lives on the assumption that sbrk returns continuous memory
    // But here's error handling in case it doesn't provide new memory at all
    long rVal = (uint64) sbrk(requiredBlocks * BLOCKSIZE);
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
    // Header for current Level
    Header* cLevel = anchor;

    // Somehow travel to right location
    if (!((cLevel->freeLeft | cLevel->freeRight) & requiredLevel)) {
        // We haven't found a correct size, so we gotta create one.
        // First we'll need to find a region 1 bigger than our required level so we can split that.
        dir region = NONE;
        Header* uHeader = traverseBestFit(requiredLevel << 1, &region);

        // Great. We found a header a size larger than our required one.
        // Create new header:
        Header* nHeader;
        if (region == LEFT) {
            nHeader = uHeader - uHeader->level;
            nHeader->dirToParent = RIGHT;
            uHeader->freeLeft = requiredLevel;
        } else if (region == RIGHT) {
            nHeader = uHeader + uHeader->level;
            nHeader->dirToParent = LEFT;
            uHeader->freeRight = requiredLevel;
        } 
        // This case should be impossible in actual execution, but you never know
        else {
            printf("BMALLOC-TRAVERSAL-ERROR: Creating new headers, returned NONE dir\n");
            DEBUGHEADER(uHeader);
            return NULL;
        }

        nHeader->level       = requiredLevel;
        nHeader->freeLeft    = requiredLevel;
        nHeader->freeRight   = requiredLevel;
        // We'll set freeLeft to its proper value once we've actually allocated the memory

        // Update header above uHeader free flags and those above it
        // This part is the worst performance offender.
        // Maybe fix?
        updateFreeFlags(uHeader);
    } 

    *rDir = NONE;
    Header* nLevel = cLevel;
    // Traverse down the path, because now there should be a free block of the right size
    // Once the correct level has been found, return
    do {
        cLevel = nLevel;
        // This assertion is the basis of the entire data structure.
        // If it fails we're fucked
        if(!((cLevel->freeLeft | cLevel->freeRight) & requiredLevel)) {
            printf("BMALLOC-TRAVERSAL-ERROR: Assertion fail: Structure corrupt\n");
            printf("REQ:%b, L:%b, R:%b, PT:%p", requiredLevel, cLevel->freeLeft, cLevel->freeRight, cLevel);
            return NULL;
        }

        // Reached bottom of Buddy Allocator and haven't found correct size.
        // Should be impossible (famous last words)
        if(cLevel->level == 1 && requiredLevel != 1) {
            printf("BMALLOC-TRAVERSAL-ERROR: Could not find Block of correct size: Structure corrupt\n");
            return NULL;
        }

        // Free size of required level on left?
        if (cLevel->freeLeft & requiredLevel) {
            // This formula arises from the neat property of level data
            // As I found out, getting from an upper level to a lower one is always +- the current level (in its 100) representation
            // Level 1 (16 Byte) +- 1 (001) to get to smallest data block
            // Level 2 (32 Byte) +- 2 (010) to get to header of level 1
            // Level 3 (64 Byte) +- 4 (100) to get to header of level 2
            // aso.
            nLevel = cLevel - cLevel->level;
            *rDir = LEFT;
        } 
        // Else it *must* be on the right.
        // This should be guaranteed by the assertion but we have a case if that fails
        else if (cLevel->freeRight & requiredLevel) {
            nLevel = cLevel + cLevel->level;
            *rDir = RIGHT;
        }
    } while (!(cLevel->level & requiredLevel));

    return cLevel;
}

void* binit(uint32 requiredLevel) {
    uint64 allocSize = ((requiredLevel << 2) - 1) * BLOCKSIZE;
    uint64 rVal = (uint64) sbrk(allocSize);
    #ifdef DEBUG_BMALLOC
    printf("BMALLOC-binit: %p\n", rVal);
    #endif
    if (rVal == -1) {
        printf("BMALLOC-CRITICAL-ERROR: NO ANCHOR");
        return NULL;
    }
    Header* nAnchor = (Header*) ((rVal + allocSize) - ((requiredLevel << 2) * BLOCKSIZE));
    #ifdef DEBUG_BMALLOC
    printf("BMALLOC-binit: New Anchor at:%p\n", nAnchor);
    #endif
    nAnchor->freeLeft    = requiredLevel; 
    nAnchor->freeRight   = requiredLevel;
    nAnchor->level       = requiredLevel;
    nAnchor->dirToParent = NONE; 
    nAnchor->allocDirs   = NONE;
    return nAnchor;
}
/**
 * Buddy malloc
*/
void* bmalloc(uint32 nBytes) {
    uint32 requiredLevel = only_fs(nBytes / BLOCKSIZE);

    // There's no anchor yet.
    // We can rebuild him, we have the technology
    if(anchor == NULL) { 
        anchor = binit(requiredLevel);
        if (anchor == NULL) {
            printf("BMALLOC-CRITICAL-ERROR: NO ANCHOR");
            return NULL;
        }
    }
    // If we need more than we have ready
    if ((anchor->freeLeft < requiredLevel && anchor->freeRight < requiredLevel)) {
        // Grow memory and validates data structure
        Header* nAnchor = bgrowmemory(requiredLevel);
        if (nAnchor == NULL) {
            printf("BMALLOC-ALLOC-ERROR: Not enough free space\n");
            return NULL;
        }
        anchor = nAnchor;
    }

    // Now we definitely have a region that's big enough for us.
    // Just gotta find it somewhere.
    dir allocDir = NONE;
    
    // Now we find a space. We can change the strategy. (But why would we)
    Header* foundSpace = traverseBestFit(requiredLevel, &allocDir);
    // Address returned by bmalloc
    void* mallocSpace;

    // Calculate required address and set freeLeft and freeRight
    if (foundSpace == NULL) {
        printf("BMALLOC-ALLOC-ERROR: traverseBestFit returned NULL\n");
        return NULL;
    } else if (allocDir == RIGHT) {
        mallocSpace = foundSpace + 1;
        foundSpace->freeRight = 0;
    } else if (allocDir == LEFT){
        mallocSpace = foundSpace - ((requiredLevel << 1) - 1);
        foundSpace->freeLeft = 0;
    } else {
        printf("BMALLOC-ALLOC-ERROR: traverseBestFit returned NONE dir\n");
        return NULL;
    }

    foundSpace->allocDirs |= allocDir;
    updateFreeFlags(foundSpace);
    // Path we took to get to free space.
    // Used to correctly set bits at every level again.
    
    return mallocSpace;
}

/**
 * For testing and setup
*/
void main(int argc, char** argv) {

    bmalloc(4096 * BLOCKSIZE);
    bmalloc(1);
    DEBUGHEADER(anchor);
}