#ifndef INCLUDED_kernel_futex_h
#define INCLUDED_kernel_futex_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/defs.h"

struct __futex_control {
    ProcessQueue queue;
};




#ifdef __cplusplus
}
#endif

#endif