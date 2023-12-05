#include "user/user.h"
#include "user/uthreads.c"


char buf[1000] = {0};
int buf_start = 0;
int buf_end = 0;

void addToBuf() {
    while(buf_end < 500) {
        // Yield every 25 chars
        if (buf_end % 25 == 0) {
            uthreads_yield();
        }
        buf[buf_end] = (buf_end % 26) + 33;
        buf_end++;

    }
}

void workBuf() {
    while(buf_start < 450) {
        while (buf_end-buf_start < 20) {
            uthreads_yield();
        }
        printf("%c", buf[buf_start] % 50 + 33);
        buf_start++;
    }
    printf("\n");
}

int main(int argc, char** argv) {
    uthreads_create((void* (*)(void*)) workBuf,NULL, 4096);
    uthreads_create((void* (*)(void*))addToBuf, NULL, 4096);
    return 0;
}