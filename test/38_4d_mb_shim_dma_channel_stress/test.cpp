// (c) Copyright 2020 Xilinx Inc. All Rights Reserved.

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <xaiengine.h>

#include "air_host.h"
#include "test_library.h"

#include "acdc_queue.h"
#include "hsa_defs.h"

#define SHMEM_BASE 0x020100000000LL

#include "aie_inc.cpp"

#define IMAGE_WIDTH 64
#define IMAGE_HEIGHT 16
#define IMAGE_SIZE  (IMAGE_WIDTH * IMAGE_HEIGHT)

#define TILE_WIDTH 16
#define TILE_HEIGHT 8
#define TILE_SIZE  (TILE_WIDTH * TILE_HEIGHT)

#define NUM_3D (IMAGE_WIDTH / TILE_WIDTH)
#define NUM_4D (IMAGE_HEIGHT / TILE_HEIGHT)

int
main(int argc, char *argv[])
{
  uint64_t col = 7;
  uint64_t row = 0;

  aie_libxaie_ctx_t *xaie = mlir_aie_init_libxaie();
  mlir_aie_init_device(xaie);

  mlir_aie_configure_cores(xaie);
  mlir_aie_configure_switchboxes(xaie);
  mlir_aie_initialize_locks(xaie);
  mlir_aie_configure_dmas(xaie);
  mlir_aie_start_cores(xaie);

  uint32_t *bram_ptr;

  // We're going to stamp over the memories
  for (int i=0; i<2*TILE_SIZE; i++) {
    mlir_aie_write_buffer_buf72_0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_buf72_1(xaie, i, 0xfeedface);
    mlir_aie_write_buffer_buf74_0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_buf74_1(xaie, i, 0xfeedface);
    mlir_aie_write_buffer_buf82_0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_buf82_1(xaie, i, 0xfeedface);
    mlir_aie_write_buffer_buf84_0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_buf84_1(xaie, i, 0xfeedface);
  }
  // create the queue
  queue_t *q = nullptr;
  auto ret = air_queue_create(MB_QUEUE_SIZE, HSA_QUEUE_TYPE_SINGLE, &q, AIR_VCK190_SHMEM_BASE);
  assert(ret == 0 && "failed to create queue!");

  // Let's make a buffer that we can transfer in the same BRAM, after the queue of HSA packets
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd == -1)
    return -1;

  bram_ptr = (uint32_t *)mmap(NULL, 0x8000, PROT_READ|PROT_WRITE, MAP_SHARED, fd,  AIR_VCK190_SHMEM_BASE+0x4000);
  
  for (int i=0;i<IMAGE_SIZE;i++) {
    bram_ptr[i] = i;
    bram_ptr[i+IMAGE_SIZE]   = 0xba110001;
    bram_ptr[i+2*IMAGE_SIZE] = 0xba110002;
    bram_ptr[i+3*IMAGE_SIZE] = 0xba110003;
    bram_ptr[i+4*IMAGE_SIZE] = 0xba110004;
  }

  uint64_t wr_idx = queue_add_write_index(q, 1);
  uint64_t packet_id = wr_idx % q->size;
  dispatch_packet_t *herd_pkt = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_herd_init(herd_pkt, 0, col, 2, row, 5);
  air_queue_dispatch_and_wait(q, wr_idx, herd_pkt);

  //
  // 8 Shim DMAs used in this test. MB controller drives them all simultaneously.
  // NOTE: MB memcpy staging/progress slots are directly mapped, i.e. odd and
  //       even Shim DMA groups one slot per channel for 8 simultaneous xfrs.  
  // 

  // Core 7,2
  // Start by sending the packet to read from the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_c = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_c, 0, 3, 0, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000+(IMAGE_SIZE*sizeof(float)), TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 
 
  // Core 7,4
  // Start by sending the packet to read from the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_d = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_d, 0, 19, 0, 1, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000+(2*IMAGE_SIZE*sizeof(float)), TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 

  // Core 8,2
  // Start by sending the packet to read from the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_g = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_g, 0, 26, 0, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000+(3*IMAGE_SIZE*sizeof(float)), TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 

  // Core 8,4
  // Start by sending the packet to read from the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_h = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_h, 0, 18, 0, 1, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000+(4*IMAGE_SIZE*sizeof(float)), TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 

  // Core 7,2
  // Send the packet to write the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_a = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_a, 0, 7, 1, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000, TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 

  // Core 7,4
  // Send the packet to write the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_b = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_b, 0, 11, 1, 1, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000, TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float)); 

  // Core 8,2
  // Send the packet to write the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_e = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_e, 0, 6, 1, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000, TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float));
  
  // Core 8,4
  // Send the packet to write the tiles
  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_f = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_f, 0, 10, 1, 1, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000, TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), NUM_4D, IMAGE_WIDTH*TILE_HEIGHT*sizeof(float));

  air_queue_dispatch(q, wr_idx, pkt_f);
  air_queue_dispatch_and_wait(q, wr_idx, pkt_e);
  air_queue_wait(q, pkt_f);

  uint32_t errs = 0;
  // Check the BRAM we updated
  for (int i=0; i<IMAGE_SIZE; i++) {
    uint32_t d = bram_ptr[IMAGE_SIZE+i];;
    u32 r = i / IMAGE_WIDTH;
    u32 c = i % IMAGE_WIDTH;
      if (d != i) {
        printf("ERROR: buf72_0 copy idx %d Expected %08X, got %08X\n", i, i, d);
        errs++;
      }
  }
  for (int i=0; i<IMAGE_SIZE; i++) {
    uint32_t d = bram_ptr[2*IMAGE_SIZE+i];;
    u32 r = i / IMAGE_WIDTH;
    u32 c = i % IMAGE_WIDTH;
      if (d != i) {
        printf("ERROR: buf74_0 copy idx %d Expected %08X, got %08X\n", i, i, d);
        errs++;
      }
  }
  for (int i=0; i<IMAGE_SIZE; i++) {
    uint32_t d = bram_ptr[3*IMAGE_SIZE+i];;
    u32 r = i / IMAGE_WIDTH;
    u32 c = i % IMAGE_WIDTH;
      if (d != i) {
        printf("ERROR: buf82_0 copy idx %d Expected %08X, got %08X\n", i, i, d);
        errs++;
      }
  }
  for (int i=0; i<IMAGE_SIZE; i++) {
    uint32_t d = bram_ptr[4*IMAGE_SIZE+i];;
    u32 r = i / IMAGE_WIDTH;
    u32 c = i % IMAGE_WIDTH;
      if (d != i) {
        printf("ERROR: buf84_0 copy idx %d Expected %08X, got %08X\n", i, i, d);
        errs++;
      }
  }

  if (errs == 0) {
    printf("PASS!\n");
    return 0;
  }
  else {
    printf("fail.\n");
    return -1;
  }

}
