#include "kernel/net/dhcp.h"

void dhcp_get_ip_address()
{
    void* buf = kalloc_zero();
    void* response_buf = kalloc_zero();
    if (!buf || !response_buf)
        panic("DHCP kalloc fail");
    struct dhcp_packet* packet = (struct dhcp_packet*) buf;
    packet->opcode = DHCP_OPCODE_REQUEST;
    packet->htype  = DHCP_HTYPE_ETHERNET;
    packet->hlen   = DHCP_HLEN_ETHERNET;
    packet->hop_count = 0;
    packet->transaction_id = r_time();
    memreverse(&packet->transaction_id, sizeof(packet->transaction_id));
    packet->seconds = 0; // No need to reverse it's 0 anyways
    packet->flags = DHCP_FLAGS_BROADCAST;
    // Leave all IP addresses at 0
    copy_card_mac(packet->client_hardware_addr);
    int options_len = 0;
    // Set magic value
    uint32 dhcp_magic = DHCP_OPTIONS_MAGIC_COOKIE_VALUE;
    memmove(packet->options + options_len, &dhcp_magic, DHCP_OPTIONS_MAGIC_COOKIE_LEN);
    options_len += DHCP_OPTIONS_MAGIC_COOKIE_LEN;
    packet->options[options_len] = DHCP_OPTIONS_MESSAGE_TYPE_NUM;
    packet->options[options_len + 1] = DHCP_OPTIONS_MESSAGE_TYPE_LEN;
    packet->options[options_len + 2] = DHCP_OPTIONS_MESSAGE_TYPE_DHCPDISCOVER;
    options_len += 3;
    packet->options[options_len] = DHCP_OPTIONS_END;
    options_len++;
    uint8 dest_ip[IP_ADDR_SIZE] = IP_ADDR_BROADCAST;

    struct spinlock dhcp_lock = {0};
    initlock(&dhcp_lock, "DHCP Lock");
    acquire(&dhcp_lock);
    send_udp_packet(dest_ip, DHCP_PORT_CLIENT, DHCP_PORT_SERVER, buf, sizeof(struct dhcp_packet) + options_len);
    connection_identifier id = {.protocol=CON_DHCP, .identification={.dhcp={.transaction_id=packet->transaction_id}}};
    wait_for_response(id, response_buf, &dhcp_lock);
    pr_debug("Woah, response!");
}