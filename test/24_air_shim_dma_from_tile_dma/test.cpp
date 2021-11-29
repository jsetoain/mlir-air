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
#include <dlfcn.h>

#include "air_host.h"
#include "air_tensor.h"
#include "test_library.h"

#define DMA_COUNT 256

#include "aie_inc.cpp"

queue_t *q = nullptr;

int
main(int argc, char *argv[])
{
  uint64_t row = 0;
  uint64_t col = 7;

  aie_libxaie_ctx_t *xaie = mlir_aie_init_libxaie();
  mlir_aie_init_device(xaie);

  mlir_aie_configure_cores(xaie);
  mlir_aie_configure_switchboxes(xaie);
  mlir_aie_initialize_locks(xaie);
  mlir_aie_configure_dmas(xaie);

  for (int i=0; i<DMA_COUNT; i++)
    mlir_aie_write_buffer_buf0(xaie, i, i+0x10);

  uint32_t *bram_ptr = nullptr;

  // use BRAM_ADDR + 0x4000 as the data address
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd != -1) {
    bram_ptr = (uint32_t *)mmap(NULL, 0x8000, PROT_READ|PROT_WRITE, MAP_SHARED, fd, AIR_VCK190_SHMEM_BASE+0x4000);
    for (int i=0; i<DMA_COUNT; i++) {
      bram_ptr[i] = 0xdeadbeef;    }
  }

  // create the queue
  auto ret = air_queue_create(MB_QUEUE_SIZE, HSA_QUEUE_TYPE_SINGLE, &q, AIR_VCK190_SHMEM_BASE);
  assert(ret == 0 && "failed to create queue!");

  printf("loading aie_ctrl.so\n");
  
  auto handle = air_module_load_from_file("./aie_ctrl.so",q);
  assert(handle && "failed to open aie_ctrl.so");

  auto graph_fn = (void (*)(void*))dlsym((void*)handle, "_mlir_ciface_graph");
  assert(graph_fn && "failed to locate _mlir_ciface_graph in aie_ctrl.so");

  tensor_t<uint32_t,1> input;
  input.shape[0] = DMA_COUNT;
  input.d = (uint32_t*)malloc(sizeof(uint32_t)*DMA_COUNT);
  for (int i=0; i<input.shape[0]; i++) {
    input.d[i] = i;
  }

  mlir_aie_start_cores(xaie);

  auto i = &input;
  graph_fn(i);

  mlir_aie_print_dma_status(xaie, 7, 2);
  mlir_aie_print_tile_status(xaie,7,2);

  int errors = 0;
  for (int i=0; i<DMA_COUNT; i++) {
    uint32_t d = bram_ptr[i];
    if (d != (i+0x10)) {
      errors++;
      printf("mismatch %x != 0x10 + %x\n", d, i);
    }
  }

  if (!errors) {
    printf("PASS!\n");
    return 0;
  }
  else {
    printf("fail %d/%d.\n", (DMA_COUNT-errors), DMA_COUNT);
    return -1;
  }

}
