#include "kernel/types.h"
#include "user/user.h"

#define DEBUGHEADER(header) printf("ptr:%p lvl:%b, fl:%b, fr:%b, dir:%b, allocDirs:%b\n", (header), (header)->level, (header)->freeLeft, (header)->freeRight, (header)->dirToParent, (header)->allocDirs)

extern uint32 only_fs(uint32);

typedef struct __header {
    uint32 freeLeft;
    uint32 freeRight;
    uint32 level;
    uint16 dirToParent;
    uint16 allocDirs;
} Header;

extern Header* anchor;

unsigned long read_cycles(void)
{
  unsigned long cycles;
  asm volatile ("rdcycle %0" : "=r" (cycles));
  return cycles;
}

// Ergebnis: Allocator kann sehr viel Speicher ausgraben
// 
void main(int argc, char ** argv) {

  char * a = malloc(31);
  DEBUGHEADER(((Header*)(a + 48)));
  for (int i = 0; i  < 31; i++) {

    *(a+i) = 7;
    /*
    uint32 requiredLevel = only_fs((i + 15) / 16);
    printf("%d: %b,\n",i,requiredLevel);*/
    

  }
  free(a);
  DEBUGHEADER(((Header*)(a + 48)));
  DEBUGHEADER(anchor);

  free(malloc(1));
  uint64* test = malloc(0x1FFFFF0);
  uint64* test2 = malloc(0x1FFFFF0);
  uint64* theEnd = test2 + ((uint64)test2 - (uint64)test) /sizeof(uint64) - 1;

  printf("%p, %p, %p", test, test2, theEnd);
  free(test);
  free(test2);
}
