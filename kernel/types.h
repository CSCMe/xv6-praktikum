/*! \file types.h
 * \brief typedefs
 */

#ifndef INCLUDED_kernel_types_h
#define INCLUDED_kernel_types_h

#ifndef __ASSEMBLER__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
typedef signed long int64;

typedef uint64 pde_t;



#ifdef __cplusplus
}
#endif

#endif

#endif
