/*! \file virtio.h
 * \brief virtio control registers
 */

#ifndef INCLUDED_kernel_virtio_h
#define INCLUDED_kernel_virtio_h

#ifdef __cplusplus
extern "C" {
#endif

#include "kernel/types.h"
#include "kernel/net/net.h"

#define VIRTIO_NET_USER_MODE

//
// virtio device definitions.
// for both the mmio interface, and virtio descriptors.
// only tested with qemu.
//
// the virtio spec:
// https://docs.oasis-open.org/virtio/virtio/v1.1/virtio-v1.1.pdf
//

// virtio mmio control registers, mapped starting at 0x10001000.
// from qemu virtio_mmio.h
#define VIRTIO_MMIO_MAGIC_VALUE		0x000 // 0x74726976
#define VIRTIO_MMIO_VERSION		0x004 // version; should be 2
#define VIRTIO_MMIO_DEVICE_ID		0x008 // device type; 1 is net, 2 is disk
#define VIRTIO_MMIO_VENDOR_ID		0x00c // 0x554d4551
#define VIRTIO_MMIO_DEVICE_FEATURES	0x010
#define VIRTIO_MMIO_DRIVER_FEATURES	0x020
#define VIRTIO_MMIO_QUEUE_SEL		0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_READY		0x044 // ready bit
#define VIRTIO_MMIO_QUEUE_NOTIFY	0x050 // write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS	0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	0x064 // write-only
#define VIRTIO_MMIO_STATUS		0x070 // read/write
#define VIRTIO_MMIO_QUEUE_DESC_LOW	0x080 // physical address for descriptor table, write-only
#define VIRTIO_MMIO_QUEUE_DESC_HIGH	0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW	0x090 // physical address for available ring, write-only
#define VIRTIO_MMIO_DRIVER_DESC_HIGH	0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW	0x0a0 // physical address for used ring, write-only
#define VIRTIO_MMIO_DEVICE_DESC_HIGH	0x0a4
#define VIRTIO_MMIO_DEVICE_CONFIG_GENERATION 0x0fc // Generate config
#define VIRTIO_MMIO_DEVICE_CONFIG 0x100 // Config memory loc

// status register bits, from qemu virtio_config.h
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8

// device feature bits (disk)
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// device feature bits (net card)

// Device handles packets with partial checksum. 
// This “checksum offload” is a common feature on modern network cards. 
#define VIRTIO_NET_F_CSUM           0
#define VIRTIO_NET_F_GUEST_CSUM     1 /* Driver handles packets with partial checksum.  */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS 2 /* Control channel offloads reconfiguration support.  */

// Device maximum MTU reporting is supported. 
// If offered by the device, device advises driver about the value of its maximum MTU. 
// If negotiated, the driver uses mtu as the maximum MTU value. 
#define VIRTIO_NET_F_MTU 3
#define VIRTIO_NET_F_MAC 5 /* Device has given MAC address.  */
#define VIRTIO_NET_F_GUEST_TSO4 7 /* Driver can receive TSOv4. */
#define VIRTIO_NET_F_GUEST_TSO6 8 /* Driver can receive TSOv6. */
#define VIRTIO_NET_F_GUEST_ECN 9 /* Driver can receive TSO with ECN. */
#define VIRTIO_NET_F_GUEST_UFO 10 /* Driver can receive UFO. */
#define VIRTIO_NET_F_HOST_TSO4 11 /* Device can receive TSOv4. */
#define VIRTIO_NET_F_HOST_TSO6 12 /* Device can receive TSOv6. */
#define VIRTIO_NET_F_HOST_ECN 13 /* Device can receive TSO with ECN. */
#define VIRTIO_NET_F_HOST_UFO 14 /* Device can receive UFO. */
#define VIRTIO_NET_F_MRG_RXBUF 15 /* Driver can merge receive buffers.  */
#define VIRTIO_NET_F_STATUS 16 /* Configuration status field is available.  */
#define VIRTIO_NET_F_CTRL_VQ 17 /* Control channel is available.  */
#define VIRTIO_NET_F_CTRL_RX 18 /* Control channel RX mode support. */
#define VIRTIO_NET_F_CTRL_VLAN 19 /* Control channel VLAN filtering. */
#define VIRTIO_NET_F_GUEST_ANNOUNCE 21 /* Driver can send gratuitous packets. */
#define VIRTIO_NET_F_MQ 22 /* Device supports multiqueue with automatic receive steering. */
#define VIRTIO_NET_F_CTRL_MAC_ADDR 23 /* Set MAC address through control channel. */
#define VIRTIO_NET_F_RSC_EXT 61 /* Device can process duplicated ACKs and report number of coalesced segments and duplicated ACKs */
#define VIRTIO_NET_F_STANDBY 62 /* Device may act as a standby for a primary device with the same MAC address. */

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

// a single descriptor, from the spec.
struct virtq_desc {
  uint64 addr;
  uint32 len;
  uint16 flags;
  uint16 next;
};
#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)


#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

// the (entire) avail ring, from the spec.
struct virtq_avail {
  // Usually zero, set to VIRTQ_AVAIL_F_NO_INTERRUPT to suppress interrupt on buffer consumption
  uint16 flags; 
  uint16 idx;   // driver will write ring[idx] next
  uint16 ring[NUM]; // descriptor numbers of chain heads
  uint16 unused;
};

// one entry in the "used" ring, with which the
// device tells the driver about completed requests.
struct virtq_used_elem {
  uint32 id;   // index of start of completed descriptor chain
  uint32 len;
};

#define VIRTQ_USED_F_NO_NOTIFY  1 

struct virtq_used {
  // Usually zero, set to VIRTQ_USED_F_NO_NOTIFY to supress notification on buffer return
  uint16 flags;
  uint16 idx;   // device increments when it adds a ring[] entry
  struct virtq_used_elem ring[NUM];
};

/*
  Redefined to new names from old version, only name has changed
*/
typedef struct virtq_desc virtq_desc;
typedef struct virtq_avail virtq_driver;
typedef struct virtq_used virtq_device;


/**
 * Most likely using split virtqueues
 * These have descriptor, available ring (in driver), used ring (in device) areas
*/
typedef struct __virt_queue {
  virtq_desc *desc;
  virtq_driver *driver;
  virtq_device *device;
} virt_queue;

// these are specific to virtio block devices, e.g. disks,
// described in Section 5.2 of the spec.

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
  uint32 type; // VIRTIO_BLK_T_IN or ..._OUT
  uint32 reserved;
  uint64 sector;
};


// net config
struct virtio_net_config { 
  uint8 mac[MAC_ADDR_SIZE]; 
  uint16 status; 
  uint16 max_virtqueue_pairs; 
  uint16 mtu;   // unused
}; 

// net request header

// Flag defines
#define VIRTIO_NET_HDR_F_NEEDS_CSUM    1 
#define VIRTIO_NET_HDR_F_DATA_VALID    2 
#define VIRTIO_NET_HDR_F_RSC_INFO      4 

// GSO type defines

#define VIRTIO_NET_HDR_GSO_NONE        0 
#define VIRTIO_NET_HDR_GSO_TCPV4       1 
#define VIRTIO_NET_HDR_GSO_UDP         3 
#define VIRTIO_NET_HDR_GSO_TCPV6       4 
#define VIRTIO_NET_HDR_GSO_ECN      0x80 

struct virtio_net_hdr { 
  uint8 flags;
  uint8 gso_type; 
  uint16 hdr_len; 
  uint16 gso_size; 
  uint16 csum_start; 
  uint16 csum_offset; 
  uint16 num_buffers; 
}; 


#ifdef __cplusplus
}
#endif

#endif
