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

#define HIGH_ADDR(addr)	((addr & 0xffffffff00000000) >> 32)
#define LOW_ADDR(addr)	(addr & 0x00000000ffffffff)

#include "aie_inc.cpp"

int
main(int argc, char *argv[])
{
  auto col = 7;
  auto row = 2;

  aie_libxaie_ctx_t *xaie = mlir_aie_init_libxaie();
  mlir_aie_init_device(xaie);

  // Run auto generated config functions
  mlir_aie_configure_cores(xaie);
  mlir_aie_configure_switchboxes(xaie);
  mlir_aie_initialize_locks(xaie);
  mlir_aie_configure_dmas(xaie);

  mlir_aie_start_cores(xaie);

  mlir_aie_print_tile_status(xaie, col, row);

  XAieDma_Shim ShimDmaInst1;
  uint32_t *bram_ptr;

  #define BRAM_ADDR AIR_VCK190_SHMEM_BASE+0x4000
  #define BUFFER_SIZE 256
  #define DMA_COUNT (BUFFER_SIZE*2)

  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd != -1) {
    bram_ptr = (uint32_t *)mmap(NULL, 0x8000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, BRAM_ADDR);
    for (int i=0; i<DMA_COUNT; i++) {
      bram_ptr[i] = i+1;
    }
  }

  // Write over the tile memory buffers
  for (int i=0; i<256; i++) {
    mlir_aie_write_buffer_b0(xaie, i, 0xdeadbeef);
    mlir_aie_write_buffer_b1(xaie, i, 0x1234FEDC);
  }

  auto burstlen = 4;
  XAieDma_ShimInitialize(&(xaie->TileInst[col][0]), &ShimDmaInst1);
  XAieDma_ShimBdSetAddr(&ShimDmaInst1, 1, HIGH_ADDR((u64)BRAM_ADDR), LOW_ADDR((u64)BRAM_ADDR), sizeof(u32) * DMA_COUNT);
  XAieDma_ShimBdSetAxi(&ShimDmaInst1, 1 , 0, burstlen, 0, 0, XAIE_ENABLE);
  XAieDma_ShimBdWrite(&ShimDmaInst1, 1);
  XAieDma_ShimSetStartBd((&ShimDmaInst1), XAIEDMA_SHIM_CHNUM_MM2S0, 1);

  auto ret = XAieDma_ShimPendingBdCount(&ShimDmaInst1, XAIEDMA_SHIM_CHNUM_MM2S0);
  if (ret)
    printf("%s %d Warn %d\n", __FUNCTION__, __LINE__, ret);

  XAieDma_ShimChControl((&ShimDmaInst1), XAIEDMA_SHIM_CHNUM_MM2S0, XAIE_DISABLE, XAIE_DISABLE, XAIE_ENABLE);

  auto count = 0;
  while (XAieDma_ShimPendingBdCount(&ShimDmaInst1, XAIEDMA_SHIM_CHNUM_MM2S0)) {
    XAieLib_usleep(1000);
    count++;
    if (!(count % 1000)) {
      printf("%d seconds\n",count/1000);
      if (count == 2000) break;
    }
  }

  mlir_aie_print_tile_status(xaie, col, row);
  
  // Read back the tile memory buffers
  int errors = 0;
  for (int i=0; i<BUFFER_SIZE; i++) {
    uint32_t d = mlir_aie_read_buffer_b0(xaie, i);
     mlir_aie_check("Check Result b0:", d, i+1, errors);
  }
  for (int i=0; i<BUFFER_SIZE; i++) {
    uint32_t d = mlir_aie_read_buffer_b1(xaie, i);
    mlir_aie_check("Check Result b1:", d, i+BUFFER_SIZE+1, errors);
  }

  if (!errors) {
    printf("PASS!\n");
    return 0;
  } else {
    printf("fail %d/%d.\n", (DMA_COUNT-errors), DMA_COUNT);
    return -1;
  }

  mlir_aie_deinit_libxaie(xaie);
}
