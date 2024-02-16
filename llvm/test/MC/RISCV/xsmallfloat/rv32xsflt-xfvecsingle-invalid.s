# RUN: not llvm-mc -triple=riscv64 -mattr=xfvecsingle,+d -riscv-no-aliases < %s 2>&1 | FileCheck %s

vfcvtu.h.h  f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
vfcvtu.h.ah f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
vfcvtu.ah.h f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction

vfcvt.h.h   f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
vfcvt.h.ah  f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
vfcvt.ah.h  f1, x1 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
