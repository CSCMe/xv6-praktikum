#include "kernel/net/dhcp.h"

void dhcp_get_ip_address()
{
    void* buf = kalloc_zero();
    void* response_buf = kalloc();
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

    connection_identifier id = {.protocol=CON_DHCP, .identification={.dhcp={.transaction_id=packet->transaction_id}}};

    struct spinlock dhcp_lock = {0};
    initlock(&dhcp_lock, "DHCP Lock");
    acquire(&dhcp_lock);

    send_udp_packet(dest_ip, DHCP_PORT_CLIENT, DHCP_PORT_SERVER, buf, sizeof(struct dhcp_packet) + options_len);
    uint8 accept_response = 1;
    struct dhcp_packet *response_packet;

receive_dhcp_offer:
  wait_for_response(id, response_buf, &dhcp_lock);

  // If everything went as expected, the next packet should be a DHCPOFFER packet.
  // We might receive more than one offer but since we don't care about specifics, we just choose
  // the first one and ignore the rest.
  response_packet = (struct dhcp_packet *)(response_buf);

  // FIXME: we should maybe make the opcode part of the identifer that we are sleeping on
  if (response_packet->opcode != DHCP_OPCODE_REPLY) {
    pr_notice("Received something other than a DHCP reply\n");
    accept_response = 0;
  }
  if (response_packet->transaction_id != packet->transaction_id) {
    pr_notice("Mismatched DHCP transaction id\n");
    accept_response = 0;
  }

  if (!accept_response) {
    // FIXME: We might have gotten a new response while we were verifying the old one
    acquire(&dhcp_lock);
    goto receive_dhcp_offer;
  }

  pr_debug("We were offered ");
  print_ip(response_packet->your_ip_addr);
  pr_debug(" - Accepting\n");

  // Send the DHCPREQUEST back to indicate that we choose the offered IP.
  // All the other DHCP servers that might have sent us their offers see this as a rejection.
  packet->transaction_id = r_time();
  memreverse(&packet->transaction_id, sizeof(packet->transaction_id));

  // Set DHCP options
  options_len = 0;
  memmove(packet->options + options_len, &dhcp_magic, DHCP_OPTIONS_MAGIC_COOKIE_LEN);
  options_len += DHCP_OPTIONS_MAGIC_COOKIE_LEN;
  packet->options[options_len]     = DHCP_OPTIONS_MESSAGE_TYPE_NUM;
  packet->options[options_len + 1] = DHCP_OPTIONS_MESSAGE_TYPE_LEN;
  packet->options[options_len + 2] = DHCP_OPTIONS_MESSAGE_TYPE_DHCPREQUEST;
  options_len += 3;
  packet->options[options_len] = DHCP_OPTIONS_SERVER_NAME_NUM;
  packet->options[options_len] = DHCP_OPTIONS_SERVER_NAME_LEN;
  memmove(packet->options + options_len + 2, response_packet->server_name, DHCP_SERVER_NAME_SIZE);
  packet->options[options_len] = DHCP_OPTIONS_END;
  options_len++;

  id.protocol                           = CON_DHCP;
  id.identification.dhcp.transaction_id = packet->transaction_id;

  acquire(&dhcp_lock);
  send_udp_packet(dest_ip, DHCP_PORT_CLIENT, DHCP_PORT_SERVER, buf, sizeof(struct dhcp_packet) + options_len);
  wait_for_response(id, response_buf, &dhcp_lock);
}