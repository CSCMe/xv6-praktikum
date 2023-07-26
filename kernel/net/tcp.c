#include "kernel/net/transport.h"

struct spinlock tcp_table_lock = {0};
tcp_connection tcp_connection_table[TCP_CONNECTION_TABLE_SIZE] = {0};


/**
 * Calculates a TCP Checksum
*/
uint16 calculate_tcp_checksum(uint8 src_ip[IP_ADDR_SIZE], uint8 dst_ip[IP_ADDR_SIZE], uint16 len, uint8* data)
{
    uint32 csum = calculate_pseudo_header_checksum(src_ip, dst_ip, TCP_PROTOCOL_ID, len) + calculate_internet_checksum(len, data);

    // Convert to 1s complement sum
    while (csum >= (1 << 16)) {
        csum = (csum >> 16) + ((csum << 16) >> 16);
    }

    // Return 1s complement
    return ~csum ;
}

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
    if (index >= TCP_CONNECTION_TABLE_SIZE)
        pr_notice("TCP connection index out of range.");
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
    pointer->next_seq_num = 0;
    pointer->last_ack_num = 0;
    pointer->last_sent_ack_num = 0;
    pointer->receive_window_size = 0;
    kfree(pointer->receive_buffer);
    kfree(pointer->send_buffer);
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

/**
 * Converts endianness of all fields of a tcp header
*/
void
tcp_header_convert_endian(struct tcp_header* header)
{
    memreverse(&header->src, sizeof(header->src));
    memreverse(&header->dst, sizeof(header->dst));
    memreverse(&header->sequence_num, sizeof(header->sequence_num));
    memreverse(&header->ack_num, sizeof(header->ack_num));
    memreverse(&header->combined_flag_offset, sizeof(header->combined_flag_offset));
    memreverse(&header->receive_window, sizeof(header->receive_window));
    // Check if checksum needs reversing
    memreverse(&header->checksum, sizeof(header->checksum));
    memreverse(&header->urgent_pointer, sizeof(header->urgent_pointer));
}

/**
 * Wake any awaiting tcp connections
 * Returns 1 on wake, 0 on nothing
*/
uint8
wake_awaiting_connection(uint8 partner_address[IP_ADDR_SIZE], struct tcp_header* tcp_packet, uint16 len)
{
    connection_identifier id = {0};
    id.protocol = CON_TCP;
    id.identification.tcp.in_port = tcp_packet->dst;
    id.identification.tcp.partner_port = tcp_packet->src;
    memmove(id.identification.tcp.partner_ip_addr, partner_address, IP_ADDR_SIZE);
    // Get index for a specific connection
    uint8 index = get_tcp_connection_index(id);

    if (index == (uint8)-1)
        return 0;
    tcp_connection* entry = &tcp_connection_table[index];
    memmove(entry->receive_buffer, tcp_packet, len);
    memmove(entry->partner_ip_addr, partner_address, IP_ADDR_SIZE);
    wakeup(entry);
    return 1;
}

