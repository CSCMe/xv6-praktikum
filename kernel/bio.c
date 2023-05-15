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
  // Buffer are in array, which is sorted
  BigBuf cacheBuffers[NBUF];
  BigBuf* sortedBuffers[NBUF];
  uint64 generation;
} bcache;

uint64 reuses = 0;
uint64 newBufs = 0;

void debug_buffer() {
  pr_debug("PRINTING: BSEARCH--------------------------------------\n");
  for (int i = 0 ; i < NBUF; i++) {
    BigBuf* buf = bcache.sortedBuffers[i];
    pr_debug("ind:%d, refC: %x,sK:%p, gen: %p, size: %d\n",i, buf->refcount,buf->searchKey, buf->generation, buf->size);
  }
  pr_debug("END-----------------------------------------\n");
}


void
binit(void)
{
  BigBuf *b;

  initlock(&bcache.lock, "bcache");
  bcache.generation = 0;
  for (int i = 0 ; i < NBUF; i++) {
    b = bcache.cacheBuffers + i;
    bcache.sortedBuffers[i] = b;
    b->size = 0;
    b->page = kalloc();
    if (b->page == 0) {
      panic("binit: kalloc buffer alloc fail");
    }
    b->device = -1;
    b->blockno = 0;
    b->generation = 0;
    b->refcount = 0;
    pr_debug("Invalid start key:%p\n", b->searchKey);
  }
}

static int binary_search(int start, int end, uint64 targetKey) {
  while (start != end) {
    int mid = (start + end) / 2;
    //pr_debug("start_%d, end:%d\n", start, end);
    BigBuf* midEntry = bcache.sortedBuffers[mid];

    // Found correct entry
    if (targetKey < (midEntry->searchKey + midEntry->size) && targetKey >= midEntry->searchKey) {

      return mid;
    } else if (targetKey < midEntry->searchKey) {
      end = mid;
      continue;
    } else if(targetKey > midEntry->searchKey) {
      if (start == mid) {
        //pr_debug("oh no\n");
        return -1;
      }
      start = mid;
      continue;
    }
  }
  // Haven't found fit
  return -1;
}

/**
 * Assumption: List is sorted except for element at index index
 * Sorts entry at index index into its correct location
 * Return the new location
*/
int binary_insert(int index) 
{
  // Bufferpointer we need the new location of
  BigBuf* searchBuf = bcache.sortedBuffers[index];
  
  int newLoc = NBUF; 
  // if our new location is NBUF - 1, we fall through the loop below without changes to newLoc
  // In this case, we are in index < newLoc case, meaning newLoc is decremented by one -> real new index is NBUf -1
  for (int i = 0; i < NBUF; i++) { // To look for new location
    BigBuf* curBuf = bcache.sortedBuffers[i];
    if (index == i) {
      continue;
    }
    if (searchBuf->searchKey == curBuf->searchKey) {
      panic("buffer: binary_insert element already present");
    }
    if (searchBuf->searchKey < curBuf->searchKey) {
      newLoc = i;
      break;
    }
  }
/* 
  pr_debug("newLoc: %d, old:%d\n", newLoc, index);
  pr_debug("BFORE:REORDER: ");
  debug_buffer(); */
  
  // We need to move things to the left 
  if (index < newLoc) {
    newLoc = newLoc - 1;
    for (int i = index; i < newLoc  && i < (NBUF - 1); i++) { 
      BigBuf* copy = bcache.sortedBuffers[i];
      bcache.sortedBuffers[i] = bcache.sortedBuffers[i + 1];
      bcache.sortedBuffers[i + 1] = copy;
    }
  } else { // We need to move everything to the right 
    for (int i = index; i > newLoc && i > 0; i--) { 
      BigBuf* copy = bcache.sortedBuffers[i];
      bcache.sortedBuffers[i] = bcache.sortedBuffers[i - 1];
      bcache.sortedBuffers[i - 1] = copy;
    }
  }
  return newLoc;
/*   pr_debug("END:  ");
  debug_buffer(); */
}

void updateSize(BigBuf* updated, uint64 nextKey) {
    uint64 newSize = ((nextKey - updated->searchKey));
    if(newSize == 0) {
      panic("buffer: Limited size to 0"); 
    } else if (newSize < BLOCKS_PER_PAGE) {
      updated->size = newSize;
    } else {
      updated->size = BLOCKS_PER_PAGE;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return a buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  BigBuf *big;

  acquire(&bcache.lock);

  int searchResult = binary_search(0, NBUF, ((uint64) dev << 32) + blockno);

  // Entry is cached
  if (searchResult != -1) {
    big = bcache.sortedBuffers[searchResult];
    big->refcount++;
    int index = blockno - big->blockno;
    
    // Increment generation and set bigs generation to the incremented value
    big->generation = ++bcache.generation;
    release(&bcache.lock);
    reuses++;
    return big->smallBuf + index;
  }

  int lru_index = - 1;
  uint64 lowest_gen = -1;

  // Entry is not cached
  // Find LRU O(n) unused buffer, 
  // this should only be called ~300 times in the entirety of usertests, performance isn't too important here
  for (int i = NBUF - 1; i >= 0; i--) {
    BigBuf* currentBuf = bcache.sortedBuffers[i];
    if (currentBuf->refcount == 0 && currentBuf->generation < lowest_gen) {
      lru_index = i;
      lowest_gen = currentBuf->generation;
    }
  }

  // Found an empty buffer
  if (lru_index > -1 && (big = bcache.sortedBuffers[lru_index])->refcount == 0) {
    newBufs++;
    big->device = dev;
    big->blockno = blockno;
    big->refcount++;

    int newPos = binary_insert(lru_index);
    big = bcache.sortedBuffers[newPos];
    if (newPos < NBUF-1) {
      updateSize(big, bcache.sortedBuffers[newPos + 1]->searchKey);
    } else {
      updateSize(big, -1);
    }

    debug_buffer();
    pr_debug("Reuses: %p, nBufs:%p\n", reuses, newBufs);

      
    struct buf* b = &(big->smallBuf[0]);
    for (int i = 0; i < BLOCKS_PER_PAGE; i++) {
      (b + i)->valid = 0;
      (b + i)->blockno = blockno + i;
      (b + i)->data = big->page + (i * BSIZE);
      (b + i)->parent = big;
    }
      // Increment generation and set bigs generation to the incremented value
    big->generation = ++bcache.generation;
    release(&bcache.lock);
    return b;
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


