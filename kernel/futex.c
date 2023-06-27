#include "kernel/defs.h"
#include "kernel/printk.h"
#include "kernel/futex.h"
#include "kernel/process_queue.h"

struct futex_control* futex_control_list;

void futex_control_init() {

    struct futex_control* next = NULL;
    for (int i = 0; i < FUTEX_CONTROL_PAGES; i++) {
        struct futex_control* prev = kalloc_zero();
        if (prev == NULL)
            panic("futex_control_init kalloc");
        prev->next = next;
        next = prev;
    }
    futex_control_list = next;
}

/**
 * Returns NULL on Error
 * Else return pointer to unused queue
*/
ProcessQueue* futex_control_find_free() {
    return NULL;
}

/**
 * Gets pointer to queue list entry from map, inits queue list if necessary
*/
ProcessQueue** futex_map_get(uint64* ) {
    return NULL;
}

void futex_map_delete(void* page_addr) {
    // page_addr is page aligned, so we get first entry
    ProcessQueue** queue_ptr = futex_map_get(page_addr);
    kfree((void*) queue_ptr);
}

uint64 __futex_init(uint64* futex) {
    // Get physical address of futex
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);

    // Get entry in Process Queue* list
    ProcessQueue** queue_ptr = futex_map_get(futex_phys_addr);

    // Insert found queue into list, if not already initialized
    if (*queue_ptr != NULL)
        return 0;

    // Find first free queue
    ProcessQueue* free_queue = futex_control_find_free();
    // Init queue
    init_queue(free_queue, "futex");
    
    
    *queue_ptr = free_queue;

    return 0;
}

uint64 __futex_deinit(void* physical_page_addr) {
    futex_map_delete(physical_page_addr);
    return 0;
}

uint64 __futex_wait(uint64* futex, int val) {
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);
    ProcessQueue** queue_ptr = futex_map_get(futex_phys_addr);
    if (queue_ptr == NULL || *queue_ptr == NULL) {
        return EINVAL;
    }
    ProcessQueue* queue = *queue_ptr;
    acquire(&queue->queue_lock);

    if (*futex == val) {
        struct proc* my_proc = myproc();
        append_queue(queue, my_proc);
        sleep(my_proc, &queue->queue_lock);
    }
    release(&queue->queue_lock);
    return 0;
}

uint64 __futex_wake(uint64* futex, int num_wake) {
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);
    ProcessQueue** queue_ptr = futex_map_get(futex_phys_addr);
    if (queue_ptr == NULL || *queue_ptr == NULL) {
        return EINVAL;
    }
    ProcessQueue* queue = *queue_ptr;
    acquire(&queue->queue_lock);
    for (int i = 0; i < num_wake; i++) {
        struct proc* proc = pop_queue(queue);
        wakeup(proc);
    }
    release(&queue->queue_lock);
    return 0;
}