#include "user/user.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
  if (!ap) return;

  Header *bp, *p;

  bp = (Header*)ap - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

static Header*
morecore(uint nu)
{
  
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  free((void*)(hp + 1));
  printf("MORECORE: size (byte):%x ptr:%p\n", hp->s.size * 16, hp->s.ptr);
  return freep;
}

void*
malloc(uint nbytes)
{
  if (!nbytes) return 0;

  Header *p, *prevp;
  uint nunits;

  // Minimum size is 2, after that it increases every 16 byte
  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits; //ps (prevp->s.ptr) size is decreased by nunits
        p += p->s.size;      //p itself is incremented by the allocated size, meaning p is now just for the free region, prevp->s.ptr describes the free region, not prevp
        p->s.size = nunits;  //sets size of new p
      }
      freep = prevp;
      printf("MALLOC: pointer pos:%p, region size (byte):%d\n", p, p->s.size * 16);
      printf("freeUpdate: pointer pos:%p, nextFree: %p ,region size (byte):%d\n", prevp->s.ptr, prevp->s.ptr->s.ptr,prevp->s.ptr->s.size * 16);
      return (void*)(p + 1); //Since p is 16 byte long, this always returns an address 16 bytes after p
    }
    if(p == freep) // Last prevp->s.ptr is pointing back to freeptr, basically the loop end condition
    //Meaning this will loop until the last entry in the free list has been reached
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}
