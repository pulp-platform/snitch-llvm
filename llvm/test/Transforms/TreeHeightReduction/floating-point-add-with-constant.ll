; RUN: opt -S -passes=tree-height-reduction -enable-fp-thr < %s | FileCheck %s

; CHECK-LABEL: @add_float_with_constant(
; CHECK: %[[V0:.*]] = fadd reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = fadd reassoc nsz float {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = fadd reassoc nsz float {{.*}}, 1.000000e+01
; CHECK-NEXT: %[[V3:.*]] = fadd reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = fadd reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = fadd reassoc nsz float %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = fadd reassoc nsz float %[[V3]], %[[V4]]
; CHECK-NEXT: fadd reassoc nsz float %[[V5]], %[[V6]]
define void @add_float_with_constant(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds float, ptr %B, i64 %indvars.iv
  %0 = load float, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds float, ptr %C, i64 %indvars.iv
  %1 = load float, ptr %arrayidx.2, align 4
  %arrayidx.3 = getelementptr inbounds float, ptr %D, i64 %indvars.iv
  %2 = load float, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds float, ptr %E, i64 %indvars.iv
  %3 = load float, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds float, ptr %F, i64 %indvars.iv
  %4 = load float, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds float, ptr %G, i64 %indvars.iv
  %5 = load float, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds float, ptr %H, i64 %indvars.iv
  %6 = load float, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds float, ptr %I, i64 %indvars.iv
  %7 = load float, ptr %arrayidx.8, align 4
  %8 = fadd reassoc nsz float %0, 1.000000e+01
  %9 = fadd reassoc nsz float %8, %1
  %10 = fadd reassoc nsz float %9, %2
  %11 = fadd reassoc nsz float %10, %3
  %12 = fadd reassoc nsz float %11, %4
  %13 = fadd reassoc nsz float %12, %5
  %14 = fadd reassoc nsz float %13, %6
  %15 = fadd reassoc nsz float %14, %7
  %arrayidx.9 = getelementptr inbounds float, ptr %A, i64 %indvars.iv
  store float %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @add_double_with_constant(
; CHECK: %[[V0:.*]] = fadd reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = fadd reassoc nsz double {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = fadd reassoc nsz double {{.*}}, 1.000000e+01
; CHECK-NEXT: %[[V3:.*]] = fadd reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = fadd reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = fadd reassoc nsz double %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = fadd reassoc nsz double %[[V3]], %[[V4]]
; CHECK-NEXT: fadd reassoc nsz double %[[V5]], %[[V6]]
define void @add_double_with_constant(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds double, ptr %B, i64 %indvars.iv
  %0 = load double, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds double, ptr %C, i64 %indvars.iv
  %1 = load double, ptr %arrayidx.2, align 4
  %arrayidx.3 = getelementptr inbounds double, ptr %D, i64 %indvars.iv
  %2 = load double, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds double, ptr %E, i64 %indvars.iv
  %3 = load double, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds double, ptr %F, i64 %indvars.iv
  %4 = load double, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds double, ptr %G, i64 %indvars.iv
  %5 = load double, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds double, ptr %H, i64 %indvars.iv
  %6 = load double, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds double, ptr %I, i64 %indvars.iv
  %7 = load double, ptr %arrayidx.8, align 4
  %8 = fadd reassoc nsz double %0, 1.000000e+01
  %9 = fadd reassoc nsz double %8, %1
  %10 = fadd reassoc nsz double %9, %2
  %11 = fadd reassoc nsz double %10, %3
  %12 = fadd reassoc nsz double %11, %4
  %13 = fadd reassoc nsz double %12, %5
  %14 = fadd reassoc nsz double %13, %6
  %15 = fadd reassoc nsz double %14, %7
  %arrayidx.9 = getelementptr inbounds double, ptr %A, i64 %indvars.iv
  store double %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
