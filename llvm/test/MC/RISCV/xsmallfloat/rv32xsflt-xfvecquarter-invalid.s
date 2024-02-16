# RUN: not llvm-mc -triple=riscv64 -mattr=xfvecsingle,+d -riscv-no-aliases < %s 2>&1 | FileCheck %s

vfcvtu.s.b  f30, x9  # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.b.s  f26, x12 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.h.b  f11, x15 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.b.h  f18, x29 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ah.b f8 , x17 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.b.ah f14, x12 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.b.b  f11, x23 # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.ab.b f30, x7  # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
vfcvtu.b.ab f19, x8  # CHECK: :[[@LINE]]:18: error: invalid operand for instruction
