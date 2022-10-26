; RUN: opt -S -passes=tree-height-reduction -enable-fp-thr < %s | FileCheck %s

; CHECK-LABEL: @add_half_with_constant(
; CHECK: %[[V0:.*]] = fadd reassoc nsz half {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = fadd reassoc nsz half {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = fadd reassoc nsz half {{.*}}, 0xH4900
; CHECK-NEXT: %[[V3:.*]] = fadd reassoc nsz half {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = fadd reassoc nsz half {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = fadd reassoc nsz half %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = fadd reassoc nsz half %[[V3]], %[[V4]]
; CHECK-NEXT: fadd reassoc nsz half %[[V5]], %[[V6]]
define void @add_half_with_constant(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds half, ptr %B, i64 %indvars.iv
  %0 = load half, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds half, ptr %C, i64 %indvars.iv
  %1 = load half, ptr %arrayidx.2, align 4
  %arrayidx.3 = getelementptr inbounds half, ptr %D, i64 %indvars.iv
  %2 = load half, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds half, ptr %E, i64 %indvars.iv
  %3 = load half, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds half, ptr %F, i64 %indvars.iv
  %4 = load half, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds half, ptr %G, i64 %indvars.iv
  %5 = load half, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds half, ptr %H, i64 %indvars.iv
  %6 = load half, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds half, ptr %I, i64 %indvars.iv
  %7 = load half, ptr %arrayidx.8, align 4
  %8 = fadd reassoc nsz half %0, 0xH4900
  %9 = fadd reassoc nsz half %8, %1
  %10 = fadd reassoc nsz half %9, %2
  %11 = fadd reassoc nsz half %10, %3
  %12 = fadd reassoc nsz half %11, %4
  %13 = fadd reassoc nsz half %12, %5
  %14 = fadd reassoc nsz half %13, %6
  %15 = fadd reassoc nsz half %14, %7
  %arrayidx.9 = getelementptr inbounds half, ptr %A, i64 %indvars.iv
  store half %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
