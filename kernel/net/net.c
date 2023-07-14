#include "kernel/net/arp.h"
#include "kernel/net/ip.h"
#include "kernel/net/net.h"

static connection_entry connections[MAX_TRACKED_CONNECTIONS] = {0};

void
net_init()
{
    arp_init();
    ip_init();
}

connection_entry*
get_entry_for_identifier(connection_identifier id)
{
    // Loop over array
    for (int i = 0; i < MAX_TRACKED_CONNECTIONS; i++) {
        connection_identifier entry_id = connections[i].identifier;
        if (entry_id.identification.value == id.identification.value && entry_id.protocol == id.protocol) {
            return &connections[i];
        }
    }

    return NULL;
}

void
copy_data_to_entry(connection_entry* entry, struct ethernet_header* ethernet_header)
{
    if (entry == NULL) {
        return;
    }

    uint64 offset = sizeof(struct ethernet_header);
    uint64 length = 0;
    switch(entry->identifier.protocol) {
        case CON_ARP:
            length = sizeof(struct arp_packet);
            break;
        case CON_DHCP:
        case CON_ICMP:
        case CON_TCP:
        case CON_UDP:
            offset += sizeof(struct ipv4_header);
            struct ipv4_header* ipv4_header = (struct ipv4_header*) (uint8*) ethernet_header + sizeof(ethernet_header);
            length = ipv4_header->total_length - ipv4_header->header_length;
            break;

        default:
            return;
    }
    uint8* data = (uint8*) ethernet_header;

    memmove(entry->buf, data, length);
}

connection_identifier 
compute_identifier(struct ethernet_header* ethernet_header)
{
    connection_identifier id = {0};
    memreverse(&ethernet_header->type, sizeof(ethernet_header->type));
    switch (ethernet_header->type)
    {
        case ETHERNET_TYPE_ARP:
            struct arp_packet* arp_packet = (struct arp_packet*) ( (uint8*) ethernet_header + sizeof(struct ethernet_header));
            memmove(id.identification.arp.ip_addr, arp_packet->ip_src, IP_ADDR_SIZE);
            id.protocol = CON_ARP;
            break;
        case ETHERNET_TYPE_IPv4:
            struct ipv4_header* ipv4_header = (struct ipv4_header*) ( (uint8*) ethernet_header + sizeof(struct ethernet_header));
            switch (ipv4_header->protocol)
            {
                case IP_PROT_ICMP:
                    id.protocol = CON_ICMP;
                    pr_notice("Not supported: Dropping ICMP\n");
                    break;
                case IP_PROT_TCP:
                    id.protocol = CON_TCP;
                    pr_notice("Not supported: Dropping TCP\n");
                    break;
                case IP_PROT_UDP:
                    id.protocol = CON_UDP;
                    pr_notice("Not supported: Dropping UDP\n");
                    break;
                default:
                    id.protocol = CON_UNKNOWN;
                    pr_notice("Unknown IP-Protocol field: Dropping\n");
                    break;
            }
            break;
        // Anything unknown, including regular ethernet
        default: 
            pr_notice("Dropping packet with type: %x\n", ethernet_header->type);
            break;

    }
    return id;
}