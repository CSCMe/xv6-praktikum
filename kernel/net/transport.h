/*! \file transport.h
 * \brief networking things for UDP/TCP
 */

#ifndef INCLUDED_kernel_net_transport_h
#define INCLUDED_kernel_net_transport_h

#include "kernel/net/net.h"
#include "kernel/net/ip.h"
#ifdef __cplusplus
extern "C" {
#endif

// ID used in prot_id field of pseudo header
#define UDP_PROTOCOL_ID 17

/**
 * Pseudo-Header
 * Used for checksums
*/
struct udp_pseudo_header {
    uint8 src_ip[IP_ADDR_SIZE];
    uint8 dst_ip[IP_ADDR_SIZE];
    uint16 zero;
    uint16 prot_id;
    uint16 len;
};

struct udp_header {
    // Source port
    uint16 src;
    // Destination port
    uint16 dst;
    // Length of data and header fields
    uint16 len;
    // Checksum over pseudo header. 0 if unused
    uint16 checksum;
    // Data
    uint8 data[];
};


/**
 * TCP Header flags
*/
// Do not use
#define TCP_FLAGS_CWR
// Do not use
#define TCP_FLAGS_ECE
// Set if urgent pointer is used. (Which it normally isn't)
#define TCP_FLAGS_URG
// Marks this segment as an ACK
#define TCP_FLAGS_ACK
// Signals data to be forwarded immediately. Normally unused
#define TCP_FLAGS_PSH
// Used to reset connection
#define TCP_FLAGS_RST
// Used in TCP connection setup
#define TCP_FLAGS_SYN
// Indicates sender does not want to send any more data
#define TCP_FLAGS_FIN


/**
 * Structure of a TCP Header.
 * We ignore options.
*/
struct tcp_header {
    // Source Port
    uint16 src;
    // Destination Port
    uint16 dst;
    // Sequence Number of the first data octett.
    uint32 sequence_num;
    // Acknowldgement Number. Next expected sequence number
    uint32 ack_num;
    /**
     * Bit field containing flags, offset and reserved fields
     * Reversed for endian reasons
    */
    struct {
        // Flags field
        uint16 flags : 6;
        uint16 : 6;
        // Number of 32-bit words in header
        uint16 offset : 4;
    };
    // Used for congestion control
    uint16 receive_window;
    // Checksum over TCPHeader, data and pseudoheader
    uint16 checksum;
    // relative pointer to important data (?)
    uint16 urgent_pointer;
    // Data
    uint8 data[];
};

void udp_init();
void send_udp_packet(uint8 dest_address[IP_ADDR_SIZE], uint16 source_port, uint16 dest_port, void* data, uint16 data_length);

#ifdef __cplusplus
}
#endif

#endif
