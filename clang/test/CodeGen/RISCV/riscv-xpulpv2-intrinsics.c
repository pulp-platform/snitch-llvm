// REQUIRES: riscv-registered-target
// Performing the same checks for each possible way to invoke clang
// for PULP extensions:
// 1. cc1 with unversioned extension (+xpulpv instead of +xpulpv2)
// RUN: %clang_cc1 -triple riscv32 -target-feature +xpulpv -emit-llvm %s -o - \
// RUN:     | FileCheck %s
// 2. clang with versioned extension
// RUN: %clang --target=riscv32 -march=rv32imafcxpulpv2 -c -S -emit-llvm %s -o - \
// RUN:     | FileCheck %s
// 3. clang with a platform triple that is supposed to provide PULP extensions
// RUN: %clang --target=riscv32-hero-unknown-elf -c -S -emit-llvm %s -o - \
// RUN:     | FileCheck %s

#include <stdint.h>

// CHECK-LABEL: @test_builtin_pulp_CoreId(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    call i32 @llvm.riscv.pulp.CoreId()
//
int32_t test_builtin_pulp_CoreId(void) {
  return __builtin_pulp_CoreId();
}

// Note: HERO provides the number of cores as the address
// of the '__rt_nb_pe' global symbol, e.g.:
// 8 cores -> __rt_nb_pe placed at address 0x00000008
// No actual intrinsic emitted.
// CHECK-LABEL: @test_builtin_pulp_CoreCount(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    ret i32 ptrtoint (i8* @__rt_nb_pe to i32)
//
int32_t test_builtin_pulp_CoreCount(void) {
  return __builtin_pulp_CoreCount();
}

// CHECK-LABEL: @test_builtin_pulp_ClusterId(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.ClusterId()
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_ClusterId(void) {
  return __builtin_pulp_ClusterId();
}

// CHECK-LABEL: @test_builtin_pulp_IsFc(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.IsFc()
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_IsFc(void) {
  return __builtin_pulp_IsFc();
}

// CHECK-LABEL: @test_builtin_pulp_HasFc(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.HasFc()
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_HasFc(void) {
  return __builtin_pulp_HasFc();
}

// CHECK-LABEL: @test_builtin_pulp_mac(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mac(i32 1, i32 2, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
int32_t test_builtin_pulp_mac(void) {
  return __builtin_pulp_mac(1, 2, 3);
}

// CHECK-LABEL: @test_builtin_pulp_machhs(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhs(i32 -131072, i32 262144, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhs(void) {
  return __builtin_pulp_machhs(-(2u << 16), 4u << 16, 3);
}

// CHECK-LABEL: @test_builtin_pulp_machhu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhu(i32 131072, i32 262144, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhu(void) {
  return __builtin_pulp_machhu(2u << 16, 4u << 16, 3);
}

// CHECK-LABEL: @test_builtin_pulp_macs(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macs(i32 2, i32 -2, i32 -3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macs(void) {
  return __builtin_pulp_macs(2, -2, -3);
}

// CHECK-LABEL: @test_builtin_pulp_macu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macu(i32 2, i32 2, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macu(void) {
  return __builtin_pulp_macu(2, 2, 3);
}

// CHECK-LABEL: @test_builtin_pulp_msu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.msu(i32 1, i32 2, i32 10)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_msu(void) {
  return __builtin_pulp_msu(1, 2, 10);
}

// CHECK-LABEL: @test_builtin_pulp_bset(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bset(i32 0, i32 240)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bset(void) {
  return __builtin_pulp_bset(0x0, ((1u << 4) - 1) << 4);
}

// CHECK-LABEL: @test_builtin_pulp_bset_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bset.r(i32 0, i32 132)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bset_r(void) {
  return __builtin_pulp_bset_r(0x0, 132u);
}

// CHECK-LABEL: @test_builtin_pulp_clb(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.clb(i32 251658240)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_clb(void) {
  return __builtin_pulp_clb(0x0F000000);
}

// CHECK-LABEL: @test_builtin_pulp_cnt(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.cnt(i32 65295)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_cnt(void) {
  return __builtin_pulp_cnt(0xFF0F);
}

