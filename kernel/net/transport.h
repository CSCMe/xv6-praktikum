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

// Size of tcp_connection_table
#define TCP_CONNECTION_TABLE_SIZE 32

// ID used in prot_id field of pseudo header
#define UDP_PROTOCOL_ID 17
#define TCP_PROTOCOL_ID 6

/**
 * Pseudo-Header
 * Used for checksums
*/
struct udp_pseudo_header {
  uint8 src_ip[IP_ADDR_SIZE];
  uint8 dst_ip[IP_ADDR_SIZE];
  uint8 zero;
  uint8 prot_id;
  // Make sure to convert endianness of len
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
#define TCP_FLAGS_CWR 0b10000000
// Do not use
#define TCP_FLAGS_ECE 0b1000000
// Set if urgent pointer is used. (Which it normally isn't)
#define TCP_FLAGS_URG 0b100000
// Marks this segment as an ACK
#define TCP_FLAGS_ACK 0b10000
// Signals data to be forwarded immediately. Normally unused
#define TCP_FLAGS_PSH 0b1000
// Used to reset connection
#define TCP_FLAGS_RST 0b100
// Used in TCP connection setup
#define TCP_FLAGS_SYN 0b10
// Indicates sender does not want to send any more data
#define TCP_FLAGS_FIN 0b1
// No flags
#define TCP_FLAGS_NONE 0


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
     * Reversed for endian reasons. Union to enable additional reversal
    */
  union {
    struct {
      // Flags field
      uint16 flags : 6;
      uint16 : 6;
      // Number of 32-bit words in header
      uint16 offset : 4;
    };
    uint16 combined_flag_offset;
  };

  // Used for congestion control
  uint16 receive_window;
  // Checksum over TCPHeader, data and pseudoheader
  uint16 checksum;
  // relative pointer to important data (?)
  uint16 urgent_pointer;
  // Variable sized options and data
  uint8 options_data[];
};

#define TCP_STATUS_INVALID 0
// We're reserving this entry
#define TCP_STATUS_RESERVED 0x1
// We're currently establishing the connection. Might not be needed
#define TCP_STATUS_ESTABLISHING 0x2
// We're waiting for an incoming connection on in_port
#define TCP_STATUS_AWAITING 0x4
//The connection has been established. Ready to send/receive data
#define TCP_STATUS_ESTABLISHED 0x8

/**
 * Struct for tcp_connection_table entries
*/
typedef struct __tcp_connection {
  // Connection status.
  uint8 status;
  // Port for incoming messages for this connection
  uint16 in_port;
  // Port for outgoing messages for this connection
  uint16 partner_port;
  // partner ip address
  uint8 partner_ip_addr[IP_ADDR_SIZE];
  // Sequence number of next sent byte
  uint32 next_seq_num;
  // Last received ack number (Everything is fine if == last_ack_num)
  uint32 last_ack_num;
  // Sequence number of last received data
  uint32 last_sent_ack_num;
  // How many data bytes we can receive next
  uint16 receive_window_size;
  // Where to place new data in receive buffer
  uint32 receive_buffer_loc;
  // Buffer for receiving. Size = receive_window_size
  void *receive_buffer;
  // Buffer for sending. Holds current_seq_num - last_ack_num bytes to allow for retransmission
  // Might not be needed
  void *send_buffer;
} tcp_connection;

void udp_init();
void send_udp_packet(uint8 dest_address[IP_ADDR_SIZE], uint16 source_port, uint16 dest_port,
  void *data, uint16 data_length);
void tcp_init();
int tcp_unbind(uint8 handle);
void send_tcp_packet(uint8 dest_address[IP_ADDR_SIZE], uint16 source_port, uint16 dest_port,
  void *data, uint16 data_length);
int32 send_tcp_packet_wait_for_ack(
  uint8 connection_id, void *data, uint16 data_length, uint16 flags, void *con_entry_buf);
uint8 await_incoming_tcp_connection(uint16 port);
int tcp_send_receive(int id, void *data, int data_len, void *rec_buf, int rec_buf_len);

uint8 wake_awaiting_connection(uint8 partner_address[IP_ADDR_SIZE], struct tcp_header *tcp_packet, uint16 len);
uint32 calculate_pseudo_header_checksum(
  uint8 src_ip[IP_ADDR_SIZE], uint8 dst_ip[IP_ADDR_SIZE], uint8 prot_id, uint16 len);
uint16 calculate_internet_checksum(uint16 len, uint8 *data);

#ifdef __cplusplus
}
#endif

#endif
