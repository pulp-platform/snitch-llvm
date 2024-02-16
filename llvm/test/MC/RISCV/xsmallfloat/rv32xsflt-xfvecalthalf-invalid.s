# RUN: not llvm-mc -triple=riscv64 -mattr=xfvecsingle,+d -riscv-no-aliases < %s 2>&1 | FileCheck %s

vfcvtu.s.ah f1, x21 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
vfcvtu.ah.s f1, x21 # CHECK: :[[@LINE]]:17: error: invalid operand for instruction
