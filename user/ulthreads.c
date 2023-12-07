#include "user/user.h"
#include "user/uthreads.h"
#include "kernel/fcntl.h"
#include "uk-shared/error_codes.h"

char buf[1000] = {0};
int buf_start = 0;
int buf_end = 0;

void addToBuf() {
    int fd = open("README", O_RDWR);
    if (fd < 0) {
        printf("File read error");
        return;
    }
    while(buf_end < 1000) {
        // Yield every 25 chars
        if (buf_end % 20 == 0) {
            uthreads_yield();
        }
        int res = read(fd, buf + buf_end, 10);
        buf_end += res;
        if (res == 0) {
            close(fd);
            return;
        }
    }
    close(fd);
}

void workBuf(thread_id worker) {
    while(buf_start < 1000) {
        while (buf_end-buf_start < 20 && buf_end < 1000) {
            int testres = uthreads_state(worker);
            if (testres != 0) {
                printf("What\n");
                return;
            } else {
                uthreads_yield();
            }
        }
        printf("%c", buf[buf_start]);
        buf_start++;
    }
    printf("Exited\n");
}

int main(int argc, char** argv) {
    thread_id worker = uthreads_create((void* (*)(void*)) addToBuf, NULL, 4096);
    uthreads_yield();
    uthreads_create((void* (*)(void*)) workBuf, (void*)worker, 4096);
    return 0;
}