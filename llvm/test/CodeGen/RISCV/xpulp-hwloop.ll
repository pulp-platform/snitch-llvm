; RUN: llc -march=riscv32 -mattr=+xpulpv < %s | FileCheck %s

; TODO: add this form to the tests:
; for(int i = 0; i < v; ++i) {
;     for(int j = 0; j < i; ++j) {...}
; }

; 1x foor loop, induction variable = 0..n-1
; CHECK-LABEL:	@for_1x
;CHECK:			# %bb.1:
;CHECK-NEXT:		lp.setup	x0, {{[ats][0-9]+}}, [[LABEL:\.LBB.+]]
;CHECK:				p.lw	[[VALUE:[ats][0-9]+]], 4([[DATA:[ats][0-9]+]]!)
;CHECK:			[[LABEL]]:
;CHECK:				add	[[ACCUMULATOR:[ats][0-9]+]], [[ACCUMULATOR]], [[VALUE]]
define dso_local i32 @for_1x(i32* nocapture noundef readonly %va, i32 noundef %na) local_unnamed_addr #0 {
entry:
  %cmp6 = icmp sgt i32 %na, 0
  br i1 %cmp6, label %for.body, label %for.cond.cleanup

for.cond.cleanup:
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add, %for.body ]
  ret i32 %sum.0.lcssa

for.body:
  %ia.08 = phi i32 [ %inc, %for.body ], [ 0, %entry ]
  %sum.07 = phi i32 [ %add, %for.body ], [ 0, %entry ]
  %arrayidx = getelementptr inbounds i32, i32* %va, i32 %ia.08
  %0 = load i32, i32* %arrayidx, align 4
  %add = add nsw i32 %0, %sum.07
  %inc = add nuw nsw i32 %ia.08, 1
  %exitcond.not = icmp eq i32 %inc, %na
  br i1 %exitcond.not, label %for.cond.cleanup, label %for.body
}

; 1x foor loop, induction variable = n-1..0
; FIXME: no post-decrement load
; CHECK-LABEL:	for_1x_reverse:
; CHECK-NEXT:	# %bb.0:
; CHECK:			.p2align	2
; CHECK:			lp.setup	x0, {{[ats][0-9]+}}, [[LABEL:\.LBB.+]]
; CHECK:			lw	[[VALUE:[ats][0-9]+]], 0([[DATA:[ats][0-9]+]])
; CHECK:			add	[[ACCUMULATOR:[ats][0-9]+]], [[ACCUMULATOR]], [[VALUE]]
; CHECK:		[[LABEL]]:
; CHECK:			addi	[[DATA:[ats][0-9]+]], [[DATA]], -4
define dso_local i32 @for_1x_reverse(i32* nocapture noundef readonly %va, i32 noundef %na) local_unnamed_addr #0 {
entry:
  %cmp7 = icmp sgt i32 %na, 0
  br i1 %cmp7, label %for.body, label %for.cond.cleanup

for.cond.cleanup:
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add, %for.body ]
  ret i32 %sum.0.lcssa

for.body:
  %ia.09.in = phi i32 [ %ia.09, %for.body ], [ %na, %entry ]
  %sum.08 = phi i32 [ %add, %for.body ], [ 0, %entry ]
  %ia.09 = add nsw i32 %ia.09.in, -1
  %arrayidx = getelementptr inbounds i32, i32* %va, i32 %ia.09
  %0 = load i32, i32* %arrayidx, align 4
  %add = add nsw i32 %0, %sum.08
  %cmp = icmp ugt i32 %ia.09.in, 1
  br i1 %cmp, label %for.body, label %for.cond.cleanup
}

