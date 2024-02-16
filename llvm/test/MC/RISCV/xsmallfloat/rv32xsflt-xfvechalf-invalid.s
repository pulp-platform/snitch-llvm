# RUN: not llvm-mc -triple=riscv64 -mattr=xfvecsingle,+d -riscv-no-aliases < %s 2>&1 | FileCheck %s

vfcvtu.s.h f1, x1 # CHECK: :[[@LINE]]:16: error: invalid operand for instruction
vfcvtu.h.s f1, x1 # CHECK: :[[@LINE]]:16: error: invalid operand for instruction
