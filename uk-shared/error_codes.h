/*! \file error_codes.h
 * \brief error codes used by syscalls and the like
 */

#ifndef INCLUDED_shared_error_codes_h
#define INCLUDED_shared_error_codes_h

#ifdef __cplusplus
extern "C" {
#endif

/* Error codes */
#define EPERM  0x1    // Permission error
#define EINVAL 0x2    // Invalid input
#define ENIMPL 0x3    // Not implemented
#define EEXIST 0x4    // Mapping exists, but we don't want to overwrite
#define ENOMEM 0x194  // Memory not found/Out of memory
#define EACCESS 0x5   // Access Error
#define EBADF   0x6   // Bad file descriptor


#ifdef __cplusplus
}
#endif

#endif



