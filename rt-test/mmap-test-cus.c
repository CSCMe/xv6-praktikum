#include "user/user.h"
#include "user/mmap.h"
#include "assert.h"

/**
 * Test checks if we save last mapped addr and use it as a basis for future mappings
 * Not required at all
*/

void main (int argc, char** argv) {
    void* addr = (void*)(((((128L << 9) + 12L) << 9) + 4L) << 12);
    void* val = mmap(addr, PAGE_SIZE, PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* val2 = mmap(addr, PAGE_SIZE, PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(val2 == (void*)((uint64)val + PAGE_SIZE));
    munmap(val, PAGE_SIZE);
    int pid = fork();
    // This condition applies to both
    val = mmap(NULL, PAGE_SIZE, PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(val == (void*)((uint64)val2 + PAGE_SIZE));
    if (pid == 0 ) {
        exit(0);
    }
    wait(NULL);
}