#include "kernel/types.h"
#include "user/user.h"
#include "user/bmalloc.h"
#include "own/alloc.h"


unsigned long read_cycles(void)
{
  unsigned long cycles;
  asm volatile ("rdcycle %0" : "=r" (cycles));
  return cycles;
}

// Ergebnis: Allocator kann sehr viel Speicher ausgraben
// 
void main(int argc, char ** argv) {
  setup_malloc();
  Header* needed = malloc(1);
  BuddyManager* manager = get_responsible_manager(needed);
  free(needed);

  int freeHeaders = 0;
  // print region
  for (int i = 0; i < 511; i++) {
    Header* cur = needed + i;
    if (cur->level != NULL) {
      printf("Free headers: %d\n", freeHeaders);
      freeHeaders = 0;
      DEBUGHEADER(cur);
    } else {
      freeHeaders++;
    }
  }
  printf("Free headers: %d\n", freeHeaders);
  printf("Manager: start:%p anch:%p end:%p, anch+freeHeaders\n",manager->base, manager->anchor, manager->end, manager->anchor + freeHeaders);


  struct block a[100] = {0};
  for (int i = 0; i < 100; i++) {
    a[i] = block_alloc(i * 100,i);
    // Write the entire thing
    for (int k = 0; k < a[i].size; k++) {
      *((char*)a[i].begin + k) = 0;
    }
  }

  DEBUGHEADER(manager->anchor);

  char* b[100] = {0};
  for (int i = 0; i < 100; i++) {
    b[i] = malloc((i+ 1) * 100);
    // Write the entire thing
    if(b[i] == 0) {
      printf("ABORTING\n");
      return;
    }
    for (int k = 0; k < (i+1) * 100; k++) {
      *(b[i] + k) = 0;
    }
    printf("%d done\n", i);
  }
  for(int i = 0; i < 100; i++) {
    block_free(a[i]);
  }

  for(int i = 0; i < 100; i++) {
    free(b[i]);
  }
  
  DEBUGHEADER(manager->anchor);

  free(malloc(1));
  printf("Bigboi test\n");
  uint64* test = malloc(0x1FFFFF0);
  uint64* test2 = malloc(0x1FFFFF0);
  uint64* theEnd = test2 + ((uint64)test2 - (uint64)test) /sizeof(uint64) - 1;

  printf("%p, %p, %p\n", test, test2, theEnd);

  char* newRegion = malloc(0xFFFFF0);
  malloc(0xFFFFF0);
  printf("new Region: %p", newRegion);
  manager = get_responsible_manager(newRegion);
  DEBUGHEADER(manager->anchor);
  newRegion = malloc(0xFFFFE0 >> 1);
  malloc(0xFFFFE0 >> 1);
  printf("new Region: %p", newRegion);
  manager = get_responsible_manager(newRegion);
  DEBUGHEADER(manager->anchor);
  newRegion = malloc(0xFFFFC0 >> 2);
  manager = get_responsible_manager(newRegion);
  DEBUGHEADER(manager->anchor);
  printf("BeforeFree\n");
  free(newRegion);
  free(test);
  free(test2);
  printf("AfterFree\n");
  DEBUGHEADER(manager->anchor);
  manager = get_responsible_manager(test);
  DEBUGHEADER(manager->anchor);
}