/**
 * Sends packets to accept an incoming tcp connection and updates the tcp_connection entry accordingly
 * Note that packets in tcp_packet already have dst and src port endianess converted
 * returns non-zero on success
*/
uint8
accept_tcp_connection(struct tcp_header* tcp_packet, uint8 connection_index)
{   
    // Since this is an awaiting connection in_port is already set and IP was set by wake
    tcp_connection* entry = &tcp_connection_table[connection_index];
    entry->partner_port = tcp_packet->src;
    entry->status = TCP_STATUS_ESTABLISHING;

    connection_identifier id = {0};
    id.protocol = CON_TCP;
    id.identification.tcp.in_port = tcp_packet->dst;
    id.identification.tcp.partner_port = tcp_packet->src;
    memmove(id.identification.tcp.partner_ip_addr, entry->partner_ip_addr, IP_ADDR_SIZE);

    // Now that we've used src and dst, we reverse endianness of entire thing
    tcp_header_convert_endian(tcp_packet);

    // We aren't expecting this connection.
    if (!(tcp_packet->flags & TCP_FLAGS_SYN))
    {
        pr_emerg("Wrong flags\n");
        return 0;
    }

    // Set tcp connection fields
    entry->last_ack_num = 0;
    entry->next_seq_num = r_time();

    // Build response
    void* buf = kalloc_zero();
    if (!buf)
        panic("TCP SYN accept response buf kalloc\n");

    struct tcp_header* resp_header = (struct tcp_header*) buf;
    
    resp_header->src = entry->in_port;
    resp_header->dst = entry->partner_port;
    resp_header->sequence_num = entry->next_seq_num;
    resp_header->ack_num = tcp_packet->sequence_num + 1;
    resp_header->flags = TCP_FLAGS_ACK | TCP_FLAGS_SYN;
    resp_header->offset = sizeof(struct tcp_header) / 4;

    resp_header->receive_window = PGSIZE - 300;

    resp_header->urgent_pointer = 0;

    struct tcp_header* final = kalloc_zero();
    if (!final)
        panic("TCP SYN final kalloc fail\n");

    tcp_header_convert_endian(resp_header);
    
    // Calculate checksum
    uint8 my_ip[IP_ADDR_SIZE] = {0};
    copy_ip_addr(my_ip);
    resp_header->checksum = calculate_tcp_checksum(my_ip, entry->partner_ip_addr, sizeof(struct tcp_header), (uint8*) resp_header);

    add_connection_entry(id, final);
    send_ipv4_packet(entry->partner_ip_addr, IP_PROT_TCP, resp_header, sizeof(struct tcp_header));
    wait_for_response(id);
    
    pr_debug("%p", final->combined_flag_offset);

    tcp_header_convert_endian(final);

    pr_emerg("Established TCP connection with: %d.%d.%d.%d:%d on port: %d\n", entry->partner_ip_addr[0], entry->partner_ip_addr[1], entry->partner_ip_addr[2], entry->partner_ip_addr[3], entry->partner_port, entry->in_port);
    entry->last_sent_ack_num = resp_header->ack_num;
    if (!(final->flags & TCP_FLAGS_ACK) || (final->flags & TCP_FLAGS_SYN)) {
        pr_notice("TCP Connection failed\n");
        pr_debug("%p", final->combined_flag_offset);
        return -1;
    }

    entry->last_ack_num = final->ack_num;
    entry->status = TCP_STATUS_ESTABLISHED;
    return 1;
}

/**
 * Sends a tcp ack for data from sequence num + len
*/
void
send_tcp_ack(uint8 connection_id, uint32 sequence_num, uint32 len)
{
    tcp_connection* connection = &tcp_connection_table[connection_id];
    void* send_buf = kalloc_zero();
    struct tcp_header* header = (struct tcp_header*) send_buf;
    header->src = connection->in_port;
    header->dst = connection->partner_port;

    // Sequence num 0 ok?
    header->sequence_num = 0;
    header->ack_num = sequence_num + len;
    header->flags = TCP_FLAGS_ACK;
    header->offset = sizeof(struct tcp_header) / 4;
    header->receive_window = connection->receive_window_size;


    header->checksum = 0;
    // Set urgent pointer
    header->urgent_pointer = 0;

    // Calculate checksum
    uint8 my_ip[IP_ADDR_SIZE] = {0};
    copy_ip_addr(my_ip);
    tcp_header_convert_endian(header);
    header->checksum = calculate_tcp_checksum(my_ip, connection->partner_ip_addr, sizeof(struct tcp_header), (uint8*) header);


    // Actually send this ack.
    // Don't expect a response.
    send_ipv4_packet(connection->partner_ip_addr, IP_PROT_TCP, header, sizeof(struct tcp_header));
}

