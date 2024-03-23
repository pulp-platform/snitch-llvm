; RUN: llc -march=riscv32 -mattr=+xpulpv -print-after-isel -o /dev/null %s 2>&1 | FileCheck %s
; CHECK-LABEL: foo:
; CHECK: {{%[0-9]+}}:gpr = PseudoPV_ADD_SCI_B_VL $x0, 2, -1
; CHECK: {{%[0-9]+}}:gpr = PseudoPV_ADD_SC_B_VL $x0, {{%[0-9]+}}:gpr, -1
; CHECK: {{%[0-9]+}}:gpr = PseudoPV_ADD_SCI_H_VL $x0, 2, -1
; CHECK: {{%[0-9]+}}:gpr = PseudoPV_ADD_SC_H_VL $x0, {{%[0-9]+}}:gpr, -1
define dso_local void @foo() local_unnamed_addr  {
entry:
  %v1 = alloca <4 x i8>, align 4
  %v2 = alloca <4 x i8>, align 4
  %v3 = alloca <2 x i16>, align 4
  %v4 = alloca <2 x i16>, align 4
  store volatile <4 x i8> <i8 2, i8 2, i8 2, i8 2>, <4 x i8>* %v1, align 4
  store volatile <4 x i8> <i8 60, i8 60, i8 60, i8 60>, <4 x i8>* %v2, align 4
  store volatile <2 x i16> <i16 2, i16 2>, <2 x i16>* %v3, align 4
  store volatile <2 x i16> <i16 60, i16 60>, <2 x i16>* %v4, align 4
  ret void
}

