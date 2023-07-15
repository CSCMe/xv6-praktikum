#include "kernel/net/ip.h"
#include "kernel/net/arp.h"

static uint8 our_ip_address[IP_ADDR_SIZE];

void print_ip(uint8 octets[]) {
  pr_debug("IP: %d.%d.%d.%d\n", octets[0], octets[1], octets[2], octets[3]);
}

void
copy_ip_addr(uint8* copy_to)
{
  memmove(copy_to, our_ip_address, IP_ADDR_SIZE);
}

void
ip_init()
{
  uint8 starting_ip[] = {10,0,2,15};
  memmove(our_ip_address, starting_ip, IP_ADDR_SIZE);
}

void
send_ipv4_packet(uint8 destination[], uint8 ip_protocol, void* data, uint16 data_length)
{
  if (data_length > PGSIZE - sizeof(struct ipv4_header)) {
    pr_info("Send IP: Can't fit data into page. Aborting\n");
    return;
  }

  void* buf = kalloc_zero();
  if (!buf)
    panic("send ip kalloc");

  struct ipv4_header* header = (struct ipv4_header*) buf;

  // Set header length
  header->header_length = (sizeof(struct ipv4_header) * 8) / 32;
  // Set version
  header->version = IP_VERSION_4;

  // No special service
  header->type_of_service = 0;

  // Set total_length
  header->total_length = (sizeof(struct ipv4_header)) + data_length;
  memreverse(&header->total_length, sizeof(header->total_length));
  
  // No identification needed, no fragmentation
  header->identification = 0;

  // No fragment offset;
  header->fragment_offset = 0;
  // No flags for now
  header->flags = 0;
  memreverse(&header->flags_fragment_mixed, sizeof(header->flags_fragment_mixed));
  
  // Set time to live to default value
  header->time_to_live = IPv4_TTL_DEFAULT;

  // Set protocol
  header->protocol = ip_protocol;

  // No checksum for now. TODO: Add calculation
  header->header_checksum = 0;

  // Copy own IP-Address
  copy_ip_addr((uint8*)&header->src);

  // Copy destination IP-Address
  memmove(header->dst, destination, IP_ADDR_SIZE);

  // Copy data
  memmove(header->data, data, data_length);

  // Default mac is broadcast
  uint8 mac_dest[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  //get_mac_for_ip(mac_dest, &header->dst); // TODO: implement
  send_ethernet_packet(mac_dest, ETHERNET_TYPE_IPv4, buf, (sizeof(struct ipv4_header)) + data_length);
  // Don't forget to free temp buffer at the end
  kfree(buf);
}


void
test_send_ip()
{
  uint8 data[] = {1,2,3,4,5,6,7,1,2,3,4,5,6,7,1,2,3,4,5,6,7};
  uint8 dest[] = {10,0,2,2};
  send_ipv4_packet(dest, IP_PROT_TESTING, data, sizeof(data));
}