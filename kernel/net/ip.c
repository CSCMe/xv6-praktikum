#include "kernel/net/ip.h"

static uint8 ip_address[IP_ADDR_SIZE];

void
copy_ip_addr(uint8 copy_to[IP_ADDR_SIZE])
{
    memmove(copy_to, ip_address, IP_ADDR_SIZE);
}

void
ip_init()
{

}


void
test_send_ip()
{
  struct ipv4_header actual_ip_header = {0};
  struct ipv4_header* packet = &actual_ip_header;

  uint8 ipdest[8] = {10, 0, 2, 3};
  memmove((void*) packet->dst, ipdest, 4);
  uint8 ipme[8] = {10, 0, 2, 15};
  memmove((void*) packet->src, ipme, 4);
  packet->total_length = 20;
  memreverse(&packet->total_length, sizeof(packet->total_length));
  packet->header_length = 5;
  packet->protocol = IP_PROT_TESTING;
  packet->time_to_live = 64;
  packet->version = IP_VERSION_4;
  packet->flags = 0;
  memreverse(&packet->flags_fragment_mixed, 2);
  //packet->fragment_offset = 4096;
  uint8 dest[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  send_ethernet_packet(dest, ETHERNET_TYPE_IPv4, (void*) packet, sizeof(struct ipv4_header));
}