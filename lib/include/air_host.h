#ifndef AIR_HOST_H
#define AIR_HOST_H

#include "acdc_queue.h"
#include "hsa_defs.h"

#include <stdlib.h>

extern "C" {

// queue operations
//

hsa_status_t air_queue_create(uint32_t size, uint32_t type, queue_t **queue, uint64_t paddr);

hsa_status_t air_queue_dispatch_and_wait(queue_t *queue, uint64_t doorbell, dispatch_packet_t *pkt);

// packet utilities
//

// initialize pkt as a herd init packet with given parameters
hsa_status_t air_packet_herd_init(dispatch_packet_t *pkt, uint16_t herd_id, 
                                  uint8_t start_col, uint8_t num_cols,
                                  uint8_t start_row, uint8_t num_rows);

// herd operations
//

struct air_herd_shim_desc_t {
  int64_t *location_data;
  int64_t *channel_data;
};

struct air_herd_desc_t {
  int32_t name_length;
  char *name;
  air_herd_shim_desc_t *shim_desc;
};

typedef size_t air_herd_handle_t;

// return 0 on failure, nonzero otherwise
air_herd_handle_t air_herd_load_from_file(const char* filename);

// return 0 on success, nonzero otherwise
int32_t air_herd_unload(air_herd_handle_t handle);

air_herd_desc_t *air_herd_get_desc(air_herd_handle_t handle);

const char *air_herd_get_name(air_herd_handle_t handle);
int32_t air_herd_get_name_length(air_herd_handle_t handle)

// memory operations
//

void* air_mem_alloc(size_t size);

void* air_mem_get_paddr(void *vaddr);
void* air_mem_get_vaddr(void *paddr);

void air_mem_dealloc(void *vaddr);

}

#endif // AIR_HOST_H