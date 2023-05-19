#include "own/alloc.h"
/**
 * IDEA: There*s a list of buddy managers, each pointing to their own buddy manager with contiguous space
 * Once a buddy manager can't add more space (because of fragmentation or whatever), we try the next one in the list
*/
//#define DEBUG_ERRORS
//#define DEBUG_GROWMAN
//#define DEBUG_MANAGERS
//#define DEBUG_INIT

struct __managerList {
    BuddyManager* start;
    uint32 length;
};

struct __managerList managerList = {.start = NULL, .length = 0};

/**
 * Returns a pointer to manager the address resides in
*/
BuddyManager* get_responsible_manager(void* addr) {
    // Code to go through different managers and return the one with the matching address
    // Required for free
    #ifdef DEBUG_MANAGERS
    printf("MALLOC-DEBUG-GRM: ML.s:%p, ML.l%d\n", managerList.start, managerList.length);
    #endif
    BuddyManager* manager = managerList.start;
    for (int i = 0; i < managerList.length; i++) {
        #ifdef DEBUG_MANAGERS
        printf("MALLOC-DEBUG-GRM:B:%p, A:%p, E:%p\n", manager->base, manager->anchor, manager->end);
        #endif
        if ((void*)manager->end > addr && (void*)manager->base <= addr) {
            return manager;
        }
        manager++;
    }
    return NULL;
}


/**
 * Frees memory
 * Gets responsible manager and calls free for that specific buddy region
*/
void free(void* pointer) {
    if (pointer == NULL) {
        return;
    }
    BuddyManager* manager = get_responsible_manager(pointer); 
    if (manager == NULL) {
        return;
    }
    bufree(pointer, manager->anchor, manager->base);
    // Heck
    manager->freeMap = manager->anchor->freeLeft | manager->anchor->freeRight;
}


/**
 * Initializes stub anchor and base
*/
int manInit(uint32 requiredLevel, BuddyManager* manager) {
    // Allocate at least 2 x 2048+ Byte Regions
    //contigStorableBytes = level << 4;
    // Allocate at least one page, later with mmap this might be page aligned
    if (requiredLevel < 0x80) {
        requiredLevel = 0x80;
    }
    
    uint32 alignFix = HEADERSIZE - ((uint64)sbrk(0) % HEADERSIZE) % HEADERSIZE;

    // Set base pointer to start of aligned Area
    manager->base = (Header*) ((uint64)sbrk(alignFix) + alignFix);

    // Amount of bytes requested from sbrk
    uint64 allocSize = (((requiredLevel << 2) - 1) * HEADERSIZE);
    // Return Value of sbrk
    uint64 rVal = 0;

    // Let's keep it simple and assume we'll never need more than MAX_INT Memory
    // Check, just in case
    if (allocSize > MAX_INT) {
        // Abort
        #ifdef DEBUG_ERRORS
        printf("MALLOC-CRITICAL-GROW: Will never allocate more than MAX_INT bytes, but required.\n");
        #endif
        return -1;
    }

    // We allocate a region of memory (4 * Requiredlevel - 1 Blocks);
    // 2* because two regions, again 2* because that's how many additional blocks we need for headers
    rVal = (uint64) sbrk(allocSize);

    #ifdef DEBUG_GROWMAN
    printf("MALLOC-DEBUG-GROW: rs:%p af:%x, blocks:%lu, byte:%lu\n", rVal, alignFix, allocSize / HEADERSIZE, allocSize);
    #endif

    if (rVal == -1 || manager->base == (void*)((uint64)0 + alignFix) ) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-CRITICAL-GROW: Could not allocate anchor for new manager, assuming not enough memory\n");
        #endif
        // Set everything to 0, just in case
        manager->base = NULL;
        manager->anchor = NULL;
        manager->end = NULL;
        manager->canExpand = 0;
        manager->freeMap = 0;
        return -1;
    }

    // Calculate position of anchor & initialize it
    // It's always requiredLevel << 1 - 1 Blocks after the base
    manager->anchor = (manager->base + ((requiredLevel << 1) - 1));
    *(manager->anchor) = (Header) {
        .freeLeft    = requiredLevel,
        .freeRight   = requiredLevel,
        .level       = requiredLevel,
        .dirToParent = NONE,
        .allocDirs   = NONE,
    };
    // Set end of allocated region
    manager->end = (Header*) (rVal + allocSize); 
    // That's our assumption for now, can be proven wrong later
    manager->canExpand = 1;
    manager->freeMap = requiredLevel;

    #ifdef DEBUG_GROWMAN
    printf("MALLOC-DEBUG-GROW: base:%p, anchor:%p, end:%p\n", manager->base, manager->anchor, manager->end);
    DEBUGHEADER(manager->anchor);
    #endif
    return 0;
}


