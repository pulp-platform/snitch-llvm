; RUN: opt -S -passes=tree-height-reduction -enable-fp-thr < %s | FileCheck %s

; CHECK-LABEL: @sub_half(
; CHECK: %[[V0:.*]] = fsub reassoc nsz half {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = fsub reassoc nsz half %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = fsub reassoc nsz half %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = fsub reassoc nsz half %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = fsub reassoc nsz half %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = fsub reassoc nsz half %[[V4]], {{.*}}
; CHECK: fsub reassoc nsz half %[[V5]], {{.*}}
define void @sub_half(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %2 = fsub reassoc nsz half %0, %1
  %arrayidx.3 = getelementptr inbounds half, ptr %D, i64 %indvars.iv
  %3 = load half, ptr %arrayidx.3, align 4
  %4 = fsub reassoc nsz half %2, %3
  %arrayidx.4 = getelementptr inbounds half, ptr %E, i64 %indvars.iv
  %5 = load half, ptr %arrayidx.4, align 4
  %6 = fsub reassoc nsz half %4, %5
  %arrayidx.5 = getelementptr inbounds half, ptr %F, i64 %indvars.iv
  %7 = load half, ptr %arrayidx.5, align 4
  %8 = fsub reassoc nsz half %6, %7
  %arrayidx.6 = getelementptr inbounds half, ptr %G, i64 %indvars.iv
  %9 = load half, ptr %arrayidx.6, align 4
  %10 = fsub reassoc nsz half %8, %9
  %arrayidx.7 = getelementptr inbounds half, ptr %H, i64 %indvars.iv
  %11 = load half, ptr %arrayidx.7, align 4
  %12 = fsub reassoc nsz half %10, %11
  %arrayidx.8 = getelementptr inbounds half, ptr %I, i64 %indvars.iv
  %13 = load half, ptr %arrayidx.8, align 4
  %14 = fsub reassoc nsz half %12, %13
  %arrayidx.9 = getelementptr inbounds half, ptr %A, i64 %indvars.iv
  store half %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @sub_double(
; CHECK: %[[V0:.*]] = fsub reassoc nsz double {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = fsub reassoc nsz double %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = fsub reassoc nsz double %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = fsub reassoc nsz double %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = fsub reassoc nsz double %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = fsub reassoc nsz double %[[V4]], {{.*}}
; CHECK: fsub reassoc nsz double %[[V5]], {{.*}}
define void @sub_double(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %2 = fsub reassoc nsz double %0, %1
  %arrayidx.3 = getelementptr inbounds double, ptr %D, i64 %indvars.iv
  %3 = load double, ptr %arrayidx.3, align 4
  %4 = fsub reassoc nsz double %2, %3
  %arrayidx.4 = getelementptr inbounds double, ptr %E, i64 %indvars.iv
  %5 = load double, ptr %arrayidx.4, align 4
  %6 = fsub reassoc nsz double %4, %5
  %arrayidx.5 = getelementptr inbounds double, ptr %F, i64 %indvars.iv
  %7 = load double, ptr %arrayidx.5, align 4
  %8 = fsub reassoc nsz double %6, %7
  %arrayidx.6 = getelementptr inbounds double, ptr %G, i64 %indvars.iv
  %9 = load double, ptr %arrayidx.6, align 4
  %10 = fsub reassoc nsz double %8, %9
  %arrayidx.7 = getelementptr inbounds double, ptr %H, i64 %indvars.iv
  %11 = load double, ptr %arrayidx.7, align 4
  %12 = fsub reassoc nsz double %10, %11
  %arrayidx.8 = getelementptr inbounds double, ptr %I, i64 %indvars.iv
  %13 = load double, ptr %arrayidx.8, align 4
  %14 = fsub reassoc nsz double %12, %13
  %arrayidx.9 = getelementptr inbounds double, ptr %A, i64 %indvars.iv
  store double %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
