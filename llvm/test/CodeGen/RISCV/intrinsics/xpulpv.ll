; RUN: llc -march=riscv32 -mattr=+xpulpv -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.pulp.CoreId()
define i32 @test_llvm_riscv_pulp_CoreId() {
; CHECK-LABEL: test_llvm_riscv_pulp_CoreId:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr a0, mhartid
; CHECK-NEXT:    andi a0, a0, 15
;
  %1 = tail call i32 @llvm.riscv.pulp.CoreId()
  ret i32 %1
}


declare i32 @llvm.riscv.pulp.ClusterId()
define i32 @test_llvm_riscv_pulp_ClusterId() {
; CHECK-LABEL: test_llvm_riscv_pulp_ClusterId:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr a0, mhartid
; CHECK-NEXT:    andi a0, a0, 2016
; CHECK-NEXT:    srli a0, a0, 5
;
  %1 = tail call i32 @llvm.riscv.pulp.ClusterId()
  ret i32 %1
}


declare i32 @llvm.riscv.pulp.IsFc()
define i32 @test_llvm_riscv_pulp_IsFc() {
; CHECK-LABEL: test_llvm_riscv_pulp_IsFc:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr a0, mhartid
; CHECK-NEXT:    andi a0, a0, 1024
; CHECK-NEXT:    srli a0, a0, 10
;
  %1 = tail call i32 @llvm.riscv.pulp.IsFc()
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.HasFc()
define i32 @test_llvm_riscv_pulp_HasFc() {
; FIXME: right now this is hardwired to zero due to HERO
;        not having a fabric controller at all
; CHECK-LABEL: test_llvm_riscv_pulp_HasFc:
; CHECK:       # %bb.0:
; CHECK-NEXT:    andi a0, zero, 0
;
  %1 = tail call i32 @llvm.riscv.pulp.HasFc()
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mac(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mac() {
; CHECK-LABEL: test_llvm_riscv_pulp_mac:
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[REG0:a[0-9]+]], zero, 1
; CHECK-DAG:     addi [[REG1:a[0-9]+]], zero, 2
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, 3
; FIXME: check register ordering (why not 0, 1, 2?)
; CHECK:         p.mac [[REG2]], [[REG0]], [[REG1]]
;
  %1 = tail call i32 @llvm.riscv.pulp.mac(i32 1, i32 2, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhs(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhs() {
; CHECK-LABEL: test_llvm_riscv_pulp_machhs:
; CHECK:       # %bb.0:
; CHECK-DAG      lui [[REG0:a[0-9]+]], 1048544
; CHECK-DAG      lui [[REG1:a[0-9]+]], 64
; CHECK-DAG      addi [[REG2:a[0-9]+]], zero, 3
; FIXME: check register ordering (why not 0, 1, 2?)
; CHECK:         p.machhs [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.machhs(i32 -131072, i32 262144, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhu() {
; CHECK-LABEL: test_llvm_riscv_pulp_machhu:
; CHECK:       # %bb.0:
; CHECK-DAG      lui [[REG0:a[0-9]+]], 1048544
; CHECK-DAG      lui [[REG1:a[0-9]+]], 64
; CHECK-DAG      addi [[REG2:a[0-9]+]], zero, 3
; FIXME: check register ordering (why not 0, 1, 2?)
; CHECK:         p.machhu [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.machhu(i32 131072, i32 262144, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macs(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macs
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.macs(i32 2, i32 -2, i32 -3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.macu(i32 2, i32 2, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.msu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_msu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_msu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.msu(i32 1, i32 2, i32 10)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bset(i32, i32)
define i32 @test_llvm_riscv_pulp_bset() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bset
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bset(i32 0, i32 240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bset.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bset_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bset_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bset.r(i32 0, i32 132)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clb(i32)
define i32 @test_llvm_riscv_pulp_clb() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clb
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.clb(i32 251658240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.cnt(i32)
define i32 @test_llvm_riscv_pulp_cnt() {
; CHECK-LABEL: @test_llvm_riscv_pulp_cnt
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.cnt(i32 65295)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.ff1(i32)
define i32 @test_llvm_riscv_pulp_ff1() {
; CHECK-LABEL: @test_llvm_riscv_pulp_ff1
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.ff1(i32 240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.fl1(i32)
define i32 @test_llvm_riscv_pulp_fl1() {
; CHECK-LABEL: @test_llvm_riscv_pulp_fl1
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.fl1(i32 31)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.parity(i32)
define i32 @test_llvm_riscv_pulp_parity() {
; CHECK-LABEL: @test_llvm_riscv_pulp_parity
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.parity(i32 5)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.rotr(i32, i32)
define i32 @test_llvm_riscv_pulp_rotr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_rotr
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.rotr(i32 4080, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.abs(i32)
define i32 @test_llvm_riscv_pulp_abs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_abs
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.abs(i32 -2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.addN(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.addN.r(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.addRN(i32 -10, i32 2, i32 2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addRN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.addRN.r(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.adduN(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.adduN.r(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.adduRN(i32 11, i32 2, i32 2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduRN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.adduRN.r(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clip(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_clip() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clip
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.clip(i32 -10, i32 -4, i32 15)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clip.r(i32, i32)
define i32 @test_llvm_riscv_pulp_clip_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clip_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.clip.r(i32 -10, i32 4)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clipu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_clipu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clipu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.clipu(i32 20, i32 0, i32 15)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clipu.r(i32, i32)
define i32 @test_llvm_riscv_pulp_clipu_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clipu_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.clipu.r(i32 10, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.maxsi(i32, i32)
define i32 @test_llvm_riscv_pulp_maxsi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_maxsi
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.maxsi(i32 -1, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhsN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhsN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.machhsN(i32 -458752, i32 131072, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhsRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhsRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.machhsRN(i32 -458752, i32 131072, i32 -2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhuN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhuN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.machhuN(i32 458752, i32 131072, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhuRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhuRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.machhuRN(i32 458752, i32 131072, i32 -2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macsN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macsN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.macsN(i32 -7, i32 2, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macsRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macsRN
; CHECK:       # %bb.0:
;
  %1 = call  i32 @llvm.riscv.pulp.macsRN(i32 -7, i32 2, i32 -2, i32 1, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macuN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macuN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.macuN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macuRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macuRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.macuRN(i32 7, i32 2, i32 2, i32 1, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.maxusi(i32, i32)
define i32 @test_llvm_riscv_pulp_maxusi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_maxusi
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.maxusi(i32 1, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.minsi(i32, i32)
define i32 @test_llvm_riscv_pulp_minsi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_minsi
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.minsi(i32 -1, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.minusi(i32, i32)
define i32 @test_llvm_riscv_pulp_minusi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_minusi
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.minusi(i32 1, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhs(i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhs
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhs(i32 -524288, i32 131072)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhsN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhsN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhsN(i32 -524288, i32 131072, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhsRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhsRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhsRN(i32 -524288, i32 131072, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhu(i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhu(i32 524288, i32 131072)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhuN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhuN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhuN(i32 524288, i32 131072, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhuRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhuRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulhhuRN(i32 524288, i32 131072, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muls(i32, i32)
define i32 @test_llvm_riscv_pulp_muls() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muls
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.muls(i32 -7, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulsN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulsN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulsN(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulsRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulsRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulsRN(i32 -7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulu(i32, i32)
define i32 @test_llvm_riscv_pulp_mulu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.mulu(i32 7, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muluN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_muluN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muluN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.muluN(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muluRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_muluRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muluRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.muluRN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subN(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subRN(i32 -7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subRN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subRN.r(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subuN(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subuN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuRN
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subuRN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuRN_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.subuRN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bclr(i32, i32)
define i32 @test_llvm_riscv_pulp_bclr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bclr
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bclr(i32 2147483647, i32 -57345)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bclr.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bclr_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bclr_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bclr.r(i32 4095, i32 100)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextract(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_bextract() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextract
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bextract(i32 -65024, i32 8, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextract.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bextract_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextract_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bextract.r(i32 -65024, i32 136)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextractu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_bextractu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextractu
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bextractu(i32 65024, i32 8, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextractu.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bextractu_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextractu_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.bextractu.r(i32 65024, i32 136)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.binsert(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_binsert() {
; CHECK-LABEL: @test_llvm_riscv_pulp_binsert
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.binsert(i32 0, i32 7, i32 1, i32 3, i32 0)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.binsert.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_binsert_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_binsert_r
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.binsert.r(i32 0, i32 7, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.OffsetedRead(i32*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedRead(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedRead
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedRead(i32* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWrite(i32, i32*, i32)
define void @test_llvm_riscv_pulp_OffsetedWrite(i32* %data) {
; CHECK-LABEL: @@test_llvm_riscv_pulp_OffsetedWrite
; CHECK:       # %bb.0:
;
  call void @llvm.riscv.pulp.OffsetedWrite(i32 1, i32* %data, i32 4)
  ret void
}

declare i32 @llvm.riscv.pulp.OffsetedReadHalf(i16*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedReadHalf(i16* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedReadHalf
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedReadHalf(i16* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWriteHalf(i32, i16*, i32)
define void @test_llvm_riscv_pulp_OffsetedWriteHalf(i16* %data) {
; CHECK-LABEL: @@test_llvm_riscv_pulp_OffsetedWriteHalf
; CHECK:       # %bb.0:
;
  call void @llvm.riscv.pulp.OffsetedWriteHalf(i32 1, i16* %data, i32 4)
  ret void
}

declare i32 @llvm.riscv.pulp.OffsetedReadByte(i8*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedReadByte(i8* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedReadByte
; CHECK:       # %bb.0:
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedReadByte(i8* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWriteByte(i32, i8*, i32)
define void @test_llvm_riscv_pulp_OffsetedWriteByte(i8* %data) {
; CHECK-LABEL: @@test_llvm_riscv_pulp_OffsetedWriteByte
; CHECK:       # %bb.0:
;
  call void @llvm.riscv.pulp.OffsetedWriteByte(i32 1, i8* %data, i32 4)
  ret void
}
