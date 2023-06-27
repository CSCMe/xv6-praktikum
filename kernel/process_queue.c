#include "kernel/process_queue.h"

ProcessQueueEntry*
next_queue_entry(ProcessQueue* queue, ProcessQueueEntry* entry) 
{
    if (entry ==  &queue->entries_buffer[NPROC -1]) {
        return &queue->entries_buffer[0];
    }
    return entry + 1;
}

void debug_queue(ProcessQueue* queue, char* startMess)
{
    pr_debug("Queue Debug: %s\n", startMess);
    ProcessQueueEntry* entry = queue->start;
    ProcessQueueEntry* first = entry;
    do
    {
        pr_debug("pid: %d, state:%d\n", entry->proc->pid, entry->proc->state);
    } while ((entry = next_queue_entry(queue, entry)) != first);
    pr_debug("End\n");
}

void
append_queue(ProcessQueue* queue, struct proc* proc)
{  
    ProcessQueueEntry* new_end = next_queue_entry(queue, queue->end);

    // If end == start, the queue was full.
    // Note that this should never happen because the queue has NPROC
    // entries, but better safe than sorry.
    if (queue->start == new_end) {
        panic("Process queue full, WTF?");
    }
    queue->end->proc = proc;
    queue->end = new_end;
}

struct proc*
pop_queue(ProcessQueue* queue)
{
    if (queue->start == queue->end) {
        // Queue is empty, can't pop element :/
        return NULL;
    }

    struct proc* popped_proc = queue->start->proc;
    queue->start = next_queue_entry(queue, queue->start);
    return popped_proc;
}

void 
init_queue(ProcessQueue* queue, char* lock_name)
{
    queue->start = &queue->entries_buffer[0];
    queue->end = &queue->entries_buffer[0];
    queue->valid = 1;
    initlock(&queue->queue_lock, lock_name);
}
