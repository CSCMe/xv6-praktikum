//
// driver for qemu's virtio network card device.
// uses qemu's mmio interface to virtio.
//
// qemu ... -netdev user,id=net0 -device virtio-net-device,netdev=net0,bus=virtio-mmio-bus.1
//

#include "defs.h"
#include "memlayout.h"
#include "buf.h"
#include "virtio.h"
#include "kernel/net/net.h"

// the address of virtio mmio register r.
#define R(r) ((volatile uint32 *)(VIRTIO1 + (r)))

static struct net_card {
    // Control, receive, transmit virtqueues
    virt_queue control, receive, transmit;
    // MAC address of card
    uint8 mac_addr[MAC_ADDR_SIZE];

    struct spinlock net_lock;
} net_card;

void copy_card_mac(uint8 copy_to[MAC_ADDR_SIZE]) {
  memmove(copy_to, net_card.mac_addr, MAC_ADDR_SIZE);
}

void debug_available_features(uint64 features) {
  pr_debug("RAW features:%p\n", features);
  pr_debug("virtio-net available features:\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CSUM)) pr_debug("VIRTIO_NET_F_CSUM\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_CSUM)) pr_debug("VIRTIO_NET_F_GUEST_CSUM\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CTRL_GUEST_OFFLOADS)) pr_debug("VIRTIO_NET_F_CTRL_GUEST_OFFLOADS\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_MTU)) pr_debug("VIRTIO_NET_F_MTU\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_MAC)) pr_debug("VIRTIO_NET_F_MAC\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_TSO4)) pr_debug("VIRTIO_NET_F_GUEST_TSO4\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_TSO6)) pr_debug("VIRTIO_NET_F_GUEST_TSO6\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_ECN)) pr_debug("VIRTIO_NET_F_GUEST_ECN\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_UFO)) pr_debug("VIRTIO_NET_F_GUEST_UFO\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_HOST_TSO4)) pr_debug("VIRTIO_NET_F_HOST_TSO4\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_HOST_TSO6)) pr_debug("VIRTIO_NET_F_HOST_TSO6\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_HOST_ECN)) pr_debug("VIRTIO_NET_F_HOST_ECN\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_HOST_UFO)) pr_debug("VIRTIO_NET_F_HOST_UFO\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_MRG_RXBUF)) pr_debug("VIRTIO_NET_F_MRG_RXBUF\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_STATUS)) pr_debug("VIRTIO_NET_F_STATUS\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_HOST_UFO)) pr_debug("VIRTIO_NET_F_HOST_UFO\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CTRL_VQ)) pr_debug("VIRTIO_NET_F_CTRL_VQ\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CTRL_RX)) pr_debug("VIRTIO_NET_F_CTRL_RX\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CTRL_VLAN)) pr_debug("VIRTIO_NET_F_CTRL_VLAN\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_GUEST_ANNOUNCE)) pr_debug("VIRTIO_NET_F_GUEST_ANNOUNCE\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_MQ)) pr_debug("VIRTIO_NET_F_MQ\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_CTRL_MAC_ADDR)) pr_debug("VIRTIO_NET_F_CTRL_MAC_ADDR\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_RSC_EXT)) pr_debug("VIRTIO_NET_F_RSC_EXT\n");
  if (features & ((uint64)1 << VIRTIO_NET_F_STANDBY)) pr_debug("VIRTIO_NET_F_STANDBY\n");
  pr_debug("\n");
}

void
virtio_queue_increment(virt_queue* queue, uint32* var)
{
  *var = (*var + 1) % queue->size;
}

