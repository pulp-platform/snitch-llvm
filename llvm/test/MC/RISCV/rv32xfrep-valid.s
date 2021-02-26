# RUN: llvm-mc %s -triple=riscv32 --mattr=xfrep -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -filetype=obj -triple=riscv32  --mattr=xfrep \
# RUN:     | llvm-objdump -M no-aliases -d -r --mattr=xfrep - \
# RUN:     | FileCheck -check-prefixes=CHECK-DISASM %s

# CHECK-ASM-AND-OBJ: frep.o t3, 4, 0, 0
# CHECK-ASM: encoding: [0x8b,0x00,0x3e,0x00]
# CHECK-DISASM: 8b 00 3e 00   frep.o  t3, 4, 0, 0
frep.o t3, 4, 0, 0b0000
# CHECK-ASM-AND-OBJ: frep.o t0, 1, 4, 15
# CHECK-ASM: encoding: [0x8b,0xcf,0x02,0x00]
# CHECK-DISASM: 8b cf 02 00   frep.o  t0, 1, 4, 15
frep.o t0, 1, 4, 0b1111
# CHECK-ASM-AND-OBJ: frep.i t3, 4, 0,  0
# CHECK-ASM: encoding: [0x0b,0x00,0x3e,0x00]
# CHECK-DISASM: 0b 00 3e 00   frep.i  t3, 4, 0, 0
frep.i t3, 4, 0, 0b0000


