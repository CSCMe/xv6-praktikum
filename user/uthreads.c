#include "user/uthreads.h"
#include "user/user.h"
#include "user/mmap.h"

// #define DEBUG_THREADING
                        
// Context swtich
__attribute__((naked)) static void thread_switch(thread_context* old, thread_context* new) 
{
    asm volatile (
        "sd ra, 0(a0)\n"
        "sd sp, 8(a0)\n"
        "sd s0, 16(a0)\n"
        "sd s1, 24(a0)\n"
        "sd s2, 32(a0)\n"
        "sd s3, 40(a0)\n"
        "sd s4, 48(a0)\n"
        "sd s5, 56(a0)\n"
        "sd s6, 64(a0)\n"
        "sd s7, 72(a0)\n"
        "sd s8, 80(a0)\n"
        "sd s9, 88(a0)\n"
        "sd s10, 96(a0)\n"
        "sd s11, 104(a0)\n"

        "ld ra, 0(a1)\n"
        "ld sp, 8(a1)\n"
        "ld s0, 16(a1)\n"
        "ld s1, 24(a1)\n"
        "ld s2, 32(a1)\n"
        "ld s3, 40(a1)\n"
        "ld s4, 48(a1)\n"
        "ld s5, 56(a1)\n"
        "ld s6, 64(a1)\n"
        "ld s7, 72(a1)\n"
        "ld s8, 80(a1)\n"
        "ld s9, 88(a1)\n"
        "ld s10, 96(a1)\n"
        "ld s11, 104(a1)\n"
        "ret\n"
    );
}

// Set after scheduling thread. If set to -1 no threading till now.
static thread threads[MAX_NUM_THREADS] = {0};
static thread_id current_thread_id = -1;
static int active_threads = 0;

static thread_id ringbuffer[MAX_NUM_THREADS] = {-1};
static int16 current_index = 0;
static int16 insert_index = 0;

static thread_context scheduler_context = {0};
static void* scheduler_stack_start = NULL;

static void append_ring(thread_id id) {
    ringbuffer[insert_index] = id;
    insert_index++;
    insert_index = insert_index % MAX_NUM_THREADS;
}

static thread_id pop_ring() {
    thread_id popped = ringbuffer[current_index];
    ringbuffer[current_index] = -1;
    current_index++;
    current_index = current_index % MAX_NUM_THREADS;
    return popped;
}

static void uthreads_sched() {
    while(1) {
        thread_id id = pop_ring();
        if (id == -1) {
            printf("Scheduler: Threading broke. -1 ID\n");
            for (int i = 0; i < MAX_NUM_THREADS; i++) {
                printf("IDX: %d, PID:%d;;; ", i, ringbuffer[i]);
            }
            printf("\nRing contents\n");
            exit(-1);
        }

        thread* current_thread = threads + id;
        switch(current_thread->state) {
            case READY:
                #ifdef DEBUG_THREADING
                printf("Scheduler: Now running: %d\n", id);
                #endif
                current_thread_id = current_thread->id;
                current_thread->state = RUNNING;
                thread_switch(&scheduler_context, &current_thread->context);
                break;
            case WAITING:
                thread waitfor = threads[current_thread->waitfor];
                #ifdef DEBUG_THREADING
                    printf("Scheduler: Thread waitfor: %d:%d, state: %d\n", current_thread->id, waitfor.id, waitfor.state);
                #endif
                // DEAD or INVALID? Stop waiting
                if (waitfor.state == DEAD || waitfor.state == INVALID) {
                    current_thread->state = READY;
                }
                
                append_ring(id);
                break;
            default:
                printf("Scheduling broke. Thread State: %d", current_thread->state);
                exit(-1);
                break;
        }
    }
    // Bei keinen aktiven threads back to the superparent
    thread_switch(&scheduler_context, &threads[0].context);
}

void uthreads_yield() {
    #ifdef DEBUG_THREADING
    printf("yield: %d\n", current_thread_id);
    #endif
    thread* current_thread = &threads[current_thread_id];
    current_thread->state = READY;
    append_ring(current_thread_id);
    thread_switch(&current_thread->context, &scheduler_context);
}

void uthreads_exit() {
    #ifdef DEBUG_THREADING
    printf("Exit: %d\n", current_thread_id);
    #endif
    active_threads--;
    thread* current_thread = &threads[current_thread_id];
    current_thread->state = DEAD;
    thread_switch(&current_thread->context, &scheduler_context);
}

static void uthreads_execute() {
    #ifdef DEBUG_THREADING
    printf("Execute: %d\n", current_thread_id);
    #endif
    active_threads++;
    thread* current_thread = &threads[current_thread_id];
    current_thread->saved_value = current_thread->func(current_thread->saved_value);
    uthreads_exit();
}

