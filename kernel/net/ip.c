#include "kernel/net/ip.h"

static ip_address our_ip_address;

void print_ip(ip_address ip) {
  pr_debug("IP: %d.%d.%d.%d\n", ip.octets[0], ip.octets[1], ip.octets[2], ip.octets[3]);
}

void
copy_ip_addr(ip_address *copy_to)
{
  memmove(&copy_to->octets, our_ip_address.octets, IP_ADDR_SIZE);
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

  uint8 ipdest[IP_ADDR_SIZE] = {10, 0, 2, 3};
  memmove((void*) &packet->dst.octets, ipdest, 4);
  uint8 ipme[IP_ADDR_SIZE] = {10, 0, 2, 15};
  memmove((void*) &packet->src.octets, ipme, 4);
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