// RUN: %clang -target riscv32-unknown-linux-gnu -march=rv32if -mno-fdiv -x c -E -dM %s \
// RUN: -o - | FileCheck %s
// RUN: %clang -target riscv64-unknown-linux-gnu -march=rv64if -mno-fdiv -x c -E -dM %s \
// RUN: -o - | FileCheck %s
// RUN: %clang -target riscv32-unknown-linux-gnu -march=rv32ifd -mno-fdiv -x c -E -dM %s \
// RUN: -o - | FileCheck %s
// RUN: %clang -target riscv64-unknown-linux-gnu -march=rv64ifd -mno-fdiv -x c -E -dM %s \
// RUN: -o - | FileCheck %s

// CHECK-NOT: __riscv_fdiv
// CHECK-NOT: __riscv_fsqrt
