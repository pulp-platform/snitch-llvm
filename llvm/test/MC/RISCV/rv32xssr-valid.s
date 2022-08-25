# RUN: llvm-mc %s -triple=riscv32 --mattr=xssr -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -filetype=obj -triple=riscv32  --mattr=xssr \
# RUN:     | llvm-objdump -M no-aliases -d -r --mattr=xssr - \
# RUN:     | FileCheck -check-prefixes=CHECK-DISASM %s

# CHECK-ASM-AND-OBJ: scfgri t0, 0
# CHECK-ASM: encoding: [0xab,0x12,0x00,0x00]
# CHECK-DISASM: ab 12 00 00   scfgri  t0, 0
scfgri t0, 0 | (0<<7)
# CHECK-ASM-AND-OBJ: scfgwi t0, 640
# CHECK-ASM: encoding: [0x2b,0xa0,0x02,0x28]
# CHECK-DISASM: 2b a0 02 28   scfgwi  t0, 640
scfgwi t0, 0 | (5<<7)
# CHECK-ASM-AND-OBJ: scfgri t0, 1025
# CHECK-ASM: encoding: [0xab,0x12,0x10,0x40]
# CHECK-DISASM: ab 12 10 40   scfgri  t0, 1025
scfgri t0, 1 | (8<<7)
# CHECK-ASM-AND-OBJ: scfgwi t0, 1281
# CHECK-ASM: encoding: [0x2b,0xa0,0x12,0x50]
# CHECK-DISASM: 2b a0 12 50   scfgwi  t0, 1281
scfgwi t0, 1 | (10<<7)
# CHECK-ASM-AND-OBJ: scfgr t0, t1
# CHECK-ASM: encoding: [0xab,0x92,0x60,0x00]
# CHECK-DISASM: ab 92 60 00   scfgr  t0, t1
scfgr t0, t1
# CHECK-ASM-AND-OBJ: scfgw t0, t1
# CHECK-ASM: encoding: [0xab,0xa0,0x62,0x00]
# CHECK-DISASM: ab a0 62 00   scfgw t0, t1
scfgw t0, t1
