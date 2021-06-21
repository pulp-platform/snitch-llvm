; RUN: llc -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O1 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O2 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O3 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; Check that we generate hardware loop instructions.

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown-elf"

; Case 1: Nested loop, inner loop with FREP, outer loop not unrolled

; CHECK:        csrrsi  {{.*}}, 1984, 1
; CHECK:        addi  [[rBound:[a-zA-Z0-9_]*]], zero, 127
; CHECK-NEXT:   frep.o  [[rBound]], 2, 0, 0
; CHECK-NEXT:   fmul.d  {{.*}}
; CHECK-NEXT:   fadd.d  {{.*}}
; CHECK-NEXT:   fmv.x.w {{.*}}, fa0
; CHECK-NEXT:   addi  [[rInd:[a-zA-Z0-9_]*]], [[rInd]], 1
; CHECK-NEXT:   csrrci  {{.*}}, 1984, 1
; CHECK-NEXT:   bne [[rInd]], a1, .LBB0_1

; Function Attrs: nounwind
define dso_local i32 @main() local_unnamed_addr #0 {
entry:
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.cond.cleanup3
  %conv = fptosi double %add to i32
  ret i32 %conv

for.body:                                         ; preds = %entry, %for.cond.cleanup3
  %i.017 = phi i32 [ 0, %entry ], [ %inc6, %for.cond.cleanup3 ]
  %sum.016 = phi double [ undef, %entry ], [ %add, %for.cond.cleanup3 ]
  call void @llvm.riscv.ssr.enable()
  call void @llvm.riscv.frep.infer()
  br label %for.body4

for.cond.cleanup3:                                ; preds = %for.body4
  call void @llvm.riscv.ssr.disable()
  %inc6 = add nuw nsw i32 %i.017, 1
  %exitcond18.not = icmp eq i32 %inc6, 4
  br i1 %exitcond18.not, label %for.cond.cleanup, label %for.body

for.body4:                                        ; preds = %for.body, %for.body4
  %j.015 = phi i32 [ 0, %for.body ], [ %inc, %for.body4 ]
  %sum.114 = phi double [ %sum.016, %for.body ], [ %add, %for.body4 ]
  %0 = call double @llvm.riscv.ssr.pop(i32 0) #1
  %1 = call double @llvm.riscv.ssr.pop(i32 1) #1
  %mul = fmul double %0, %1
  %add = fadd double %sum.114, %mul
  %inc = add nuw nsw i32 %j.015, 1
  %exitcond.not = icmp eq i32 %inc, 128
  br i1 %exitcond.not, label %for.cond.cleanup3, label %for.body4
}

; Case 2: Nested loop, inner loop with FREP, outer loop unrolled 2

; CHECK:        csrrsi  {{.*}}, 1984, 1
; CHECK:        addi  [[rBound:[a-zA-Z0-9_]*]], zero, 127
; CHECK-NEXT:   frep.o  [[rBound]], 2, 0, 0
; CHECK-NEXT:   fmul.d  {{.*}}
; CHECK-NEXT:   fadd.d  {{.*}}
; CHECK-NEXT:   fmv.x.w {{.*}}, fa0
; CHECK-NEXT:   csrrci  {{.*}}, 1984, 1
; CHECK-NEXT:   csrrsi  {{.*}}, 1984, 1
; CHECK:        addi  [[rBound:[a-zA-Z0-9_]*]], zero, 127
; CHECK:        frep.o  [[rBound]], 2, 0, 0
; CHECK-NEXT:   fmul.d  {{.*}}
; CHECK-NEXT:   fadd.d  {{.*}}
; CHECK:        fmv.x.w {{.*}}, fa0

; Function Attrs: nounwind
define dso_local i32 @main1() local_unnamed_addr #0 {
entry:
  call void @llvm.riscv.ssr.enable()
  call void @llvm.riscv.frep.infer()
  br label %for.body4

for.cond.cleanup3:                                ; preds = %for.body4
  call void @llvm.riscv.ssr.disable()
  call void @llvm.riscv.ssr.enable()
  call void @llvm.riscv.frep.infer()
  br label %for.body4.1

for.body4:                                        ; preds = %entry, %for.body4
  %j.015 = phi i32 [ 0, %entry ], [ %inc, %for.body4 ]
  %sum.114 = phi double [ undef, %entry ], [ %add, %for.body4 ]
  %0 = call double @llvm.riscv.ssr.pop(i32 0) #1
  %1 = call double @llvm.riscv.ssr.pop(i32 1) #1
  %mul = fmul double %0, %1
  %add = fadd double %sum.114, %mul
  %inc = add nuw nsw i32 %j.015, 1
  %exitcond.not = icmp eq i32 %inc, 128
  br i1 %exitcond.not, label %for.cond.cleanup3, label %for.body4

for.body4.1:                                      ; preds = %for.body4.1, %for.cond.cleanup3
  %j.015.1 = phi i32 [ 0, %for.cond.cleanup3 ], [ %inc.1, %for.body4.1 ]
  %sum.114.1 = phi double [ %add, %for.cond.cleanup3 ], [ %add.1, %for.body4.1 ]
  %2 = call double @llvm.riscv.ssr.pop(i32 0) #1
  %3 = call double @llvm.riscv.ssr.pop(i32 1) #1
  %mul.1 = fmul double %2, %3
  %add.1 = fadd double %sum.114.1, %mul.1
  %inc.1 = add nuw nsw i32 %j.015.1, 1
  %exitcond.1.not = icmp eq i32 %inc.1, 128
  br i1 %exitcond.1.not, label %for.cond.cleanup3.1, label %for.body4.1

for.cond.cleanup3.1:                              ; preds = %for.body4.1
  call void @llvm.riscv.ssr.disable()
  %conv = fptosi double %add.1 to i32
  ret i32 %conv
}


; Function Attrs: nounwind
declare void @llvm.riscv.ssr.enable() #1
; Function Attrs: nounwind
declare void @llvm.riscv.frep.infer() #1
declare double @llvm.riscv.ssr.pop(i32 immarg)
; Function Attrs: nounwind
declare void @llvm.riscv.ssr.disable() #1


attributes #0 = { nounwind "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="snitch" "target-features"="+a,+d,+f,+m,+relax,+xdma,+xfrep,+xssr,-64bit,-save-restore" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind }
