#include "kernel/types.h"
#include "user/user.h"

void main (int argc, char** argv)
{
    // Fork to not hang terminal if networking goes wrong
    // (which it inevitably does ^^ )
    if (fork() == 0)
        net_test();
}