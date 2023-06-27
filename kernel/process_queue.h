/*! \file process_queue.h
 * \brief contains process queue related things. Required for scheduler and futex
 */

#ifndef INCLUDED_kernel_process_queue_h
#define INCLUDED_kernel_process_queue_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/defs.h"

typedef struct __queue_entry {
  struct proc* proc;
} ProcessQueueEntry;

/**
 * Queue struct for scheduler queues
*/
typedef struct __queue {
  // Ringbuffer containing entries
  ProcessQueueEntry entries_buffer[NPROC];    // Array containing procs, acts as ring buffer queue
  ProcessQueueEntry* start;                   // Start of current queue
  ProcessQueueEntry* end;                     // End of current queue
  struct spinlock queue_lock;                 // Lock for this queue
  int valid;                                  // 1 when initialized queue, else undefined
} ProcessQueue;


void debug_queue(ProcessQueue* queue, char* startMess);
void init_queue(ProcessQueue* queue, char* lock_name);
void append_queue(ProcessQueue* queue, struct proc* proc);
struct proc* pop_queue(ProcessQueue* queue);

#ifdef __cplusplus
}
#endif

#endif
