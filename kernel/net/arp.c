#include "kernel/net/arp.h"

static struct spinlock arp_lock = {0};

static struct __arp_table {
  uint index;
  arp_table_entry entries[NUM_ARP_TABLE_ENTRIES];
} arp_table;

void arp_init() {
  initlock(&arp_lock, "ARP lock");

  // Initialize the ARP table
  for (int i = 0; i < NUM_ARP_TABLE_ENTRIES; i++) { arp_table.entries[i].is_valid = 0; }
  arp_table.index = 0;

  // Insert broadcast address
  uint8 ip_broadcst[IP_ADDR_SIZE] = IP_ADDR_BROADCAST;
  uint8 mac_broadcast[MAC_ADDR_SIZE] = MAC_ADDR_BROADCAST;
  arp_table_insert(ip_broadcst, mac_broadcast);
}

uint8 arp_table_lookup(uint8 ip_addr[], uint8 write_mac_addr_to[]) {
  for (int i = 0; i < NUM_ARP_TABLE_ENTRIES; i++) {
    if (arp_table.entries[i].is_valid) {
      if (memcmp(ip_addr, arp_table.entries[i].ip_addr, IP_ADDR_SIZE) == 0) {
        // Found our entry
        memmove(write_mac_addr_to, arp_table.entries[i].mac_addr, IP_ADDR_SIZE);
        return 1;
      }
    }
  }

  // Entry not found
  return 0;
}

// Since we never remove entries from the table (TTL is not respected,
// due to a complete lack of time in the kernel)
// insertion is quite simple, we simply replace the oldest element
void arp_table_insert(uint8 ip_addr[], uint8 mac_addr[]) {
  arp_table.entries[arp_table.index].is_valid = 1;
  memmove(arp_table.entries[arp_table.index].ip_addr, ip_addr, IP_ADDR_SIZE);
  memmove(arp_table.entries[arp_table.index].mac_addr, mac_addr, MAC_ADDR_SIZE);

  arp_table.index += 1;
  arp_table.index %= NUM_ARP_TABLE_ENTRIES;
}

void get_mac_for_ip(uint8 mac_addr[], uint8 ip_addr[]) {
  uint8 cached = 0;

  // Check if the ARP table has the mac address cached
  if (arp_table_lookup(ip_addr, mac_addr)) {
    cached = 1;
    goto log_result;
  }

  struct arp_packet arp;
  arp.hw_type   = ARP_HW_TYPE_ETHERNET;
  arp.prot_type = ARP_PROT_TYPE_IP;
  arp.hlen      = ARP_HLEN_48_MAC;
  arp.plen      = ARP_PLEN_32_IP;
  arp.opcode    = ARP_OPCODE_REQ;

  copy_card_mac(arp.mac_src);
  copy_ip_addr(arp.ip_src);

  // For an ARP request, the destination mac is set to broadcast
  uint8 dest[MAC_ADDR_SIZE] = MAC_ADDR_BROADCAST;
  memmove(arp.mac_dest, dest, MAC_ADDR_SIZE);
  memmove(arp.ip_dest, ip_addr, IP_ADDR_SIZE);

  // Compute an identifier, which is used to match the response ethernet packet back
  // to our request.
  // In the case of ARP, the identifier is simply the target IP address
  connection_identifier token = {0};
  token.protocol              = CON_ARP;
  memmove(token.identification.arp.target_ip, ip_addr, IP_ADDR_SIZE);

  // Acquire a lock so we don't get a response before we are waiting for it
  acquire(&arp_lock);

  struct arp_packet arp_response;
  add_connection_entry(token, (void *)&arp_response);

  send_ethernet_packet(arp.mac_dest, ETHERNET_TYPE_ARP, (void *)&arp, sizeof(struct arp_packet));

  wait_for_response(token, &arp_lock);

  memmove(mac_addr, arp_response.mac_src, MAC_ADDR_SIZE);

  // Store the result in the ARP table so we don't have to look it up again
  arp_table_insert(ip_addr, mac_addr);

log_result:
  pr_debug("resolved ");
  print_ip(ip_addr);
  pr_debug(" to ");
  print_mac_addr(mac_addr);
  if (cached) pr_debug(" (cached)");
  pr_debug("\n");
}