/*! \file mmap.h
 * \brief mmap defines and function declarations
 */

#ifndef INCLUDED_user_mmap_h
#define INCLUDED_user_mmap_h

#ifdef __cplusplus
extern "C" {
#endif

#include "user/user.h"
#include "uk-shared/mmap_defs.h"

#define PAGE_SIZE 4096


// stolen from linux headers
void *mmap(void *addr, uint64 length, int prot, int flags, int fd, uint64 offset);
int munmap(void *addr, uint64 length);


#ifdef __cplusplus
}
#endif

#endif
