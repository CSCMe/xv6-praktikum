#include "own/alloc.h"

// Despite the numerous mentions of "Block", this is not Block allocation
//#define DEBUG_BDMALLOC
//#define DEBUG_BDFREE
//#define DEBUG_ANCHOR
//#define DEBUG_TRAVERSAL
//#define DEBUG_GROWMEM
//#define DEBUG_ERRORS


/**
 * Goes up the structure to update free flags
 * uHeader: previously updated Header
*/
void updateFreeFlags(Header* uHeader, Header* anchor){
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

void bu_merge(Header* toMerge, Header* anchor) {
    // Is the entire region free?
    #ifdef DEBUG_BDFREE
        printf("BDFREE-Merging: ");
        DEBUGHEADER(toMerge);
    #endif
    while (toMerge != anchor && (toMerge->freeLeft == toMerge->freeRight && toMerge->freeLeft == toMerge->level)) {
        #ifdef DEBUG_BDFREE
            printf("BDFREE-Merging-Cont: ");
            DEBUGHEADER(toMerge);
        #endif
        uint32 nextLevel = (toMerge->level << 1);
        if (toMerge->dirToParent == LEFT) {
            *toMerge = (Header){0};
            toMerge = toMerge - nextLevel;
            toMerge->freeRight = nextLevel;
        } else {
            *toMerge = (Header){0};
            toMerge = toMerge + nextLevel;
            toMerge->freeLeft = nextLevel;
        }
    }
    updateFreeFlags(toMerge, anchor);
}

/**
 * Frees memory allocated with BDMALLOC
 * Merges sides if both are free
*/
void bufree(void* address, Header* anchor, Header* base) {
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
    bu_merge(respHeader, anchor);
    #ifdef DEBUG_ANCHOR
    printf("BDFREE-End: ");
    DEBUGHEADER(anchor);
    #endif
}

/**
 * Creates headers needed for consistent data structure
 * Returns pointer to new anchor
*/
Header* bu_fix(uint32 highestLevel, Header* anchor) {
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
        *nHeader = (Header){
            .allocDirs = NONE, 
            .dirToParent = NONE, 
            .freeLeft= prevHeader->freeLeft | prevHeader->freeRight,
            .freeRight = nLevel,
            .level = nLevel,
            };

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
 * Assumption:
 * We always have more free blocks than nBlock
*/
Header* traverseBestFit(uint32 requiredLevel, dir* rDir, Header* anchor) {
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
            uHeader -= uHeader->level;
        } else if (uHeader->freeRight & nextBestLevel){
            uHeader += uHeader->level;
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
 * Buddy malloc
*/
void* bumalloc(uint32 requiredLevel, Header* anchor) {

    // Now we definitely have a region that's big enough for us.
    // Just gotta find it somewhere.
    dir allocDir = NONE;
    
    // Now we find a space. We can change the strategy. (But why would we)
    Header* foundSpace = traverseBestFit(requiredLevel, &allocDir, anchor);
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
    updateFreeFlags(foundSpace, anchor);
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