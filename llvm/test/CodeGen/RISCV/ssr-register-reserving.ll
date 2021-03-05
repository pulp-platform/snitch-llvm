; RUN: llc -O0 --ssr-noregmerge %s -o - -verify-machineinstrs | FileCheck %s --check-prefix=CHECK-SSR0
; RUN: llc -O1 --ssr-noregmerge %s -o - -verify-machineinstrs | FileCheck %s --check-prefix=CHECK-SSR0
; RUN: llc -O2 --ssr-noregmerge %s -o - -verify-machineinstrs | FileCheck %s --check-prefix=CHECK-SSR0
; RUN: llc -O3 --ssr-noregmerge %s -o - -verify-machineinstrs | FileCheck %s --check-prefix=CHECK-SSR0

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown-elf"

@ssr_region.data = internal global [8 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8], align 4
@ssr_region.d = internal global double 4.200000e+01, align 8

; Function Attrs: noinline nounwind optnone
define dso_local void @ssr_region0() #0 {
entry:
  %e = alloca double, align 8
  call void @llvm.riscv.ssr.enable()
  %0 = load volatile double, double* @ssr_region.d, align 8
  ; ft0 and ft1 are reserved, make sure it allocates ft3 and uses ft0 for streaming
  ; CHECK-SSR0: fmv.d  ft0, ft3
  call void @llvm.riscv.ssr.push(i32 0, double %0)
  ; CHECK-SSR0: fmv.d  ft3, ft0
  %1 = call double @llvm.riscv.ssr.pop(i32 0)
  store volatile double %1, double* %e, align 8
  call void @llvm.riscv.ssr.disable()
  ret void
}
; Function Attrs: noinline nounwind optnone
define dso_local void @ssr_region1() #0 {
entry:
  %e = alloca double, align 8
  call void @llvm.riscv.ssr.enable()
  %0 = load volatile double, double* @ssr_region.d, align 8
  ; ft0 and ft1 are reserved, make sure it allocates ft3 and uses ft0 for streaming
  ; CHECK-SSR0: fmv.d  ft1, ft3
  call void @llvm.riscv.ssr.push(i32 1, double %0)
  ; CHECK-SSR0: fmv.d  ft3, ft1
  %1 = call double @llvm.riscv.ssr.pop(i32 1)
  store volatile double %1, double* %e, align 8
  call void @llvm.riscv.ssr.disable()
  ret void
}

; Function Attrs: nounwind
declare void @llvm.riscv.ssr.enable() #1

; Function Attrs: nounwind
declare void @llvm.riscv.ssr.push(i32 immarg, double) #1

; Function Attrs: nounwind
declare double @llvm.riscv.ssr.pop(i32 immarg) #1

; Function Attrs: nounwind
declare void @llvm.riscv.ssr.disable() #1

; Function Attrs: noinline nounwind optnone
define dso_local i32 @main() #0 {
entry:
  %a = alloca double, align 8
  call void @ssr_region0()
  call void @ssr_region1()
  store volatile double 0.000000e+00, double* %a, align 8
  %0 = load volatile double, double* %a, align 8
  ; Here, outside the SSR region, ft0 and ft1 use are allowed
  ; CHECK-SSR0: fadd.d ft0, ft0, ft1
  %add = fadd double %0, 3.000000e+00
  store volatile double %add, double* %a, align 8
  ret i32 0
}

attributes #0 = { noinline nounwind optnone "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="snitch" "target-features"="+a,+d,+f,+m,+relax,+xdma,+xfrep,+xssr,-64bit,-save-restore" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
