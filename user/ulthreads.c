#include "user/user.h"
#include "user/mmap.h"
#include "user/ulthreads.h"
                        
// Context swtich
__attribute__((naked)) void thread_switch(thread_context* old, thread_context* new) 
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
thread threads[MAX_NUM_THREADS] = {0};
thread_id current_thread_id = -1;
int active_threads = 0;

thread_id ringbuffer[MAX_NUM_THREADS] = {-1};
int16 current_index = 0;
int16 insert_index = 0;

thread_context scheduler_context = {0};
void* scheduler_stack_start = NULL;

void append_ring(thread_id id) {
    ringbuffer[insert_index] = id;
    insert_index++;
    insert_index = insert_index % MAX_NUM_THREADS;
}

thread_id pop_ring() {
    thread_id popped = ringbuffer[current_index];
    current_index++;
    current_index = current_index % MAX_NUM_THREADS;
    return popped;
}

void uthreads_sched() {
    while(1) {

        printf("Scheddy\n");
        thread_id id = pop_ring();
        if (id == -1) {
            printf("Threading broke\n");
            exit(0);
        }

        thread* current_thread = threads + id;
        switch(current_thread->state) {
            case READY:
                current_thread_id = current_thread->id;
                current_thread->state = RUNNING;
                thread_switch(&scheduler_context, &current_thread->context);
                break;
            case WAITING:
                thread waitfor = threads[current_thread->waitfor];
                // If waitfor not DEAD or INVALID (means its done executing or is invalid)
                if (!(waitfor.state == DEAD || waitfor.state == INVALID)) {
                    // Means it's either Ready, Running or Waiting
                    // Means we gotta wait
                    // join protects against circular waits
                    append_ring(current_thread->id);
                }
                // Otherwise it's done.
                // Mark current thread as ready
                current_thread->state = READY;
                current_thread->saved_value = waitfor.saved_value;
                break;
            default:
                printf("Scheduling broke. Thread State: %d", current_thread->state);
                break;
        }
    }
}

void uthreads_yield() {
    printf("yield\n");
}

void uthreads_exit() {
    printf("Exit\n");
    active_threads--;
    thread* current_thread = &threads[current_thread_id];
    current_thread->state = DEAD;
    thread_switch(&current_thread->context, &scheduler_context);
}

void uthreads_execute() {
    printf("Execute\n");
    active_threads++;
    thread* current_thread = &threads[current_thread_id];
    current_thread->saved_value = current_thread->func(current_thread->saved_value);
    uthreads_exit();
}

void* uthreads_join(thread_id id) {
    if (!(id >= 0 && id < MAX_NUM_THREADS)) {
        printf("Threading: Invalid join ID\n");
        return (void*)-1;
    }
    thread* current_thread = &threads[current_thread_id];
    thread* waitfor_thread = &threads[id];

    // Protect against circular waits
    if (waitfor_thread->state == WAITING && waitfor_thread->waitfor == current_thread_id) {
        printf("Threading: Circular wait\n");
        return (void*)-1;
    }

    // This is an optimization
    if (waitfor_thread->state == READY) {
        current_thread->state = WAITING;
        current_thread->waitfor = id;
        
        append_ring(current_thread_id);
        thread_switch(&current_thread->context, &scheduler_context);
    }

    return current_thread->saved_value;
}


thread_id uthreads_create(void* func_pointer (void*), void* arg, int stack_size) {
    // Inits parent if not initted
    thread* parent_thread = NULL;
    if (current_thread_id == -1) {
        parent_thread = &threads[0];
        parent_thread->state = RUNNING;
        parent_thread->id = 0;
        parent_thread->stack_start = (void*)-1;

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
    if (new_thread != NULL) {
        void* new_stack = malloc(stack_size);
        if (new_stack != MAP_FAILED) {
            new_thread->stack_start = new_stack;
            new_thread->context.sp = (new_stack + stack_size);
            new_thread->func = func_pointer;
            new_thread->saved_value = arg;
            new_thread->context.ra = uthreads_execute;
        } else {
            printf("Thread stack allocation fail\n");
            return -1;
        }
    }
    parent_thread->state = READY;
    new_thread->state = READY;

    
    append_ring(parent_thread->id);
    append_ring(new_thread->id);
    
    thread_switch(&parent_thread->context, &scheduler_context);
    printf("Returning from create\n");
    return new_thread->id;
}  

int hello(int arg) {
    printf("hi %d\n", arg);
    return arg + 5;
}

int i = 0;
int main(){
    i++;
    uthreads_create((void * (*)(void*))hello, (void*) (uint64)i, PAGE_SIZE);
    uthreads_yield();
    return 0;
}


// Wrapper so it's ok if main doesn't call exit
// Modified to lead back to scheduler unless everyone is done
void
_main() 
{
  active_threads++;
  // Uncomment if needed
  // extern int main();
  int code = main();
  active_threads--;
  for (int i = 1; i < MAX_NUM_THREADS; i++) {
    printf("EXIT: Collecting Threads: %d\n", i);
    uthreads_join(i);
    // Free new stacks
    free(threads[i].stack_start);
  }
  free(scheduler_stack_start);
  exit(code);
}