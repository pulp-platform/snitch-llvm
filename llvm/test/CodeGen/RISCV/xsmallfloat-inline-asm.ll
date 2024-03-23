; RUN: llc -mtriple=riscv32 -verify-machineinstrs \
; RUN:     -mattr=+m -mattr=+f -mattr=+zfh \
; RUN:     -mattr=+xfalthalf -mattr=+xfquarter -mattr=+xfaltquarter \
; RUN:     -mattr=+xfvechalf -mattr=+xfvecalthalf -mattr=+xfvecquarter \
; RUN:     -mattr=+xfvecaltquarter -mattr=+xfauxhalf -mattr=+xfauxalthalf \
; RUN:     -mattr=+xfauxquarter -mattr=+xfauxaltquarter -mattr=+xfauxvechalf \
; RUN:     -mattr=+xfauxvecalthalf -mattr=+xfauxvecquarter \
; RUN:     -mattr=+xfauxvecaltquarter -mattr=+xfexpauxvechalf \
; RUN:     -mattr=+xfexpauxvecalthalf -mattr=+xfexpauxvecquarter \
; RUN:     -mattr=+xfexpauxvecaltquarter < %s \
; RUN:  | FileCheck %s

; This is a regression test for llc complaining about:
; error: couldn't allocate output register for constraint 'f'

; We just need to ensure llc is able to emit something
; since this is a regression test for llc being unable
; to allocate a proper float register for asm arguments
; CHECK-LABEL: test:
; CHECK:       # %bb.0:
; CHECK:           vfadd.h
define void @test() nounwind {
entry:
  %v0 = alloca <2 x i16>, align 4
  %v1 = alloca <2 x i16>, align 4
  %v2 = alloca <2 x i16>, align 4
  %0 = load <2 x i16>, <2 x i16>* %v1, align 4
  %1 = load <2 x i16>, <2 x i16>* %v2, align 4
  %2 = call <2 x i16> asm sideeffect "vfadd.h $0, $1, $2", "=f,f,f"(<2 x i16> %0, <2 x i16> %1)
  store <2 x i16> %2, <2 x i16>* %v0, align 4
  ret void
}

