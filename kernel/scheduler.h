/*! \file scheduler.h
 * \brief contains scheduler and related things
 */

#ifndef INCLUDED_kernel_scheduler_h
#define INCLUDED_kernel_scheduler_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/defs.h"

typedef struct __queue_entry {
  struct proc* proc;
  struct __queue_entry* next;
} ProcessQueueEntry;

/**
 * Queue struct for scheduler queues
*/
typedef struct __queue {
  // Ringbuffer containing entries
  ProcessQueueEntry entries_buffer[NPROC];
  // Index of next free entry
  int next_buffer_loc;
  struct spinlock queue_lock;
  ProcessQueueEntry* first;
  ProcessQueueEntry* last;
} ProcessQueue;


ProcessQueue runnable_queue;
ProcessQueue wait_queue;

#ifdef __cplusplus
}
#endif

#endif
