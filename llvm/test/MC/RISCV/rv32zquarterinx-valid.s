# RUN: llvm-mc %s -triple=riscv32 -mattr=+zhinx -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -triple=riscv64 -mattr=+zhinx -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=riscv32 -mattr=+zhinx %s \
# RUN:     | llvm-objdump --mattr=+zhinx -M no-aliases -d -r - \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc -filetype=obj -triple=riscv64 -mattr=+zhinx %s \
# RUN:     | llvm-objdump --mattr=+zhinx -M no-aliases -d -r - \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM-AND-OBJ %s

# CHECK-ASM-AND-OBJ: fmadd.b a0, a1, a2, a3, dyn
# CHECK-ASM: encoding: [0x43,0xf5,0xc5,0x6c]
fmadd.b x10, x11, x12, x13, dyn
# CHECK-ASM-AND-OBJ: fmsub.b a4, a5, a6, a7, dyn
# CHECK-ASM: encoding: [0x47,0xf7,0x07,0x8d]
fmsub.b x14, x15, x16, x17, dyn
# CHECK-ASM-AND-OBJ: fnmsub.b s2, s3, s4, s5, dyn
# CHECK-ASM: encoding: [0x4b,0xf9,0x49,0xad]
fnmsub.b x18, x19, x20, x21, dyn
# CHECK-ASM-AND-OBJ: fnmadd.b s6, s7, s8, s9, dyn
# CHECK-ASM: encoding: [0x4f,0xfb,0x8b,0xcd]
fnmadd.b x22, x23, x24, x25, dyn

# CHECK-ASM-AND-OBJ: fadd.b s10, s11, t3, dyn
# CHECK-ASM: encoding: [0x53,0xfd,0xcd,0x05]
fadd.b x26, x27, x28, dyn
# CHECK-ASM-AND-OBJ: fsub.b t4, t5, t6, dyn
# CHECK-ASM: encoding: [0xd3,0x7e,0xff,0x0d]
fsub.b x29, x30, x31, dyn
# CHECK-ASM-AND-OBJ: fmul.b s0, s1, s2, dyn
# CHECK-ASM: encoding: [0x53,0xf4,0x24,0x15]
fmul.b s0, s1, s2, dyn
# CHECK-ASM-AND-OBJ: fdiv.b s3, s4, s5, dyn
# CHECK-ASM: encoding: [0xd3,0x79,0x5a,0x1d]
fdiv.b s3, s4, s5, dyn
# CHECK-ASM-AND-OBJ: fsqrt.b s6, s7, dyn
# CHECK-ASM: encoding: [0x53,0xfb,0x0b,0x5c]
fsqrt.b s6, s7, dyn
# CHECK-ASM-AND-OBJ: fsgnj.b s1, a0, a1
# CHECK-ASM: encoding: [0xd3,0x04,0xb5,0x24]
fsgnj.b x9, x10, x11
# CHECK-ASM-AND-OBJ: fsgnjn.b a1, a3, a4
# CHECK-ASM: encoding: [0xd3,0x95,0xe6,0x24]
fsgnjn.b x11, x13, x14
# CHECK-ASM-AND-OBJ: fsgnjx.b a4, a3, a2
# CHECK-ASM: encoding: [0x53,0xa7,0xc6,0x24]
fsgnjx.b x14, x13, x12
# CHECK-ASM-AND-OBJ: fmin.b a5, a6, a7
# CHECK-ASM: encoding: [0xd3,0x07,0x18,0x2d]
fmin.b x15, x16, x17
# CHECK-ASM-AND-OBJ: fmax.b s2, s3, s4
# CHECK-ASM: encoding: [0x53,0x99,0x49,0x2d]
fmax.b x18, x19, x20
# CHECK-ASM-AND-OBJ: fcvt.w.b a0, s5, dyn
# CHECK-ASM: encoding: [0x53,0xf5,0x0a,0xc4]
fcvt.w.b x10, x21, dyn
# CHECK-ASM-AND-OBJ: fcvt.wu.b a1, s6, dyn
# CHECK-ASM: encoding: [0xd3,0x75,0x1b,0xc4]
fcvt.wu.b x11, x22, dyn
# CHECK-ASM-AND-OBJ: feq.b a1, s8, s9
# CHECK-ASM: encoding: [0xd3,0x25,0x9c,0xa5]
feq.b x11, x24, x25
# CHECK-ASM-AND-OBJ: flt.b a2, s10, s11
# CHECK-ASM: encoding: [0x53,0x16,0xbd,0xa5]
flt.b x12, x26, x27
# CHECK-ASM-AND-OBJ: fle.b a3, t3, t4
# CHECK-ASM: encoding: [0xd3,0x06,0xde,0xa5]
fle.b x13, x28, x29
# CHECK-ASM-AND-OBJ: fclass.b a3, t5
# CHECK-ASM: encoding: [0xd3,0x16,0x0f,0xe4]
fclass.b x13, x30
# CHECK-ASM-AND-OBJ: fcvt.b.w t6, a4, dyn
# CHECK-ASM: encoding: [0xd3,0x7f,0x07,0xd4]
fcvt.b.w x31, x14, dyn
# CHECK-ASM-AND-OBJ: fcvt.b.wu s0, a5, dyn
# CHECK-ASM: encoding: [0x53,0xf4,0x17,0xd4]
fcvt.b.wu s0, x15, dyn

