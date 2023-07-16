/*! \file net.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_net_h
#define INCLUDED_kernel_net_net_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/types.h"
#include "kernel/defs.h"
#include "kernel/net/netdefs.h"
#include "kernel/net/ethernet.h"

#define MAX_TRACKED_CONNECTIONS 16

enum protocol {
    CON_UNKNOWN = 0,
    CON_ARP = 1,
    CON_TCP = 2,
    CON_UDP = 3,
    CON_DHCP = 4,
    CON_ICMP = 5,
};

/**
 * Identifier for a specific connection. 
 * Used to get buffer from connections array to wake/notify waiting processes and supply received data
 * Initialize with = {0}!
*/
typedef struct __connection_identifier {
    uint16 protocol;
    union __identification {
        uint64 value;
        struct arp {
          uint8 target_ip[IP_ADDR_SIZE];
        } arp;
        struct tcp {
            uint8  dst_ip_addr[IP_ADDR_SIZE];
            uint16 dst_port;
            uint16 src_port;
        } tcp;
        struct udp {
            uint8  dst_ip_addr[IP_ADDR_SIZE];
            uint16 dst_port;
            uint16 src_port;
        } udp;
        struct dhcp {
            uint32 transaction_id;
            uint8 message_type;
        } dhcp;
        struct icmp {
            uint32 sequence_num;
        } icmp;
    } identification;
} connection_identifier;

typedef struct __connections_entry {
    // Signal. 0 if buffer empty, 1 waiting, 2 not waiting
    uint32 signal; 
    connection_identifier identifier;
    void* buf;
} connection_entry;

connection_identifier compute_identifier(struct ethernet_header* ethernet_header);
void copy_card_mac(uint8 copy_to[MAC_ADDR_SIZE]);
connection_entry* get_entry_for_identifier(connection_identifier id);
void copy_data_to_entry(connection_entry* entry, struct ethernet_header* ethernet_header);
void net_init();
int handle_incoming_connection(struct ethernet_header* ethernet_header);
int notify_of_response(struct ethernet_header* ethernet_header);
void add_connection_entry(connection_identifier id, void *buf);
void wait_for_response(connection_identifier id, struct spinlock* lock);

void print_mac_addr(uint8 mac_addr[MAC_ADDR_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
