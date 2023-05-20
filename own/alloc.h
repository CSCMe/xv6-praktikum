#ifndef INCLUDED_own_alloc_h
#define INCLUDED_own_alloc_h

#ifdef __cplusplus
extern "C" {
#endif

#include "user/user.h"
#include "user/mmap.h"

#define MAX_INT 0x7FFFFFFF 
#define HEADERSIZE sizeof(Header)
#define HEADERALIGN (HEADERSIZE - 1)
#define MIN_BUDDY_SIZE 0x40 // Get an entire page at first

// Stolen from riscv.h, needed for mmap parts
#ifndef PGROUNDUP
    #define PGROUNDUP(sz)  (((sz)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
#endif

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

/**
 * Enum for directions, can be used in bitmap
*/
typedef enum __dir{
    NONE = 0,
    LEFT = 1,
    RIGHT  = 2
} dir;

/**
 * Manager for a buddy allocator section
 * Contains base, end and anchor pointers
*/
typedef struct __buddy_manager {
    // Where the buddy region start
    Header* base;
    // Anchor of the buddy region
    Header* anchor;
    // Where the buddy region ends (Might not be necessary)
    Header* end;
    // What parts are free in the buddy region
    uint32 freeMap;
    // If our buddy region can expand
    // Uses a lot of space right now, might add other variables later
    uint32 canExpand;
} BuddyManager;

void bufree(void* pointer, Header* anchor, Header* base);
void* bumalloc(uint32, Header* anchor);
Header* bu_fix(uint32 highestLevel, Header* anchor);
void bu_merge(Header* from, Header* to);
BuddyManager* get_responsible_manager(void* pointer);

// Prints all the information needed to debug a particular header
#define DEBUGHEADER(header) printf("ptr:%p lvl:%b, fl:%b, fr:%b, dir:%b, allocDirs:%b\n", (header), (header)->level, (header)->freeLeft, (header)->freeRight, (header)->dirToParent, (header)->allocDirs)


#ifdef __cplusplus
}
#endif

#endif