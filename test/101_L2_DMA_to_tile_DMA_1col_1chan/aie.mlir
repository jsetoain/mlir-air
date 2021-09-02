// (c) Copyright 2020 Xilinx Inc. All Rights Reserved.

module @aie.0  {
  %0 = AIE.tile(7, 1)
  %1 = AIE.tile(7, 0)
  %2 = AIE.tile(0, 0)
  %3 = AIE.tile(7, 4)
  %4 = AIE.lock(%3, 2)
  %5 = AIE.buffer(%3) {sym_name = "buf2"} : memref<16xi32, 2>
  %6 = AIE.lock(%3, 1)
  %7 = AIE.buffer(%3) {sym_name = "buf1"} : memref<16xi32, 2>
  %8 = AIE.lock(%3, 0)
  %9 = AIE.buffer(%3) {sym_name = "buf0"} : memref<4xi32, 2>
  %10 = AIE.mem(%3)  {
    %14 = AIE.dmaStart(S2MM0, ^bb1, ^bb4)
  ^bb1:  // 2 preds: ^bb0, ^bb3
    AIE.useLock(%8, Acquire, 0, 0)
    AIE.dmaBd(<%9 : memref<4xi32, 2>, 0, 1>, 0)
    AIE.useLock(%8, Release, 1, 0)
    br ^bb2
  ^bb2:  // pred: ^bb1
    AIE.useLock(%6, Acquire, 0, 0)
    AIE.dmaBd(<%7 : memref<16xi32, 2>, 0, 16>, 0)
    AIE.useLock(%6, Release, 1, 0)
    br ^bb3
  ^bb3:  // pred: ^bb2
    AIE.useLock(%4, Acquire, 0, 0)
    AIE.dmaBd(<%5 : memref<16xi32, 2>, 0, 16>, 0)
    AIE.useLock(%4, Release, 1, 0)
    br ^bb1
  ^bb4:  // pred: ^bb0
    AIE.end
  }
  %11 = AIE.core(%3)  {
    br ^bb1
  ^bb1:  // pred: ^bb0
    br ^bb2
  ^bb2:  // pred: ^bb1
    AIE.useLock(%8, Acquire, 1, 0)
    AIE.useLock(%6, Acquire, 1, 0)
    AIE.useLock(%4, Acquire, 1, 0)
    AIE.end
  }
  %12 = AIE.switchbox(%1)  {
    AIE.connect<South : 0, North : 0>
  }
  AIE.flow(%0, South : 0, %3, DMA : 0)
  %13 = AIE.shimmux(%1)  {
    AIE.connect<DMA : 0, South : 3>
    AIE.connect<DMA : 1, South : 7>
    AIE.connect<South : 2, DMA : 0>
    AIE.connect<South : 3, DMA : 1>
  }
}
