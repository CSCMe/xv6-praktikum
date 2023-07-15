/*! \file ip.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_ip_h
#define INCLUDED_kernel_net_ip_h

#include "kernel/net/net.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Defines for IPv4 headers
*/
// Define for IPv4
#define IP_VERSION_4             4
// Not supported (for now) but still defined
#define IP_VERSION_6             6 

// Flag to not fragment packet
#define IPv4_FLAG_DF  0x2
// Indicates that more fragments come after this. Unset if last/only.
#define IPv4_FLAG_MF  0x1

#define IP_PROT_ICMP    0x01
#define IP_PROT_TCP     0x06
#define IP_PROT_UDP     0x11
#define IP_PROT_TESTING 0xFD

typedef union __ip_address {
    uint8 octets[IP_ADDR_SIZE];
    uint32 value;
} ip_address;

void print_ip(ip_address ip);


#define IPv4_TTL_DEFAULT 64

/**
 * IPv4 header for an IPv4 packet
 * No support for options/padding
*/
struct ipv4_header {
    /**
     * Bitfield for version and header length
     * Swapped around because endian(?)
    */
    struct {
        /**
         * Length of header
         * Amount of 32 bit segments in header
        */
        uint8 header_length : 4;
        /**
         * IP Protocol version
         * Either IP_VERSION_4 or IP_VERSION_6
        */
        uint8 version : 4;
    };
    // Complicated and congestion control. Just set to 0
    uint8 type_of_service;
    // Total length of IP packet in bytes. Includes data & header. CONVERT TO BIG ENDIAN!
    uint16 total_length;
    // Unique number for IP fragmentation and reassembly. Just set to 0. CONVERT TO BIG ENDIAN!
    uint16 identification;
    /**
     * Union containing flags and fragment offset
     * Watch endianness
    */
   union {
        /**
         * Bit field for fragment offset/flags field
         * Flags and fragment offset swapped because endian
        */
        struct {

            // What position a fragmented packet's data starts at when reassembled
            uint16 fragment_offset : 13;
            /**
            * Bit of weirdness because endianness. Use Defines
            * Bit 0: always 0
            * Bit 1: Don't Fragment (IPv4_FLAGS_DF)
            * Bit 2: More Fragments (IPv4_FLAGS_MF)
            */
            uint16 flags : 3;
        };
        // Needed to convert endian
        uint16 flags_fragment_mixed;
   };
    // Describes lifetime of packet. De-facto max hop-count
    uint8 time_to_live;
    /**
     * ICMP: IP_PROT_ICMP
     * TCP:  IP_PROT_TCP
     * UDP:  IP_PROT_UDP
    */
    uint8 protocol;
    // Header checksum. 0 for sake of calculation. CONVERT TO BIG ENDIAN (?)
    uint16 header_checksum;
    // Source IP-Addr
    ip_address src;
    // Dest IP-Addr
    ip_address dst;
    // Contained data
    uint8 data[];
};


void test_send_ip();
void ip_init();
void send_ipv4_packet(ip_address destination, uint8 ip_protocol, void* data, uint16 data_length);

#ifdef __cplusplus
}
#endif

#endif
