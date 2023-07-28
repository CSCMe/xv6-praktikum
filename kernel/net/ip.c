#include "kernel/net/ip.h"
#include "kernel/net/arp.h"
#include "kernel/net/dhcp.h"

static uint8 our_ip_address[IP_ADDR_SIZE];


void print_ip(uint8 octets[]) {
  pr_debug("%d.%d.%d.%d", octets[0], octets[1], octets[2], octets[3]);
}

void copy_ip_addr(uint8 copy_to[IP_ADDR_SIZE]) {
  memmove(copy_to, our_ip_address, IP_ADDR_SIZE);
}

/**
 * Sets initial IP to 0
 * Starts DHCP Request
*/
void ip_init() {
  uint8 starting_ip[] = {0, 0, 0, 0};
  memmove(our_ip_address, starting_ip, IP_ADDR_SIZE);
  dhcp_get_ip_address(our_ip_address);
}

void send_ipv4_packet(uint8 destination[IP_ADDR_SIZE], uint8 ip_protocol, void *data, uint16 data_length) {
  if (data_length > PGSIZE - sizeof(struct ipv4_header)) {
    pr_info("Send IP: Can't fit data into page. Aborting\n");
    return;
  }

  void *buf = kalloc_zero();
  if (!buf) panic("send ip kalloc");

  struct ipv4_header *header = (struct ipv4_header *)buf;

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

  // Checksum is calculated only after the rest of
  // the header is completed. For now, it is left as zero
  header->header_checksum = 0;

  // Copy own IP-Address
  copy_ip_addr((uint8 *)&header->src);

  // Copy destination IP-Address
  memmove(header->dst, destination, IP_ADDR_SIZE);

  // Copy data
  memmove(header->data, data, data_length);

  // Default mac is broadcast
  uint8 mac_dest[6] = MAC_ADDR_BROADCAST;

  // Calculate the checksum
  header->header_checksum = ipv4_checksum(header);

  // For debug purposes, verify said checksum
  // The IPv4 checksum should complement the header such that
  // calculating the checksum again should yield zero.
  uint16 checksum_verify = ipv4_checksum(header);
  if (checksum_verify != 0) {
    pr_emerg("IPv4 checksum failed: Should be 0 but is 0x%x\n", checksum_verify);
    pr_emerg("This will cause our packet to be dropped!\n");
    pr_emerg("(That being said, let's try sending it anyways ¯\\_(ツ)_/¯)\n");
  }

  get_mac_for_ip(mac_dest, header->dst);
  send_ethernet_packet(mac_dest, ETHERNET_TYPE_IPv4, header, (sizeof(struct ipv4_header)) + data_length);
  // Don't forget to free temp buffer at the end
  kfree(buf);
}

// From https://web.archive.org/web/20120430075019/http://web.eecs.utk.edu/~cs594np/unp/checksum.html
// Refer to https://en.wikipedia.org/wiki/Internet_checksum for an explanation of how
// the checksum is calculated.
// Actually implementing this is nontrivial because you have to take care of numeric overflows
uint16 ipv4_checksum(void *ip) {
  uint16 remaining_length = IPV4_HEADER_SIZE;
  uint32 sum              = 0;

  while (remaining_length > 1) {
    sum += *((uint16 *)ip);
    ip += 2;
    if (sum & 0x80000000) /* if high order bit set, fold */
      sum = (sum & 0xFFFF) + (sum >> 16);
    remaining_length -= 2;
  }
  if (remaining_length != 0) /* take care of left over byte */
    sum += (uint16) * (uint8 *)ip;

  while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);

  return ~sum;
}

void test_send_ip() {
  uint8 data[] = {1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7, 1, 2, 3, 4, 5, 6, 7};
  uint8 dest[] = {10, 0, 2, 2};
  send_ipv4_packet(dest, IP_PROT_TESTING, data, sizeof(data));
}