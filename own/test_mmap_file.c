#include "user/mmap.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void main (int argc, char** argv) {
    int fd = open("README", O_RDWR);
    void* pointer;
    void* preshared = mmap(NULL, PAGE_SIZE, PROT_READ |PROT_WRITE, MAP_SHARED |MAP_ANON, -1 ,0);
    int pid = fork();
    if (pid == 0) {
        mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, -1, 0);
        
        pointer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        
        if (pointer == MAP_FAILED) {
            printf("child fail\n");
            return;
        }
        printf("child print:%p\n", pointer);
        for (int i = 0; i < 100; i++) {
            printf("%c", *(((char*) pointer) + i));
            *(((char*) pointer) + i) = 'a';
        }

        for (int i = 0; i < 50; i++) {
            *(((char*) preshared) + i) = 'b';
        }
        printPT();
        sleep(10);
        exit(0);
    } else {
        pointer = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    if (pointer == MAP_FAILED) {
        printf("parent fail\n");
        return;
    }
    sleep(20);

    printf("par print:%p\n", pointer);
    for (int i = 0; i < 100; i++) {
        printf("%c", *(((char*) preshared) + i));
        printf("%c", *(((char*) pointer) + i));
    }
    printPT();
    munmap(pointer, PAGE_SIZE);
    return;
}