void
send_ethernet_packet(uint8 dest_mac[MAC_ADDR_SIZE], enum EtherType type, void* data, uint16 data_length)
{

  if (data_length > PGSIZE - sizeof(struct virtio_net_hdr) - sizeof(struct ethernet_header)) {
    pr_info("Can't send packet in one go. Abort");
    return;
  }

  // Now interact with card
  acquire(&net_card.net_lock);

  // Get Send Buffer and increment first_free_transmit
  uint8 current_ringbuffer_pos = net_card.transmit.next_used_idx;
  virtio_queue_increment(&net_card.transmit, &net_card.transmit.next_used_idx);
  void* buf = (void*) net_card.transmit.desc[current_ringbuffer_pos].addr;

  struct ethernet_header* eth_header;
  struct ethernet_tailer* eth_tailer;

  // Fun with driver weirdness
  #ifdef VIRTIO_NET_USER_MODE
  // Only required if not tap (for some reason)
  struct virtio_net_hdr* virtio_header = (struct virtio_net_hdr*) buf;
  virtio_header->flags = VIRTIO_NET_HDR_GSO_NONE;
  virtio_header->num_buffers = 0;
  virtio_header->hdr_len = sizeof(struct virtio_net_hdr);
  eth_header = (struct ethernet_header*) ((uint8*) virtio_header + sizeof(struct virtio_net_hdr));
  #endif

  #ifndef VIRTIO_NET_USER_MODE
  eth_header = (struct ethernet_header*) buf;
  #endif

  // Set src address
  memmove((void*) eth_header->src, (void*) net_card.mac_addr, MAC_ADDR_SIZE);
  // Set dest address
  memmove((void*) eth_header->dest, (void*) dest_mac, MAC_ADDR_SIZE);
  // Copy data
  memmove((void*) eth_header->data, data, data_length);

  // Pad data if smaller than minimum data size
  for (; data_length < ETHERNET_MIN_DATA_LEN; data_length++) {
    eth_header->data[data_length] = 0;
  }


  if (type == ETHERNET_TYPE_DATA) {
    eth_header->len  = data_length;
  } else {
    eth_header->type = type;
  }

  // Convert type/length to little endian
  memreverse(&eth_header->type, sizeof(eth_header->type));

  // Checksum operations
  // Get location of tailer
  eth_tailer = (struct ethernet_tailer*) (((uint8*) eth_header) + sizeof(struct ethernet_header) + data_length);
  // Test: Write 5 in byte 0 of checksum. TODO: Add actual checksum calc
  eth_tailer->crc[0] = 5;

  // Calculate size of buffer
  net_card.transmit.desc[current_ringbuffer_pos].len = ((uint64)eth_tailer + sizeof(struct ethernet_tailer) - (uint64)buf);

  __sync_synchronize();
  net_card.transmit.driver->idx++;
  __sync_synchronize();
  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 1; // value is queue number

  release(&net_card.net_lock);
}

void
virtio_net_init(void)
{
    uint32 status = 0;

    initlock(&net_card.net_lock, "virtio_net");

    if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
        *R(VIRTIO_MMIO_VERSION) != 2 ||
        *R(VIRTIO_MMIO_DEVICE_ID) != 1 || // Net device has ID 1
        *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
        panic("could not find virtio net");
    }
    
    // reset device
    *R(VIRTIO_MMIO_STATUS) = status;

    // set ACKNOWLEDGE status bit
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    // set DRIVER status bit
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // negotiate features (only automatic mac address)
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    debug_available_features(features);

    // ignore features we have, set our own
    // VIRTIO_NET_F_MAC: We have a mac address
    // VIRTIO_NET_F_STATUS: We are allowed to use the status register
    features &= (1 << VIRTIO_NET_F_MAC) | (1 << VIRTIO_NET_F_STATUS) | (1 << VIRTIO_NET_F_MRG_RXBUF);

    // We require these features to function, therefore we should
    // assert that they are actually supported
    if (!(features & (1 << VIRTIO_NET_F_MAC))) 
      panic("virtio-net device does not supply a MAC address");
    if (!(features & (1 << VIRTIO_NET_F_STATUS)))
      panic("virtio-net device does not support configuration via the status register");
    // Complete feature negotiation
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
    status |= VIRTIO_CONFIG_S_FEATURES_OK;

    *R(VIRTIO_MMIO_STATUS) = status;
    status = *R(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
        panic("virtio net feature negotiation failed");
    }


    // Init virt queues
    #define NUM_QUEUES 3
    virt_queue* queues[NUM_QUEUES] = {&net_card.receive, &net_card.transmit, &net_card.control};
    for (int i = 0; i < NUM_QUEUES; i++) {
        queues[i]->desc = kalloc_zero();
        queues[i]->driver = kalloc_zero();
        queues[i]->device = kalloc_zero();
        if(!queues[i]->desc || !queues[i]->driver || !queues[i]->device)
            panic("virtio net kalloc");

        for (int k = 0; k < NUM; k++) {
          // kalloc for buffers
          queues[i]->desc[k].addr = (uint64) kalloc_zero();
          if (!queues[i]->desc[k].addr)
              panic("virtio net buffer kalloc");
          
          queues[i]->desc[k].len = PGSIZE;

          // Set write property for receive queues, exclude control queue
          if (i < (NUM_QUEUES - 1) && (i % 2 == 0)) {
              queues[i]->desc[k].flags = VRING_DESC_F_WRITE;
          } else {
              queues[i]->desc[k].flags = 0;
          }
          // Ring describes descriptor index for this ring index
          queues[i]->driver->ring[k] = (k) % NUM;
        }

        *R(VIRTIO_MMIO_QUEUE_SEL) = i;
        *R(VIRTIO_MMIO_QUEUE_NUM) = NUM; // Stolen from virtio_disk
        queues[i]->size = NUM;

        // write physical addresses.
        *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)queues[i]->desc;
        *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)queues[i]->desc >> 32;
        *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)queues[i]->driver;
        *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)queues[i]->driver >> 32;
        *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)queues[i]->device;
        *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)queues[i]->device >> 32;

        *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;
    }
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    // Queues are ready
    *R(VIRTIO_MMIO_STATUS) = status;

    // Read config
    struct virtio_net_config config = {0};

    // The config generation identifies a specific set of configuration parameters.
    // This protects us from race conditions if someone else modifies the config
    // while we are (not-atomically) reading it.
    uint32 config_gen;
    do {
      config_gen = *R(VIRTIO_MMIO_DEVICE_CONFIG_GENERATION);
      uint32 *config_smol = (uint32 *)&config;
      for (int i = 0; i < sizeof(struct virtio_net_config) / sizeof(uint32); i++) {
          *(config_smol + i) = *(R(VIRTIO_MMIO_DEVICE_CONFIG) + i);
      }
    } while (config_gen != *R(VIRTIO_MMIO_DEVICE_CONFIG_GENERATION));

    memmove((void *)net_card.mac_addr, (void *)config.mac, MAC_ADDR_SIZE);
    // We have a mac address!
    pr_debug("Our ");
    print_mac_addr(net_card.mac_addr);
    // Config reading done
    pr_debug("status:%p\n", config.status);

    // Expose all receive buffers
    for (int i = 0; i < NUM; i++)
      net_card.receive.driver->idx++;
}



