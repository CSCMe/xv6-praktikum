/*! \file netdefs.h
 * \brief networking things
 */

#ifndef INCLUDED_kernel_net_netdefs_h
#define INCLUDED_kernel_net_netdefs_h

#ifdef __cplusplus
extern "C" {
#endif


#define MAC_ADDR_SIZE 6
#define MAC_ADDR_BROADCAST {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}
#define IP_ADDR_SIZE 4
#define IP_ADDR_BROADCAST  {255, 255, 255, 255}

#ifdef __cplusplus
}
#endif

#endif
