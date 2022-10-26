; RUN: opt -S -passes=tree-height-reduction -enable-int-thr < %s | FileCheck %s

; CHECK-LABEL: @add_with_constant_i8(
; CHECK: %[[V0:.*]] = add i8 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = add i8 {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = add i8 {{.*}}, 10
; CHECK-NEXT: %[[V3:.*]] = add i8 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = add i8 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = add i8 %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = add i8 %[[V3]], %[[V4]]
; CHECK-NEXT: add i8 %[[V5]], %[[V6]]
define void @add_with_constant_i8(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %arrayidx.3 = getelementptr inbounds i8, ptr %D, i64 %indvars.iv
  %2 = load i8, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds i8, ptr %E, i64 %indvars.iv
  %3 = load i8, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds i8, ptr %F, i64 %indvars.iv
  %4 = load i8, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds i8, ptr %G, i64 %indvars.iv
  %5 = load i8, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds i8, ptr %H, i64 %indvars.iv
  %6 = load i8, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds i8, ptr %I, i64 %indvars.iv
  %7 = load i8, ptr %arrayidx.8, align 4
  %8 = add i8 %0, 10
  %9 = add i8 %8, %1
  %10 = add i8 %9, %2
  %11 = add i8 %10, %3
  %12 = add i8 %11, %4
  %13 = add i8 %12, %5
  %14 = add i8 %13, %6
  %15 = add i8 %14, %7
  %arrayidx.9 = getelementptr inbounds i8, ptr %A, i64 %indvars.iv
  store i8 %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @add_with_constant_i16(
; CHECK: %[[V0:.*]] = add i16 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = add i16 {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = add i16 {{.*}}, 10
; CHECK-NEXT: %[[V3:.*]] = add i16 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = add i16 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = add i16 %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = add i16 %[[V3]], %[[V4]]
; CHECK-NEXT: add i16 %[[V5]], %[[V6]]
define void @add_with_constant_i16(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %arrayidx.3 = getelementptr inbounds i16, ptr %D, i64 %indvars.iv
  %2 = load i16, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds i16, ptr %E, i64 %indvars.iv
  %3 = load i16, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds i16, ptr %F, i64 %indvars.iv
  %4 = load i16, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds i16, ptr %G, i64 %indvars.iv
  %5 = load i16, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds i16, ptr %H, i64 %indvars.iv
  %6 = load i16, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds i16, ptr %I, i64 %indvars.iv
  %7 = load i16, ptr %arrayidx.8, align 4
  %8 = add i16 %0, 10
  %9 = add i16 %8, %1
  %10 = add i16 %9, %2
  %11 = add i16 %10, %3
  %12 = add i16 %11, %4
  %13 = add i16 %12, %5
  %14 = add i16 %13, %6
  %15 = add i16 %14, %7
  %arrayidx.9 = getelementptr inbounds i16, ptr %A, i64 %indvars.iv
  store i16 %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @add_with_constant_i32(
; CHECK: %[[V0:.*]] = add i32 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = add i32 {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = add i32 {{.*}}, 10
; CHECK-NEXT: %[[V3:.*]] = add i32 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = add i32 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = add i32 %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = add i32 %[[V3]], %[[V4]]
; CHECK-NEXT: add i32 %[[V5]], %[[V6]]
define void @add_with_constant_i32(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %arrayidx.3 = getelementptr inbounds i32, ptr %D, i64 %indvars.iv
  %2 = load i32, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds i32, ptr %E, i64 %indvars.iv
  %3 = load i32, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds i32, ptr %F, i64 %indvars.iv
  %4 = load i32, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds i32, ptr %G, i64 %indvars.iv
  %5 = load i32, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds i32, ptr %H, i64 %indvars.iv
  %6 = load i32, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds i32, ptr %I, i64 %indvars.iv
  %7 = load i32, ptr %arrayidx.8, align 4
  %8 = add i32 %0, 10
  %9 = add i32 %8, %1
  %10 = add i32 %9, %2
  %11 = add i32 %10, %3
  %12 = add i32 %11, %4
  %13 = add i32 %12, %5
  %14 = add i32 %13, %6
  %15 = add i32 %14, %7
  %arrayidx.9 = getelementptr inbounds i32, ptr %A, i64 %indvars.iv
  store i32 %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}

; CHECK-LABEL: @add_with_constant_i64(
; CHECK: %[[V0:.*]] = add i64 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V1:.*]] = add i64 {{.*}}, %[[V0]]
; CHECK-NEXT: %[[V2:.*]] = add i64 {{.*}}, 10
; CHECK-NEXT: %[[V3:.*]] = add i64 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V4:.*]] = add i64 {{.*}}, {{.*}}
; CHECK-NEXT: %[[V5:.*]] = add i64 %[[V1]], %[[V2]]
; CHECK-NEXT: %[[V6:.*]] = add i64 %[[V3]], %[[V4]]
; CHECK-NEXT: add i64 %[[V5]], %[[V6]]
define void @add_with_constant_i64(ptr noalias %A, ptr noalias %B, ptr noalias %C, ptr noalias %D, ptr noalias %E, ptr noalias %F, ptr noalias %G, ptr noalias %H, ptr noalias %I, i32 %N) norecurse nounwind {
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
  %arrayidx.3 = getelementptr inbounds i64, ptr %D, i64 %indvars.iv
  %2 = load i64, ptr %arrayidx.3, align 4
  %arrayidx.4 = getelementptr inbounds i64, ptr %E, i64 %indvars.iv
  %3 = load i64, ptr %arrayidx.4, align 4
  %arrayidx.5 = getelementptr inbounds i64, ptr %F, i64 %indvars.iv
  %4 = load i64, ptr %arrayidx.5, align 4
  %arrayidx.6 = getelementptr inbounds i64, ptr %G, i64 %indvars.iv
  %5 = load i64, ptr %arrayidx.6, align 4
  %arrayidx.7 = getelementptr inbounds i64, ptr %H, i64 %indvars.iv
  %6 = load i64, ptr %arrayidx.7, align 4
  %arrayidx.8 = getelementptr inbounds i64, ptr %I, i64 %indvars.iv
  %7 = load i64, ptr %arrayidx.8, align 4
  %8 = add i64 %0, 10
  %9 = add i64 %8, %1
  %10 = add i64 %9, %2
  %11 = add i64 %10, %3
  %12 = add i64 %11, %4
  %13 = add i64 %12, %5
  %14 = add i64 %13, %6
  %15 = add i64 %14, %7
  %arrayidx.9 = getelementptr inbounds i64, ptr %A, i64 %indvars.iv
  store i64 %15, ptr %arrayidx.9, align 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %zext
  br i1 %exitcond, label %for.end, label %for.body

for.end:                                          ; preds = %for.body, %entry
  ret void
}
