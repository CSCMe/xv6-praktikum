#include "user/futex.h"
#include <stdatomic.h>
#include "user/user.h"

enum osdev_mutex_lock_state {
    LOCK_FREE = 0,
    LOCK_TAKEN = 1,
    LOCK_WAITING = 2,
};

void osdev_mutex_init(osdev_mutex_t *mutex) {
    atomic_store(&mutex->inner, LOCK_FREE);
}


// atomic_compare_exchange_(OBJ, EXP, DES)* replaces OBJ with DES if OBJ == EXP 
// In either case sets EXP to value of OBJ
// Adapted from https://akkadia.org/drepper/futex.pdf mutex take 3
void osdev_mutex_lock(osdev_mutex_t *mutex) {
    osdev_mutex_inner_t lock_state = LOCK_FREE;
    // Check if lock state is != LOCK_FREE (means !atomic_OP, (returns true if inner == state))
    if (!atomic_compare_exchange_strong_explicit(&mutex->inner, &lock_state, LOCK_TAKEN, memory_order_acquire, memory_order_acquire)) {
        // If lock is not FREE && not WAITING (means lock_state == LOCK_TAKEN) set to Waiting
        if (lock_state != LOCK_WAITING) {
            lock_state = atomic_exchange_explicit(&mutex->inner, LOCK_WAITING, memory_order_acquire);
        }
        // If lock_state is either TAKEN or WAITING, wait, set lock to waiting after wait
        while (lock_state != LOCK_FREE) {
            futex_wait(mutex, LOCK_WAITING); // uses stub rn
            lock_state = atomic_exchange_explicit(&mutex->inner, LOCK_WAITING, memory_order_acquire);
        }
    }
}

//! unlock mutex
void osdev_mutex_unlock(osdev_mutex_t *mutex) {
    // If we're not waiting, explicitely set to free
    if (atomic_fetch_sub_explicit(&mutex->inner, 1, memory_order_release) != LOCK_TAKEN) {
        // Not atomic in the original but why shouldn't it be?
        atomic_store_explicit(&mutex->inner, LOCK_FREE, memory_order_release);
        //atomic_store_explicit(&mutex->inner, LOCK_FREE, memory_order_release);
        futex_wake(mutex, 1); // Only wakes a single sleeper
    }
}

//! try lock mutex
bool osdev_mutex_trylock(osdev_mutex_t *mutex) {
    // Check if lock state is != LOCK_FREE (means !atomic_OP, (returns true if inner == state))
    osdev_mutex_inner_t lock_state = LOCK_FREE;
    // return mutex->inner == LOCK_FREE and sets Lock to taken
    return atomic_compare_exchange_strong_explicit(&mutex->inner, &lock_state, LOCK_TAKEN, memory_order_acquire, memory_order_acquire);
}
