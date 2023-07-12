/*! \file net.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_h
#define INCLUDED_kernel_net_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/types.h"

#define IP_ADDR_SIZE 4
#define MAC_ADDR_SIZE 6

// End of valid frame len values
#define ETHERNET_MAX_FRAME_LEN 1500

// All in big endian
enum EtherType {
    ETHERNET_TYPE_DATA  = 0x0000,
    ETHERNET_TYPE_IPv4   = 0x0800,
    ETHERNET_TYPE_ARP    = 0x0806,
};

/**
 * All the values that actually matter to the card.
 * We don't support VLAN
*/
struct ethernet_header {
    uint8 dest[MAC_ADDR_SIZE];
    uint8 src[MAC_ADDR_SIZE];
    // Len/Type/VLAN field. Gets ignored by card. Set in desc->len instead. Still useful for bookkeeping
    union {
        // Length of Ethernet frame. Valid values 0-1500   CONVERTED TO LITTLE ENDIAN
        uint16 len;
        // Type of Ethernet frame. Valid values 1536-65535 CONVERTED TO LITTLE ENDIAN
        uint16 type;
    }; 
    uint8 data[]; // Data array of unspecified length
};


/**
 * Defines for ARP packet fields. Little endian
*/

// ARP Ethernet hardware type  (Little Endian)
#define ARP_HW_TYPE_ETHERNET   (1 << 8)
// ARP IEEE802.2 hardware type (Little Endian)
#define ARP_HW_TYPE_IEEE802    (1 << 6) 
// ARP IPv4 protocol type      (Little Endian)
#define ARP_PROT_TYPE_IP       (2048 >> 8)
// ARP HLEN value for 48 bit Mac Addr
#define ARP_HLEN_48_MAC        6
// ARP PLEN value for 32 bit IP Addr
#define ARP_PLEN_32_IP         4
// ARP opcode for request      (Little Endian)
#define ARP_OPCODE_REQ         (1 << 8)
// ARP opcode for reply       (Little Endian)
#define ARP_OPCODE_REPLY       (2 << 8)

/**
 * Fields inside an arp packet
*/
struct arp_packet {
    /**
     * hardware type
     * Either Ethernet (ARP_HW_TYPE_ETHERNET)
     * or IEEE 802.2   (ARP_HW_TYPE_IEEE802)
     * Recommend use of Ethernet, unkown what IEEE802.2 is
    */
    uint16 hw_type;
    /**
     * Protocol Type ARP packet is for
     * Known: ARP_PROT_TYPE_IP
    */
    uint16 prot_type;
    // Only supports ARP_HLEN_48_MAC
    uint8 hlen;
    // Only supports ARP_PLEN_32_IP
    uint8 plen;
    /**
     * opcode
     * Either ARP_OPCODE_REQUEST or ARP_OPCODE_REPLY
    */
    uint16 opcode;
    // Source MAC
    uint8 mac_src[MAC_ADDR_SIZE];
    // Source IP
    uint8 ip_src[IP_ADDR_SIZE];
    // Dest MAC
    uint8 mac_dest[MAC_ADDR_SIZE];
    // DEST IP
    uint8 ip_dest[IP_ADDR_SIZE];
};

#define ARP_PACKET_SIZE sizeof(struct arp_packet)


#ifdef __cplusplus
}
#endif

#endif
