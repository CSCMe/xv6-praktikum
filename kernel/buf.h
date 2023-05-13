/*! \file buf.h
 * \brief fs cache structure
 */

#ifndef INCLUDED_kernel_buf_h
#define INCLUDED_kernel_buf_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/sleeplock.h"
#include "kernel/fs.h"

#define BLOCKS_PER_PAGE PGSIZE / BSIZE

struct buf {
  int disk;    // does disk "own" buf?
  uint blockno;   // blockno, aligned to BUFFER_SIZE
  uint valid;
  void* parent;
  uchar* data;
};

typedef struct __BigBuf {
  int disk;       // does disk "own" buf?
  uint device;    // device the data is from
  uint blockno;   // starting blockno, aligned to BUFFER_SIZE
  uchar* page;    // Page
  uint32 refcount; // num of references to PageBuffer 
  struct buf smallBuf[BLOCKS_PER_PAGE];
} BigBuf;


#ifdef __cplusplus
}
#endif

#endif
