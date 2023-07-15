/*! \file dhcp.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_dhcp_h
#define INCLUDED_kernel_net_dhcp_h

#include "kernel/net/net.h"
#include "kernel/net/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DHCP_HARDWARE_ADDR_SIZE 16
#define DHCP_SERVER_NAME_SIZE 64
#define DHCP_BOOT_FILENAME_SIZE 128
#define DHCP_OPTIONS_SIZE 64

// DHCP opcodes. Specifics are encoded in message type option
#define DHCP_OPCODE_REQUEST 1
#define DHCP_OPCODE_REPLY   2

#define DHCP_HTYPE_ETHERNET 1

#define DHCP_FLAGS_BROADCAST 1 << 8


struct dhcp_packet {
    /**
     * opcode. 
     * Possible values: DHCP_OPCODE_REQUEST, DHCP_OPCODE_REPLY
    */
    uint8 opcode;
    // Underlying hardware type. For now only DHCP_HTYPE_ETHERNET allowed
    uint8 htype;
    // Length of hardware address. MAC_ADDR_SIZE for DHCP_HTYPE_ETHERNET
    uint8 hlen;
    // Set to 0 by client before transmitting request
    uint8 hop_count;
    // Randomly generated number by client. Used to identify conversation
    uint32 transaction_id;
    // Number of seconds since client began to acquire a new lease
    uint16 seconds;
    // Set to DHCP_FLAGS_BROADCAST if ip address is unknown
    uint16 flags;
    // Client IP Address. 0 when acquiring new
    uint8 client_ip_addr[IP_ADDR_SIZE];
    // Offered IP address
    uint8 your_ip_addr[IP_ADDR_SIZE];
    // IP address of server to contact in next step
    uint8 server_ip_addr[IP_ADDR_SIZE];
    // Just ignore. Used for forwarding
    uint8 gateway_ip_addr[IP_ADDR_SIZE];
    // hardware (ethernet) address used for identification and communication
    uint8 client_hardware_addr[MAC_ADDR_SIZE];
    // Server appends name in DHCPOFFER or DHCPACK messages
    char server_name[DHCP_SERVER_NAME_SIZE];
    // Just ignore
    char boot_filename[DHCP_BOOT_FILENAME_SIZE];
    // see http://www.tcpipguide.com/free/t_DHCPOptionsOptionFormatandOptionOverloading-2.htm
    uint8 options[DHCP_OPTIONS_SIZE];
};

#ifdef __cplusplus
}
#endif

#endif