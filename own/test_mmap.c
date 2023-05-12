#include "user/user.h"
#include "user/mmap.h"

void main (int arg, char** argv) {
    
    void* val = mmap(NULL, PAGE_SIZE, PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    void* orgVal = val;
    printf("%p\n", val);
    val = mmap(orgVal, 2 * PAGE_SIZE, PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("%p\n", val);
    val = mmap(orgVal, 3 * PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    printf("%p\n", val);
    *((char*) val) = 33;
    uint64 pid = fork();
    if (pid == 0) {
        *((char*) val) = 5;
        munmap(val + PAGE_SIZE, 2 * PAGE_SIZE);
    }
    int status = 0;
    wait(&status);
    if (pid == 0) {

    printf("Childhere");
    } else {

    printf("parhere");
    }
    printPT();
    printf("Expected: 33, got: %d", *((char*) val));
    printf("forkend\n");
}