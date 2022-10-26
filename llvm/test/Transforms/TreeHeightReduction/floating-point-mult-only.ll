; RUN: opt -S -passes=tree-height-reduction -enable-fp-thr < %s | FileCheck %s

; CHECK-LABEL: @add_float(
; CHECK: %[[V0:.*]] = fmul reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = fmul reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V2:.*]] = fmul reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V3:.*]] = fmul reassoc nsz float {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = fmul reassoc nsz float %[[V0]], %[[V1]]
; CHECK-NEXT: %[[V5:.*]] = fmul reassoc nsz float %[[V2]], %[[V3]]
; CHECK-NEXT: fmul reassoc nsz float %[[V4]], %[[V5]]
define void @add_float(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %2 = fmul reassoc nsz float %1, %0
  %arrayidx.3 = getelementptr inbounds float, ptr %D, i64 %indvars.iv
  %3 = load float, ptr %arrayidx.3, align 4
  %4 = fmul reassoc nsz float %2, %3
  %arrayidx.4 = getelementptr inbounds float, ptr %E, i64 %indvars.iv
  %5 = load float, ptr %arrayidx.4, align 4
  %6 = fmul reassoc nsz float %4, %5
  %arrayidx.5 = getelementptr inbounds float, ptr %F, i64 %indvars.iv
  %7 = load float, ptr %arrayidx.5, align 4
  %8 = fmul reassoc nsz float %6, %7
  %arrayidx.6 = getelementptr inbounds float, ptr %G, i64 %indvars.iv
  %9 = load float, ptr %arrayidx.6, align 4
  %10 = fmul reassoc nsz float %8, %9
  %arrayidx.7 = getelementptr inbounds float, ptr %H, i64 %indvars.iv
  %11 = load float, ptr %arrayidx.7, align 4
  %12 = fmul reassoc nsz float %10, %11
  %arrayidx.8 = getelementptr inbounds float, ptr %I, i64 %indvars.iv
  %13 = load float, ptr %arrayidx.8, align 4
  %14 = fmul reassoc nsz float %12, %13
  %arrayidx.9 = getelementptr inbounds float, ptr %A, i64 %indvars.iv
  store float %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @add_double(
; CHECK: %[[V0:.*]] = fmul reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = fmul reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V2:.*]] = fmul reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V3:.*]] = fmul reassoc nsz double {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = fmul reassoc nsz double %[[V0]], %[[V1]]
; CHECK-NEXT: %[[V5:.*]] = fmul reassoc nsz double %[[V2]], %[[V3]]
; CHECK-NEXT: fmul reassoc nsz double %[[V4]], %[[V5]]
define void @add_double(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %2 = fmul reassoc nsz double %1, %0
  %arrayidx.3 = getelementptr inbounds double, ptr %D, i64 %indvars.iv
  %3 = load double, ptr %arrayidx.3, align 4
  %4 = fmul reassoc nsz double %2, %3
  %arrayidx.4 = getelementptr inbounds double, ptr %E, i64 %indvars.iv
  %5 = load double, ptr %arrayidx.4, align 4
  %6 = fmul reassoc nsz double %4, %5
  %arrayidx.5 = getelementptr inbounds double, ptr %F, i64 %indvars.iv
  %7 = load double, ptr %arrayidx.5, align 4
  %8 = fmul reassoc nsz double %6, %7
  %arrayidx.6 = getelementptr inbounds double, ptr %G, i64 %indvars.iv
  %9 = load double, ptr %arrayidx.6, align 4
  %10 = fmul reassoc nsz double %8, %9
  %arrayidx.7 = getelementptr inbounds double, ptr %H, i64 %indvars.iv
  %11 = load double, ptr %arrayidx.7, align 4
  %12 = fmul reassoc nsz double %10, %11
  %arrayidx.8 = getelementptr inbounds double, ptr %I, i64 %indvars.iv
  %13 = load double, ptr %arrayidx.8, align 4
  %14 = fmul reassoc nsz double %12, %13
  %arrayidx.9 = getelementptr inbounds double, ptr %A, i64 %indvars.iv
  store double %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
