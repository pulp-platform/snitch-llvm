; RUN: opt -S -passes=tree-height-reduction -enable-int-thr < %s | FileCheck %s

; CHECK-LABEL: @sub_i8(
; CHECK: %[[V0:.*]] = sub i8 {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = sub i8 %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = sub i8 %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = sub i8 %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = sub i8 %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = sub i8 %[[V4]], {{.*}}
; CHECK: sub i8 %[[V5]], {{.*}}
define void @sub_i8(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds i8, ptr %B, i64 %indvars.iv
  %0 = load i8, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds i8, ptr %C, i64 %indvars.iv
  %1 = load i8, ptr %arrayidx.2, align 4
  %2 = sub i8 %0, %1
  %arrayidx.3 = getelementptr inbounds i8, ptr %D, i64 %indvars.iv
  %3 = load i8, ptr %arrayidx.3, align 4
  %4 = sub i8 %2, %3
  %arrayidx.4 = getelementptr inbounds i8, ptr %E, i64 %indvars.iv
  %5 = load i8, ptr %arrayidx.4, align 4
  %6 = sub i8 %4, %5
  %arrayidx.5 = getelementptr inbounds i8, ptr %F, i64 %indvars.iv
  %7 = load i8, ptr %arrayidx.5, align 4
  %8 = sub i8 %6, %7
  %arrayidx.6 = getelementptr inbounds i8, ptr %G, i64 %indvars.iv
  %9 = load i8, ptr %arrayidx.6, align 4
  %10 = sub i8 %8, %9
  %arrayidx.7 = getelementptr inbounds i8, ptr %H, i64 %indvars.iv
  %11 = load i8, ptr %arrayidx.7, align 4
  %12 = sub i8 %10, %11
  %arrayidx.8 = getelementptr inbounds i8, ptr %I, i64 %indvars.iv
  %13 = load i8, ptr %arrayidx.8, align 4
  %14 = sub i8 %12, %13
  %arrayidx.9 = getelementptr inbounds i8, ptr %A, i64 %indvars.iv
  store i8 %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @sub_i16(
; CHECK: %[[V0:.*]] = sub i16 {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = sub i16 %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = sub i16 %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = sub i16 %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = sub i16 %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = sub i16 %[[V4]], {{.*}}
; CHECK: sub i16 %[[V5]], {{.*}}
define void @sub_i16(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds i16, ptr %B, i64 %indvars.iv
  %0 = load i16, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds i16, ptr %C, i64 %indvars.iv
  %1 = load i16, ptr %arrayidx.2, align 4
  %2 = sub i16 %0, %1
  %arrayidx.3 = getelementptr inbounds i16, ptr %D, i64 %indvars.iv
  %3 = load i16, ptr %arrayidx.3, align 4
  %4 = sub i16 %2, %3
  %arrayidx.4 = getelementptr inbounds i16, ptr %E, i64 %indvars.iv
  %5 = load i16, ptr %arrayidx.4, align 4
  %6 = sub i16 %4, %5
  %arrayidx.5 = getelementptr inbounds i16, ptr %F, i64 %indvars.iv
  %7 = load i16, ptr %arrayidx.5, align 4
  %8 = sub i16 %6, %7
  %arrayidx.6 = getelementptr inbounds i16, ptr %G, i64 %indvars.iv
  %9 = load i16, ptr %arrayidx.6, align 4
  %10 = sub i16 %8, %9
  %arrayidx.7 = getelementptr inbounds i16, ptr %H, i64 %indvars.iv
  %11 = load i16, ptr %arrayidx.7, align 4
  %12 = sub i16 %10, %11
  %arrayidx.8 = getelementptr inbounds i16, ptr %I, i64 %indvars.iv
  %13 = load i16, ptr %arrayidx.8, align 4
  %14 = sub i16 %12, %13
  %arrayidx.9 = getelementptr inbounds i16, ptr %A, i64 %indvars.iv
  store i16 %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @sub_i32(
; CHECK: %[[V0:.*]] = sub i32 {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = sub i32 %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = sub i32 %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = sub i32 %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = sub i32 %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = sub i32 %[[V4]], {{.*}}
; CHECK: sub i32 %[[V5]], {{.*}}
define void @sub_i32(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %2 = sub i32 %0, %1
  %arrayidx.3 = getelementptr inbounds i32, ptr %D, i64 %indvars.iv
  %3 = load i32, ptr %arrayidx.3, align 4
  %4 = sub i32 %2, %3
  %arrayidx.4 = getelementptr inbounds i32, ptr %E, i64 %indvars.iv
  %5 = load i32, ptr %arrayidx.4, align 4
  %6 = sub i32 %4, %5
  %arrayidx.5 = getelementptr inbounds i32, ptr %F, i64 %indvars.iv
  %7 = load i32, ptr %arrayidx.5, align 4
  %8 = sub i32 %6, %7
  %arrayidx.6 = getelementptr inbounds i32, ptr %G, i64 %indvars.iv
  %9 = load i32, ptr %arrayidx.6, align 4
  %10 = sub i32 %8, %9
  %arrayidx.7 = getelementptr inbounds i32, ptr %H, i64 %indvars.iv
  %11 = load i32, ptr %arrayidx.7, align 4
  %12 = sub i32 %10, %11
  %arrayidx.8 = getelementptr inbounds i32, ptr %I, i64 %indvars.iv
  %13 = load i32, ptr %arrayidx.8, align 4
  %14 = sub i32 %12, %13
  %arrayidx.9 = getelementptr inbounds i32, ptr %A, i64 %indvars.iv
  store i32 %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @sub_i64(
; CHECK: %[[V0:.*]] = sub i64 {{.*}}, {{.*}}
; CHECK: %[[V1:.*]] = sub i64 %[[V0]], %{{.*}}
; CHECK: %[[V2:.*]] = sub i64 %[[V1]], {{.*}}
; CHECK: %[[V3:.*]] = sub i64 %[[V2]], {{.*}}
; CHECK: %[[V4:.*]] = sub i64 %[[V3]], {{.*}}
; CHECK: %[[V5:.*]] = sub i64 %[[V4]], {{.*}}
; CHECK: sub i64 %[[V5]], {{.*}}
define void @sub_i64(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
entry:
  %cmp.1 = icmp sgt i32 %N, 0
  br i1 %cmp.1, label %preh, label %for.end

preh:                                             ; preds = %entry
  %zext = zext i32 %N to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %preh
  %indvars.iv = phi i64 [ 0, %preh ], [ %indvars.iv.next, %for.body ]
  %arrayidx.1 = getelementptr inbounds i64, ptr %B, i64 %indvars.iv
  %0 = load i64, ptr %arrayidx.1, align 4
  %arrayidx.2 = getelementptr inbounds i64, ptr %C, i64 %indvars.iv
  %1 = load i64, ptr %arrayidx.2, align 4
  %2 = sub i64 %0, %1
  %arrayidx.3 = getelementptr inbounds i64, ptr %D, i64 %indvars.iv
  %3 = load i64, ptr %arrayidx.3, align 4
  %4 = sub i64 %2, %3
  %arrayidx.4 = getelementptr inbounds i64, ptr %E, i64 %indvars.iv
  %5 = load i64, ptr %arrayidx.4, align 4
  %6 = sub i64 %4, %5
  %arrayidx.5 = getelementptr inbounds i64, ptr %F, i64 %indvars.iv
  %7 = load i64, ptr %arrayidx.5, align 4
  %8 = sub i64 %6, %7
  %arrayidx.6 = getelementptr inbounds i64, ptr %G, i64 %indvars.iv
  %9 = load i64, ptr %arrayidx.6, align 4
  %10 = sub i64 %8, %9
  %arrayidx.7 = getelementptr inbounds i64, ptr %H, i64 %indvars.iv
  %11 = load i64, ptr %arrayidx.7, align 4
  %12 = sub i64 %10, %11
  %arrayidx.8 = getelementptr inbounds i64, ptr %I, i64 %indvars.iv
  %13 = load i64, ptr %arrayidx.8, align 4
  %14 = sub i64 %12, %13
  %arrayidx.9 = getelementptr inbounds i64, ptr %A, i64 %indvars.iv
  store i64 %14, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
