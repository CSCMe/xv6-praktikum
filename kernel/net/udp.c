#include "kernel/net/transport.h"
#include "kernel/net/ip.h"


void udp_init()
{
    uint8 dest_address[IP_ADDR_SIZE] = {10,0,2,3};
    uint8 data[] = {1,2,3,4,5,1,2,3,4,5,1,2,3,4,5};
    send_udp_packet(dest_address, 52525, 1255, data, sizeof(data));
}

/**
 * Calculates a UDP Checksum
*/
uint16 calculate_udp_checksum(uint8 src_ip[IP_ADDR_SIZE], uint8 dst_ip[IP_ADDR_SIZE], uint16 len, uint8* data)
{
    uint32 csum = calculate_pseudo_header_checksum(src_ip, dst_ip, UDP_PROTOCOL_ID, len) + calculate_internet_checksum(len, data);

    // Convert to 1s complement sum
    while (csum >= (1 << 16)) {
        csum = (csum >> 16) + ((csum << 16) >> 16);
    }

    // Return 1s complement
    return ~csum ;
}

void send_udp_packet(uint8 dest_address[IP_ADDR_SIZE], uint16 source_port, uint16 dest_port, void* data, uint16 data_length) {
    if (data_length > PGSIZE - sizeof(struct udp_header)) {
        pr_info("Send UDP: Can't fit data into page. Aborting\n");
        return;
    }

    void* buf = kalloc_zero();
    if (!buf)
        panic("send udp kalloc");

    struct udp_header* header = (struct udp_header*) buf;

    // Set and convert endian of source port
    header->src = source_port;
    memreverse(&header->src, sizeof(header->src));
    // Set and convert endian of destination port
    header->dst = dest_port;
    memreverse(&header->dst, sizeof(header->dst));
    // Set and convert endian of length
    header->len = data_length + sizeof(struct udp_header);
    memreverse(&header->len, sizeof(header->len));

    memmove(header->data, data, data_length);

    // Checksum calculation
    uint8 my_ip[IP_ADDR_SIZE] = {0};
    copy_ip_addr(my_ip);
    header->checksum = calculate_udp_checksum(my_ip, dest_address, data_length + sizeof(struct udp_header), (uint8*) buf);

    send_ipv4_packet(dest_address, IP_PROT_UDP, header, data_length + sizeof(struct udp_header));
    kfree(buf);
}