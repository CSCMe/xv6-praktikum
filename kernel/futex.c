#include "kernel/defs.h"
#include "kernel/printk.h"


uint64 __futex_init(uint64* futex) {
    void* page = kalloc_zero();
    (void) page;
    return 0;
}

uint64 __futex_deimit(uint64* futex) {
    return 0;
}

uint64 __futex_wait(uint64* futex, int val) {
    if (*futex == val) {

    }
    return 0;
}

uint64 __futex_wake(uint64* futex, int num_wake) {
    return 0;
}