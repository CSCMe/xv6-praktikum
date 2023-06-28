#include "kernel/defs.h"
#include "kernel/printk.h"
#include "kernel/futex.h"
#include "kernel/process_queue.h"

static struct futex_control* futex_control_list;
static struct futex_queue_map futex_queue_map;

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
    for (int i = 0 ; i < FUTEX_MAX_LOCKS; i++) {
        futex_queue_map.entries[i].key = NULL;
    }
    initlock(&futex_queue_map.map_lock, "futex-map-lock");
}

/**
 * Return pointer to unused queue
 * else return NULL
*/
ProcessQueue* futex_control_find_free() {
    struct futex_control* current = futex_control_list;
    for (int i = 0; i < FUTEX_MAX_LOCKS; i++)
    {
        // If we've reached the end of a page, go to next
        if (i > 0 && (i % FUTEX_QUEUES_PER_PAGE == 0)) {
            current = current->next;
        }

        ProcessQueue* current_queue = &current->futex_queues[i % FUTEX_QUEUES_PER_PAGE];
        if (current_queue->valid != 1) {
            return current_queue;
        }
    }
    return NULL;
}

/**
 * Inits queue list for a specific lock (if necessary)
 * Returns pointer to queue list
*/
void futex_map_init(uint64* addr, ProcessQueue* queue) {
    acquire(&futex_queue_map.map_lock);
    for (int i = 0; i < FUTEX_MAX_LOCKS; i++) {
        if (futex_queue_map.entries[i].key == NULL) {
            futex_queue_map.entries[i].key = addr;
            futex_queue_map.entries[i].value = queue;
            release(&futex_queue_map.map_lock);
            return;
        }
    }
    release(&futex_queue_map.map_lock);
    return;
}

/**
 * Gets pointer to queue list entry from map, inits queue list if necessary
*/
ProcessQueue* futex_map_get(uint64* addr) {
    acquire(&futex_queue_map.map_lock);
    for (int i = 0; i < FUTEX_MAX_LOCKS; i++) {
        struct futex_queue_map_entry* entry = &futex_queue_map.entries[i];
        if (entry->key == addr) {
            release(&futex_queue_map.map_lock);
            return entry->value;
        }
    }
    release(&futex_queue_map.map_lock);
    return NULL;
}


uint64 __futex_init(uint64* futex) {
    // Get physical address of futex
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);

    // Find first free queue
    ProcessQueue* free_queue = futex_control_find_free();
    if (free_queue == NULL) {
        return ENOMEM;
    }
    // Init queue
    init_queue(free_queue, "futex");
    
    // Init queue pointer for lock
    futex_map_init(futex_phys_addr, free_queue);

    return 0;
}

void __futex_deinit(void* physical_page_addr) {
    acquire(&futex_queue_map.map_lock);
    for (int i = 0; i < FUTEX_MAX_LOCKS; i++) {
        struct futex_queue_map_entry* entry = &futex_queue_map.entries[i];
        // Check if it's on the same page
        if (((uint64)entry->key >> PGSHIFT) == ((uint64)physical_page_addr >> PGSHIFT)) {
            entry->key = NULL;
            entry->value->valid = 0;
            entry->value = NULL;
        }
    }
    release(&futex_queue_map.map_lock);
}

uint64 __futex_wait(uint64* futex, int val) {
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);
    ProcessQueue* queue = futex_map_get(futex_phys_addr);
    if (queue == NULL) {
        return EINVAL;
    }
    acquire(&queue->queue_lock);

    if (*futex_phys_addr == val) {
        struct proc* my_proc = myproc();
        append_queue(queue, my_proc);
        sleep(my_proc, &queue->queue_lock);
    }
    release(&queue->queue_lock);
    return 0;
}

uint64 __futex_wake(uint64* futex, int num_wake) {
    uint64* futex_phys_addr = (uint64*) walkaddr(myproc()->pagetable, (uint64)futex);
    ProcessQueue* queue = futex_map_get(futex_phys_addr);
    if (queue == NULL) {
        return EINVAL;
    }
    acquire(&queue->queue_lock);
    for (int i = 0; i < num_wake; i++) {
        struct proc* proc = pop_queue(queue);
        wakeup(proc);
    }
    release(&queue->queue_lock);
    yield(); // Yield to allow other procs to get lock
    return 0;
}