// Returns 0 if thread is joinable
int uthreads_test_joinable(thread_id id) {
    if (!(id >= 0 && id < MAX_NUM_THREADS)) {
        printf("Joinable: Invalid join ID\n");
        return EINVAL;
    }
    thread* waitfor_thread = &threads[id];
    // Threads can only be joined by one thread at a time. Invalid threads are not joinable
    return !(waitfor_thread->subscriber == -1) && waitfor_thread->state != INVALID;
}

// Returns 0 on success
int uthreads_join(thread_id id, void** retval) {
    #ifdef DEBUG_THREADING
    printf("Join: %d waitfor: %d\n", current_thread_id, id);
    #endif

    if (!(id >= 0 && id < MAX_NUM_THREADS) || id == current_thread_id) {
        #ifdef DEBUG_THREADING
        printf("Join: Invalid join ID\n");
        #endif
        return EINVAL;
    }

    thread* current_thread = &threads[current_thread_id];
    thread* waitfor_thread = &threads[id];

    // Protect against circular waits
    if (waitfor_thread->state == WAITING && waitfor_thread->waitfor == current_thread_id) {
        #ifdef DEBUG_THREADING
        printf("Join: Circular wait\n");
        #endif
        // Only fails join. Not entire Process? Smart? Dunno
        return ECIRC;
    }
    
    if (uthreads_test_joinable(id)) {
        // Thread not joinable. Return immeadiately
        #ifdef DEBUG_THREADING
        printf("Threading: Not joinable: %d\n", id);
        #endif
        return ENOJOIN;
    }

    switch(waitfor_thread->state) {
        case READY:
        case WAITING:
            current_thread->state = WAITING;
            current_thread->waitfor = id;
            append_ring(current_thread_id);
            waitfor_thread->subscriber = current_thread_id;
            thread_switch(&current_thread->context, &scheduler_context);
            break;
        case INVALID:
        case DEAD:
            waitfor_thread->state = INVALID;
            break;
        case RUNNING:
            // Gets handled earlier, unless Threading broke
            printf("Join: Threading broke: Waiting for running thread?!?");
            exit(-1);
    }

    if (retval != NULL) {
        *retval = waitfor_thread->saved_value;
    }
    return 0;
}


thread_id uthreads_create(void* func_pointer (void*), void* arg, int stack_size) {
    #ifdef DEBUG_THREADING
    printf("Create: %d\n", current_thread_id);
    #endif
    // Inits parent if not initted
    thread* parent_thread = NULL;
    if (current_thread_id == -1) {
        parent_thread = &threads[0];
        parent_thread->state = RUNNING;
        parent_thread->id = 0;
        parent_thread->stack_start = (void*)-1;
        parent_thread->subscriber = -1;
        // Actually init scheduler
        scheduler_stack_start = malloc(PAGE_SIZE);
        scheduler_context.ra = uthreads_sched;
        // Stack grows down so we use highest address as stack
        scheduler_context.sp = scheduler_stack_start + PAGE_SIZE;

    } else {
        parent_thread = &threads[current_thread_id];
    }

    // Get an empty thread from Thread table
    thread* new_thread = NULL;
    for (int i = 0; i < MAX_NUM_THREADS; i++) {
        if (threads[i].state == INVALID) {
            new_thread = &threads[i];
            new_thread->id = i;
            break;
        }
    }

    if (new_thread == NULL) {
        printf("No valid thread\n");
        return -1;
    }

    void* new_stack = malloc(stack_size);

    if (new_stack == NULL) {
        printf("Thread stack allocation fail\n");
        return -1;
    }

    new_thread->stack_start = new_stack;
    new_thread->context.sp = (new_stack + stack_size);
    new_thread->func = func_pointer;
    new_thread->saved_value = arg;
    new_thread->context.ra = uthreads_execute;
    new_thread->state = READY;
    new_thread->subscriber = -1;
    append_ring(new_thread->id);

    parent_thread->state = READY;
    append_ring(parent_thread->id);
    
    // Context switch to scheduler
    thread_switch(&parent_thread->context, &scheduler_context);
    return new_thread->id;
}  


// Wrapper so it's ok if main doesn't call exit
// Modified to lead back to scheduler unless everyone is done
void
_main() 
{
  active_threads++;
  // Uncomment if needed
  extern int main();
  int code = main();
  active_threads--;
  // Only do all this if necessary, aka threading been invoked
  if (current_thread_id != -1) {
    for (int i = 1; i < MAX_NUM_THREADS; i++) {
        #ifdef DEBUG_THREADING
        printf("EXIT: Collecting Threads: %d\n", i);
        #endif
        uthreads_join(i, NULL);
        // Free new stacks
        free(threads[i].stack_start);
    }
    // Free scheduler stack
    free(scheduler_stack_start);
  }
  exit(code);
}