/**
 * Returns a pointer to the first manager that can fit the required level
*/
BuddyManager* get_and_init_manager(uint32 requiredLevel) {

    // Go through our managers and check if we can find one
    BuddyManager* manager = managerList.start;
    for (int i = 0; i < managerList.length; i++) {
        // Get first matching manager
        if (manager->freeMap >= requiredLevel || manager->canExpand) {
            return manager;
        }
        manager++;
    }

    // We're going to be mad if we fail at this point, but it could happen
    // Allocate memory for new manager list
    uint64 neededMem = PGROUNDUP(sizeof(BuddyManager) * (managerList.length + 1));

    void* rVal = mmap(NULL, neededMem, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (rVal == MAP_FAILED) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-CRITICAL-ERROR: Couldn't allocate space for new list\n");
        #endif
        return NULL;
    }

    BuddyManager* newStart = (BuddyManager*) (rVal);
    // Copy old list to new location
    // Not using mmove cuz I'm incapable of using it
    for (int i = 0; i < managerList.length; i++) {
        *(newStart + i) = *(managerList.start + i);
    }

    // Unmap previous managerList
    munmap( managerList.start, PGROUNDUP(sizeof(BuddyManager) * managerList.length));

    managerList.length++;
    managerList.start = newStart;

    // We need to initialize the manager
    // If we can't initialize a new manager with the right size, we'll never find one -> abort
    // Move this to get_and_init_manager
    BuddyManager* newManager = managerList.start + (managerList.length - 1);

    #ifdef DEBUG_INIT
    printf("MALLOC-DEBUG-GIM: listStart:%p, newManager:%p, manSize:%x \n",managerList.start, newManager, sizeof(BuddyManager));
    #endif

    if (manInit(requiredLevel, newManager)) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-MAJOR-ERROR: Couldn't initialize new manager\n");
        #endif
        // Critical Error, return NULL
        return NULL;
    }

    #ifdef DEBUG_MANAGERS
    printf("MALLOC-DEBUG-MOREMAN: pos:%p, b:%p, a:%p, e:%p, fM:%b\n", newManager, newManager->base, newManager->anchor, newManager->end, newManager->freeMap);
    #endif
    return newManager;
}


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
 * Grows a manager's buddy region
 * Lots of similarities to manInit, yet not the same
 * Return Values:
 * 0: Success
 * 1: Cannot grow: Not contiguous
 * 2: Cannot grow: sbrk error
 * 3: Cannot grow: Max alloc size
*/
int manGrow(uint32 requiredLevel, BuddyManager* manager) {
    // First, determine if this manager can even grow
    if (manager->canExpand == 0 || manager->end != (Header*)sbrk(0)) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-MINOR-GROW-ERROR: Cannot expand. End:%p Sbrk:%p\n", manager->end, (Header*)sbrk(0));
        #endif
        manager->canExpand = 0;
        return 1;
    }
    // Since we're now contiguous, which means our alignment is preserved, we don't need to worry about alignment

    // Do we need more space than we should?
    // I.e is our current region full
    if (manager->anchor->level >= requiredLevel) {
        requiredLevel = manager->anchor->level << 1;
    }

    // Formula for blocks/level = (level << 2) - 1
    // formula for new blocks with existing anchor = ((requiredLevel << 2) - 1) - ((anchor->level << 2) - 1)
    // Simplified to requiredLevel << 2 - anchor->level << 2
    uint64 requiredBlocks = (requiredLevel << 2) - (manager->anchor->level << 2);
    #ifdef DEBUG_GROWMAN
    printf("MALLOC-DEBUG-GROW: nLevel: %b, newBlocks:%d, existing:%d, total:%d, bytes:%d\n", requiredLevel, requiredBlocks, ((manager->anchor->level << 2) - 1), ((requiredLevel << 2) - 1), requiredBlocks * HEADERSIZE);
    #endif
    // Actual byte size of our allocation
    uint64 allocSize = (requiredBlocks * HEADERSIZE);

    // Like in manInit, we'll never allocate more than MAX_INT bytes, for simplicity reasons
    if (allocSize > MAX_INT) {
        // Abort
        #ifdef DEBUG_ERRORS
        printf("MALLOC-MAJOR-GROW-ERROR: Will never allocate more than MAX_INT bytes, but required.\n");
        #endif
        return 3;
    }

    // Return value of sbrk
    uint64 rVal = 0;
    rVal = (uint64) sbrk(allocSize);

    // Sbrk failed!
    if (rVal == -1) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-MAJOR-GROW-ERROR: sbrk fail\n");
        #endif
        // Means we can't expand anymore
        manager->canExpand = 0;
        return 2;
    }

    Header* oldAnchor = manager->anchor;
    manager->anchor = bu_fix(requiredLevel, manager->anchor);
    manager->end = (Header*)(rVal + allocSize);
    // Not very fast to call this here but needed to fix allocation bug
    bu_merge(oldAnchor, manager->anchor);
    return 0;
}

/**
 *
*/
void* malloc(uint32 nBytes) {
    if(nBytes == 0) {
        return NULL;
    }
    
    // Level needed
    uint32 requiredLevel = only_fs((nBytes + (HEADERSIZE - 1)) / HEADERSIZE);

    // Manager responsible for this region
    BuddyManager* manager;

    // Assign manager, if it's NULL display error
    while ((manager = get_and_init_manager(requiredLevel)) != NULL) {
        
        // If we need more than we have ready, we try to grow our area
        if ((manager->freeMap < requiredLevel)) {
            // Grow memory and validates data structure
            // If growing doesn't work, we go to next manager
            // Perhaps mmap will throw better error codes, right now we can't differentiate between
            // "Not enough space in general", "Not enough space for this region", "Whatever", so we just try each one
            if (manGrow(requiredLevel, manager)) {
                #ifdef DEBUG_ERRORS
                printf("MALLOC-MINOR-ERROR: Manager couldn't grow to right size\n");
                #endif
                continue;
            }
        }
        
        void* result = bumalloc(requiredLevel, manager->anchor);
        manager->freeMap = manager->anchor->freeLeft | manager->anchor->freeRight;
        return result;
    }

    // If we couldn't find a manager, we bail
    if (manager == NULL) {
        #ifdef DEBUG_ERRORS
        printf("MALLOC-CRITICAL-ERROR: Couldn't find manager\n");
        #endif
        return NULL;
    }
    
    //If we somehow get here, we return NULL
    #ifdef DEBUG_ERRORS
    printf("MALLOC-UNKNOWN-ERROR: Got to end without finding space\n");
    #endif
    return NULL;
}


void setup_malloc() {
    // Make first Manager in here
    // And initialize it
    get_and_init_manager(0x80);
}
