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
  bsearchEntry bsearchEntries[NBUF];
} bcache;


void debug_bsearch() {
  pr_debug("PRINTING: BSEARCH--------------------------------------\n");
  for (int i = 0 ; i < NBUF; i++) {
    bsearchEntry* entry = bcache.bsearchEntries + i;
    pr_debug("ind:%d, sK:%p, size: %d\n",i,entry->searchKey, entry->buffer->size);
  }
  pr_debug("END-----------------------------------------\n");
}


void
binit(void)
{
  BigBuf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0 ; i < NBUF; i++) {
    b = bcache.cachedBuffers + i;
    b->size = 0;
    b->page = kalloc();
    if (b->page == 0) {
      panic("binit: kalloc buffer alloc fail");
    }
    bsearchEntry* entry = bcache.bsearchEntries + i;
    entry->device = -1;
    entry->blocknum = 0;
    entry->buffer = b;
    pr_debug("Invalid start key:%p\n", entry->searchKey);
  }
}

static int binary_search(int start, int end, uint64 targetKey) {

  
  while (start != end) {
    int mid = (start + end) / 2;
    //pr_debug("start_%d, end:%d\n", start, end);
    bsearchEntry* midEntry = bcache.bsearchEntries + mid;

    // Found correct entry
    if (targetKey < (midEntry->searchKey + midEntry->buffer->size) && targetKey >= midEntry->searchKey) {

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
  bsearchEntry searchEntry = *(bcache.bsearchEntries + index);
  
  //pr_debug("ind:%d, sK:%p, lR:%p\n",index,searchEntry.searchKey,searchEntry.lastReleased);

  int newLoc = NBUF - 1;
  for (int i = 0; i < NBUF; i++) { // To look for new location
    bsearchEntry* curEntry = (bcache.bsearchEntries + i);
    if (index == i) {
      continue;
    }
    if (searchEntry.searchKey == curEntry->searchKey) {
      panic("buffer: binary_insert element already present");
    }
    if (searchEntry.searchKey < curEntry->searchKey) {
      newLoc = i;
      break;
    }
  }
/* 
  pr_debug("newLoc: %d, old:%d\n", newLoc, index);
  pr_debug("BFORE:REORDER: ");
  debug_bsearch(); */
  
  // We need to move things to the left 
  if (index < newLoc) {
    newLoc = newLoc - 1;
    for (int i = index; i < newLoc  && i < (NBUF - 1); i++) { 
      bsearchEntry copy = bcache.bsearchEntries[i];
      bcache.bsearchEntries[i] = bcache.bsearchEntries[i + 1];
      bcache.bsearchEntries[i + 1] = copy;
    }
  } else {
    // We need to move everything to the right 
    for (int i = index; i > newLoc && i > 0; i--) { 
      bsearchEntry copy = bcache.bsearchEntries[i];
      bcache.bsearchEntries[i] = bcache.bsearchEntries[i - 1];
      bcache.bsearchEntries[i - 1] = copy;
    }
  }



  return newLoc;
/*   pr_debug("END:  ");
  debug_bsearch(); */
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.

void updateSize(bsearchEntry* updated, bsearchEntry* after) {
    uint64 newSize = ((after->searchKey - updated->searchKey));
      if (newPos < NBUF - 1 && newSize < BLOCKS_PER_PAGE) {
        big->size = newSize;
        if (newSize == 0)
          panic("buffer: Limited size to 0");  
        //debug_bsearch();
      } else {
        big->size = BLOCKS_PER_PAGE;
      }
}
static struct buf*
bget(uint dev, uint blockno)
{
  BigBuf *big;

  acquire(&bcache.lock);

  int searchResult = binary_search(0, NBUF, ((uint64) dev << 32) + blockno);

  // Entry is cached
  if (searchResult != -1) {
    big = bcache.bsearchEntries[searchResult].buffer;
    big->refcount++;
    int index = blockno - big->blockno;
    release(&bcache.lock);
    return big->smallBuf + index;
  }

  // Entry is not cached, first possible
  for (int i = 0; i < NBUF; i++) {
    bsearchEntry* entry = (bcache.bsearchEntries + i );
    big = entry->buffer;
    if (big->refcount == 0) {
      big->device = dev;
      big->blockno = blockno;
      big->refcount = 1;
      entry->blocknum = blockno;
      entry->device = dev;

      // Adjust old entry prev size
      if (i != 0) {
        (entry - 1)->buffer->size = (entry+1)->
      }

      int newPos = binary_insert(i);
      entry = bcache.bsearchEntries + newPos;
      debug_bsearch();

      
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


