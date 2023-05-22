#include "user/user.h"
#include "user/mmap.h"
#include "assert.h"

void main (int arg, char** argv) {
    
    void* val = mmap(NULL, PAGE_SIZE, PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* orgVal = val;
    assert(val != MAP_FAILED);

    val = mmap(orgVal, 2 * PAGE_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(val != orgVal);

    val = mmap(orgVal, 3 * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    assert(val == orgVal);

    void* shared = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_ANONYMOUS |MAP_SHARED, -1, 0);
    assert(shared != MAP_FAILED);

    // Set value for parent and child
    *(char*) val = 33;
    uint64 pid = fork();

    if (pid == 0) {
        assert(*(char*)val == 33);
        // Child sets value
        *((char*) val) = 5;
        // And unmaps two pages
        assert(munmap(val, 2 * PAGE_SIZE) == 0);
        // Tries again, should fail
        assert(munmap(val, 2 * PAGE_SIZE) == (int) (uint64) MAP_FAILED);
        // Should succeed on last page, cuz that one wasn't unmapped
        assert(munmap(val + 2 * PAGE_SIZE, PAGE_SIZE) == 0);

        *((uint64*) shared) = -5;
        assert(munmap(shared, PAGE_SIZE) == 0);
        exit(0);
    }

    int status = 0;
    wait(&status);

    // Parent time after child is done

    // Value shouldn't have changed for parent
    assert(*(char*)val == 33);

    // Child pages should still be mapped, unmap for realz
    assert(munmap(val + PAGE_SIZE, 2 * PAGE_SIZE) == 0);

    // Tries again, should fail
    assert(munmap(val + PAGE_SIZE, 2 * PAGE_SIZE) ==  (int) (uint64) MAP_FAILED);

    // This shouldn't fail
    *((uint64*) shared + 8) = 52;
    assert(*((uint64*) shared) == -5);

    assert(munmap(shared, PAGE_SIZE) == 0);
}