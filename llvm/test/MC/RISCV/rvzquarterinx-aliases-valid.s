# RUN: llvm-mc %s -triple=riscv32 -mattr=+zhinx -riscv-no-aliases \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s
# RUN: llvm-mc %s -triple=riscv32 -mattr=+zhinx \
# RUN:     | FileCheck -check-prefix=CHECK-ALIAS %s
# RUN: llvm-mc %s -triple=riscv64 -mattr=+zhinx -riscv-no-aliases \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s
# RUN: llvm-mc %s -triple=riscv64 -mattr=+zhinx \
# RUN:     | FileCheck -check-prefix=CHECK-ALIAS %s
# RUN: llvm-mc -filetype=obj -triple riscv32 -mattr=+zhinx %s \
# RUN:     | llvm-objdump -d --mattr=+zhinx -M no-aliases - \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s
# RUN: llvm-mc -filetype=obj -triple riscv32 -mattr=+zhinx %s \
# RUN:     | llvm-objdump -d --mattr=+zhinx - \
# RUN:     | FileCheck -check-prefix=CHECK-ALIAS %s
# RUN: llvm-mc -filetype=obj -triple riscv64 -mattr=+zhinx %s \
# RUN:     | llvm-objdump -d --mattr=+zhinx -M no-aliases - \
# RUN:     | FileCheck -check-prefix=CHECK-INST %s
# RUN: llvm-mc -filetype=obj -triple riscv64 -mattr=+zhinx %s \
# RUN:     | llvm-objdump -d --mattr=+zhinx - \
# RUN:     | FileCheck -check-prefix=CHECK-ALIAS %s

##===----------------------------------------------------------------------===##
## Assembler Pseudo Instructions (User-Level ISA, Version 2.2, Chapter 20)
##===----------------------------------------------------------------------===##

# CHECK-INST: fsgnjx.b s1, s2, s2
# CHECK-ALIAS: fabs.b s1, s2
fabs.b s1, s2
# CHECK-INST: fsgnjn.b s2, s3, s3
# CHECK-ALIAS: fneg.b s2, s3
fneg.b s2, s3

# CHECK-INST: flt.b tp, s6, s5
# CHECK-ALIAS: flt.b tp, s6, s5
fgt.b x4, s5, s6
# CHECK-INST: fle.b t2, s1, s0
# CHECK-ALIAS: fle.b t2, s1, s0
fge.b x7, x8, x9

##===----------------------------------------------------------------------===##
## Aliases which omit the rounding mode.
##===----------------------------------------------------------------------===##

# CHECK-INST: fmadd.b a0, a1, a2, a3, dyn
# CHECK-ALIAS: fmadd.b a0, a1, a2, a3
fmadd.b x10, x11, x12, x13
# CHECK-INST: fmsub.b a4, a5, a6, a7, dyn
# CHECK-ALIAS: fmsub.b a4, a5, a6, a7
fmsub.b x14, x15, x16, x17
# CHECK-INST: fnmsub.b s2, s3, s4, s5, dyn
# CHECK-ALIAS: fnmsub.b s2, s3, s4, s5
fnmsub.b x18, x19, x20, x21
# CHECK-INST: fnmadd.b s6, s7, s8, s9, dyn
# CHECK-ALIAS: fnmadd.b s6, s7, s8, s9
fnmadd.b x22, x23, x24, x25
# CHECK-INST: fadd.b s10, s11, t3, dyn
# CHECK-ALIAS: fadd.b s10, s11, t3
fadd.b x26, x27, x28
# CHECK-INST: fsub.b t4, t5, t6, dyn
# CHECK-ALIAS: fsub.b t4, t5, t6
fsub.b x29, x30, x31
# CHECK-INST: fmul.b s0, s1, s2, dyn
# CHECK-ALIAS: fmul.b s0, s1, s2
fmul.b s0, s1, s2
# CHECK-INST: fdiv.b s3, s4, s5, dyn
# CHECK-ALIAS: fdiv.b s3, s4, s5
fdiv.b s3, s4, s5
# CHECK-INST: fsqrt.b s6, s7, dyn
# CHECK-ALIAS: fsqrt.b s6, s7
fsqrt.b s6, s7
# CHECK-INST: fcvt.w.b a0, s5, dyn
# CHECK-ALIAS: fcvt.w.b a0, s5
fcvt.w.b a0, s5
# CHECK-INST: fcvt.wu.b a1, s6, dyn
# CHECK-ALIAS: fcvt.wu.b a1, s6
fcvt.wu.b a1, s6
# CHECK-INST: fcvt.b.w t6, a4, dyn
# CHECK-ALIAS: fcvt.b.w t6, a4
fcvt.b.w t6, a4
# CHECK-INST: fcvt.b.wu s0, a5, dyn
# CHECK-ALIAS: fcvt.b.wu s0, a5
fcvt.b.wu s0, a5