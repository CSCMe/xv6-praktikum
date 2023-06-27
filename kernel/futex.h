#ifndef INCLUDED_kernel_futex_h
#define INCLUDED_kernel_futex_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/process_queue.h"
#include "kernel/defs.h"
#include "uk-shared/error_codes.h"

#define FUTEX_MAX_LOCKS     21      // only multiples of queues_per_page
#define FUTEX_QUEUES_PER_PAGE     (PGSIZE / sizeof(ProcessQueue))
#define FUTEX_CONTROL_PAGES (FUTEX_MAX_LOCKS / FUTEX_QUEUES_PER_PAGE)

struct futex_control {
    ProcessQueue futex_queues[FUTEX_QUEUES_PER_PAGE];
    struct futex_control* next;
};

struct futex_queue_map_entry {
    void* key;
    ProcessQueue* value;
};

struct futex_queue_map {
    struct futex_queue_map_entry entries[FUTEX_MAX_LOCKS];
    struct spinlock map_lock;
};


#ifdef __cplusplus
}
#endif

#endif