// CHECK-LABEL: @test_builtin_pulp_ff1(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.ff1(i32 240)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_ff1(void) {
  return __builtin_pulp_ff1(0xF0);
}

// CHECK-LABEL: @test_builtin_pulp_fl1(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.fl1(i32 31)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_fl1(void) {
  return __builtin_pulp_fl1(31);
}

// CHECK-LABEL: @test_builtin_pulp_parity(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.parity(i32 5)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_parity(void) {
  return __builtin_pulp_parity(5);
}

// CHECK-LABEL: @test_builtin_pulp_rotr(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.rotr(i32 4080, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_rotr(void) {
  return __builtin_pulp_rotr(0xFF0, 1);
}

// CHECK-LABEL: @test_builtin_pulp_abs(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.abs(i32 -2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_abs(void) {
  return __builtin_pulp_abs(-2);
}

// CHECK-LABEL: @test_builtin_pulp_addN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.addN(i32 -10, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_addN(void) {
  return __builtin_pulp_addN(-10, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_addN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.addN.r(i32 -10, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_addN_r(void) {
  return __builtin_pulp_addN_r(-10, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_addRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.addRN(i32 -10, i32 2, i32 2, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_addRN(void) {
  return __builtin_pulp_addRN(-10, 2, 2u, 2u);
}

// CHECK-LABEL: @test_builtin_pulp_addRN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.addRN.r(i32 -10, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_addRN_r(void) {
  return __builtin_pulp_addRN_r(-10, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_adduN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.adduN(i32 11, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_adduN(void) {
  return __builtin_pulp_adduN(11, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_adduN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.adduN.r(i32 11, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_adduN_r(void) {
  return __builtin_pulp_adduN_r(11, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_adduRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.adduRN(i32 11, i32 2, i32 2, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_adduRN(void) {
  return __builtin_pulp_adduRN(11, 2, 2u, 2u);
}

// CHECK-LABEL: @test_builtin_pulp_adduRN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.adduRN.r(i32 11, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_adduRN_r(void) {
  return __builtin_pulp_adduRN_r(11, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_clip(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.clip(i32 -10, i32 -4, i32 15)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_clip(void) {
  return __builtin_pulp_clip(-10, -4, 15);
}

// CHECK-LABEL: @test_builtin_pulp_clip_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.clip.r(i32 -10, i32 4)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_clip_r(void) {
  return __builtin_pulp_clip_r(-10, 4);
}

// CHECK-LABEL: @test_builtin_pulp_clipu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.clipu(i32 20, i32 0, i32 15)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_clipu(void) {
  return __builtin_pulp_clipu(20, 0, 15);
}

// CHECK-LABEL: @test_builtin_pulp_clipu_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.clipu.r(i32 10, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_clipu_r(void) {
  return __builtin_pulp_clipu_r(10, 2);
}

// CHECK-LABEL: @test_builtin_pulp_maxsi(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.maxsi(i32 -1, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_maxsi(void) {
  return __builtin_pulp_maxsi(-1, 2);
}

// CHECK-LABEL: @test_builtin_pulp_machhsN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhsN(i32 -458752, i32 131072, i32 -2, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhsN(void) {
  return __builtin_pulp_machhsN(-(7u << 16), 2u << 16, -2, 2);
}

// CHECK-LABEL: @test_builtin_pulp_machhsRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhsRN(i32 -458752, i32 131072, i32 -2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhsRN(void) {
  return __builtin_pulp_machhsRN(-(7u << 16), 2u << 16, -2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_machhuN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhuN(i32 458752, i32 131072, i32 -2, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhuN(void) {
  return __builtin_pulp_machhuN(7u << 16, 2u << 16, -2, 2);
}

// CHECK-LABEL: @test_builtin_pulp_machhuRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.machhuRN(i32 458752, i32 131072, i32 -2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_machhuRN(void) {
  return __builtin_pulp_machhuRN(7u << 16, 2u << 16, -2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_macsN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macsN(i32 -7, i32 2, i32 -2, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macsN(void) {
  return __builtin_pulp_macsN(-7, 2, -2, 2);
}

// CHECK-LABEL: @test_builtin_pulp_macsRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macsRN(i32 -7, i32 2, i32 -2, i32 1, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macsRN(void) {
  return __builtin_pulp_macsRN(-7, 2, -2, 1, 1);
}

// CHECK-LABEL: @test_builtin_pulp_macuN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macuN(i32 7, i32 2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macuN(void) {
  return __builtin_pulp_macuN(7, 2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_macuRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.macuRN(i32 7, i32 2, i32 2, i32 1, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_macuRN(void) {
  return __builtin_pulp_macuRN(7, 2, 2, 1, 1);
}

// CHECK-LABEL: @test_builtin_pulp_maxusi(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.maxusi(i32 1, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_maxusi(void) {
  return __builtin_pulp_maxusi(1, 3);
}

// CHECK-LABEL: @test_builtin_pulp_minsi(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.minsi(i32 -1, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_minsi(void) {
  return __builtin_pulp_minsi(-1, 2);
}

// CHECK-LABEL: @test_builtin_pulp_minusi(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.minusi(i32 1, i32 3)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_minusi(void) {
  return __builtin_pulp_minusi(1, 3);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhs(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhs(i32 -524288, i32 131072)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhs(void) {
  return __builtin_pulp_mulhhs(-(8u << 16), 2u << 16);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhsN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhsN(i32 -524288, i32 131072, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhsN(void) {
  return __builtin_pulp_mulhhsN(-(8u << 16), 2u << 16, 1);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhsRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhsRN(i32 -524288, i32 131072, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhsRN(void) {
  return __builtin_pulp_mulhhsRN(-(8u << 16), 2u << 16, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhu(i32 524288, i32 131072)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhu(void) {
  return __builtin_pulp_mulhhu(8u << 16, 2u << 16);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhuN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhuN(i32 524288, i32 131072, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhuN(void) {
  return __builtin_pulp_mulhhuN(8u << 16, 2u << 16, 1);
}

// CHECK-LABEL: @test_builtin_pulp_mulhhuRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulhhuRN(i32 524288, i32 131072, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulhhuRN(void) {
  return __builtin_pulp_mulhhuRN(8u << 16, 2u << 16, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_muls(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.muls(i32 -7, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_muls(void) {
  return __builtin_pulp_muls(-7, 2);
}

// CHECK-LABEL: @test_builtin_pulp_mulsN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulsN(i32 -7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulsN(void) {
  return __builtin_pulp_mulsN(-7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_mulsRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulsRN(i32 -7, i32 2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulsRN(void) {
  return __builtin_pulp_mulsRN(-7, 2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_mulu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.mulu(i32 7, i32 2)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_mulu(void) {
  return __builtin_pulp_mulu(7, 2);
}

// CHECK-LABEL: @test_builtin_pulp_muluN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.muluN(i32 7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_muluN(void) {
  return __builtin_pulp_muluN(7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_muluRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.muluRN(i32 7, i32 2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_muluRN(void) {
  return __builtin_pulp_muluRN(7, 2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subN(i32 -7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subN(void) {
  return __builtin_pulp_subN(-7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subN.r(i32 7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subN_r(void) {
  return __builtin_pulp_subN_r(7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subRN(i32 -7, i32 2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subRN(void) {
  return __builtin_pulp_subRN(-7, 2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subRN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subRN.r(i32 -7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subRN_r(void) {
  return __builtin_pulp_subRN_r(-7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subuN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subuN(i32 7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subuN(void) {
  return __builtin_pulp_subuN(7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subuN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subuN.r(i32 7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subuN_r(void) {
  return __builtin_pulp_subuN_r(7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subuRN(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subuRN(i32 7, i32 2, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subuRN(void) {
  return __builtin_pulp_subuRN(7, 2, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_subuRN_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.subuRN.r(i32 7, i32 2, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_subuRN_r(void) {
  return __builtin_pulp_subuRN_r(7, 2, 1);
}

// CHECK-LABEL: @test_builtin_pulp_bclr(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bclr(i32 2147483647, i32 -57345)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bclr(void) {
  return __builtin_pulp_bclr(0x7FFFFFFF, 0xFFFF1FFF);
}

// CHECK-LABEL: @test_builtin_pulp_bclr_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bclr.r(i32 4095, i32 100)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bclr_r(void) {
  return __builtin_pulp_bclr_r(0xFFF, (3u << 5) | 4u);
}

// CHECK-LABEL: @test_builtin_pulp_bextract(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bextract(i32 -65024, i32 8, i32 8)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bextract(void) {
  return __builtin_pulp_bextract(-0x0000FE00, 0x8, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_bextract_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bextract.r(i32 -65024, i32 136)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bextract_r(void) {
  return __builtin_pulp_bextract_r(-0x0000FE00, 0x88);
}

// CHECK-LABEL: @test_builtin_pulp_bextractu(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bextractu(i32 65024, i32 8, i32 8)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bextractu(void) {
  return __builtin_pulp_bextractu(0x0000FE00, 0x8, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_bextractu_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.bextractu.r(i32 65024, i32 136)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_bextractu_r(void) {
  return __builtin_pulp_bextractu_r(0x0000FE00, 0x88);
}

// CHECK-LABEL: @test_builtin_pulp_binsert(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.binsert(i32 0, i32 7, i32 1, i32 3, i32 0)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_binsert(void) {
  return __builtin_pulp_binsert(0x0, 7, 1, 3, 0);
}

// CHECK-LABEL: @test_builtin_pulp_binsert_r(
// CHECK-NEXT:  entry:
// CHECK-NEXT:    [[RES:%.*]] = call i32 @llvm.riscv.pulp.binsert.r(i32 0, i32 7, i32 1)
// CHECK-NEXT:    ret i32 [[RES]]
//
int32_t test_builtin_pulp_binsert_r(void) {
  return __builtin_pulp_binsert_r(0x0, 7, 1);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedRead(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         [[RES:%.*]] = call i32 @llvm.riscv.pulp.OffsetedRead(i32* [[PTR]], i32 4)
//
int32_t test_builtin_pulp_OffsetedRead(int32_t *data) {
  return __builtin_pulp_OffsetedRead(data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedWrite(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call void @llvm.riscv.pulp.OffsetedWrite(i32 1, i32* [[PTR]], i32 4)
//
void test_builtin_pulp_OffsetedWrite(int32_t *data) {
  __builtin_pulp_OffsetedWrite(1, data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedReadHalf(
// CHECK:         [[PTR:%.*]] = load i16*, i16** %data.addr, align 4
// CHECK:         [[RES:%.*]] = call i32 @llvm.riscv.pulp.OffsetedReadHalf(i16* [[PTR]], i32 4)
//
int16_t test_builtin_pulp_OffsetedReadHalf(int16_t *data) {
  return __builtin_pulp_OffsetedReadHalf(data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedWriteHalf(
// CHECK:         [[PTR:%.*]] = load i16*, i16** %data.addr, align 4
// CHECK:         call void @llvm.riscv.pulp.OffsetedWriteHalf(i32 1, i16* [[PTR]], i32 4)
//
void test_builtin_pulp_OffsetedWriteHalf(int16_t *data) {
  __builtin_pulp_OffsetedWriteHalf(1, data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedReadByte(
// CHECK:         [[PTR:%.*]] = load i8*, i8** %data.addr, align 4
// CHECK:         [[RES:%.*]] = call i32 @llvm.riscv.pulp.OffsetedReadByte(i8* [[PTR]], i32 4)
//
char test_builtin_pulp_OffsetedReadByte(char *data) {
  return __builtin_pulp_OffsetedReadByte(data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_OffsetedWriteByte(
// CHECK:         [[PTR:%.*]] = load i8*, i8** %data.addr, align 4
// CHECK:         call void @llvm.riscv.pulp.OffsetedWriteByte(i32 1, i8* [[PTR]], i32 4)
//
void test_builtin_pulp_OffsetedWriteByte(char *data) {
  __builtin_pulp_OffsetedWriteByte(1, data, 4);
}

// CHECK-LABEL: @test_builtin_pulp_read_base_off(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call i32 @llvm.riscv.pulp.read.base.off(i32* [[PTR]], i32 15)
//
int32_t test_builtin_pulp_read_base_off(int32_t* data) {
  return __builtin_pulp_read_base_off(data, 0xF);
}

// CHECK-LABEL: @test_builtin_pulp_write_base_off(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call void @llvm.riscv.pulp.write.base.off(i32 1, i32* [[PTR]], i32 15)
//
void test_builtin_pulp_write_base_off(int32_t* data) {
  __builtin_pulp_write_base_off(0x1, data, 0xF);
}

// CHECK-LABEL: @test_builtin_pulp_read_base_off_v(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call i32 @llvm.riscv.pulp.read.base.off.v(i32* [[PTR]], i32 15)
//
int32_t test_builtin_pulp_read_base_off_v(int32_t* data) {
  return __builtin_pulp_read_base_off_v(data, 0xF);
}

// CHECK-LABEL: @test_builtin_pulp_write_base_off_v(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call void @llvm.riscv.pulp.write.base.off.v(i32 1, i32* [[PTR]], i32 15)
//
void test_builtin_pulp_write_base_off_v(int32_t* data) {
  __builtin_pulp_write_base_off_v(0x1, data, 0xF);
}

// CHECK-LABEL: @test_builtin_pulp_read_then_spr_bit_clr(
// CHECK:         call i32 @llvm.riscv.pulp.read.then.spr.bit.clr(i32 3860, i32 8)
//
int32_t test_builtin_pulp_read_then_spr_bit_clr() {
  return __builtin_pulp_read_then_spr_bit_clr(0xF14, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_read_then_spr_bit_set(
// CHECK:         call i32 @llvm.riscv.pulp.read.then.spr.bit.set(i32 3860, i32 8)
//
int32_t test_builtin_pulp_read_then_spr_bit_set() {
  return __builtin_pulp_read_then_spr_bit_set(0xF14, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_read_then_spr_write(
// CHECK:         call i32 @llvm.riscv.pulp.read.then.spr.write(i32 3860, i32 8)
//
int32_t test_builtin_pulp_read_then_spr_write() {
  return __builtin_pulp_read_then_spr_write(0xF14, 8);
}

// CHECK-LABEL: @test_builtin_pulp_spr_bit_clr(
// CHECK:         call void @llvm.riscv.pulp.spr.bit.clr(i32 3860, i32 8)
//
void test_builtin_pulp_spr_bit_clr() {
  __builtin_pulp_spr_bit_clr(0xF14, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_spr_bit_set(
// CHECK:         call void @llvm.riscv.pulp.spr.bit.set(i32 3860, i32 8)
//
void test_builtin_pulp_spr_bit_set() {
  __builtin_pulp_spr_bit_set(0xF14, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_spr_read(
// CHECK:         call i32 @llvm.riscv.pulp.spr.read(i32 3860)
//
int32_t test_builtin_pulp_spr_read() {
  return __builtin_pulp_spr_read(0xF14);
}

// CHECK-LABEL: @test_builtin_pulp_spr_read_vol(
// CHECK:         call i32 @llvm.riscv.pulp.spr.read.vol(i32 3860)
//
int32_t test_builtin_pulp_spr_read_vol() {
  return __builtin_pulp_spr_read_vol(0xF14);
}

// CHECK-LABEL: @test_builtin_pulp_spr_write(
// CHECK:         call void @llvm.riscv.pulp.spr.write(i32 3860, i32 8)
//
void test_builtin_pulp_spr_write() {
  __builtin_pulp_spr_write(0xF14, 0x8);
}

// CHECK-LABEL: @test_builtin_pulp_event_unit_read(
// CHECK:         [[PTR:%.*]] = load i32*, i32** %data.addr, align 4
// CHECK:         call i32 @llvm.riscv.pulp.event.unit.read(i32* [[PTR]], i32 8)
//
int32_t test_builtin_pulp_event_unit_read(int32_t* data) {
  return __builtin_pulp_event_unit_read(data, 0x8);
}
