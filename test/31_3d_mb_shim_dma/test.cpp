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

#define IMAGE_WIDTH 128
#define IMAGE_HEIGHT 16
#define IMAGE_SIZE  (IMAGE_WIDTH * IMAGE_HEIGHT)

#define TILE_WIDTH 16
#define TILE_HEIGHT 8
#define TILE_SIZE  (TILE_WIDTH * TILE_HEIGHT)

#define NUM_3D (IMAGE_WIDTH / TILE_WIDTH)

int
main(int argc, char *argv[])
{
  uint64_t col = 7;
  uint64_t row = 0;

  aie_libxaie_ctx_t *xaie = mlir_aie_init_libxaie();
  mlir_aie_init_device(xaie);

  mlir_aie_print_dma_status(xaie, 7, 2);

  mlir_aie_configure_cores(xaie);
  mlir_aie_configure_switchboxes(xaie);
  mlir_aie_initialize_locks(xaie);
  mlir_aie_configure_dmas(xaie);
  mlir_aie_start_cores(xaie);

  XAieDma_Shim ShimDmaInst1;
  uint32_t *bram_ptr;

  // We're going to stamp over the memories
  for (int i=0; i<2*TILE_SIZE; i++) {
    mlir_aie_write_buffer_buf72_0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_buf72_1(xaie, i, 0xfeedface);
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
    bram_ptr[i+IMAGE_SIZE] = 0xf001ba11;
  }

  uint64_t wr_idx = queue_add_write_index(q, 1);
  uint64_t packet_id = wr_idx % q->size;
  dispatch_packet_t *herd_pkt = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_herd_init(herd_pkt, 0, col, 1, row, 3);
  air_queue_dispatch_and_wait(q, wr_idx, herd_pkt);

  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *shim_pkt = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_device_init(shim_pkt,XAIE_NUM_COLS);
  air_queue_dispatch_and_wait(q, wr_idx, shim_pkt);

  //printf("This starts the copying to the tiles\n");

  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_c = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_c, 0, col, 0, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000+(IMAGE_SIZE*sizeof(float)), TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), 1, 0);
  //air_queue_dispatch_and_wait(q, wr_idx, pkt_a);

  //printf("This completes the copying to the tiles, let's move the pattern back\n");

  wr_idx = queue_add_write_index(q, 1);
  packet_id = wr_idx % q->size;
  dispatch_packet_t *pkt_a = (dispatch_packet_t*)(q->base_address_vaddr) + packet_id;
  air_packet_nd_memcpy(pkt_a, 0, col, 1, 0, 4, 2, AIR_VCK190_SHMEM_BASE+0x4000, TILE_WIDTH*sizeof(float), TILE_HEIGHT, IMAGE_WIDTH*sizeof(float), NUM_3D, TILE_WIDTH*sizeof(float), 1, 0);
  air_queue_dispatch_and_wait(q, wr_idx-1, pkt_c);

  mlir_aie_print_dma_status(xaie, 7, 2);
  mlir_aie_print_dma_status(xaie, 7, 4);
  uint32_t errs = 0;
  // Can check the tile memory however it will be last two tiles transferred into the ping-pong tiles
  // NOTE this is set up for IMAGE_WIDTH 32
  //for (int j=0; j<2; j++) {
  //  for (int i=0; i<TILE_SIZE; i++) {
  //    uint32_t d = mlir_aie_read_buffer_buf72_0(xaie, i+TILE_SIZE*j);
  //    u32 row = i / TILE_WIDTH;
  //    u32 col = i % TILE_WIDTH;
  //    u32 o_i = row * IMAGE_WIDTH + col + TILE_WIDTH*j; 
  //    if (d != o_i) {`
  //      printf("ERROR: buf72_0 idx %d Expected %08X, got %08X\n", i, o_i, d);
  //      errs++;
  //    } 
  //  }
  //}
 
  // And the BRAM we updated
  for (int i=0; i<IMAGE_SIZE; i++) {
    uint32_t d = bram_ptr[IMAGE_SIZE+i];;
    u32 r = i / IMAGE_WIDTH;
    u32 c = i % IMAGE_WIDTH;
    if ((r < TILE_HEIGHT)) {
      if (d != i) {
        printf("ERROR: copy idx %d Expected %08X, got %08X\n", i, i, d);
        errs++;
      }
    } else {
      if (d != 0xf001ba11) {
        printf("ERROR: copy idx %d Expected %08X, got %08X\n", i, 0xf001ba11, d);
        errs++;
      }
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
