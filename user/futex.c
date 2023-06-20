#include "user/futex.h"
#include "user/sutex.h"
#include <stdatomic.h>

enum osdev_mutex_lock_state {
    LOCK_WAITING = -1,
    LOCK_TAKEN = 0,
    LOCK_FREE = 1,
};

void osdev_mutex_init(osdev_mutex_t *mutex) {
    atomic_store(&mutex->inner, LOCK_FREE);
    osdev_sutex_init(&mutex->lock);
}


void osdev_mutex_lock(osdev_mutex_t *mutex) {
    int spincount = 0;
    while (true) {
        osdev_sutex_lock(&mutex->lock);
        if (atomic_fetch_sub(&mutex->inner, 1) != LOCK_FREE) {
            atomic_store(&mutex->inner, LOCK_WAITING);
            if ((++spincount) > MAX_SPINCOUNT) {
                //futex_sleep(mutex);
                spincount = 0;
                osdev_sutex_unlock(&mutex->lock);
                continue;
            }
        } else {
            osdev_sutex_unlock(&mutex->lock);
            break;
        }
        osdev_sutex_unlock(&mutex->lock);
    }
}

//! unlock mutex
void osdev_mutex_unlock(osdev_mutex_t *mutex) {
    /*
    enum osdev_mutex_lock_state old_state = atomic_fetch_add(&mutex->inner, 1);
    // We wake all our waiting guys if there were any before
    if (old_state != LOCK_TAKEN)
        //futex_wake(mutex);    
        */
    osdev_sutex_lock(&mutex->lock);
    atomic_store(&mutex->inner, LOCK_FREE);
    osdev_sutex_unlock(&mutex->lock);
    // atomic_store(&mutex->inner, LOCK_FREE);
}

//! try lock mutex
bool osdev_mutex_trylock(osdev_mutex_t *mutex) {
    //osdev_mutex_t other = {0};
    osdev_sutex_lock(&mutex->lock);
    enum osdev_mutex_lock_state old_val = atomic_fetch_sub(&mutex->inner, 1);
    if (old_val != LOCK_FREE) {
        atomic_store(&mutex->inner, old_val);
        osdev_sutex_unlock(&mutex->lock);
        return !true;
    }
    osdev_sutex_unlock(&mutex->lock);
    return !false;
}
