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

void uthreads_sched() {
    while(active_threads > 0) {

        printf("Scheddy\n");
        if (ringbuffer[current_index] == -1) {
            printf("Threading broke\n");
            exit(0);
        }

        thread* current_thread = threads + ringbuffer[current_index];
        switch(current_thread->state) {
            case READY:
                current_thread_id = current_thread->id;
                current_thread->state = RUNNING;
                thread_switch(&scheduler_context, &current_thread->context);
                break;
            case WAITING:
                // Remove from ringbuffer
                ringbuffer[current_index] = -1;
                // TODO: Maybe add check for 
                // Insert into ringbuffer
                ringbuffer[insert_index] = current_thread->id;
                insert_index += 1;
                insert_index = insert_index % MAX_NUM_THREADS;
                break;
            case INVALID:
        }
        if (current_thread->state !=)
        thread_switch(&scheduler_context, &current_thread->context);
    }
}

void uthreads_execute() {
    thread* current_thread = &threads[current_thread_id];
    current_thread->saved_value = current_thread->func(current_thread->saved_value);

}

void uthreads_exit() {
    printf("Exit\n");
}

void uthreads_yield() {
    printf("yield\n");
}

thread_id uthreads_create(void* func_pointer (void*), void* arg, int stack_size) {
    // Inits parent if not initted
    thread* parent_thread = NULL;
    if (current_thread_id == -1) {
        parent_thread = &threads[0];
        parent_thread->state = RUNNING;

        // Actually init scheduler
        void* scheduler_stack = malloc(PAGE_SIZE);
        scheduler_context.ra = uthreads_sched;
        // Stack grows down so we use highest address as stack
        scheduler_context.sp = scheduler_stack + PAGE_SIZE;

    } else {
        parent_thread = &threads[current_thread_id];
    }

    // Get an empty thread from Thread table
    thread* new_thread = NULL;
    thread_id new_thread_id = -1;
    for (int i = 0; i < MAX_NUM_THREADS; i++) {
        if (threads[i].state == INVALID) {
            new_thread = &threads[i];
            new_thread_id = i;
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
    thread_switch(&parent_thread->context, &scheduler_context);
    printf("Returning from create\n");
    return new_thread_id;
}  

void hello() {
    printf("hi\n");
}

int i = 0;
int main(){
    i++;
    int64 a = uthreads_create((void * (*)(void*))hello, (void*) (uint64)i, PAGE_SIZE);
    printf("freedom!: %d\n", a);
    uthreads_create((void * (*)(void*))hello, (void*) (uint64)i + 5, PAGE_SIZE);
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
  while(active_threads != 0) {
    uthreads_yield();
  }
  exit(code);
}