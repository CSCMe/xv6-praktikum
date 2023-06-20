#include "user/sutex.h"
#include <stdatomic.h>

enum osdev_sutex_lock_state {
    LOCK_TAKEN = 0,
    LOCK_FREE = 1,
};

void osdev_sutex_init(osdev_sutex_t *sutex) {
    atomic_store(&sutex->inner, LOCK_FREE);
}

void osdev_sutex_lock(osdev_sutex_t *sutex) {
    osdev_sutex_t other = {LOCK_FREE};
    while(!atomic_compare_exchange_weak(&sutex->inner, &other.inner, LOCK_TAKEN))
        other.inner = LOCK_FREE;
}

//! unlock sutex
void osdev_sutex_unlock(osdev_sutex_t *sutex) {
    atomic_store(&sutex->inner, LOCK_FREE);
}
