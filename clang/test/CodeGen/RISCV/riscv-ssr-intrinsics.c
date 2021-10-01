// RUN: %clang -march=rv32ifd_xssr -S -emit-llvm -o - %s \
// RUN:  | FileCheck %s --check-prefix=CHECK --check-prefix=CHECK-RISCV

#include <stdint.h>

// CHECK-LABEL: test_ssr_setup_1d
void test_ssr_setup_1d_r(uint32_t rep, uint32_t b, uint32_t s, double* data) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.1d.r
  __builtin_ssr_setup_1d_r(1,rep,b,s,(int*)data);
}
// CHECK-LABEL: test_ssr_setup_1d
void test_ssr_setup_1d_w(uint32_t rep, uint32_t b, uint32_t s, double* data) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.1d.w
  __builtin_ssr_setup_1d_w(1,rep,b,s,(int*)data);
}
// CHECK-LABEL: test_ssr_push
void test_ssr_push(double val) {
// CHECK-RISCV: call void @llvm.riscv.ssr.push
  __builtin_ssr_push(1,val);
}
// CHECK-LABEL: test_ssr_read
void test_ssr_read(double* data) {
// CHECK-RISCV: call void @llvm.riscv.ssr.read
  __builtin_ssr_read(0,1,data);
}
// CHECK-LABEL: test_ssr_write
void test_ssr_write(double* data) {
// CHECK-RISCV: call void @llvm.riscv.ssr.write
  __builtin_ssr_write(1,3,data);
}
// CHECK-LABEL: test_ssr_repetition
void test_ssr_repetition(uint32_t rep) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.repetition
  __builtin_ssr_setup_repetition(1,rep);
}
// CHECK-LABEL: test_ssr_pop
double test_ssr_pop(void) {
// CHECK-RISCV: = call double @llvm.riscv.ssr.pop
  return __builtin_ssr_pop(1);
}
// CHECK-LABEL: test_ssr_enable
void test_ssr_enable(void) {
// CHECK-RISCV: call void @llvm.riscv.ssr.enable()
  __builtin_ssr_enable();
}
// CHECK-LABEL: test_ssr_disable
void test_ssr_disable(void) {
// CHECK-RISCV: call void @llvm.riscv.ssr.disable()
  __builtin_ssr_disable();
}
// CHECK-LABEL: test_ssr_setup_bound_stride_1d
void test_ssr_setup_bound_stride_1d(uint32_t b, uint32_t s) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.bound.stride.1d
  __builtin_ssr_setup_bound_stride_1d(1,b,s);
}
// CHECK-LABEL: test_ssr_setup_bound_stride_2d
void test_ssr_setup_bound_stride_2d(uint32_t b, uint32_t s) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.bound.stride.2d
  __builtin_ssr_setup_bound_stride_2d(1,b,s);
}
// CHECK-LABEL: test_ssr_setup_bound_stride_3d
void test_ssr_setup_bound_stride_3d(uint32_t b, uint32_t s) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.bound.stride.3d
  __builtin_ssr_setup_bound_stride_3d(1,b,s);
}
// CHECK-LABEL: test_ssr_setup_bound_stride_4d
void test_ssr_setup_bound_stride_4d(uint32_t b, uint32_t s) {
// CHECK-RISCV: call void @llvm.riscv.ssr.setup.bound.stride.4d
  __builtin_ssr_setup_bound_stride_4d(1,b,s);
}
