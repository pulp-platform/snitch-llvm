# RUN: not llvm-mc %s -triple=riscv32 -riscv-no-aliases -show-encoding 2>&1 \
# RUN:     | FileCheck %s

# CHECK: :[[@LINE+1]]:13: error: system register use requires an option to be enabled
csrr    t0, trace
