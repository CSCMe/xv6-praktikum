#include "kernel/types.h"
#include "user/user.h"

void main (int argc, char** argv)
{
    // Fork to not hang terminal if networking goes wrong
    // (which it inevitably does ^^ )
    if (fork() == 0) {
        // Start a tcp server on port 23 (telnet)
        int port = 23;
        int handle = net_bind(port);
        printf("Established connection on port %d\n", port);

        // ...and immediately close it
        net_unbind(handle);
        printf("Done.\n");
    }
}