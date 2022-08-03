; RUN: llc -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O1 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O2 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; RUN: llc -O3 -mcpu=snitch -snitch-frep-inference < %s | FileCheck %s
; Check that we generate hardware loop instructions.

target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-unknown-elf"

; Case 1 : Loop with a constant number of iterations
; CHECK-LABEL: @hwloop1
; CHECK: li [[REGISTER:[a-zA-Z0-9_]*]], 127
; CHECK-NEXT: frep.o [[REGISTER]], 2, 0, 0
; CHECK-NEXT: fmul.d {{.*}}
; CHECK-NEXT: fadd.d {{.*}}

define i32 @hwloop1() nounwind {
entry:
  %a = alloca [128 x double], align 8
  call void @llvm.riscv.ssr.enable()
  br label %for.cond

for.cond:                                         ; preds = %for.inc, %entry
  %sum.0 = phi double [ 0.000000e+00, %entry ], [ %add, %for.inc ]
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.inc ]
  %cmp = icmp ult i32 %i.0, 128
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %0 = call double @llvm.riscv.ssr.pop(i32 0)
  %1 = call double @llvm.riscv.ssr.pop(i32 1)
  %mul = fmul double %0, %1
  %add = fadd double %sum.0, %mul
  br label %for.inc

for.inc:                                          ; preds = %for.body
  %inc = add i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  call void @llvm.riscv.ssr.disable()
  %arrayidx = getelementptr inbounds [128 x double], [128 x double]* %a, i32 0, i32 0
  store volatile double %sum.0, double* %arrayidx, align 8
  ret i32 0
}

; Case 2 : Loop with a constant number of iterations and rotated form
; CHECK-LABEL: @hwloop2
; CHECK: li [[REGISTER:[a-zA-Z0-9_]*]], 127
; CHECK-NEXT: frep.o [[REGISTER]], 1, 0, 0
; CHECK-NEXT: fmadd.d {{.*}}

define i32 @hwloop2() nounwind {
entry:
  %a.sroa.0 = alloca double, align 8
  call void @llvm.riscv.ssr.enable()
  br label %for.body

for.cond.cleanup:                                 ; preds = %for.body
  call void @llvm.riscv.ssr.disable()
  store volatile double %add, double* %a.sroa.0, align 8
  ret i32 0

for.body:                                         ; preds = %for.body, %entry
  %i.013 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %sum.012 = phi double [ 0.000000e+00, %entry ], [ %add, %for.body ]
  %0 = call fast double @llvm.riscv.ssr.pop(i32 0)
  %1 = call fast double @llvm.riscv.ssr.pop(i32 1)
  %mul = fmul fast double %1, %0
  %add = fadd fast double %mul, %sum.012
  %inc = add nuw nsw i32 %i.013, 1
  %exitcond.not = icmp eq i32 %inc, 128
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}



; Function Attrs: nounwind
declare void @llvm.riscv.ssr.enable() #1
declare double @llvm.riscv.ssr.pop(i32 immarg) #1
declare void @llvm.riscv.ssr.disable() #1
