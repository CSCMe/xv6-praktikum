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

struct {
  struct spinlock lock;
  // Buffer are in array
  BigBuf cachedBuffers[NBUF];
  // And ordered by linkedList
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  BigBuf head;
} bcache;

void
binit(void)
{
  BigBuf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.cachedBuffers; b < bcache.cachedBuffers+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
    
    b->page = kalloc();
    if (b->page == 0) {
      panic("binit: kalloc buffer alloc fail");
    }
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

  // Is the block already cached?
  for(big = bcache.head.next; big != &bcache.head; big = big->next){
    if(big->device == dev && (blockno < (big->blockno + BLOCKS_PER_PAGE) && blockno >= big->blockno)){
      //pr_debug("Reusing bigbuf\n");
      big->refcount++;
      int index = blockno - big->blockno;
      release(&bcache.lock);
      return big->smallBuf + index;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(big = bcache.head.prev; big != &bcache.head; big = big->prev){
    if(big->refcount == 0) {
      big->device = dev;
      big->blockno = blockno;

      big->refcount = 1;
      
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
  if (parent->refcount == 0) {
    // no one is waiting for it.
    parent->next->prev = parent->prev;
    parent->prev->next = parent->next;
    parent->next = bcache.head.next;
    parent->prev = &bcache.head;
    bcache.head.next->prev = parent;
    bcache.head.next = parent;

  }
  
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


