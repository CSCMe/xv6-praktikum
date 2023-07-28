/*! \file ethernet.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_ethernet_h
#define INCLUDED_kernel_net_ethernet_h


#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/net/netdefs.h"

// Minimum data length of ethernet packet
#define ETHERNET_MIN_DATA_LEN 46
#define ETHERNET_CRC_LEN 4

// In wrong Endian. Convert type field before send
enum EtherType {
  ETHERNET_TYPE_DATA = 0x0000,
  ETHERNET_TYPE_IPv4 = 0x0800,
  ETHERNET_TYPE_ARP  = 0x0806,
};

/**
 * All the values that actually matter to the card.
 * We don't support VLAN
*/
struct ethernet_header {
  uint8 dest[MAC_ADDR_SIZE];
  uint8 src[MAC_ADDR_SIZE];
  // Len/Type field sent with the packet
  union {
    // Length of Ethernet frame. Valid values 0-1500   CONVERTED TO BIG ENDIAN
    uint16 len;
    // Type of Ethernet frame. Valid values 1536-65535 CONVERTED TO BIG ENDIAN
    uint16 type;
  };
  uint8 data[]; // Data array of unspecified length
};


/**
 * Tailer of ethernet frame
 * Contains CRC Checksum
*/
struct ethernet_tailer {
  uint8 crc[ETHERNET_CRC_LEN];
};

void send_ethernet_packet(uint8 dest_mac[MAC_ADDR_SIZE], enum EtherType type, void *data, uint16 data_length);

#ifdef __cplusplus
}
#endif

#endif
