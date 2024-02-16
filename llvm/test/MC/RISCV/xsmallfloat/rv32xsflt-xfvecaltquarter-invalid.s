# RUN: not llvm-mc -triple=riscv64 -mattr=xfvecsingle,+d -riscv-no-aliases < %s 2>&1 | FileCheck %s

vfcvtu.s.ab  f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ab.s  f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.h.ab  f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ab.h  f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ah.ab f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ab.ah f1, x1 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
