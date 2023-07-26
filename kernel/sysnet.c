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

  pr_info("testnet done\n");
  return 0;
}

uint64 sys_net_bind(void) {
  net_init();

  uint8 port;
  argint(0, (int*) &port);

  return await_incoming_tcp_connection(port);
}

uint64 sys_net_unbind(void) {
  uint8 handle;
  argint(0, (int *)&handle);

  tcp_unbind(handle);
  return 0;
}

uint64 sys_net_send_listen(void) {
  uint8 con_id;
  uint64 send_buffer;
  int send_buffer_len;
  uint64 receive_buffer;
  int receive_buffer_len;
  argint(0, (int*)&con_id);
  argaddr(1, &send_buffer);
  argint(2, &send_buffer_len);
  argaddr(3, &receive_buffer);
  argint(4, &receive_buffer_len);

  uint8 data[5] = {'b', 'a', 'c','\r','\n'};
  send_buffer_len = 5;
  // Somehow convert user addresses to kernel bois
  void* kernel_send_buffer = data;
  void* kernel_receive_buffer = NULL;

  int received_data_len = tcp_send_receive(con_id, kernel_send_buffer, send_buffer_len, kernel_receive_buffer, receive_buffer_len);


  return received_data_len;
}