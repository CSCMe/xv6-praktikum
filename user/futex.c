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
    mutex->current_ticket = 0;
    mutex->last_ticket_num = 0;
}

void futex_wait(osdev_mutex_t *mutex, int ticket_num) {
    while (mutex->current_ticket != ticket_num) {
        sleep(1);
    }
}

void futex_wake(osdev_mutex_t *mutex) {
    mutex->current_ticket++;
    // idk do nothing I guess
}

// atomic_compare_exchange_(OBJ, EXP, DES)* replaces OBJ with DES if OBJ == EXP 
// In either case sets EXP to value of OBJ
// Adapted from https://akkadia.org/drepper/futex.pdf mutex take 3
void osdev_mutex_lock(osdev_mutex_t *mutex) {
    osdev_mutex_inner_t lock_state = LOCK_FREE;

    // Check if lock state is != LOCK_FREE (means !atomic_OP, (returns true if inner == state))
    if (!atomic_compare_exchange_strong_explicit(&mutex->inner, &lock_state, LOCK_TAKEN, memory_order_acquire, memory_order_acquire))
    {
        // If lock is not waiting, set to waiting
        // (Because we only enter here if lock is already taken)
        if (lock_state != LOCK_WAITING) {
            lock_state = atomic_exchange_explicit(&mutex->inner, LOCK_WAITING, memory_order_acquire);
        }
        int my_ticket = atomic_fetch_add_explicit(&mutex->last_ticket_num, 1, memory_order_acquire);
        // Loop while lock state is still "waiting"
        while (lock_state != LOCK_FREE) {
            futex_wait(mutex, my_ticket); // uses stub rn
            lock_state = atomic_exchange_explicit(&mutex->inner, LOCK_WAITING, memory_order_acquire);
        }
    }
}

//! unlock mutex
void osdev_mutex_unlock(osdev_mutex_t *mutex) {
    // If we're not waiting, explicitely set to free
    if (atomic_fetch_sub(&mutex->inner, 1) != LOCK_TAKEN) {
        // Not atomic in the original but why shouldn't it be?
        atomic_store(&mutex->inner, LOCK_FREE);
        futex_wake(mutex);
    }
}

//! try lock mutex
bool osdev_mutex_trylock(osdev_mutex_t *mutex) {
    // Check if lock state is != LOCK_FREE (means !atomic_OP, (returns true if inner == state))
    osdev_mutex_inner_t lock_state = LOCK_FREE;
    // return mutex->inner == LOCK_FREE and sets Lock to taken
    return atomic_compare_exchange_strong_explicit(&mutex->inner, &lock_state, LOCK_TAKEN, memory_order_acquire, memory_order_acquire);
}
