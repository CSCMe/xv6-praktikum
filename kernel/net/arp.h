/*! \file arp.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_arp_h
#define INCLUDED_kernel_net_arp_h

#include "kernel/net/ip.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Defines for ARP packet fields. Endian preconverted
*/

// ARP Ethernet hardware type  (Endian preconverted)
#define ARP_HW_TYPE_ETHERNET   (1 << 8)
// ARP IEEE802.2 hardware type (Endian preconverted)
#define ARP_HW_TYPE_IEEE802    (1 << 6) 
// ARP IPv4 protocol type      (Endian preconverted)
#define ARP_PROT_TYPE_IP       (2048 >> 8)
// ARP HLEN value for 48 bit Mac Addr
#define ARP_HLEN_48_MAC        6
// ARP PLEN value for 32 bit IP Addr
#define ARP_PLEN_32_IP         4
// ARP opcode for request      (Endian preconverted)
#define ARP_OPCODE_REQ         (1 << 8)
// ARP opcode for reply        (Endian preconverted)
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

void arp_init();
void test_send_arp();

#ifdef __cplusplus
}
#endif

#endif
