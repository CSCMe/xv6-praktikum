#ifndef INCLUDED_kernel_futex_h
#define INCLUDED_kernel_futex_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/process_queue.h"
#include "kernel/defs.h"

#define FUTEX_MAX_LOCKS     21      // only multiples of queues_per_page
#define QUEUES_PER_PAGE     (PGSIZE / sizeof(ProcessQueue))
#define FUTEX_CONTROL_PAGES (FUTEX_MAX_LOCKS / QUEUES_PER_PAGE)

struct futex_control {
    ProcessQueue futex_queues[QUEUES_PER_PAGE];
    struct futex_control* next;
};


#ifdef __cplusplus
}
#endif

#endif