; 2x foor loop nest, induction variables = 0..n-1
; FIXME: post-increment load for both DATAA and DATAB pointers
; CHECK-LABEL:	for_2x:
; CHECK-NEXT:		# %bb.0:
; CHECK:	.p2align	2
; CHECK:	lp.setup	x1, a2, [[A_LABEL:\.LBB.+]]
; CHECK:	lw	[[VALUE_A:[ats][0-9]+]], 0({{[ats][0-9]+}})
; CHECK:	.p2align	2
; CHECK:	lp.setup	x0, {{[ats][0-9]+}}, [[B_LABEL:\.LBB.+]]
; CHECK:	p.lw	[[DATA_B:[ats][0-9]+]], 4({{[ats][0-9]+}}!)
; CHECK:	add	[[ACCUMULATOR:[ats][0-9]+]], [[ACCUMULATOR]], [[VALUE_A]]
; CHECK:[[B_LABEL]]:
; CHECK:	sub	[[ACCUMULATOR]], [[ACCUMULATOR]], [[DATA_B]]
; CHECK:[[A_LABEL]]:
; CHECK:	addi	{{[ats][0-9]+}}, {{[ats][0-9]+}}, 1
define dso_local i32 @for_2x(i32* nocapture noundef readonly %va, i32* nocapture noundef readonly %vb, i32 noundef %na, i32 noundef %nb) local_unnamed_addr #0 {
entry:
  %cmp20 = icmp sgt i32 %na, 0
  %cmp217 = icmp sgt i32 %nb, 0
  %or.cond = and i1 %cmp20, %cmp217
  br i1 %or.cond, label %for.cond1.preheader.us, label %for.cond.cleanup

for.cond1.preheader.us:
  %ia.022.us = phi i32 [ %inc7.us, %for.cond1.for.cond.cleanup3_crit_edge.us ], [ 0, %entry ]
  %sum.021.us = phi i32 [ %add.us, %for.cond1.for.cond.cleanup3_crit_edge.us ], [ 0, %entry ]
  %arrayidx.us = getelementptr inbounds i32, i32* %va, i32 %ia.022.us
  %0 = load i32, i32* %arrayidx.us, align 4
  br label %for.body4.us

for.body4.us:
  %ib.019.us = phi i32 [ 0, %for.cond1.preheader.us ], [ %inc.us, %for.body4.us ]
  %sum.118.us = phi i32 [ %sum.021.us, %for.cond1.preheader.us ], [ %add.us, %for.body4.us ]
  %arrayidx5.us = getelementptr inbounds i32, i32* %vb, i32 %ib.019.us
  %1 = load i32, i32* %arrayidx5.us, align 4
  %sub.us = add i32 %0, %sum.118.us
  %add.us = sub i32 %sub.us, %1
  %inc.us = add nuw nsw i32 %ib.019.us, 1
  %exitcond.not = icmp eq i32 %inc.us, %nb
  br i1 %exitcond.not, label %for.cond1.for.cond.cleanup3_crit_edge.us, label %for.body4.us

for.cond1.for.cond.cleanup3_crit_edge.us:
  %inc7.us = add nuw nsw i32 %ia.022.us, 1
  %exitcond26.not = icmp eq i32 %inc7.us, %na
  br i1 %exitcond26.not, label %for.cond.cleanup, label %for.cond1.preheader.us

for.cond.cleanup:
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add.us, %for.cond1.for.cond.cleanup3_crit_edge.us ]
  ret i32 %sum.0.lcssa
}

; 2x foor loop nest, induction variables = n-1..0
; FIXME only the inner loop is lowered to lp.setup
; FIXME no post-increment loads with negative offset are emitted
; CHECK-LABEL:	for_2x_reverse:
; CHECK-NEXT:		# %bb.0:
; CHECK:			lp.setup	x0, t2, [[LABEL:\.LBB.+]]
; CHECK:			lw	a5, 0(a1)
; CHECK:			add	a4, a4, t3
; CHECK:			sub	a4, a4, a5
; CHECK:			[[LABEL]]:
define dso_local i32 @for_2x_reverse(i32* nocapture noundef readonly %va, i32* nocapture noundef readonly %vb, i32 noundef %na, i32 noundef %nb) local_unnamed_addr #0 {
entry:
  %cmp24 = icmp sgt i32 %na, 0
  %cmp320 = icmp sgt i32 %nb, 0
  %or.cond = and i1 %cmp24, %cmp320
  br i1 %or.cond, label %for.cond2.preheader.us, label %for.cond.cleanup

