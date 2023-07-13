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

// Minimum data length of ethernet packet
#define ETHERNET_MIN_DATA_LEN 46
#define ETHERNET_CRC_LEN 4

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
    // Len/Type field sent with the packet
    union {
        // Length of Ethernet frame. Valid values 0-1500   CONVERTED TO LITTLE ENDIAN
        uint16 len;
        // Type of Ethernet frame. Valid values 1536-65535 CONVERTED TO LITTLE ENDIAN
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

/**
 * IPv4 header for an IPv4 packet
 * Without options/padding
*/

struct ipv4_header {
    struct {
        uint8 version : 4;
        uint8 header_length : 4;
    };
    uint8 type_of_service;
    uint16 total_length;
    uint16 identified;
    struct {
        uint16 flags : 3;
        uint16 fragment_offset : 13;
    };
    uint8 time_to_live;
    uint8 protocol;
    uint16 header_checksum;
    uint8 src[IP_ADDR_SIZE];
    uint8 dst[IP_ADDR_SIZE];
    uint8 data[];
};

#ifdef __cplusplus
}
#endif

#endif
