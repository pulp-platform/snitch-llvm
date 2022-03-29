// RUN: %clang_cc1 -triple riscv32 -target-feature +xpulpv -emit-llvm %s -o - \
// RUN:     | FileCheck %s

#include <stdint.h>

// CHECK-LABEL: test_builtin_pulp_CoreId
int32_t test_builtin_pulp_CoreId(void) {
// CHECK: call i32 @llvm.riscv.pulp.CoreId
  return __builtin_pulp_CoreId();
}

// CHECK-LABEL: test_builtin_pulp_CoreCount
int32_t test_builtin_pulp_CoreCount(void) {
// CHECK: ret i32 ptrtoint (i8* @__rt_nb_pe to i32)
  return __builtin_pulp_CoreCount();
  // Note: right now, this builtin lowers to an external
  // symbol's address as needed by the HERO platform.
}

// CHECK-LABEL: test_builtin_pulp_ClusterId
int32_t test_builtin_pulp_ClusterId(void) {
// CHECK: call i32 @llvm.riscv.pulp.ClusterId
  return __builtin_pulp_ClusterId();
}

// CHECK-LABEL: test_builtin_pulp_IsFc
int32_t test_builtin_pulp_IsFc(void) {
// CHECK: call i32 @llvm.riscv.pulp.IsFc
  return __builtin_pulp_IsFc();
}

// CHECK-LABEL: test_builtin_pulp_HasFc
int32_t test_builtin_pulp_HasFc(void) {
// CHECK: call i32 @llvm.riscv.pulp.HasFc
  return __builtin_pulp_HasFc();
}

// CHECK-LABEL: test_builtin_pulp_mac
int32_t test_builtin_pulp_mac(void) {
// CHECK: call i32 @llvm.riscv.pulp.mac
  return __builtin_pulp_mac(1, 2, 3);
}

// CHECK-LABEL: test_builtin_pulp_machhs
int32_t test_builtin_pulp_machhs(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhs
  return __builtin_pulp_machhs(-(2u << 16), 4u << 16, 3);
}

// CHECK-LABEL: test_builtin_pulp_machhu
int32_t test_builtin_pulp_machhu(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhu
  return __builtin_pulp_machhu(2u << 16, 4u << 16, 3);
}

// CHECK-LABEL: test_builtin_pulp_macs
int32_t test_builtin_pulp_macs(void) {
// CHECK: call i32 @llvm.riscv.pulp.macs
  return __builtin_pulp_macs(2, -2, -3);
}

// CHECK-LABEL: test_builtin_pulp_macu
int32_t test_builtin_pulp_macu(void) {
// CHECK: call i32 @llvm.riscv.pulp.macu
  return __builtin_pulp_macu(2, 2, 3);
}

// CHECK-LABEL: test_builtin_pulp_msu
int32_t test_builtin_pulp_msu(void) {
// CHECK: call i32 @llvm.riscv.pulp.msu
  return __builtin_pulp_msu(1, 2, 10);
}

// CHECK-LABEL: test_builtin_pulp_bset
int32_t test_builtin_pulp_bset(void) {
// CHECK: call i32 @llvm.riscv.pulp.bset
  return __builtin_pulp_bset(0x0, ((1u << 4) - 1) << 4);
}

// CHECK-LABEL: test_builtin_pulp_bset_r
int32_t test_builtin_pulp_bset_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.bset.r
  return __builtin_pulp_bset_r(0x0, 132u);
}

// CHECK-LABEL: test_builtin_pulp_clb
int32_t test_builtin_pulp_clb(void) {
// CHECK: call i32 @llvm.riscv.pulp.clb
  return __builtin_pulp_clb(0x0F000000);
}

// CHECK-LABEL: test_builtin_pulp_cnt
int32_t test_builtin_pulp_cnt(void) {
// CHECK: call i32 @llvm.riscv.pulp.cnt
  return __builtin_pulp_cnt(0xFF0F);
}

// CHECK-LABEL: test_builtin_pulp_ff1
int32_t test_builtin_pulp_ff1(void) {
// CHECK: call i32 @llvm.riscv.pulp.ff1
  return __builtin_pulp_ff1(0xF0);
}

// CHECK-LABEL: test_builtin_pulp_fl1
int32_t test_builtin_pulp_fl1(void) {
// CHECK: call i32 @llvm.riscv.pulp.fl1
  return __builtin_pulp_fl1(31);
}

