#ifndef ULTHREADS_H
#define ULTHREADS_H

#include "user/user.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NUM_THREADS 4

typedef int64 thread_id;

typedef enum {
    INVALID,
    WAITING,
    READY,
    RUNNING,
    DEAD,
} thread_state;

// Saved registers for context switches.
typedef struct __context {
  void* ra;
  void* sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
} thread_context;


typedef struct {
    thread_context context;
    void* (*func) (void*);
    void* saved_value;
    void* stack_start;
    // only one thread can wait for another
    thread_id subscriber;
    thread_id waitfor;
    thread_id id;
    thread_state state;
} thread;

// Moves Thread to scheduler. ä
// Invokes scheduler
extern void uthreads_yield();

// Moves current thread to scheduler.
// Creates new thread with stack_size amount of stack memory. 
// Invokes new thread
extern thread_id uthreads_create(void* func_pointer (void*), void* arg, int stack_size);

// Stops execution of current ULT. 
// Invokes scheduler
extern void uthreads_exit();

// Stops execution of current ULT
// retval = NULL if ignore return value of Thread
// returns 0 on success
extern int uthreads_join(thread_id thread_id, void** retval);

/**
 * @brief Returns a threads state
 * @return 0: joinable,
 * @return ENOJOIN: not joinable but not invalid,
 * @return EINVAL: Invalid thread id
 * @return EEXIST: Thread invalid
*/
extern int uthreads_state(thread_id thread_id);

#ifdef __cplusplus
}
#endif

#endif