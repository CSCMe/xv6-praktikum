// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "defs.h"
#include "buf.h"



typedef struct {
  BigBuf* buffer;
  union {
    uint64 searchKey;
    struct {
      uint32 device;
      uint32 blocknum;
    };
  };
  uint64 lastReleased;
} bsearchEntry;


struct {
  struct spinlock lock;
  // Buffer are in array
  BigBuf cachedBuffers[NBUF];
  bsearchEntry bsearchEntries[NBUF];
  uint64 accessCounter;
} bcache;

void
binit(void)
{
  BigBuf *b;

  initlock(&bcache.lock, "bcache");
  bcache.accessCounter = 0;

  for (int i = 0 ; i < NBUF; i++) {
    b = bcache.cachedBuffers + i;
    
    b->page = kalloc();
    if (b->page == 0) {
      panic("binit: kalloc buffer alloc fail");
    }
    bsearchEntry* entry = bcache.bsearchEntries + i;
    entry->device = -1;
    entry->blocknum = 0;
    entry->buffer = b;
  }
}

static int binary_search(int start, int end, uint64 targetKey) {
  int mid = (start + end) / 2;
  bsearchEntry* midEntry = bcache.bsearchEntries + mid;
  if (start == end) {
    return -1;
  } else if (targetKey < midEntry->searchKey + BLOCKS_PER_PAGE && targetKey >= midEntry->searchKey){
    return mid;
  } else if (targetKey < midEntry->searchKey) {
    return binary_search(start, mid, targetKey);
  } else if (targetKey > midEntry->searchKey) {
    return binary_search(mid + 1, end, targetKey);
  }
}

/**
 * TODO: REDO, cuz pointers are not static or smth
*/
void binary_insert(int index) 
{
  bsearchEntry* searchEntry = bcache.bsearchEntries + index;
  for (int i = 0; i < NBUF; i++) {
    bsearchEntry* curEntry = (bcache.bsearchEntries + i);
    if (i == index) {
      continue;
    }
    if (searchEntry)
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  BigBuf *big;

  acquire(&bcache.lock);

  int searchResult = binary_search(0, NBUF, (((uint64)dev) << 32 ) + blockno);

  // Entry is cached
  if (searchResult != -1) {
    big->refcount++;
    int index = blockno - big->blockno;
    release(&bcache.lock);
    return big->smallBuf + index;
  }

  // Entry is not cached, first possible
  for (int i = 0; i < NBUF; i++) {
    big = (bcache.bsearchEntries + i )->buffer;
    if (big->refcount == 0) {
      big->device = dev;
      big->blockno = blockno;
      big->refcount = 1;

      binary_insert(i);
      
      struct buf* b = &(big->smallBuf[0]);
      for (int i = 0; i < BLOCKS_PER_PAGE; i++) {
        (b + i)->valid = 0;
        (b + i)->blockno = blockno + i;
        (b + i)->data = big->page + (i * BSIZE);
        (b + i)->parent = big;
      }

      release(&bcache.lock);
      return b;
    }
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    //pr_debug("Readin': %p\n", b->data);
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{

  BigBuf* parent = b->parent;
  acquire(&bcache.lock);
  parent->refcount--;
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  ((BigBuf*)b->parent)->refcount++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  ((BigBuf*)b->parent)->refcount--;
  release(&bcache.lock);
}


