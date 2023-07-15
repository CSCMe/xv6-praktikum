/*! \file netdefs.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_netdefs_h
#define INCLUDED_kernel_net_netdefs_h

#ifdef __cplusplus
extern "C" {
#endif


#define MAC_ADDR_SIZE 6
#define IP_ADDR_SIZE 4

typedef union __ip_address {
  uint8 octets[IP_ADDR_SIZE];
  uint32 value;
} ip_address;

_Static_assert(sizeof(ip_address) == 4, "IP address is not 4 bytes");

#ifdef __cplusplus
}
#endif

#endif