# Rounding modes

# CHECK-ASM-AND-OBJ: fmadd.b a0, a1, a2, a3, rne
# CHECK-ASM: encoding: [0x43,0x85,0xc5,0x6c]
fmadd.b x10, x11, x12, x13, rne
# CHECK-ASM-AND-OBJ: fmsub.b a4, a5, a6, a7, rtz
# CHECK-ASM: encoding: [0x47,0x97,0x07,0x8d]
fmsub.b x14, x15, x16, x17, rtz
# CHECK-ASM-AND-OBJ: fnmsub.b s2, s3, s4, s5, rdn
# CHECK-ASM: encoding: [0x4b,0xa9,0x49,0xad]
fnmsub.b x18, x19, x20, x21, rdn
# CHECK-ASM-AND-OBJ: fnmadd.b s6, s7, s8, s9, rup
# CHECK-ASM: encoding: [0x4f,0xbb,0x8b,0xcd]
fnmadd.b x22, x23, x24, x25, rup
# CHECK-ASM-AND-OBJ: fmadd.b a0, a1, a2, a3, rmm
# CHECK-ASM: encoding: [0x43,0xc5,0xc5,0x6c]
fmadd.b x10, x11, x12, x13, rmm
# CHECK-ASM-AND-OBJ: fmsub.b a4, a5, a6, a7
# CHECK-ASM: encoding: [0x47,0xf7,0x07,0x8d]
fmsub.b x14, x15, x16, x17, dyn

# CHECK-ASM-AND-OBJ: fadd.b s10, s11, t3, rne
# CHECK-ASM: encoding: [0x53,0x8d,0xcd,0x05]
fadd.b x26, x27, x28, rne
# CHECK-ASM-AND-OBJ: fsub.b t4, t5, t6, rtz
# CHECK-ASM: encoding: [0xd3,0x1e,0xff,0x0d]
fsub.b x29, x30, x31, rtz
# CHECK-ASM-AND-OBJ: fmul.b s0, s1, s2, rdn
# CHECK-ASM: encoding: [0x53,0xa4,0x24,0x15]
fmul.b s0, s1, s2, rdn
# CHECK-ASM-AND-OBJ: fdiv.b s3, s4, s5, rup
# CHECK-ASM: encoding: [0xd3,0x39,0x5a,0x1d]
fdiv.b s3, s4, s5, rup

# CHECK-ASM-AND-OBJ: fsqrt.b s6, s7, rmm
# CHECK-ASM: encoding: [0x53,0xcb,0x0b,0x5c]
fsqrt.b s6, s7, rmm
# CHECK-ASM-AND-OBJ: fcvt.w.b a0, s5, rup
# CHECK-ASM: encoding: [0x53,0xb5,0x0a,0xc4]
fcvt.w.b x10, x21, rup
# CHECK-ASM-AND-OBJ: fcvt.wu.b a1, s6, rdn
# CHECK-ASM: encoding: [0xd3,0x25,0x1b,0xc4]
fcvt.wu.b x11, x22, rdn
# CHECK-ASM-AND-OBJ: fcvt.b.w t6, a4, rtz
# CHECK-ASM: encoding: [0xd3,0x1f,0x07,0xd4]
fcvt.b.w x31, x14, rtz
# CHECK-ASM-AND-OBJ: fcvt.b.wu s0, a5, rne
# CHECK-ASM: encoding: [0x53,0x84,0x17,0xd4]
fcvt.b.wu s0, a5, rne
