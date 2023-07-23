#include "kernel/net/transport.h"

struct spinlock tcp_table_lock = {0};
tcp_connection tcp_connection_table[TCP_CONNECTION_TABLE_SIZE] = {0};

/**
 * Sets a tcp_connection_table entry's status to TCP_STATUS_RESERVED.
 * Returns an index to the table, or -1 on fail.
 * Users are advised to never zero the status of the corresponding entry themselves
*/
uint8 register_tcp_connection()
{
    acquire(&tcp_table_lock);
    // Loops through the entire table
    for (int i = 0; i < TCP_CONNECTION_TABLE_SIZE; i++) {
        // Checks if entry is available
        if (tcp_connection_table[i].status == 0) {
            tcp_connection_table[i].status = TCP_STATUS_RESERVED;
            release(&tcp_table_lock);
            return i;
        }
    }
    release(&tcp_table_lock);
    return -1;
}

/**
 * Closes a tcp connection with index index in the tcp_connection_table
 * Sends a packet with FIN flag if connection is already established
*/
void close_tcp_connection(uint8 index)
{
    if (index < 0 || index >= TCP_CONNECTION_TABLE_SIZE)
        pr_notice("TCO connection index out of range.");
    tcp_connection* pointer = &tcp_connection_table[index];

    if (pointer->status == TCP_STATUS_ESTABLISHED ) {
        // Send packet with FIN flag (and await ACK)
        // TODO
    }

    // Free Table Entry
    acquire(&tcp_table_lock);
    pointer->status = TCP_STATUS_INVALID;
    pointer->in_port = 0;
    pointer->partner_port = 0;
    memset(pointer->partner_ip_addr, 0, IP_ADDR_SIZE);
    pointer->current_seq_num = 0;
    pointer->last_ack_num = 0;
    pointer->receive_window_size = 0;
    pointer->receive_buffer = NULL;
    pointer->send_buffer = NULL;
    release(&tcp_table_lock);
}

/**
 * Returns index of tcp connection that corresponds to a specific connection_identifier
 * Assumptions:
 * id.protocol == CON_TCP
*/
uint8 get_tcp_connection_index(connection_identifier id)
{
    uint8 return_index = -1;
    // Don't know if we need this lock here but oh well
    acquire(&tcp_table_lock);
    for (int i = 0; i < TCP_CONNECTION_TABLE_SIZE; i++)
    {
        tcp_connection* entry = &tcp_connection_table[i];
        // Check if in port macthes
        if (id.identification.tcp.in_port == entry->in_port) {
            // If in port matches either TCP_STATUS_AWAITING or everything matches
            if (entry->status == TCP_STATUS_AWAITING 
                || (entry->status & (TCP_STATUS_ESTABLISHED|TCP_STATUS_ESTABLISHING)
                    && memcmp(id.identification.tcp.partner_ip_addr, entry->partner_ip_addr, IP_ADDR_SIZE)
                    && entry->partner_port == id.identification.tcp.partner_port)) {
                return_index = i;
                break;
            }
        }
    }
    release(&tcp_table_lock);
    return return_index;
}

 // Initialize TCP Connection table in here
void tcp_init()
{
    initlock(&tcp_table_lock, "TCP Table lock");
    // For now just copied from udp_init to test
    uint8 dest_address[IP_ADDR_SIZE] = {10,0,2,3};
    uint8 data[] = {1,2,3,4,5,1,2,3,4,5,1,2,3,4,5};
    send_tcp_packet(dest_address, 52525, 1255, data, sizeof(data));
    // Test listen on port 22
    uint8 idx = register_tcp_connection();
    if (idx == -1)
        return;
    tcp_connection* entry = &tcp_connection_table[idx];
    entry->in_port = 22;
    entry->status = TCP_STATUS_AWAITING;
}

/**
 * Sends packets to accept an incoming tcp connection and updates the tcp_connection entry accordingly
 * Note that packets in tcp_packet already have dst and src port endianess converted
 * len is length of tcp_header + tcp_options + tcp_data
 * returns non-zero on success
*/
uint8
accept_tcp_connection(uint8 partner_address[IP_ADDR_SIZE], struct tcp_header* tcp_packet, uint16 len)
{
    connection_identifier id = {0};
    id.protocol = CON_TCP;
    id.identification.tcp.in_port = tcp_packet->dst;
    id.identification.tcp.partner_port = tcp_packet->src;
    memmove(id.identification.tcp.partner_ip_addr, partner_address, IP_ADDR_SIZE);
    // Get index for a specific connection
    uint8 index = get_tcp_connection_index(id);
    // We aren't expecting this connection.
    if (index == -1)
        return 0;
    tcp_connection* entry = &tcp_connection_table[index];
    entry->partner_port = tcp_packet->src;
    pr_debug("%d", len);

    // TODO: Set entry fields while establishing connection
    // TODO: Actually establish connection :D
    return 1;
}

// Just for testing, need to adapt for general use (add connection idx as a parameter or something?)
void
send_tcp_packet(uint8 dest_address[IP_ADDR_SIZE], uint16 source_port, uint16 dest_port, void* data, uint16 data_length)
{
    void* buf = kalloc_zero();
    struct tcp_header* head = (struct tcp_header*) buf;
    head->src = source_port;
    memreverse(&head->src, sizeof(head->src));
    head->dst = dest_port;
    memreverse(&head->dst, sizeof(head->dst));
    head->sequence_num = r_time();
    memreverse(&head->sequence_num, sizeof(head->sequence_num));
    head->ack_num = 0;
    head->flags = TCP_FLAGS_FIN;
    // Header is size 5 * 32
    head->offset = 5;
    memreverse(&head->combined_flag_offset, sizeof(head->combined_flag_offset));

    head->receive_window = 3000;
    memreverse(&head->receive_window, sizeof(head->receive_window));
    head->checksum = 0;
    head->urgent_pointer = 0;
    memmove(head->options_data, data, data_length);
    send_ipv4_packet(dest_address, IP_PROT_TCP, head, sizeof(struct tcp_header) + data_length);
}