for.cond2.preheader.us:                           ; preds = %entry, %for.cond2.for.cond.loopexit_crit_edge.us
  %ia.026.us.in = phi i32 [ %ia.026.us, %for.cond2.for.cond.loopexit_crit_edge.us ], [ %na, %entry ]
  %sum.025.us = phi i32 [ %add.us, %for.cond2.for.cond.loopexit_crit_edge.us ], [ 0, %entry ]
  %ia.026.us = add nsw i32 %ia.026.us.in, -1
  %arrayidx.us = getelementptr inbounds i32, i32* %va, i32 %ia.026.us
  %0 = load i32, i32* %arrayidx.us, align 4
  br label %for.body5.us

for.body5.us:                                     ; preds = %for.cond2.preheader.us, %for.body5.us
  %ib.022.us.in = phi i32 [ %nb, %for.cond2.preheader.us ], [ %ib.022.us, %for.body5.us ]
  %sum.121.us = phi i32 [ %sum.025.us, %for.cond2.preheader.us ], [ %add.us, %for.body5.us ]
  %ib.022.us = add nsw i32 %ib.022.us.in, -1
  %arrayidx6.us = getelementptr inbounds i32, i32* %vb, i32 %ib.022.us
  %1 = load i32, i32* %arrayidx6.us, align 4
  %sub7.us = add i32 %0, %sum.121.us
  %add.us = sub i32 %sub7.us, %1
  %cmp3.us = icmp sgt i32 %ib.022.us.in, 1
  br i1 %cmp3.us, label %for.body5.us, label %for.cond2.for.cond.loopexit_crit_edge.us

for.cond2.for.cond.loopexit_crit_edge.us:         ; preds = %for.body5.us
  %cmp.us = icmp sgt i32 %ia.026.us.in, 1
  br i1 %cmp.us, label %for.cond2.preheader.us, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond2.for.cond.loopexit_crit_edge.us, %entry
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add.us, %for.cond2.for.cond.loopexit_crit_edge.us ]
  ret i32 %sum.0.lcssa
}

; 3x foor loop nest, induction variables = 0..n-1
; FIXME post-increment loads are emitted only for inner loop
; CHECK-LABEL:	for_3x:
; CHECK-NEXT:	# %bb.0:
; CHECK:			bnez	{{[ats][0-9]+}}, [[A_LABEL:\.LBB.+]]
; CHECK:			.p2align	2
; CHECK:			lp.setup	x1, {{[ats][0-9]+}}, [[B_LABEL:\.LBB.+]]
; CHECK:			lw	{{[ats][0-9]+}}, 0({{[ats][0-9]+}})
; CHECK:			.p2align	2
; CHECK:			lp.setup	x0, {{[ats][0-9]+}}, [[C_LABEL:\.LBB.+]]
; CHECK:			p.lw	{{[ats][0-9]+}}, 4({{[ats][0-9]+}}!)
; CHECK:		[[C_LABEL]]:
; CHECK:		[[B_LABEL]]:
; CHECK:		[[A_LABEL]]:
define dso_local i32 @for_3x(i32* nocapture noundef readonly %va, i32* nocapture noundef readonly %vb, i32* nocapture noundef readonly %vc, i32 noundef %na, i32 noundef %nb, i32 noundef %nc) local_unnamed_addr #0 {
entry:
  %cmp39 = icmp sgt i32 %na, 0
  %cmp234 = icmp sgt i32 %nb, 0
  %or.cond = and i1 %cmp39, %cmp234
  %cmp631 = icmp sgt i32 %nc, 0
  %or.cond55 = and i1 %or.cond, %cmp631
  br i1 %or.cond55, label %for.cond1.preheader.us.us, label %for.cond.cleanup

for.cond1.preheader.us.us:                        ; preds = %entry, %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us
  %ia.041.us.us = phi i32 [ %inc16.us.us, %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us ], [ 0, %entry ]
  %sum.040.us.us = phi i32 [ %add.us.us.us, %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us ], [ 0, %entry ]
  %arrayidx.us.us = getelementptr inbounds i32, i32* %va, i32 %ia.041.us.us
  %0 = load i32, i32* %arrayidx.us.us, align 4
  br label %for.cond5.preheader.us.us.us

