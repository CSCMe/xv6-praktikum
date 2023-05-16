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

#define BLOCKS_PER_PAGE 4096 / BSIZE

struct buf {
  uint blockno;   // blockno
  uint valid; // is this buf valid? (read from disk)
  struct sleeplock lock; // lock for disk r/w
  void* parent; // BigBuf parent of this buffer
  uchar* data; // 
};

typedef struct __BigBuf {
  int disk;       // does disk "own" buffer

  // Union for searchKey (used for sorting for binary search)
  union {
    uint64 searchKey;
    struct {
      uint blockno;   // starting blockno, aligned to BUFFER_SIZE
      uint device;    // device the data is from
    };
  };

  uint32 refcount;    // num of references to PageBuffer 
  uint32 size;       // (1-BLOCKS_PER_PAGE) Size of the bigbuf, how many smallBufs should be used?
  uint64 generation; // Generation this BigBuf belongs to
  struct buf smallBuf[BLOCKS_PER_PAGE];
  uchar* page;    // page where data for smallBufs is stored

} BigBuf;


#ifdef __cplusplus
}
#endif

#endif
