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
    char * a = malloc(16 * 1888500);// a lot of size unitsi (0x41)
    char * b = malloc(256); // 16 (+1) size units aka 0x11
    free(a);
    char * c = malloc(128); // 8 (+1) size units
    a[2] = 'a';
    b[2] = 'b';
    c[2] = 'c';
    Header* p = (Header*)(a - 16);
    printf("TEST: pointer pos:%p, start free region:%p, free size (byte):%d\n", p, p->s.ptr, p->s.size * 16);
    printf("%c,%c,%c\n",a[2],b[2],c[2]);
    printf("%p, %p, %p\n",a,b,c);
}