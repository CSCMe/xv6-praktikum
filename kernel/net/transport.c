#include "kernel/net/transport.h"

uint32 calculate_pseudo_header_checksum(uint8 src_ip[IP_ADDR_SIZE], uint8 dst_ip[IP_ADDR_SIZE], uint8 prot_id, uint16 len)
{
    uint16 space[sizeof(struct udp_pseudo_header) / 2] = {0};
    struct udp_pseudo_header* pseudo = (struct udp_pseudo_header*) space;

    memmove(pseudo->dst_ip, dst_ip, sizeof(pseudo->dst_ip));
    memmove(pseudo->src_ip, src_ip, sizeof(pseudo->src_ip));
    pseudo->len = len;
    memreverse(&pseudo->len, sizeof(pseudo->len));
    pseudo->zero = 0;
    pseudo->prot_id = prot_id;
    uint32 result = 0;
    for (int i = 0; i < sizeof(struct udp_pseudo_header ) / 2; i++) {
        result += space[i];
    }

    return result;
}

uint16 calculate_internet_checksum(uint16 len, uint8* data)
{
    uint32 csum = 0;

    for (int i = 0; i < len - (len % 2); i+=2) {
        csum += data[i + 1] << 8 | data[i];
    }

    if (len % 2 == 1) {
        csum += data[len - 1];
    }

    // Convert to 1s complement sum
    while (csum >= (1 << 16)) {
        csum = (csum >> 16) + ((csum << 16) >> 16);
    }

    return csum;
}
