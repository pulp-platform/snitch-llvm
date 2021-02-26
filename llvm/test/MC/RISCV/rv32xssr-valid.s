# RUN: llvm-mc %s -triple=riscv32 --mattr=xssr -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -filetype=obj -triple=riscv32  --mattr=xssr \
# RUN:     | llvm-objdump -M no-aliases -d -r --mattr=xssr - \
# RUN:     | FileCheck -check-prefixes=CHECK-DISASM %s

# CHECK-ASM-AND-OBJ: scfgri t0, 2750
# CHECK-ASM: encoding: [0xab,0x12,0xe0,0xab]
# CHECK-DISASM: ab 12 e0 ab   scfgri  t0, 2750
scfgri t0, 0b101010111110
# CHECK-ASM-AND-OBJ: scfgwi t0, 2750
# CHECK-ASM: encoding: [0x2b,0xa0,0xe2,0xab]
# CHECK-DISASM: 2b a0 e2 ab   scfgwi  t0, 2750
scfgwi t0, 0b101010111110
# CHECK-ASM-AND-OBJ: scfgr t0, t1
# CHECK-ASM: encoding: [0xab,0x92,0x60,0x00]
# CHECK-DISASM: ab 92 60 00   scfgr  t0, t1
scfgr t0, t1
# CHECK-ASM-AND-OBJ: scfgw t0, t1
# CHECK-ASM: encoding: [0xab,0xa0,0x62,0x00]
# CHECK-DISASM: ab a0 62 00   scfgw t0, t1
scfgw t0, t1
