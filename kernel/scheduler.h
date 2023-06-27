/*! \file scheduler.h
 * \brief contains scheduler and related things
 */

#ifndef INCLUDED_kernel_scheduler_h
#define INCLUDED_kernel_scheduler_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/defs.h"
#include "kernel/process_queue.h"

ProcessQueue runnable_queue;
ProcessQueue wait_queue;

#ifdef __cplusplus
}
#endif

#endif