for.cond5.preheader.us.us.us:                     ; preds = %for.cond5.for.cond.cleanup7_crit_edge.us.us.us, %for.cond1.preheader.us.us
  %ib.036.us.us.us = phi i32 [ 0, %for.cond1.preheader.us.us ], [ %inc13.us.us.us, %for.cond5.for.cond.cleanup7_crit_edge.us.us.us ]
  %sum.135.us.us.us = phi i32 [ %sum.040.us.us, %for.cond1.preheader.us.us ], [ %add.us.us.us, %for.cond5.for.cond.cleanup7_crit_edge.us.us.us ]
  %arrayidx9.us.us.us = getelementptr inbounds i32, i32* %vb, i32 %ib.036.us.us.us
  %1 = load i32, i32* %arrayidx9.us.us.us, align 4
  br label %for.body8.us.us.us

for.body8.us.us.us:                               ; preds = %for.body8.us.us.us, %for.cond5.preheader.us.us.us
  %ic.033.us.us.us = phi i32 [ 0, %for.cond5.preheader.us.us.us ], [ %inc.us.us.us, %for.body8.us.us.us ]
  %sum.232.us.us.us = phi i32 [ %sum.135.us.us.us, %for.cond5.preheader.us.us.us ], [ %add.us.us.us, %for.body8.us.us.us ]
  %arrayidx10.us.us.us = getelementptr inbounds i32, i32* %vc, i32 %ic.033.us.us.us
  %2 = load i32, i32* %arrayidx10.us.us.us, align 4
  %.neg30.us.us.us = add i32 %0, %sum.232.us.us.us
  %3 = add i32 %1, %2
  %add.us.us.us = sub i32 %.neg30.us.us.us, %3
  %inc.us.us.us = add nuw nsw i32 %ic.033.us.us.us, 1
  %exitcond.not = icmp eq i32 %inc.us.us.us, %nc
  br i1 %exitcond.not, label %for.cond5.for.cond.cleanup7_crit_edge.us.us.us, label %for.body8.us.us.us

for.cond5.for.cond.cleanup7_crit_edge.us.us.us:   ; preds = %for.body8.us.us.us
  %inc13.us.us.us = add nuw nsw i32 %ib.036.us.us.us, 1
  %exitcond53.not = icmp eq i32 %inc13.us.us.us, %nb
  br i1 %exitcond53.not, label %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us, label %for.cond5.preheader.us.us.us

for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us: ; preds = %for.cond5.for.cond.cleanup7_crit_edge.us.us.us
  %inc16.us.us = add nuw nsw i32 %ia.041.us.us, 1
  %exitcond54.not = icmp eq i32 %inc16.us.us, %na
  br i1 %exitcond54.not, label %for.cond.cleanup, label %for.cond1.preheader.us.us

for.cond.cleanup:                                 ; preds = %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us, %entry
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add.us.us.us, %for.cond1.for.cond.cleanup3_crit_edge.split.us.us.us ]
  ret i32 %sum.0.lcssa
}

; CHECK-LABEL:	for_3x_reverse:
; CHECK-NEXT:	# %bb.0:
; CHECK:			bnez	{{[ats][0-9]+}}, [[GUARD:\.LBB.+]]
; CHECK:		[[A_LABEL:\.LBB.+]]:
; CHECK:			lw	{{[ats][0-9]+}}, 0({{[ats][0-9]+}})
; CHECK:		[[B_LABEL:\.LBB.+]]:
; CHECK:			lw	{{[ats][0-9]+}}, 0({{[ats][0-9]+}})
; CHECK:			.p2align	2
; CHECK:			lp.setup	x0, {{[ats][0-9]+}}, [[C_LABEL:\.LBB.+]]
; CHECK:			lw	{{[ats][0-9]+}}, 0({{[ats][0-9]+}})
; CHECK:		[[C_LABEL]]:
; CHECK:			blt	{{[ats][0-9]+}}, {{[ats][0-9]+}}, [[B_LABEL]]
; CHECK:			blt	{{[ats][0-9]+}}, {{[ats][0-9]+}}, [[A_LABEL]]
; CHECK:		[[GUARD]]:
define dso_local i32 @for_3x_reverse(i32* nocapture noundef readonly %va, i32* nocapture noundef readonly %vb, i32* nocapture noundef readonly %vc, i32 noundef %na, i32 noundef %nb, i32 noundef %nc) local_unnamed_addr #0 {
entry:
  %cmp44 = icmp sgt i32 %na, 0
  %cmp339 = icmp sgt i32 %nb, 0
  %or.cond = and i1 %cmp44, %cmp339
  %cmp835 = icmp sgt i32 %nc, 0
  %or.cond57 = and i1 %or.cond, %cmp835
  br i1 %or.cond57, label %for.cond2.preheader.us.us, label %for.cond.cleanup

