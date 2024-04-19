; RUN: llc -mtriple=riscv32 -mattr=+f -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=riscv32 -mattr=+d -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=riscv64 -mattr=+f -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=riscv64 -mattr=+d -mattr=+nofdiv -verify-machineinstrs < %s | FileCheck %s

define float @fdiv_s(float %a, float %b) {
; CHECK-LABEL: fdiv_s:
; CHECK:       call	__divsf3@plt
  %ret = fdiv float %a, %b
  ret float %ret
}

define double @fdiv_d(double %a, double %b) {
; CHECK-LABEL: fdiv_d:
; CHECK:       call	__divdf3@plt
  %ret = fdiv double %a, %b
  ret double %ret
}

define float @sqrt_s(float %a) {
; CHECK-LABEL: sqrt_s:
; CHECK:       call	sqrtf@plt
  %ret = call float @llvm.sqrt.f32(float %a)
  ret float %ret
}

define double @sqrt_d(double %a) {
; CHECK-LABEL: sqrt_d:
; CHECK:       call	sqrt@plt
  %ret = call double @llvm.sqrt.f64(double %a)
  ret double %ret
}

declare float @llvm.sqrt.f32(float %a)
declare double @llvm.sqrt.f64(double %a)
