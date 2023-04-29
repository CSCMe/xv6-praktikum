#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

/**
 * Since malloc calls need to be 16 Byte aligned when objects are larger than 16 Byte,
 * we can have a struct with 16 Byte size so we keep that alignment
 * Meaning we have a minimul allocation size of 32 Byte, just like before
*/

/**
 * What do we need in this data structure?
 * Level and occupied data
 * occupied is a bitmap in this case, detailing the different levels and what is free.
 * EXAMPLE:
 * Let's say we have a header at 64 byte.
 * We have allocated a section of 16 byte
 * Meaning our buddy allocator has a free 16 byte and 32 byte section.
 * meaning our occupied flag is [011] because we have 0 64 byte sections, 1 32 byte section, 1 16 byte section
 * Examplle 128 byte section:
 * Allocated 16 byte.
 * We got:
 * 1 free 16
 * 1 free 32
 * 1 free 64
 * 0 free 128
 * occupied flag is [0111], while size is [1000],
 * ANOTHER EXAMPLE:
 * We have 128 byte section
 * Allocated 16 byte, 16 byte, 64 byte, 16 byte
 * We have:
 * 1 free 16
 * 0 free 32
 * 0 free 64
 * 0 free 128
 * occupied flag is [0001]
 * 
 * 
 * Meaning if occupiedFlag == level we gucci and have the entire thingy free
*/

// Structure in Memory:
/**
 * 64 Byte Space, with additional 67 Byte for Data structure
 * 64 Byte Header:
 * At address
*/

/**
 * Header for everything over 32 byte
*//*
typedef struct __header {
    uint32 level;
    uint32 occupiedFlag;

} Header;
*/


//Think about header structure later.
// For now think about validity of concept

#define NULL 0
#define BLOCKSIZE 16

// Due to the funny nature of the data structure,
// nBlocks of 1 Block smaller than the next level always fit in the lower one.
// What does this mean?
// EXAMPLE:
// Allocation: 7 Blocks can fit in a 4 Block (64 Byte) region.
// Because the additional 3 Blocks of Data structure are not required since the entire region is getting allocated.
// Meaning we can fit size 111 into a 100 Region.
// This means to get the level of a specific amount of blocks, we just need to mask for the most significant bit
// Please tell me if there's a better solution
#define BLOCKSTOLEVEL(nBlock) 

typedef enum __dir{
    LEFT = 0,
    RIGHT  = 1,
    NONE = 2
} dir;

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
    // 0 if branch right, 1 if branch left
    dir dirToParent;
} Header;

#define DEBUGHEADER(header) printf("fl:%b, fr:%b, lvl:%b, dir:%b\n", header->freeLeft, header->freeRight, header->level, header->dirToParent)


Header* anchor;

/**
 * Returns integer with only first set bit
*/
uint32 only_fs(uint32 num) {
    uint32 l = 0;
    while (num >>= 1) { ++l; }
    return 1 << l;
}

void updateFreeFlags(Header* uHeader){
    while (uHeader != anchor) {
        uint32 nFlag = uHeader->freeLeft | uHeader->freeRight;
        Header* pHeader;
        if (uHeader->dirToParent == RIGHT) {
            pHeader = uHeader + (uHeader->level << 1);
            pHeader->freeLeft = nFlag;
        } else {
            pHeader = uHeader - (uHeader->level << 1);
            pHeader->freeRight = nFlag;
        }
        printf("uHeader:%p, pHeader:%p\n", uHeader, pHeader);
        uHeader = pHeader;
    }
}

/**
 * Buddy allocator memory grower.
 * Allocates correct amount of memory needed to place all headers (i.e. 15 Blocks for 4 Blocks of user memory);
 * And validates data structure
 * Returns Pointer to new Anchor OR NULL on error
*/
Header* bgrowmemory(uint32 nBlocks) {
    return NULL;
}

/**
 * Assumption:
 * We always have more free blocks than nBlock
*/
Header* traverseBestFit(uint32 nBlock, dir* rDir) {
    // Header for current Level
    Header* cLevel = anchor;
    // Somehow travel to right location
    uint32 requiredLevel = only_fs(nBlock);
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
        } else {
            nHeader = uHeader + uHeader->level;
            nHeader->dirToParent = LEFT;
            uHeader->freeRight = requiredLevel;
        }

        nHeader->level       = requiredLevel;
        nHeader->freeLeft    = requiredLevel;
        nHeader->freeRight   = requiredLevel;
        // We'll set freeLeft to its proper value once we've actually allocated the memory

        // Update header above uHeader free flags and those above it
        updateFreeFlags(uHeader);
    } 

    // Traverse down the path, because now there should be a free block of the right size
    // Once the correct level has been found, return
    while (!(cLevel->level & requiredLevel)) {

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
            cLevel = cLevel - cLevel->level;
            *rDir = LEFT;
        } 
        // Else it *must* be on the right.
        // This should be guaranteed by the assertion but we have a case if that fails
        else if (cLevel->freeRight & requiredLevel) {
            cLevel = cLevel + cLevel->level;
            *rDir = RIGHT;
        }
    }

    return cLevel;
}


/**
 * Buddy malloc
*//*
void* bmalloc(uint32 nBytes) {
    printf("bMALLOC: ");
    uint32 nBlocks = nBytes / BLOCKSIZE;
    printf("nBytes: %l -> %l Blocks, ", nBytes, nBlocks);

    // POTBUG: Equation might not be correct.
    if (anchor->freeLeft < nBlocks && anchor->freeRight < nBlocks) {
        Header* nAnchor = bgrowmemory(nBlocks);
        if (nAnchor == NULL) {
            return NULL;
        }
        anchor = nAnchor;
    }

    // Now we definitely have a region that's big anough for us.
    // Just gotta find it somewhere.
    Header* foundSpace = traverseBestFit(nBlocks);

    // Path we took to get to free space.
    // Used to correctly set bits at every level again.
    

    printf("\n");
}
*/

Header testCase[15] = {0};

#define NORMALIZEPOINTERPOS(pointer, base) ((void*) pointer - (void*)base)
/**
 * Buddy free
*/
void main(int argc, char** argv) {
    Header* anchorTest = testCase + BLOCKSIZE * (0b100 - 1);
    anchorTest->freeLeft  = 0b100;
    anchorTest->freeRight = 0b100;
    anchorTest->level     = 0b100;
    anchorTest->dirToParent = NONE;/*
    Header* h2 = anchorTest - anchorTest->level;
    h2->freeLeft  = 0b01;
    h2->freeRight = 0b10;
    h2->level     = 0b10;
    h2->dirToParent      = 0b0;
    Header* h1    = h2 - h2->level;
    h1->freeLeft  = 0b0;
    h1->freeRight = 0b1;
    h1->level     = 0b1;
    h1->dirToParent      = 0b0;
    (void)h1;*/
    printf("%p, %p, %p\n", anchorTest, testCase, NORMALIZEPOINTERPOS(anchorTest, testCase));
    anchor = anchorTest;
    DEBUGHEADER(anchor);
    dir ahh;
    Header* test = traverseBestFit(3, &ahh);
    printf("%p, %p, %p\n", test, testCase, NORMALIZEPOINTERPOS(test, testCase));
    DEBUGHEADER(anchor);
    test = traverseBestFit(1, &ahh);
    printf("%p, %p, %p\n", test, testCase, NORMALIZEPOINTERPOS(test, testCase));
    DEBUGHEADER(anchor);
}