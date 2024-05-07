; RUN: llc -mtriple=riscv32 -mattr=+f -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,RVF
; RUN: llc -mtriple=riscv32 -mattr=+d -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,RVD 
; RUN: llc -mtriple=riscv64 -mattr=+f -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,RVF
; RUN: llc -mtriple=riscv64 -mattr=+d -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s --check-prefixes=CHECK,RVD

define float @fdiv_s(float %a, float %b) {
; CHECK-LABEL: fdiv_s:
; RVF:       tail	__divsf3
; RVD:       tail	__divsf3
  %ret = fdiv float %a, %b
  ret float %ret
}

define double @fdiv_d(double %a, double %b) {
; CHECK-LABEL: fdiv_d:
; RVF:       call	__divdf3
; RVD:       tail	__divdf3
  %ret = fdiv double %a, %b
  ret double %ret
}

define float @sqrt_s(float %a) {
; CHECK-LABEL: sqrt_s:
; RVF:       tail	sqrtf
; RVD:       tail	sqrtf
  %ret = call float @llvm.sqrt.f32(float %a)
  ret float %ret
}

define double @sqrt_d(double %a) {
; CHECK-LABEL: sqrt_d:
; RVF:       call	sqrt
; RVD:       tail	sqrt
  %ret = call double @llvm.sqrt.f64(double %a)
  ret double %ret
}

declare float @llvm.sqrt.f32(float %a)
declare double @llvm.sqrt.f64(double %a)
