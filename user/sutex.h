/*! \file sutex.h
 * \brief header for userspace sutexes
 */

#ifndef INCLUDED_user_sutex_h
#define INCLUDED_user_sutex_h

//! not technically needed, but heavily suggested
typedef unsigned long long osdev_sutex_underlying_t;

#ifndef __cplusplus

#include <stdbool.h>

typedef _Atomic osdev_sutex_underlying_t osdev_sutex_inner_t;

#else

#include <atomic>

using osdev_sutex_inner_t = std::atomic<osdev_sutex_underlying_t>;

#endif

#ifdef __cplusplus
extern "C" {
#endif


struct osdev_sutex_t;
//! having fun with linkage...
typedef struct osdev_sutex_t osdev_sutex_t;

/*!
 * \brief wrap object to (hopefully) be ABI compatible
 */
struct osdev_sutex_t {
    //! an atomic counter
    osdev_sutex_inner_t inner;

    //! \attention you can add more stuff here if you want
};

//! initialize sutex
void osdev_sutex_init(osdev_sutex_t *sutex);
//! lock sutex
void osdev_sutex_lock(osdev_sutex_t *sutex);
//! unlock sutex
void osdev_sutex_unlock(osdev_sutex_t *sutex);

//! try lock sutex
bool osdev_sutex_trylock(osdev_sutex_t *sutex);

#ifdef __cplusplus
}
#endif


#endif
