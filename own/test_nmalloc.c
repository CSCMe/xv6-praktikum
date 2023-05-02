#include "kernel/types.h"
#include "user/user.h"

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};
typedef union header Header;

void main(int argc, char ** argv) {
  sbrk(0b1000);
  char* arr[29] = {0};
  for (int i = 0; i < 29; i++) {
    arr[i] = malloc(2<<i);
    printf("%d: %p\n", i, arr[i]);
  }
  for (int i = 0; i < 29; i++) {
    free(arr[i]);
  }
  printf("Break===========================================================\n");
  // now we should have an empty anchor again
  char* aah = malloc(1);
  // Grow DS once
  for (int i = 0; i < 4; i++) {
    malloc(1 * 16);
  }
  for (int i = 0; i < 4; i++) {
    malloc(8 * 16);
  }
  free(aah);
}