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
  sbrk(1);
  // Grow DS once
  for (int i = 0; i < 4; i++) {
    malloc(1 * 16);
  }
  printf("Break===========================================================\n");
  for (int i = 0; i < 4; i++) {
    malloc(8 * 16);
  }
}