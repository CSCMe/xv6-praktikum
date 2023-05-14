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
  uint32 refcount; // num of references to PageBuffer 
  uint32 size;    // (1-BLOCKS_PER_PAGE) Size of the bigbuf, how many smallBufs are usable?
  struct buf smallBuf[BLOCKS_PER_PAGE];
  uchar* page;    // Page
} BigBuf;

typedef struct {
  BigBuf* buffer;
  union {
    uint64 searchKey;
    struct {
      uint32 blocknum;
      uint32 device;
    };
  };
} bsearchEntry;


#ifdef __cplusplus
}
#endif

#endif
