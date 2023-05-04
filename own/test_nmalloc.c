#include "kernel/types.h"
#include "user/user.h"

#define DEBUGHEADER(header) printf("ptr:%p lvl:%b, fl:%b, fr:%b, dir:%b, allocDirs:%b\n", (header), (header)->level, (header)->freeLeft, (header)->freeRight, (header)->dirToParent, (header)->allocDirs)


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

  free(malloc(1));
  uint64* test = malloc(0x1FFFFFF);
  uint64* test2 = malloc(0x1FFFFFF);
  printf("%p, %p", test, test2);
  free(test);
  free(test2);
}
