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
  //long* test1 = malloc( (1 << 25) - 1);
  //long* test2 = malloc( (1 << 25) - 1);
  //printf("b1:%p, b2:%p", test1, test2);
  //free(test1);
  //free(test2);
  char* arr[29] = {0};
  for (int k = 0; k < 5; k++){
  for (int i = 0; i < 29; i++) {
    arr[i] = malloc((1<<i) - 1);
    printf("%d: %p\n", i, arr[i]);
  }
  for (int i = 0; i < 29; i++) {
    free(arr[i]);
  }
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