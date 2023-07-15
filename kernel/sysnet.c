// Network system calls.

#include "defs.h"
#include "fcntl.h"

#include "kernel/net/net.h"
#include "kernel/net/ip.h"
#include "kernel/net/arp.h"

uint64 sys_net_test(void) {
  uint8 ip_to_resolve[IP_ADDR_SIZE]      = {10, 0, 2, 2};
  uint8 resolved_mac_addr[MAC_ADDR_SIZE] = {};
  get_mac_for_ip(resolved_mac_addr, ip_to_resolve);
  get_mac_for_ip(resolved_mac_addr, ip_to_resolve);
  return 0;
}