// CHECK-LABEL: test_builtin_pulp_parity
int32_t test_builtin_pulp_parity(void) {
// CHECK: call i32 @llvm.riscv.pulp.parity
  return __builtin_pulp_parity(5);
}

// CHECK-LABEL: test_builtin_pulp_rotr
int32_t test_builtin_pulp_rotr(void) {
// CHECK: call i32 @llvm.riscv.pulp.rotr
  return __builtin_pulp_rotr(0xFF0, 1);
}

// CHECK-LABEL: test_builtin_pulp_abs
int32_t test_builtin_pulp_abs(void) {
// CHECK: call i32 @llvm.riscv.pulp.abs
  return __builtin_pulp_abs(-2);
}

// CHECK-LABEL: test_builtin_pulp_addN
int32_t test_builtin_pulp_addN(void) {
// CHECK: call i32 @llvm.riscv.pulp.addN
  return __builtin_pulp_addN(-10, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_addN_r
int32_t test_builtin_pulp_addN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.addN.r
  return __builtin_pulp_addN_r(-10, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_addRN
int32_t test_builtin_pulp_addRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.addRN
  return __builtin_pulp_addRN(-10, 2, 2u, 2u);
}

// CHECK-LABEL: test_builtin_pulp_addRN_r
int32_t test_builtin_pulp_addRN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.addRN.r
  return __builtin_pulp_addRN_r(-10, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_adduN
int32_t test_builtin_pulp_adduN(void) {
// CHECK: call i32 @llvm.riscv.pulp.adduN
  return __builtin_pulp_adduN(11, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_adduN_r
int32_t test_builtin_pulp_adduN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.adduN.r
  return __builtin_pulp_adduN_r(11, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_adduRN
int32_t test_builtin_pulp_adduRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.adduRN
  return __builtin_pulp_adduRN(11, 2, 2u, 2u);
}

// CHECK-LABEL: test_builtin_pulp_adduRN_r
int32_t test_builtin_pulp_adduRN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.adduRN.r
  return __builtin_pulp_adduRN_r(11, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_clip
int32_t test_builtin_pulp_clip(void) {
// CHECK: call i32 @llvm.riscv.pulp.clip
  return __builtin_pulp_clip(-10, -4, 15);
}

// CHECK-LABEL: test_builtin_pulp_clip_r
int32_t test_builtin_pulp_clip_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.clip.r
  return __builtin_pulp_clip_r(-10, 4);
}

// CHECK-LABEL: test_builtin_pulp_clipu
int32_t test_builtin_pulp_clipu(void) {
// CHECK: call i32 @llvm.riscv.pulp.clipu
  return __builtin_pulp_clipu(20, 0, 15);
}

// CHECK-LABEL: test_builtin_pulp_clipu_r
int32_t test_builtin_pulp_clipu_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.clipu.r
  return __builtin_pulp_clipu_r(10, 2);
}

// CHECK-LABEL: test_builtin_pulp_maxsi
int32_t test_builtin_pulp_maxsi(void) {
// CHECK: call i32 @llvm.riscv.pulp.maxsi
  return __builtin_pulp_maxsi(-1, 2);
}

// CHECK-LABEL: test_builtin_pulp_machhsN
int32_t test_builtin_pulp_machhsN(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhsN
  return __builtin_pulp_machhsN(-(7u << 16), 2u << 16, -2, 2);
}

// CHECK-LABEL: test_builtin_pulp_machhsRN
int32_t test_builtin_pulp_machhsRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhsRN
  return __builtin_pulp_machhsRN(-(7u << 16), 2u << 16, -2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_machhuN
int32_t test_builtin_pulp_machhuN(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhuN
  return __builtin_pulp_machhuN(7u << 16, 2u << 16, -2, 2);
}

// CHECK-LABEL: test_builtin_pulp_machhuRN
int32_t test_builtin_pulp_machhuRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.machhuRN
  return __builtin_pulp_machhuRN(7u << 16, 2u << 16, -2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_macsN
int32_t test_builtin_pulp_macsN(void) {
// CHECK: call i32 @llvm.riscv.pulp.macsN
  return __builtin_pulp_macsN(-7, 2, -2, 2);
}

// CHECK-LABEL: test_builtin_pulp_macsRN
int32_t test_builtin_pulp_macsRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.macsRN
  return __builtin_pulp_macsRN(-7, 2, -2, 1, 1);
}

// CHECK-LABEL: test_builtin_pulp_macuN
int32_t test_builtin_pulp_macuN(void) {
// CHECK: call i32 @llvm.riscv.pulp.macuN
  return __builtin_pulp_macuN(7, 2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_macuRN
int32_t test_builtin_pulp_macuRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.macuRN
  return __builtin_pulp_macuRN(7, 2, 2, 1, 1);
}

// CHECK-LABEL: test_builtin_pulp_maxusi
int32_t test_builtin_pulp_maxusi(void) {
// CHECK: call i32 @llvm.riscv.pulp.maxusi
  return __builtin_pulp_maxusi(1, 3);
}

// CHECK-LABEL: test_builtin_pulp_minsi
int32_t test_builtin_pulp_minsi(void) {
// CHECK: call i32 @llvm.riscv.pulp.minsi
  return __builtin_pulp_minsi(-1, 2);
}

// CHECK-LABEL: test_builtin_pulp_minusi
int32_t test_builtin_pulp_minusi(void) {
// CHECK: call i32 @llvm.riscv.pulp.minusi
  return __builtin_pulp_minusi(1, 3);
}

// CHECK-LABEL: test_builtin_pulp_mulhhs
int32_t test_builtin_pulp_mulhhs(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhs
  return __builtin_pulp_mulhhs(-(8u << 16), 2u << 16);
}

// CHECK-LABEL: test_builtin_pulp_mulhhsN
int32_t test_builtin_pulp_mulhhsN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhsN
  return __builtin_pulp_mulhhsN(-(8u << 16), 2u << 16, 1);
}

// CHECK-LABEL: test_builtin_pulp_mulhhsRN
int32_t test_builtin_pulp_mulhhsRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhsRN
  return __builtin_pulp_mulhhsRN(-(8u << 16), 2u << 16, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_mulhhu
int32_t test_builtin_pulp_mulhhu(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhu
  return __builtin_pulp_mulhhu(8u << 16, 2u << 16);
}

// CHECK-LABEL: test_builtin_pulp_mulhhuN
int32_t test_builtin_pulp_mulhhuN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhuN
  return __builtin_pulp_mulhhuN(8u << 16, 2u << 16, 1);
}

// CHECK-LABEL: test_builtin_pulp_mulhhuRN
int32_t test_builtin_pulp_mulhhuRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulhhuRN
  return __builtin_pulp_mulhhuRN(8u << 16, 2u << 16, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_muls
int32_t test_builtin_pulp_muls(void) {
// CHECK: call i32 @llvm.riscv.pulp.muls
  return __builtin_pulp_muls(-7, 2);
}

// CHECK-LABEL: test_builtin_pulp_mulsN
int32_t test_builtin_pulp_mulsN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulsN
  return __builtin_pulp_mulsN(-7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_mulsRN
int32_t test_builtin_pulp_mulsRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulsRN
  return __builtin_pulp_mulsRN(-7, 2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_mulu
int32_t test_builtin_pulp_mulu(void) {
// CHECK: call i32 @llvm.riscv.pulp.mulu
  return __builtin_pulp_mulu(7, 2);
}

// CHECK-LABEL: test_builtin_pulp_muluN
int32_t test_builtin_pulp_muluN(void) {
// CHECK: call i32 @llvm.riscv.pulp.muluN
  return __builtin_pulp_muluN(7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_muluRN
int32_t test_builtin_pulp_muluRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.muluRN
  return __builtin_pulp_muluRN(7, 2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subN
int32_t test_builtin_pulp_subN(void) {
// CHECK: call i32 @llvm.riscv.pulp.subN
  return __builtin_pulp_subN(-7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subN_r
int32_t test_builtin_pulp_subN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.subN.r
  return __builtin_pulp_subN_r(7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subRN
int32_t test_builtin_pulp_subRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.subRN
  return __builtin_pulp_subRN(-7, 2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subRN_r
int32_t test_builtin_pulp_subRN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.subRN.r
  return __builtin_pulp_subRN_r(-7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subuN
int32_t test_builtin_pulp_subuN(void) {
// CHECK: call i32 @llvm.riscv.pulp.subuN
  return __builtin_pulp_subuN(7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subuN_r
int32_t test_builtin_pulp_subuN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.subuN.r
  return __builtin_pulp_subuN_r(7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subuRN
int32_t test_builtin_pulp_subuRN(void) {
// CHECK: call i32 @llvm.riscv.pulp.subuRN
  return __builtin_pulp_subuRN(7, 2, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_subuRN_r
int32_t test_builtin_pulp_subuRN_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.subuRN.r
  return __builtin_pulp_subuRN_r(7, 2, 1);
}

// CHECK-LABEL: test_builtin_pulp_bclr
int32_t test_builtin_pulp_bclr(void) {
// CHECK: call i32 @llvm.riscv.pulp.bclr
  return __builtin_pulp_bclr(0x7FFFFFFF, 0xFFFF1FFF);
}

// CHECK-LABEL: test_builtin_pulp_bclr_r
int32_t test_builtin_pulp_bclr_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.bclr.r
  return __builtin_pulp_bclr_r(0xFFF, (3u << 5) | 4u);
}

// CHECK-LABEL: test_builtin_pulp_bextract
int32_t test_builtin_pulp_bextract(void) {
// CHECK: call i32 @llvm.riscv.pulp.bextract
  return __builtin_pulp_bextract(-0x0000FE00, 0x8, 0x8);
}

// CHECK-LABEL: test_builtin_pulp_bextract_r
int32_t test_builtin_pulp_bextract_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.bextract.r
  return __builtin_pulp_bextract_r(-0x0000FE00, 0x88);
}

// CHECK-LABEL: test_builtin_pulp_bextractu
int32_t test_builtin_pulp_bextractu(void) {
// CHECK: call i32 @llvm.riscv.pulp.bextractu
  return __builtin_pulp_bextractu(0x0000FE00, 0x8, 0x8);
}

// CHECK-LABEL: test_builtin_pulp_bextractu_r
int32_t test_builtin_pulp_bextractu_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.bextractu.r
  return __builtin_pulp_bextractu_r(0x0000FE00, 0x88);
}

// CHECK-LABEL: test_builtin_pulp_binsert
int32_t test_builtin_pulp_binsert(void) {
// CHECK: call i32 @llvm.riscv.pulp.binsert
  return __builtin_pulp_binsert(0x0, 7, 1, 3, 0);
}

// CHECK-LABEL: test_builtin_pulp_binsert_r
int32_t test_builtin_pulp_binsert_r(void) {
// CHECK: call i32 @llvm.riscv.pulp.binsert.r
  return __builtin_pulp_binsert_r(0x0, 7, 1);
}

// CHECK-LABEL: test_builtin_pulp_OffsetedRead
int32_t test_builtin_pulp_OffsetedRead(int32_t* data) {
// CHECK: call i32 @llvm.riscv.pulp.OffsetedRead
  return __builtin_pulp_OffsetedRead(data, 4);
}

// CHECK-LABEL:test_builtin_pulp_OffsetedWrite
void test_builtin_pulp_OffsetedWrite(int32_t* data) {
// CHECK: call void @llvm.riscv.pulp.OffsetedWrite
  __builtin_pulp_OffsetedWrite(1, data, 4);
}

// CHECK-LABEL: test_builtin_pulp_OffsetedReadHalf
int16_t test_builtin_pulp_OffsetedReadHalf(int16_t* data) {
// CHECK: call i32 @llvm.riscv.pulp.OffsetedReadHalf
  return __builtin_pulp_OffsetedReadHalf(data, 4);
}

// CHECK-LABEL:test_builtin_pulp_OffsetedWriteHalf
void test_builtin_pulp_OffsetedWriteHalf(int16_t* data) {
// CHECK: call void @llvm.riscv.pulp.OffsetedWriteHalf
  __builtin_pulp_OffsetedWriteHalf(1, data, 4);
}

// CHECK-LABEL:test_builtin_pulp_OffsetedReadByte
char test_builtin_pulp_OffsetedReadByte(char* data) {
// CHECK: call i32 @llvm.riscv.pulp.OffsetedReadByte
  return __builtin_pulp_OffsetedReadByte(data, 4);
}

// CHECK-LABEL:test_builtin_pulp_OffsetedWriteByte
void test_builtin_pulp_OffsetedWriteByte(char* data) {
// CHECK: call void @llvm.riscv.pulp.OffsetedWriteByte
  __builtin_pulp_OffsetedWriteByte(1, data, 4);
}
