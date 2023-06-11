#include "kernel/proc.h"
#include "kernel/scheduler.h"

void debug_queue(ProcessQueue* queue, char* startMess)
{
    pr_debug("Queue Debug: %s\n", startMess);
    ProcessQueueEntry* entry = queue->first;
    do
    {
        pr_debug("pid: %d, state:%d\n", entry->proc->pid, entry->proc->state);
    } while ((entry = entry->next) != NULL);
    pr_debug("End\n");
}

void
append_queue(ProcessQueue* queue, struct proc* proc)
{  
    ProcessQueueEntry* entry = &queue->entries_buffer[queue->next_buffer_loc];
    entry->proc = proc;
    entry->next = NULL;

    if (queue->last != NULL) {
        queue->last->next = entry;
    } else {
        // If queue is empty: queue->last & queue->first are both NULL
        queue->first = entry;
    }
    queue->last = entry;
    queue->next_buffer_loc = (queue->next_buffer_loc + 1) % NPROC;
}

struct proc*
pop_queue(ProcessQueue* queue)
{
    if (queue->first == NULL) {
        return NULL;
    }
    struct proc* popped_proc = queue->first->proc;

    queue->first = queue->first->next;
    // If now the queue is empty, set first and last to NULL
    if (queue->first == NULL) {
        queue->last = NULL;
    }

    return popped_proc;
}

void 
init_queue(ProcessQueue* queue, char* lock_name)
{
    queue->first = NULL;
    queue->last = NULL;
    queue->next_buffer_loc = 0;
    initlock(&queue->queue_lock, lock_name);
}

void
schedulerinit()
{
    init_queue(&runnable_queue, "Runnable Queue");
    init_queue(&wait_queue, "Wait Queue");
}


/**
 * Assumes held proc lock. Fails otherwise
*/
void
schedule_proc(struct proc* proc)
{
    if (!holding(&proc->lock))
        panic("schedule-proc: lock not held");
    
    proc->state = RUNNABLE;
    acquire(&runnable_queue.queue_lock);
    append_queue(&runnable_queue,  proc);
    release(&runnable_queue.queue_lock);
}



// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  
  struct cpu *c = mycpu();
  
  // Sets current cpu process to None
  c->proc = 0;

  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    struct proc *p = NULL;

    // Spin baby, spin
    while (p == NULL) {
        acquire(&runnable_queue.queue_lock);
        p = pop_queue(&runnable_queue);
        release(&runnable_queue.queue_lock);
    }
    
    acquire(&p->lock);
    // If any process is not runnable, runnable_queue broke -> panic.
    if(p->state != RUNNABLE)
        panic("scheduler: Not runnable");

    // Switch to chosen process.  It is the process's job
    // to release its lock and then reacquire it
    // before jumping back to us.
    p->state = RUNNING;
    c->proc = p;

    swtch(&c->context, &p->context);
    // Switch returns here after done with execution
    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    release(&p->lock);

  }
}