void
virtio_net_intr(){
    acquire(&net_card.net_lock);
    int reason = *R(VIRTIO_MMIO_INTERRUPT_STATUS);
    // Used buffer notification
    if (reason & 0x1) {
        *R(VIRTIO_MMIO_INTERRUPT_ACK) = 0x1;
        while (net_card.transmit.device->idx % NUM != net_card.transmit.first_used_idx) {
          // Just throw our transmit bufs back into the ring.
          // No special processing (for now?)
          
          virtio_queue_increment(&net_card.transmit, &net_card.transmit.first_used_idx);
          //pr_info("Recycled transmit\n");
        }
        // Transmit queue return interrupts
        /**
         * TODO
        */

       while (net_card.receive.device->idx % NUM != net_card.receive.first_used_idx) {
        // Receive interrupts
          struct virtio_net_hdr* ptr;

          uint8 desc_index = net_card.receive.device->ring[net_card.receive.first_used_idx].id;
          // Len is also written to descriptor ring but we'll just ignore it
          ptr = (struct virtio_net_hdr*) net_card.receive.desc[desc_index].addr;
          struct ethernet_header* ethernet_header = (struct ethernet_header*)((uint8*) ptr + sizeof(struct virtio_net_hdr));

          // Notify any waiting processes.
          if (notify_of_response(ethernet_header) != 0) {
            // Unexpected packet. Maybe establishing connection?
            if(handle_incoming_connection(ethernet_header) != 0) {
              pr_notice("Dropping unexpected packet.\n");
            }
          }

          // Hand used buffer back to card
          net_card.receive.driver->idx++;
          
          virtio_queue_increment(&net_card.receive, &net_card.receive.first_used_idx);
       }
    } 
    // Both can happen in the same interrupt
    if (reason & 0x2) { // Config change
        pr_warning("Well this shouldn't happen. virtio net interrupt: Device config changed\n");
        *R(VIRTIO_MMIO_INTERRUPT_ACK) = 0x2;
    }


    release(&net_card.net_lock);

}

