#include "kernel/net/arp.h"
#include "kernel/net/ip.h"

void
net_init()
{
    arp_init();
    ip_init();
}