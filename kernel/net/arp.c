#include "kernel/net/arp.h"


void
arp_init()
{

}

void
test_send_arp()
{
    struct arp_packet actual_arp = {0};
    struct arp_packet* arp = &actual_arp;
    arp->hw_type = ARP_HW_TYPE_ETHERNET;
    arp->prot_type = ARP_PROT_TYPE_IP;
    arp->hlen = ARP_HLEN_48_MAC;
    arp->plen = ARP_PLEN_32_IP;
    arp->opcode = ARP_OPCODE_REQ;

    copy_card_mac(arp->mac_src);
    uint8 ipme[8] = {10, 0, 2, 15};
    memmove((void*) arp->ip_src, ipme, 4);
    uint8 dest[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    memmove((void*)arp->mac_dest, dest, 6);
    uint8 ipdest[8] = {10, 0, 2, 2};
    memmove((void*) arp->ip_dest, ipdest, 4);

    
    send_ethernet_packet(dest, ETHERNET_TYPE_ARP, (void*) arp, sizeof(struct arp_packet));
}
