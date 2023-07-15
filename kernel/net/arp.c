#include "kernel/net/arp.h"

static struct spinlock arp_lock = {0};

void arp_init() {
    initlock(&arp_lock, "ARP lock");
}

void get_mac_for_ip(uint8 mac_addr[], uint8 ip_addr[]) {
  print_ip(ip_addr);
  struct arp_packet arp;
  arp.hw_type   = ARP_HW_TYPE_ETHERNET;
  arp.prot_type = ARP_PROT_TYPE_IP;
  arp.hlen      = ARP_HLEN_48_MAC;
  arp.plen      = ARP_PLEN_32_IP;
  arp.opcode    = ARP_OPCODE_REQ;

  copy_card_mac(arp.mac_src);
  copy_ip_addr(arp.ip_src);

  // For an ARP request, the destination mac is set to all 0xFF
  uint8 dest[MAC_ADDR_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memmove(arp.mac_dest, dest, MAC_ADDR_SIZE);
  memmove(arp.ip_dest, ip_addr, IP_ADDR_SIZE);

  // Compute an identifier, which is used to match the response ethernet packet back
  // to our request.
  // In the case of ARP, the identifier is simply the target IP address
  connection_identifier token;
  token.protocol                           = CON_ARP;
  memmove(token.identification.arp.target_ip, ip_addr, IP_ADDR_SIZE);

  // Acquire a lock so we don't get a response before we are waiting for it
  acquire(&arp_lock);
  pr_debug("Sending ethernet packet\n");
  send_ethernet_packet(arp.mac_dest, ETHERNET_TYPE_ARP, (void *)&arp, sizeof(struct arp_packet));

  pr_debug("Waiting for ARP response\n");
  struct arp_packet arp_response;
  wait_for_response(token, (void *)&arp_response, &arp_lock);

  pr_debug("Got a response\n");
}

void test_send_arp() {
  struct arp_packet actual_arp = {0};
  struct arp_packet *arp       = &actual_arp;

  copy_card_mac(arp->mac_src);
  uint8 ipme[8] = {10, 0, 2, 15};
  memmove((void *)&arp->ip_src, ipme, 4);
  uint8 dest[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  memmove((void *)&arp->mac_dest, dest, 6);
  uint8 ipdest[8] = {10, 0, 2, 2};
  memmove((void *)&arp->ip_dest, ipdest, 4);


  send_ethernet_packet(dest, ETHERNET_TYPE_ARP, (void *)arp, sizeof(struct arp_packet));
}
