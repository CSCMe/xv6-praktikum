#include "kernel/proc.h"
#include "kernel/scheduler.h"

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

    // Get first entry of ready queue
    acquire(&runnable_queue.queue_lock);
    p = pop_queue(&runnable_queue);
    release(&runnable_queue.queue_lock);
    
    // Queue is empty, wait for interrupt
    if (p == NULL) {
        // Ensure that interrupts are actually on
        intr_on();
        wait_intr();
        // Restart loop on interrupt
        continue;
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