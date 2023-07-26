// Network system calls.

#include "defs.h"
#include "fcntl.h"

#include "kernel/net/net.h"
#include "kernel/net/ip.h"
#include "kernel/net/arp.h"
#include "kernel/net/dhcp.h"
#include "kernel/net/transport.h"

uint64 sys_net_test(void) {
  net_init();

  // Some ARP testing
  uint8 ip_to_resolve[IP_ADDR_SIZE]      = {10, 0, 2, 2};
  uint8 resolved_mac_addr[MAC_ADDR_SIZE] = {};
  get_mac_for_ip(resolved_mac_addr, ip_to_resolve);
  get_mac_for_ip(resolved_mac_addr, ip_to_resolve);

  // Start a tcp server (port 23 = telnet)
  uint8 handle = await_incoming_tcp_connection(23);

  // ...and immediately close it
  tcp_unbind(handle);

  pr_info("testnet done\n");
  return 0;
}

uint64 sys_net_bind(void) {
  net_init();
  uint8 port;
  argint(0, (int*) &port);
  return await_incoming_tcp_connection(port);
}

uint64 sys_net_send_listen(void) {
  pr_emerg("TODO: IMPLEMENT");
  return -1;
}