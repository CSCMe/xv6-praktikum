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
        // Tries again, should succeed cuz "It is not an error if the indicated range does not contain any mapped pages."
        assert(munmap(val, 2 * PAGE_SIZE) == 0);
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

    // Tries again, should succeed because "It is not an error if the indicated range does not contain any mapped pages.""
    
    assert(munmap(val + PAGE_SIZE, 2 * PAGE_SIZE) == 0);

    // This shouldn't fail
    *((uint64*) shared + 8) = 52;
    assert(*((uint64*) shared) == -5);

    assert(munmap(shared, PAGE_SIZE) == 0);

    // Tesst with lotsa repetitions, should not run out of kernel or shared memory
    for (int i = 1; i < 2000; i++) {
        void* addr1 = mmap(NULL, ((i % 255) + 1) * PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        void* addr2 = mmap(NULL, ((i % 255) + 1) * PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        assert(addr1 != MAP_FAILED);
        assert(addr2 != MAP_FAILED);
        assert(addr1 != addr2);
        assert(munmap(addr1, ((i % 255) + 1) * PAGE_SIZE) != (uint64)MAP_FAILED);
        assert(munmap(addr2, ((i % 255) + 1) * PAGE_SIZE) != (uint64)MAP_FAILED);
    }
}