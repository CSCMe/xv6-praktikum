#include "kernel/types.h"
#include "user/user.h"
#include "assert.h"


char* mockCons = 0;
int lastPos = 0;
int bufSize = 250;

#define ASSERT_BUD(str) assert(strcmp(mockCons, (str)) == 0); buildMockCons()


void buildMockCons() {
    lastPos = 0;
    free(mockCons);
    mockCons = malloc(bufSize * sizeof(char));
    for (int i = 0; i < bufSize; i++) {
        (mockCons)[i] = 0;
    }
}

// This is our write mock, just writes into the buffer
int write(int f, const void* c, int n) {
    (void) f;
    (void) n;
    mockCons[lastPos] = *(char*)c;
    lastPos++;
    return 0;
}

void main(int argc, char **argv) {
    buildMockCons();

    printf("Test %d", 1);
    ASSERT_BUD("Test 1");

    printf("%d\n", 12);
    ASSERT_BUD("12\n");

    printf("%d, %d, %d\n", 12,-12, 0xFFFFFFFFE);
    ASSERT_BUD("12, -12, -2\n");

    printf("%l, %l\n", 0xFFFFFFFFF, -0xFFFFFFFFF);
    ASSERT_BUD("68719476735, -68719476735\n");

    printf("%x, %x\n", 0xFF, -0xFF);
    ASSERT_BUD("FF, FFFFFFFFFFFFFF01\n");

    printf("%p\n", 0xFFFFFFFFF);
    ASSERT_BUD("0x0000000FFFFFFFFF\n");

    printf("%s\n", "hi");
    ASSERT_BUD("hi\n");

    printf("%u, %u\n", 12,-12);
    ASSERT_BUD("12, 4294967284\n");

    printf("%lu, %lu\n", 0xFFFFFFFFF, -0xFFFFFFFFF);
    ASSERT_BUD("68719476735, 18446744004990074881\n");

    printf("%lul\n", 0xFFF);
    ASSERT_BUD("4095l\n");

    printf("%b, %b, %b, %b, %b\n", 0b0100, 0b1000000, 0xFF0FF, 2, 0xFFFFFFFFF);
    ASSERT_BUD("100, 1000000, 11111111000011111111, 10, 1111111111111111111111111111111111111111111111111111111111111111\n");

    printf("%l, %x, %b\n", -1, -1, -1);
    ASSERT_BUD("-1, FFFFFFFFFFFFFFFF, 1111111111111111111111111111111111111111111111111111111111111111\n");
}