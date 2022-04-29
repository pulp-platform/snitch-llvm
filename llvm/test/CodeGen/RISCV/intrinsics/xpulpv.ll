; RUN: llc -march=riscv32 -mattr=+xpulpv -verify-machineinstrs < %s | FileCheck %s

declare i32 @llvm.riscv.pulp.CoreId()
define i32 @test_llvm_riscv_pulp_CoreId() {
; CHECK-LABEL: test_llvm_riscv_pulp_CoreId:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr [[REG:a[0-9]+]], mhartid
; CHECK-NEXT:    andi [[REG]], [[REG]], 15
;
  %1 = tail call i32 @llvm.riscv.pulp.CoreId()
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.ClusterId()
define i32 @test_llvm_riscv_pulp_ClusterId() {
; CHECK-LABEL: test_llvm_riscv_pulp_ClusterId:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr [[REG:a[0-9]+]], mhartid
; CHECK-NEXT:    andi [[REG]], [[REG]], 2016
; CHECK-NEXT:    srli [[REG]], [[REG]], 5
;
  %1 = tail call i32 @llvm.riscv.pulp.ClusterId()
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.IsFc()
define i32 @test_llvm_riscv_pulp_IsFc() {
; CHECK-LABEL: test_llvm_riscv_pulp_IsFc:
; CHECK:       # %bb.0:
; CHECK-NEXT:    csrr [[REG:a[0-9]+]], mhartid
; CHECK-NEXT:    andi [[REG]], [[REG]], 1024
; CHECK-NEXT:    srli [[REG]], [[REG]], 10
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
; CHECK-NEXT:    andi {{a[0-9]+}}, zero, 0
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
; CHECK-NEXT:    p.mac [[REG2]], [[REG0]], [[REG1]]
;
  %1 = tail call i32 @llvm.riscv.pulp.mac(i32 1, i32 2, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhs(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhs() {
; CHECK-LABEL: test_llvm_riscv_pulp_machhs:
; CHECK:       # %bb.0:
; CHECK-DAG:     lui [[REG0:a[0-9]+]], 1048544
; CHECK-DAG:     lui [[REG1:a[0-9]+]], 64
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, 3
; CHECK-NEXT:    p.machhs [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.machhs(i32 -131072, i32 262144, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhu() {
; CHECK-LABEL: test_llvm_riscv_pulp_machhu:
; CHECK:       # %bb.0:
; CHECK-DAG:     lui [[REG0:a[0-9]+]], 32
; CHECK-DAG:     lui [[REG1:a[0-9]+]], 64
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, 3
; CHECK-NEXT:    p.machhu [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.machhu(i32 131072, i32 262144, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macs(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macs
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[REG0:a[0-9]+]], zero, 2
; CHECK-DAG:     addi [[REG1:a[0-9]+]], zero, -2
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, -3
; CHECK-NEXT:    p.macs [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.macs(i32 2, i32 -2, i32 -3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macu
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[REG0:a[0-9]+]], zero, 2
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, 3
; CHECK-NEXT:    p.macu [[REG2]], [[REG0]], [[REG0]]
;
  %1 = call i32 @llvm.riscv.pulp.macu(i32 2, i32 2, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.msu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_msu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_msu
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[REG0:a[0-9]+]], zero, 1
; CHECK-DAG:     addi [[REG1:a[0-9]+]], zero, 2
; CHECK-DAG:     addi [[REG2:a[0-9]+]], zero, 10
; CHECK-NEXT:    p.msu [[REG2]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.msu(i32 1, i32 2, i32 10)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bset(i32, i32)
define i32 @test_llvm_riscv_pulp_bset() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bset
; CHECK:       # %bb.0:
; CHECK-NEXT:    p.bset [[REG:a[0-9]+]], zero, 3, 4
;
  %1 = call i32 @llvm.riscv.pulp.bset(i32 0, i32 240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bset.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bset_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bset_r
; CHECK:       # %bb.0:
; CHECK-NEXT:    addi [[REG:a[0-9]+]], zero, 132
; CHECK-NEXT:    p.bsetr [[REG]], zero, [[REG]]
;
  %1 = call i32 @llvm.riscv.pulp.bset.r(i32 0, i32 132)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clb(i32)
define i32 @test_llvm_riscv_pulp_clb() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clb
; CHECK:       # %bb.0:
; CHECK-NEXT:    lui [[REG:a[0-9]+]], 61440
; CHECK-NEXT:    p.clb [[REG]], [[REG]]
;
  %1 = call i32 @llvm.riscv.pulp.clb(i32 251658240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.cnt(i32)
define i32 @test_llvm_riscv_pulp_cnt() {
; CHECK-LABEL: @test_llvm_riscv_pulp_cnt
; CHECK:       # %bb.0:
; CHECK:         p.cnt {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.cnt(i32 65295)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.ff1(i32)
define i32 @test_llvm_riscv_pulp_ff1() {
; CHECK-LABEL: @test_llvm_riscv_pulp_ff1
; CHECK:       # %bb.0:
; CHECK:         p.ff1 {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.ff1(i32 240)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.fl1(i32)
define i32 @test_llvm_riscv_pulp_fl1() {
; CHECK-LABEL: @test_llvm_riscv_pulp_fl1
; CHECK:       # %bb.0:
; CHECK:         p.fl1 {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.fl1(i32 31)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.parity(i32)
define i32 @test_llvm_riscv_pulp_parity() {
; CHECK-LABEL: @test_llvm_riscv_pulp_parity
; CHECK:       # %bb.0:
; CHECK-NEXT:    addi [[REG:a[0-9]+]], zero, 5
; CHECK-NEXT:    p.cnt [[REG]], [[REG]]
; CHECK-NEXT:    p.bclr [[REG]], [[REG]], 30, 1
;
  %1 = call i32 @llvm.riscv.pulp.parity(i32 5)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.rotr(i32, i32)
define i32 @test_llvm_riscv_pulp_rotr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_rotr
; CHECK:       # %bb.0:
; CHECK-DAG:     lui [[REG0:a[0-9]+]], 1
; CHECK-DAG:     addi [[REG0]], [[REG0]], -16
; CHECK-DAG:     addi [[REG1:a[0-9]+]], zero, 1
; CHECK-NEXT:    p.ror [[REG0]], [[REG0]], [[REG1]]
;
  %1 = call i32 @llvm.riscv.pulp.rotr(i32 4080, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.abs(i32)
define i32 @test_llvm_riscv_pulp_abs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_abs
; CHECK:       # %bb.0:
; CHECK-NEXT:    addi [[REG:a[0-9]+]], zero, -2
; CHECK-NEXT:    p.abs [[REG]], [[REG]]
;
  %1 = call i32 @llvm.riscv.pulp.abs(i32 -2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addN
; CHECK:       # %bb.0:
; CHECK:         p.addn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.addN(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addN_r
; CHECK:       # %bb.0:
; CHECK:         p.addnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.addN.r(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addRN
; CHECK:       # %bb.0:
; CHECK:         p.addrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.addRN(i32 -10, i32 2, i32 2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.addRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_addRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_addRN_r
; CHECK:       # %bb.0:
; CHECK:         p.addrnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.addRN.r(i32 -10, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduN
; CHECK:       # %bb.0:
; CHECK:         p.addun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.adduN(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduN_r
; CHECK:       # %bb.0:
; CHECK:         p.addunr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.adduN.r(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduRN
; CHECK:       # %bb.0:
; CHECK:         p.addurn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.adduRN(i32 11, i32 2, i32 2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.adduRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_adduRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_adduRN_r
; CHECK:       # %bb.0:
; CHECK:         p.addurnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.adduRN.r(i32 11, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clip(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_clip() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clip
; CHECK:       # %bb.0:
; CHECK:         p.clip {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.clip(i32 -10, i32 -4, i32 15)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clip.r(i32, i32)
define i32 @test_llvm_riscv_pulp_clip_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clip_r
; CHECK:       # %bb.0:
; CHECK:         p.clipr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.clip.r(i32 -10, i32 4)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clipu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_clipu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clipu
; CHECK:       # %bb.0:
; CHECK:         p.clipu {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.clipu(i32 20, i32 0, i32 15)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.clipu.r(i32, i32)
define i32 @test_llvm_riscv_pulp_clipu_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_clipu_r
; CHECK:       # %bb.0:
; CHECK:         p.clipur {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.clipu.r(i32 10, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.maxsi(i32, i32)
define i32 @test_llvm_riscv_pulp_maxsi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_maxsi
; CHECK:       # %bb.0:
; CHECK:         p.max {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.maxsi(i32 -1, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhsN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhsN
; CHECK:       # %bb.0:
; CHECK:         p.machhsn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.machhsN(i32 -458752, i32 131072, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhsRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhsRN
; CHECK:       # %bb.0:
; CHECK:         p.machhsrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.machhsRN(i32 -458752, i32 131072, i32 -2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhuN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhuN
; CHECK:       # %bb.0:
; CHECK:         p.machhun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.machhuN(i32 458752, i32 131072, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.machhuRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_machhuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_machhuRN
; CHECK:       # %bb.0:
; CHECK:         p.machhurn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.machhuRN(i32 458752, i32 131072, i32 -2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macsN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macsN
; CHECK:       # %bb.0:
; CHECK:         p.macsn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.macsN(i32 -7, i32 2, i32 -2, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macsRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macsRN
; CHECK:       # %bb.0:
; CHECK:         p.macsrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call  i32 @llvm.riscv.pulp.macsRN(i32 -7, i32 2, i32 -2, i32 1, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macuN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macuN
; CHECK:       # %bb.0:
; CHECK:         p.macun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.macuN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.macuRN(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_macuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_macuRN
; CHECK:       # %bb.0:
; CHECK:         p.macurn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.macuRN(i32 7, i32 2, i32 2, i32 1, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.maxusi(i32, i32)
define i32 @test_llvm_riscv_pulp_maxusi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_maxusi
; CHECK:       # %bb.0:
; CHECK:         p.maxu {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.maxusi(i32 1, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.minsi(i32, i32)
define i32 @test_llvm_riscv_pulp_minsi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_minsi
; CHECK:       # %bb.0:
; CHECK:         p.min {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.minsi(i32 -1, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.minusi(i32, i32)
define i32 @test_llvm_riscv_pulp_minusi() {
; CHECK-LABEL: @test_llvm_riscv_pulp_minusi
; CHECK:       # %bb.0:
; CHECK:         p.minu {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.minusi(i32 1, i32 3)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhs(i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhs() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhs
; CHECK:       # %bb.0:
; CHECK:         p.mulhhs {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhs(i32 -524288, i32 131072)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhsN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhsN
; CHECK:       # %bb.0:
; CHECK:         p.mulhhsn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhsN(i32 -524288, i32 131072, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhsRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhsRN
; CHECK:       # %bb.0:
; CHECK:         p.mulhhsrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhsRN(i32 -524288, i32 131072, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhu(i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhu
; CHECK:       # %bb.0:
; CHECK:         p.mulhhu {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhu(i32 524288, i32 131072)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhuN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhuN
; CHECK:       # %bb.0:
; CHECK:         p.mulhhun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhuN(i32 524288, i32 131072, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulhhuRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulhhuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulhhuRN
; CHECK:       # %bb.0:
; CHECK:         p.mulhhurn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulhhuRN(i32 524288, i32 131072, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muls(i32, i32)
define i32 @test_llvm_riscv_pulp_muls() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muls
; CHECK:       # %bb.0:
; CHECK:         p.muls {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.muls(i32 -7, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulsN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulsN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulsN
; CHECK:       # %bb.0:
; CHECK:         p.mulsn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulsN(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulsRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_mulsRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulsRN
; CHECK:       # %bb.0:
; CHECK:         p.mulsrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulsRN(i32 -7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.mulu(i32, i32)
define i32 @test_llvm_riscv_pulp_mulu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_mulu
; CHECK:       # %bb.0:
; CHECK:         p.mulu {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.mulu(i32 7, i32 2)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muluN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_muluN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muluN
; CHECK:       # %bb.0:
; CHECK:         p.mulun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.muluN(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.muluRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_muluRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_muluRN
; CHECK:       # %bb.0:
; CHECK:         p.mulurn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.muluRN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subN
; CHECK:       # %bb.0:
; CHECK:         p.subn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subN(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subN_r
; CHECK:       # %bb.0:
; CHECK:         p.subnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subRN
; CHECK:       # %bb.0:
; CHECK:         p.subrn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subRN(i32 -7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subRN_r
; CHECK:       # %bb.0:
; CHECK:         p.subrnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subRN.r(i32 -7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuN(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuN
; CHECK:       # %bb.0:
; CHECK:         p.subun {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subuN(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuN_r
; CHECK:       # %bb.0:
; CHECK:         p.subunr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subuN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuRN(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuRN() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuRN
; CHECK:       # %bb.0:
; CHECK:         p.suburn {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subuRN(i32 7, i32 2, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.subuRN.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_subuRN_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_subuRN_r
; CHECK:       # %bb.0:
; CHECK:         p.suburnr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.subuRN.r(i32 7, i32 2, i32 1)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bclr(i32, i32)
define i32 @test_llvm_riscv_pulp_bclr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bclr
; CHECK:       # %bb.0:
; CHECK:         p.bclr {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bclr(i32 2147483647, i32 -57345)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bclr.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bclr_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bclr_r
; CHECK:       # %bb.0:
; CHECK:         p.bclrr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bclr.r(i32 4095, i32 100)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextract(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_bextract() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextract
; CHECK:       # %bb.0:
; CHECK:         p.extract {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bextract(i32 -65024, i32 8, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextract.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bextract_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextract_r
; CHECK:       # %bb.0:
; CHECK:         p.extractr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bextract.r(i32 -65024, i32 136)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextractu(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_bextractu() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextractu
; CHECK:       # %bb.0:
; CHECK:         p.extractu {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bextractu(i32 65024, i32 8, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.bextractu.r(i32, i32)
define i32 @test_llvm_riscv_pulp_bextractu_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_bextractu_r
; CHECK:       # %bb.0:
; CHECK:         p.extractur {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.bextractu.r(i32 65024, i32 136)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.binsert(i32, i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_binsert() {
; CHECK-LABEL: @test_llvm_riscv_pulp_binsert
; CHECK:       # %bb.0:
; CHECK:         p.insert {{a[0-9]+}}, {{a[0-9]+}}, {{[0-9]+}}, {{[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.binsert(i32 0, i32 7, i32 1, i32 3, i32 0)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.binsert.r(i32, i32, i32)
define i32 @test_llvm_riscv_pulp_binsert_r() {
; CHECK-LABEL: @test_llvm_riscv_pulp_binsert_r
; CHECK:       # %bb.0:
; CHECK:         p.insertr {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %1 = call i32 @llvm.riscv.pulp.binsert.r(i32 0, i32 7, i32 1)
  ret i32 %1
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Memory intrinsics

declare i32 @llvm.riscv.pulp.OffsetedRead(i32*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedRead(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedRead
; CHECK:       # %bb.0:
; CHECK:         addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK:         p.lw [[PTR:a[0-9]+]], [[OFFSET]]([[PTR]])
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedRead(i32* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWrite(i32, i32*, i32)
define void @test_llvm_riscv_pulp_OffsetedWrite(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedWrite
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK-DAG:     addi [[VALUE:a[0-9]+]], zero, 1
; CHECK:         p.sw [[VALUE]], [[OFFSET]]({{a[0-9]+}})
;
  call void @llvm.riscv.pulp.OffsetedWrite(i32 1, i32* %data, i32 4)
  ret void
}

declare i32 @llvm.riscv.pulp.OffsetedReadHalf(i16*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedReadHalf(i16* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedReadHalf
; CHECK:       # %bb.0:
; CHECK:         addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK:         p.lh [[PTR:a[0-9]+]], [[OFFSET]]([[PTR]])
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedReadHalf(i16* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWriteHalf(i32, i16*, i32)
define void @test_llvm_riscv_pulp_OffsetedWriteHalf(i16* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedWriteHalf
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK-DAG:     addi [[VALUE:a[0-9]+]], zero, 1
; CHECK:         p.sh [[VALUE]], [[OFFSET]]({{a[0-9]+}})
;
  call void @llvm.riscv.pulp.OffsetedWriteHalf(i32 1, i16* %data, i32 4)
  ret void
}

declare i32 @llvm.riscv.pulp.OffsetedReadByte(i8*, i32)
define i32 @test_llvm_riscv_pulp_OffsetedReadByte(i8* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedReadByte
; CHECK:       # %bb.0:
; CHECK:         addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK:         p.lb [[PTR:a[0-9]+]], [[OFFSET]]([[PTR]])
;
  %1 = call i32 @llvm.riscv.pulp.OffsetedReadByte(i8* %data, i32 4)
  ret i32 %1
}

declare void @llvm.riscv.pulp.OffsetedWriteByte(i32, i8*, i32)
define void @test_llvm_riscv_pulp_OffsetedWriteByte(i8* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_OffsetedWriteByte
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[OFFSET:a[0-9]+]], zero, 4
; CHECK-DAG:     addi [[VALUE:a[0-9]+]], zero, 1
; CHECK:         p.sb [[VALUE]], [[OFFSET]]({{a[0-9]+}})
;
  call void @llvm.riscv.pulp.OffsetedWriteByte(i32 1, i8* %data, i32 4)
  ret void
}

declare i32 @llvm.riscv.pulp.read.base.off(i32* %data, i32)
define i32 @test_llvm_riscv_pulp_read_base_off(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_read_base_off
; CHECK:       # %bb.0:
; CHECK:         addi [[OFFSET:a[0-9]+]], zero, 15
; CHECK:         p.lw [[PTR:a[0-9]+]], [[OFFSET]]([[PTR]])
;
  %1 = call i32 @llvm.riscv.pulp.read.base.off(i32* %data, i32 15)
  ret i32 %1
}

declare void @llvm.riscv.pulp.write.base.off(i32, i32*, i32)
define void @test_llvm_riscv_pulp_write_base_off(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_write_base_off
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[OFFSET:a[0-9]+]], zero, 15
; CHECK-DAG:     addi [[VALUE:a[0-9]+]], zero, 1
; CHECK:         p.sw [[VALUE]], [[OFFSET]]({{a[0-9]+}})
;
  call void @llvm.riscv.pulp.write.base.off(i32 1, i32* %data, i32 15)
  ret void
}

declare i32 @llvm.riscv.pulp.read.base.off.v(i32*, i32)
define i32 @test_llvm_riscv_pulp_read_base_off_v(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_read_base_off_v
; CHECK:       # %bb.0:
; CHECK:         addi [[OFFSET:a[0-9]+]], zero, 15
; CHECK:         p.lw [[PTR:a[0-9]+]], [[OFFSET]]([[PTR]])
;
  %1 = call i32 @llvm.riscv.pulp.read.base.off.v(i32* %data, i32 15)
  ret i32 %1
}

declare void @llvm.riscv.pulp.write.base.off.v(i32, i32*, i32)
define void @test_llvm_riscv_pulp_write_base_off_v(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_write_base_off_v
; CHECK:       # %bb.0:
; CHECK-DAG:     addi [[OFFSET:a[0-9]+]], zero, 15
; CHECK-DAG:     addi [[VALUE:a[0-9]+]], zero, 1
; CHECK:         p.sw [[VALUE]], [[OFFSET]]({{a[0-9]+}})
;
  call void @llvm.riscv.pulp.write.base.off.v(i32 1, i32* %data, i32 15)
  ret void
}

declare i32 @llvm.riscv.pulp.read.then.spr.bit.clr(i32, i32)
define i32 @test_llvm_riscv_pulp_read_then_spr_bit_clr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_read_then_spr_bit_clr
; CHECK:       # %bb.0:
; CHECK:         csrrci {{a[0-9]+}}, mhartid, 8
;
  %1 = call i32 @llvm.riscv.pulp.read.then.spr.bit.clr(i32 3860, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.read.then.spr.bit.set(i32, i32)
define i32 @test_llvm_riscv_pulp_read_then_spr_bit_set() {
; CHECK-LABEL: @test_llvm_riscv_pulp_read_then_spr_bit_set
; CHECK:       # %bb.0:
; CHECK:         csrrsi {{a[0-9]+}}, mhartid, 8
;
  %1 = call i32 @llvm.riscv.pulp.read.then.spr.bit.set(i32 3860, i32 8)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.read.then.spr.write(i32, i32)
define i32 @test_llvm_riscv_pulp_read_then_spr_write() {
; CHECK-LABEL: @test_llvm_riscv_pulp_read_then_spr_write
; CHECK:       # %bb.0:
; CHECK:         csrrw [[REG:a[0-9]+]], mhartid, [[REG]]
;
  %1 = call i32 @llvm.riscv.pulp.read.then.spr.write(i32 3860, i32 8)
  ret i32 %1
}

declare void @llvm.riscv.pulp.spr.bit.clr(i32, i32)
define void @test_llvm_riscv_pulp_spr_bit_clr() {
; CHECK-LABEL: @test_llvm_riscv_pulp_spr_bit_clr
; CHECK:       # %bb.0:
; CHECK:         csrrci {{a[0-9]+}}, mhartid, 8
;
  call void @llvm.riscv.pulp.spr.bit.clr(i32 3860, i32 8)
  ret void
}

declare void @llvm.riscv.pulp.spr.bit.set(i32, i32)
define void @test_llvm_riscv_pulp_spr_bit_set() {
; CHECK-LABEL: @test_llvm_riscv_pulp_spr_bit_set
; CHECK:       # %bb.0:
; CHECK:         csrrsi {{a[0-9]+}}, mhartid, 8
;
  call void @llvm.riscv.pulp.spr.bit.set(i32 3860, i32 8)
  ret void
}

declare i32 @llvm.riscv.pulp.spr.read(i32)
define i32 @test_llvm_riscv_pulp_spr_read() {
; CHECK-LABEL: @
; CHECK:       # %bb.0:
; CHECK:         csrr {{a[0-9]+}}, mhartid
;
  %1 = call i32 @llvm.riscv.pulp.spr.read(i32 3860)
  ret i32 %1
}

declare i32 @llvm.riscv.pulp.spr.read.vol(i32)
define i32 @test_llvm_riscv_pulp_spr_read_vol() {
; CHECK-LABEL: @test_llvm_riscv_pulp_spr_read_vol
; CHECK:       # %bb.0:
; CHECK:         csrr {{a[0-9]+}}, mhartid
;
  %1 = call i32 @llvm.riscv.pulp.spr.read.vol(i32 3860)
  ret i32 %1
}

declare void @llvm.riscv.pulp.spr.write(i32, i32)
define void @test_llvm_riscv_pulp_spr_write() {
; CHECK-LABEL: @test_llvm_riscv_pulp_spr_write
; CHECK:       # %bb.0:
; CHECK:         csrrw [[REG:a[0-9]+]], mhartid, [[REG]]
;
  call void @llvm.riscv.pulp.spr.write(i32 3860, i32 8)
  ret void
}

declare i32 @llvm.riscv.pulp.event.unit.read(i32*, i32)
define i32 @test_llvm_riscv_pulp_event_unit_read(i32* %data) {
; CHECK-LABEL: @test_llvm_riscv_pulp_event_unit_read
; CHECK:       # %bb.0:
; CHECK:         p.elw [[REG:a[0-9]+]], 8([[REG]])
;
  %1 = call i32 @llvm.riscv.pulp.event.unit.read(i32* %data, i32 8)
  ret i32 %1
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Packed SIMD intrinsics
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; <2 x i16>

declare i32 @llvm.riscv.pulp.dotsp2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_dotsp2(i32 %a, i32 %b) {
; CHECK-LABEL: @test_llvm_riscv_pulp_dotsp2
; CHECK:       # %bb.0:
; CHECK:         pv.dotsp.h {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotsp2(<2 x i16> %va, <2 x i16> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotup2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_dotup2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotup2(<2 x i16> %va, <2 x i16> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotspsc2(<2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_dotspsc2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotspsc2(<2 x i16> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotusp2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_dotusp2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotusp2(<2 x i16> %va, <2 x i16> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotsp2(<2 x i16>, <2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_sdotsp2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotsp2(<2 x i16> %va, <2 x i16> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotspsc2(<2 x i16>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotspsc2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotspsc2(<2 x i16> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotusp2(<2 x i16>, <2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_sdotusp2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotusp2(<2 x i16> %va, <2 x i16> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotupsc2(<2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_dotupsc2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotupsc2(<2 x i16> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotuspsc2(<2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_dotuspsc2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.dotuspsc2(<2 x i16> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotup2(<2 x i16>, <2 x i16>, i32)
define i32 @test_llvm_riscv_pulp_sdotup2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotup2(<2 x i16> %va, <2 x i16> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotupsc2(<2 x i16>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotupsc2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotupsc2(<2 x i16> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotuspsc2(<2 x i16>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotuspsc2(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %ret = call i32 @llvm.riscv.pulp.sdotuspsc2(<2 x i16> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.abs2(<2 x i16>)
define i32 @test_llvm_riscv_pulp_abs2(i32 %a) {
; CHECK-LABEL: @test_llvm_riscv_pulp_abs2
; CHECK:       # %bb.0:
; CHECK:         pv.abs.h {{a[0-9]+}}, {{a[0-9]+}}
;
  %va = bitcast i32 %a to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.abs2(<2 x i16> %va)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.add2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_add2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.add2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.and2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_and2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.and2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.avg2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_avg2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.avg2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.avgu2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_avgu2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.avgu2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.exor2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_exor2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.exor2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.max2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_max2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.max2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.min2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_min2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.min2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.maxu2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_maxu2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.maxu2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.minu2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_minu2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call  <2 x i16> @llvm.riscv.pulp.minu2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.neg2(<2 x i16>)
define i32 @test_llvm_riscv_pulp_neg2(i32 %a) {
  %va = bitcast i32 %a to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.neg2(<2 x i16> %va)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.or2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_or2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.or2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.sll2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_sll2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.sll2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.sra2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_sra2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.sra2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.sub2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_sub2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.sub2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.pack2(i32, i32)
define i32 @test_llvm_riscv_pulp_pack2(i32 %a, i32 %b) {
; CHECK-LABEL: @test_llvm_riscv_pulp_pack2
; CHECK:       # %bb.0:
; CHECK:         pv.pack.h {{a[0-9]+}}, {{a[0-9]+}}, {{a[0-9]+}}
;
  %res = call <2 x i16> @llvm.riscv.pulp.pack2(i32 %a, i32 %b)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.srl2(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_srl2(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call  <2 x i16> @llvm.riscv.pulp.srl2(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.shuffle2h(<2 x i16>, <2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_shuffle2h(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %vc = bitcast i32 %c to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.shuffle2h(<2 x i16> %va, <2 x i16> %vb, <2 x i16> %vc)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

declare <2 x i16> @llvm.riscv.pulp.shuffleh(<2 x i16>, <2 x i16>)
define i32 @test_llvm_riscv_pulp_shuffleh(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <2 x i16>
  %vb = bitcast i32 %b to <2 x i16>
  %res = call <2 x i16> @llvm.riscv.pulp.shuffleh(<2 x i16> %va, <2 x i16> %vb)
  %ret = bitcast <2 x i16> %res to i32
  ret i32 %ret
}

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; <4 x i8>

declare i32 @llvm.riscv.pulp.dotsp4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_dotsp4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotsp4(<4 x i8> %va, <4 x i8> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotspsc4(<4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_dotspsc4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotspsc4(<4 x i8> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotup4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_dotup4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotup4(<4 x i8> %va, <4 x i8> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotupsc4(<4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_dotupsc4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotupsc4(<4 x i8> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotusp4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_dotusp4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotusp4(<4 x i8> %va, <4 x i8> %vb)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.dotuspsc4(<4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_dotuspsc4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.dotuspsc4(<4 x i8> %va, i32 %b)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotsp4(<4 x i8>, <4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_sdotsp4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotsp4(<4 x i8> %va, <4 x i8> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotspsc4(<4 x i8>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotspsc4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotspsc4(<4 x i8> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotup4(<4 x i8>, <4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_sdotup4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotup4(<4 x i8> %va, <4 x i8> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotupsc4(<4 x i8>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotupsc4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotupsc4(<4 x i8> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotusp4(<4 x i8>, <4 x i8>, i32)
define i32 @test_llvm_riscv_pulp_sdotusp4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotusp4(<4 x i8> %va, <4 x i8> %vb, i32 %c)
  ret i32 %ret
}

declare i32 @llvm.riscv.pulp.sdotuspsc4(<4 x i8>, i32, i32)
define i32 @test_llvm_riscv_pulp_sdotuspsc4(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %ret = call i32 @llvm.riscv.pulp.sdotuspsc4(<4 x i8> %va, i32 %b, i32 %c)
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.abs4(<4 x i8>)
define i32 @test_llvm_riscv_pulp_abs4(i32 %a) {
  %va = bitcast i32 %a to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.abs4(<4 x i8> %va)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.add4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_add4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.add4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.and4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_and4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.and4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.avg4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_avg4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.avg4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.avgu4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_avgu4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.avgu4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.exor4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_exor4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.exor4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.max4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_max4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.max4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.maxu4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_maxu4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.maxu4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.min4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_min4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.min4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.minu4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_minu4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.minu4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.neg4(<4 x i8>)
define i32 @test_llvm_riscv_pulp_neg4(i32 %a) {
  %va = bitcast i32 %a to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.neg4(<4 x i8> %va)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.or4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_or4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.or4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.sll4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_sll4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.sll4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.sra4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_sra4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.sra4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.srl4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_srl4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.srl4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.sub4(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_sub4(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.sub4(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.pack4(i32, i32, i32, i32)
define i32 @test_llvm_riscv_pulp_pack4(i32 %a, i32 %b, i32 %c, i32 %d) {
; CHECK-LABEL: @test_llvm_riscv_pulp_pack4
; CHECK:       # %bb.0:
; CHECK:         pv.packhi.b [[REG:a[0-9]+]], {{a[0-9]+}}, {{a[0-9]+}}
; CHECIK:        pv.packlo.b [[REG]], {{a[0-9]+}}, {{a[0-9]+}}
;
  %res = call <4 x i8> @llvm.riscv.pulp.pack4(i32 %a, i32 %b, i32 %c, i32 %d)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.pack4.hi(i32, i32, <4 x i8>)
define i32 @test_llvm_riscv_pulp_pack4_hi(i32 %a, i32 %b, i32 %c) {
  %vc = bitcast i32 %c to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.pack4.hi(i32 %a, i32 %b, <4 x i8> %vc)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.pack4.lo(i32, i32, <4 x i8>)
define i32 @test_llvm_riscv_pulp_pack4_lo(i32 %a, i32 %b, i32 %c) {
  %vc = bitcast i32 %c to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.pack4.lo(i32 %a, i32 %b, <4 x i8> %vc)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.shuffle4b(<4 x i8>, <4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_shuffle4b(i32 %a, i32 %b, i32 %c) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %vc = bitcast i32 %c to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.shuffle4b(<4 x i8> %va, <4 x i8> %vb, <4 x i8> %vc)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}

declare <4 x i8> @llvm.riscv.pulp.shuffleb(<4 x i8>, <4 x i8>)
define i32 @test_llvm_riscv_pulp_shuffleb(i32 %a, i32 %b) {
  %va = bitcast i32 %a to <4 x i8>
  %vb = bitcast i32 %b to <4 x i8>
  %res = call <4 x i8> @llvm.riscv.pulp.shuffleb(<4 x i8> %va, <4 x i8> %vb)
  %ret = bitcast <4 x i8> %res to i32
  ret i32 %ret
}
