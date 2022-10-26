; RUN: opt -S -passes=tree-height-reduction -enable-int-thr < %s | FileCheck %s

; CHECK-LABEL: @leaf_num_is_3(
; CHECK: %[[V0:.*]] = add nsw i32 {{.*}}, {{.*}}
; CHECK: add nsw i32 %[[V0]], {{.*}}
define void @leaf_num_is_3(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds i32, ptr %B, i64 %indvars.iv
  %0 = load i32, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds i32, ptr %C, i64 %indvars.iv
  %1 = load i32, ptr %arrayidx.2, align 4
  %2 = add nsw i32 %1, %0
  %arrayidx.3 = getelementptr inbounds i32, ptr %D, i64 %indvars.iv
  %3 = load i32, ptr %arrayidx.3, align 4
  %4 = add nsw i32 %2, %3
  %arrayidx.4 = getelementptr inbounds i32, ptr %A, i64 %indvars.iv
  store i32 %4, ptr %arrayidx.1, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @leaf_num_is_4(
; CHECK: %[[V0:.*]] = add nsw i32 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = add nsw i32 {{.*}}, {{.*}}
; CHECK-NEXT: add nsw i32 %[[V0]], %[[V1]]
define void @leaf_num_is_4(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds i32, ptr %B, i64 %indvars.iv
  %0 = load i32, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds i32, ptr %C, i64 %indvars.iv
  %1 = load i32, ptr %arrayidx.2, align 4
  %2 = add nsw i32 %1, %0
  %arrayidx.3 = getelementptr inbounds i32, ptr %D, i64 %indvars.iv
  %3 = load i32, ptr %arrayidx.3, align 4
  %4 = add nsw i32 %2, %3
  %arrayidx.4 = getelementptr inbounds i32, ptr %E, i64 %indvars.iv
  %5 = load i32, ptr %arrayidx.4, align 4
  %6 = add nsw i32 %4, %5
  %arrayidx.5 = getelementptr inbounds i32, ptr %A, i64 %indvars.iv
  store i32 %6, ptr %arrayidx.5, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
