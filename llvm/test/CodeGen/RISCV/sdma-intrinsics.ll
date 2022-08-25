; RUN: llc -O0 %s -o - -stop-after=riscv-expand-sdma | FileCheck %s
; RUN: llc -O1 %s -o - -stop-after=riscv-expand-sdma | FileCheck %s
; RUN: llc -O2 %s -o - -stop-after=riscv-expand-sdma | FileCheck %s
; RUN: llc -O3 %s -o - -stop-after=riscv-expand-sdma | FileCheck %s

; ModuleID = 'src/sdma_simple.c'
source_filename = "src/sdma_simple.c"
target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown-elf"

; Function Attrs: noinline nounwind optnone
define dso_local i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %src = alloca i64, align 8
  %dst = alloca i64, align 8
  %stat = alloca i64, align 8
  %size = alloca i32, align 4
  %cfg = alloca i32, align 4
  %tid = alloca i32, align 4
  %sstride = alloca i32, align 4
  %dstride = alloca i32, align 4
  %nreps = alloca i32, align 4
  %errs = alloca i32, align 4
  %0 = load i64, i64* %src, align 8
  %1 = load i64, i64* %dst, align 8
  %2 = load i32, i32* %size, align 4
  %3 = load i32, i32* %cfg, align 4
  ; CHECK: %14:gpr = ANDI %5, -3
  ; CHECK-NEXT: DMSRC %1, %0
  ; CHECK-NEXT: DMDST %3, %2
  ; CHECK-NEXT: %6:gpr = DMCPY %4, killed %14
  %4 = call i32 @llvm.riscv.sdma.start.oned(i64 %0, i64 %1, i32 %2, i32 %3)
  ; CHECK: %15:gpr = DMSTATI 2
  ; CHECK-NEXT: BNE killed %15, $x0, %bb.1
  call void @llvm.riscv.sdma.wait.for.idle()
  %5 = load i32, i32* %sstride, align 4
  %6 = load i32, i32* %dstride, align 4
  %7 = load i32, i32* %nreps, align 4
  ; CHECK: %16:gpr = ORI %5, 2
  ; CHECK-NEXT: DMSRC %1, %0
  ; CHECK-NEXT: DMDST %3, %2
  ; CHECK-NEXT: DMSTR %7, %8
  ; CHECK-NEXT: DMREP %9
  ; CHECK-NEXT: %10:gpr = DMCPY %4, killed %16
  %8 = call i32 @llvm.riscv.sdma.start.twod(i64 %0, i64 %1, i32 %2, i32 %5, i32 %6, i32 %7, i32 %3)
  %9 = load i32, i32* %tid, align 4
  ; CHECK: %12:gpr = DMSTAT %11
  %10 = call i32 @llvm.riscv.sdma.stat(i32 %9)
  %conv = zext i32 %10 to i64
  %11 = load i32, i32* %errs, align 4
  ret i32 %11
}

; Function Attrs: nounwind
declare i32 @llvm.riscv.sdma.start.oned(i64, i64, i32, i32) #1

; Function Attrs: nounwind
declare void @llvm.riscv.sdma.wait.for.idle() #1

; Function Attrs: nounwind
declare i32 @llvm.riscv.sdma.start.twod(i64, i64, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind
declare i32 @llvm.riscv.sdma.stat(i32) #1

attributes #0 = { noinline nounwind optnone "disable-tail-calls"="false" "frame-pointer"="all" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="snitch" "target-features"="+a,+d,+f,+m,+relax,+xdma,+xfrep,+xssr,-64bit,-save-restore" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"target-abi", !"ilp32d"}
!2 = !{i32 1, !"SmallDataLimit", i32 8}
!3 = !{!"clang version 12.0.0 (git@iis-git.ee.ethz.ch:huettern/snitch-llvm.git 96781c8a855d5f9df55b5bef3c614bf7f12ee42c)"}