for.cond2.preheader.us.us:                        ; preds = %entry, %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us
  %ia.046.us.us.in = phi i32 [ %ia.046.us.us, %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us ], [ %na, %entry ]
  %sum.045.us.us = phi i32 [ %add.us.us.us, %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us ], [ 0, %entry ]
  %ia.046.us.us = add nsw i32 %ia.046.us.us.in, -1
  %arrayidx.us.us = getelementptr inbounds i32, i32* %va, i32 %ia.046.us.us
  %0 = load i32, i32* %arrayidx.us.us, align 4
  br label %for.cond7.preheader.us.us.us

for.cond7.preheader.us.us.us:                     ; preds = %for.cond7.for.cond2.loopexit_crit_edge.us.us.us, %for.cond2.preheader.us.us
  %ib.041.us.us.us.in = phi i32 [ %nb, %for.cond2.preheader.us.us ], [ %ib.041.us.us.us, %for.cond7.for.cond2.loopexit_crit_edge.us.us.us ]
  %sum.140.us.us.us = phi i32 [ %sum.045.us.us, %for.cond2.preheader.us.us ], [ %add.us.us.us, %for.cond7.for.cond2.loopexit_crit_edge.us.us.us ]
  %ib.041.us.us.us = add nsw i32 %ib.041.us.us.us.in, -1
  %arrayidx11.us.us.us = getelementptr inbounds i32, i32* %vb, i32 %ib.041.us.us.us
  %1 = load i32, i32* %arrayidx11.us.us.us, align 4
  br label %for.body10.us.us.us

for.body10.us.us.us:                              ; preds = %for.body10.us.us.us, %for.cond7.preheader.us.us.us
  %ic.037.us.us.us.in = phi i32 [ %nc, %for.cond7.preheader.us.us.us ], [ %ic.037.us.us.us, %for.body10.us.us.us ]
  %sum.236.us.us.us = phi i32 [ %sum.140.us.us.us, %for.cond7.preheader.us.us.us ], [ %add.us.us.us, %for.body10.us.us.us ]
  %ic.037.us.us.us = add nsw i32 %ic.037.us.us.us.in, -1
  %arrayidx13.us.us.us = getelementptr inbounds i32, i32* %vc, i32 %ic.037.us.us.us
  %2 = load i32, i32* %arrayidx13.us.us.us, align 4
  %.neg33.us.us.us = add i32 %0, %sum.236.us.us.us
  %3 = add i32 %1, %2
  %add.us.us.us = sub i32 %.neg33.us.us.us, %3
  %cmp8.us.us.us = icmp sgt i32 %ic.037.us.us.us.in, 1
  br i1 %cmp8.us.us.us, label %for.body10.us.us.us, label %for.cond7.for.cond2.loopexit_crit_edge.us.us.us

for.cond7.for.cond2.loopexit_crit_edge.us.us.us:  ; preds = %for.body10.us.us.us
  %cmp3.us.us.us = icmp sgt i32 %ib.041.us.us.us.in, 1
  br i1 %cmp3.us.us.us, label %for.cond7.preheader.us.us.us, label %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us

for.cond2.for.cond.loopexit_crit_edge.split.us.us.us: ; preds = %for.cond7.for.cond2.loopexit_crit_edge.us.us.us
  %cmp.us.us = icmp sgt i32 %ia.046.us.us.in, 1
  br i1 %cmp.us.us, label %for.cond2.preheader.us.us, label %for.cond.cleanup

for.cond.cleanup:                                 ; preds = %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us, %entry
  %sum.0.lcssa = phi i32 [ 0, %entry ], [ %add.us.us.us, %for.cond2.for.cond.loopexit_crit_edge.split.us.us.us ]
  ret i32 %sum.0.lcssa
}

attributes #0 = { nofree norecurse nosync nounwind readonly "target-features"="+a,+c,+f,+m,+xpulpv" }
