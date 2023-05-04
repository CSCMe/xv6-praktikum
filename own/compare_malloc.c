#include "user/user.h"

unsigned long read_cycles(void)
{
  unsigned long cycles;
  asm volatile ("rdcycle %0" : "=r" (cycles));
  return cycles;
}

void main(int argc, char** argv) {
    uint64 start_time, end_time;
    uint64 result = 0;
    uint64** morepointies = malloc(10024 * sizeof(uint64*));
    start_time = read_cycles();
    
    for (int i = 0; i < 10024; i++) {
        morepointies[i] = malloc((i%256 + 1) * sizeof(uint64));
        result += (uint64)morepointies[i];
        if (morepointies[i] != 0) {
            *(morepointies[i]) = result;
        } else {
            printf("%d: 0", i);
        }
        
    }

    for (int i = 0; i < 10024; i++) {
        if(i% 2) {
            free(morepointies[i]);
        }
    }

    for(int i = 0; i < 10024; i++) {
        if(i% 2) {
            morepointies[i] = malloc(2 * (i%256 + 1) * sizeof(uint64));
            result += (uint64)morepointies[i];
        if (morepointies[i] != 0) {
            *(morepointies[i]) = result;
        } else {
            printf("%d: 0", i);
        }
        }
    }

    for (int i = 0; i < 10024; i++) {
        free(morepointies[i]);
    }

    end_time = read_cycles();
    printf("Malloc: time: %lu\n", end_time - start_time);
    printf("Result: %lu\n", result);
    //printCallcount();

}