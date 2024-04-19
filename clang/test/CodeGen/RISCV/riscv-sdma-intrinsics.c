// RUN: %clang_cc1 -triple riscv32 -target-feature +xdma -emit-llvm %s -o - \
// RUN:     | FileCheck %s

#include <stdint.h>

// CHECK-LABEL: test_sdma_start_oned
uint32_t test_sdma_start_oned(uint64_t src, uint64_t dst, uint32_t size, uint32_t cfg) {
  // CHECK: call i32 @llvm.riscv.sdma.start.oned
  return __builtin_sdma_start_oned(src, dst, size, cfg);
}
// CHECK-LABEL: test_sdma_start_twod
uint32_t test_sdma_start_twod(uint64_t src, uint64_t dst, uint32_t size, 
    uint32_t sstrd, uint32_t dstrd, uint32_t nreps, uint32_t cfg) {
  // CHECK: call i32 @llvm.riscv.sdma.start.twod
  return __builtin_sdma_start_twod(src, dst, size, sstrd, dstrd, nreps, cfg);
}
// CHECK-LABEL: test_sdma_stat
uint32_t test_sdma_stat(uint32_t tid) {
  // CHECK: call i32 @llvm.riscv.sdma.stat
  return __builtin_sdma_stat(tid);
}
// CHECK-LABEL: test_sdma_wait_for_idle
void test_sdma_wait_for_idle(void) {
  // CHECK: call void @llvm.riscv.sdma.wait.for.idle()
  __builtin_sdma_wait_for_idle();
}