/**
 * Sends a tcp packet and waits for an ACK response.
 * Returns offset to the receive buffer in case the response contains new data.
 * Else -1
*/
int32
send_tcp_packet_wait_for_ack(uint8 connection_id, void* data, uint16 data_length, uint16 flags)
{
    tcp_connection* connection = &tcp_connection_table[connection_id];
    void* send_buf = kalloc_zero();
    void* rec_buf = kalloc_zero();
    struct tcp_header* header = (struct tcp_header*) send_buf;

    // Set destination and source and flags
    header->src = connection->in_port;
    header->dst = connection->partner_port;

    // Set sequence number and increase next sequence number
    header->sequence_num = connection->next_seq_num;
    connection->next_seq_num += data_length;

     // Set No flags ( but still correct ack num in case that's important)
    header->ack_num = connection->last_sent_ack_num;
    header->flags = flags;

    // Size of header in 32 bits
    // No support for sending options
    header->offset = sizeof(struct tcp_header) / 4;

    // Copy data to options_data field
    header->receive_window = connection->receive_window_size;
    // TODO: Add checksum calc
    header->checksum = 0;
    // No urgent pointer
    header->urgent_pointer = 0;

    // Convert to correct endianness
    tcp_header_convert_endian(header);

    // Calculate connection identifier
    connection_identifier id = {0};
    id.protocol = CON_TCP;
    id.identification.tcp.in_port = connection->in_port;
    id.identification.tcp.partner_port = connection->partner_port;
    memmove(&id.identification.tcp.partner_ip_addr, connection->partner_ip_addr, IP_ADDR_SIZE);

    // Send packet
    add_connection_entry(id, rec_buf);
    send_ipv4_packet(connection->partner_ip_addr, IP_PROT_TCP, header, sizeof(struct tcp_header) + data_length);
    uint32 resp_len = wait_for_response(id);
    kfree(send_buf);

    // We now have a response in rec_buf
    struct tcp_header* response = (struct tcp_header*) rec_buf;
    // Convert endianess of response to something usable
    tcp_header_convert_endian(response);

    // Update ack num
    if (response->flags & TCP_FLAGS_ACK) {
        if (response->ack_num > connection->last_ack_num) {
            connection->last_ack_num = response->ack_num;
        } else {
            // Error handling? Not right now, thanks. TODO?
            pr_notice("TCP: Lost data on connection id: %d\n", connection_id);
        }
    }

    // If we requested to close the connection, expect the server to respect that
    // (Don't actually do anything though, if the server doesn't respect our FIN then thats
    // it's own problem)
    if (flags & TCP_FLAGS_FIN && !(response->flags & TCP_FLAGS_FIN))
        pr_notice("Server did not agree to close connection");

    // Received new data! Yay. Do something?
    if (response->sequence_num == connection->last_sent_ack_num) {
        // Calculate length of new data
        uint32 resp_data_length = resp_len - (response->offset * 4);

        // Copy new data to receive_buffer_loc
        // Don't actually panic tho
        if (resp_data_length > connection->receive_window_size)
            pr_notice("TCP response larger than receive buffer. Error\n");

        // Copy remaining data to receive buffer
        memmove(connection->receive_buffer + connection->receive_buffer_loc, (uint8*) response + (response->offset * 4), resp_data_length);

        // Reduce receive window size
        connection->receive_window_size -= resp_data_length;

        // Increase offset for buffer
        uint32 old_buf_loc = connection->receive_buffer_loc;
        connection->receive_buffer_loc += resp_data_length;

        // Send ack
        send_tcp_ack(connection_id, response->sequence_num, resp_data_length);
        return old_buf_loc;
    }
    return -1;
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


/**
 * Waits for an incoming connection on port port
 * Returns connection ID
*/
uint8
await_incoming_tcp_connection(uint16 port)
{
    // Register a new connection
    uint8 idx = register_tcp_connection();
    if (idx == -1)
        return -1;
    tcp_connection* entry = &tcp_connection_table[idx];
    entry->in_port = port;
    entry->status = TCP_STATUS_AWAITING;
    entry->receive_buffer = kalloc_zero();
    entry->send_buffer = kalloc_zero();

    // Sleep and wait for wake_awaiting_connection
    struct spinlock needed = {0};
    initlock(&needed, "Needed awaiting");
    acquire(&needed);
    sleep(entry, &needed);
    release(&needed);
    accept_tcp_connection(entry->receive_buffer, idx);
    return idx;
}


 // Initialize TCP Connection table in here
void tcp_init()
{
    initlock(&tcp_table_lock, "TCP Table lock");
    // For now just copied from udp_init to test
    uint8 dest_address[IP_ADDR_SIZE] = {10,0,2,3};
    uint8 data[] = {1,2,3,4,5,1,2,3,4,5,1,2,3,4,5};
    send_tcp_packet(dest_address, 52525, 1255, data, sizeof(data));
}

int tcp_unbind(uint8 connection_handle) {
    if (TCP_CONNECTION_TABLE_SIZE <= connection_handle) {
        return -1;
    }

    // Let's be nice and tell our partner that we're not
    // going to listen to them anymore.
    send_tcp_packet_wait_for_ack(connection_handle, NULL, 0, TCP_FLAGS_FIN);

    acquire(&tcp_table_lock);
    tcp_connection_table[connection_handle].status = 0;
    release(&tcp_table_lock);
    return 0;
}