/*
  // tell device that feature negotiation is complete.
  status |= VIRTIO_CONFIG_S_FEATURES_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // re-read status to ensure FEATURES_OK is set.
  status = *R(VIRTIO_MMIO_STATUS);
  if(!(status & VIRTIO_CONFIG_S_FEATURES_OK))
    panic("virtio disk FEATURES_OK unset");

  // initialize queue 0.
  *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

  // ensure queue 0 is not in use.
  if(*R(VIRTIO_MMIO_QUEUE_READY))
    panic("virtio disk should not be ready");

  // check maximum queue size.
  uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
  if(max == 0)
    panic("virtio disk has no queue 0");
  if(max < NUM)
    panic("virtio disk max queue too short");

  // allocate and zero queue memory.
  disk.desc = kalloc_zero();
  disk.avail = kalloc_zero();
  disk.used = kalloc_zero();
  if(!disk.desc || !disk.avail || !disk.used)
    panic("virtio disk kalloc");

  // set queue size.
  *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

  // write physical addresses.
  *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
  *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
  *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
  *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
  *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
  *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;

  // queue is ready.
  *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

  // all NUM descriptors start out unused.
  for(int i = 0; i < NUM; i++)
    disk.free[i] = 1;

  // tell device we're completely ready.
  status |= VIRTIO_CONFIG_S_DRIVER_OK;
  *R(VIRTIO_MMIO_STATUS) = status;

  // plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ.
}

// find a free descriptor, mark it non-free, return its index.
static int
alloc_desc()
{
  for(int i = 0; i < NUM; i++){
    if(disk.free[i]){
      disk.free[i] = 0;
      return i;
    }
  }
  return -1;
}

// mark a descriptor as free.
static void
free_desc(int i)
{
  if(i >= NUM)
    panic("free_desc 1");
  if(disk.free[i])
    panic("free_desc 2");
  disk.desc[i].addr = 0;
  disk.desc[i].len = 0;
  disk.desc[i].flags = 0;
  disk.desc[i].next = 0;
  disk.free[i] = 1;
  wakeup(&disk.free[0]);
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while(1){
    int flag = disk.desc[i].flags;
    int nxt = disk.desc[i].next;
    free_desc(i);
    if(flag & VRING_DESC_F_NEXT)
      i = nxt;
    else
      break;
  }
}

// allocate three descriptors (they need not be contiguous).
// disk transfers always use three descriptors.
static int
alloc3_desc(int *idx)
{
  for(int i = 0; i < 3; i++){
    idx[i] = alloc_desc();
    if(idx[i] < 0){
      for(int j = 0; j < i; j++)
        free_desc(idx[j]);
      return -1;
    }
  }
  return 0;
}

void
virtio_disk_rw(struct buf *b, int write)
{
  uint64 sector = b->blockno * (BSIZE / 512);

  acquire(&disk.vdisk_lock);

  // the spec's Section 5.2 says that legacy block operations use
  // three descriptors: one for type/reserved/sector, one for the
  // data, one for a 1-byte status result.

  // allocate the three descriptors.
  int idx[3];
  while(1){
    if(alloc3_desc(idx) == 0) {
      break;
    }
    sleep(&disk.free[0], &disk.vdisk_lock);
  }

  // format the three descriptors.
  // qemu's virtio-blk.c reads them.

  struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

  if(write)
    buf0->type = VIRTIO_BLK_T_OUT; // write the disk
  else
    buf0->type = VIRTIO_BLK_T_IN; // read the disk
  buf0->reserved = 0;
  buf0->sector = sector;

  disk.desc[idx[0]].addr = (uint64) buf0;
  disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
  disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
  disk.desc[idx[0]].next = idx[1];

  disk.desc[idx[1]].addr = (uint64) b->data;
  disk.desc[idx[1]].len = BSIZE;
  if(write)
    disk.desc[idx[1]].flags = 0; // device reads b->data
  else
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE; // device writes b->data
  disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
  disk.desc[idx[1]].next = idx[2];

  disk.info[idx[0]].status = 0xff; // device writes 0 on success
  disk.desc[idx[2]].addr = (uint64) &disk.info[idx[0]].status;
  disk.desc[idx[2]].len = 1;
  disk.desc[idx[2]].flags = VRING_DESC_F_WRITE; // device writes the status
  disk.desc[idx[2]].next = 0;

  // record struct buf for virtio_disk_intr().
  disk.info[idx[0]].in_use = 1;
  disk.info[idx[0]].b = b;

  // tell the device the first index in our chain of descriptors.
  disk.avail->ring[disk.avail->idx % NUM] = idx[0];

  __sync_synchronize();

  // tell the device another avail ring entry is available.
  disk.avail->idx += 1; // not % NUM ...

  __sync_synchronize();

  *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0; // value is queue number

  // Wait for virtio_disk_intr() to say request has finished.
  int* in_use = &disk.info[idx[0]].in_use;
  while(*in_use == 1) {
    sleep(in_use, &disk.vdisk_lock);
  }

  disk.info[idx[0]].b = 0;
  free_chain(idx[0]);

  release(&disk.vdisk_lock);
}

void
virtio_disk_intr()
{
  acquire(&disk.vdisk_lock);

  // the device won't raise another interrupt until we tell it
  // we've seen this interrupt, which the following line does.
  // this may race with the device writing new entries to
  // the "used" ring, in which case we may process the new
  // completion entries in this interrupt, and have nothing to do
  // in the next interrupt, which is harmless.
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  __sync_synchronize();

  // the device increments disk.used->idx when it
  // adds an entry to the used ring.

  while(disk.used_idx != disk.used->idx){
    __sync_synchronize();
    int id = disk.used->ring[disk.used_idx % NUM].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");

    int* in_use = &disk.info[id].in_use;
    *in_use = 0;   // disk is done with buf
    wakeup(in_use);

    disk.used_idx += 1;
  }

  release(&disk.vdisk_lock);
}
*/