# RUN: llvm-mc %s -triple=riscv32 --mattr=xmempool -riscv-no-aliases -show-encoding \
# RUN:     | FileCheck -check-prefixes=CHECK-ASM,CHECK-ASM-AND-OBJ %s
# RUN: llvm-mc %s -filetype=obj -triple=riscv32  --mattr=xmempool \
# RUN:     | llvm-objdump -M no-aliases -d -r --mattr=xmempool - \
# RUN:     | FileCheck -check-prefixes=CHECK-DISASM %s

# CHECK-ASM-AND-OBJ: csrrs t0, trace, zero
# CHECK-ASM: encoding: [0xf3,0x22,0x00,0x7d]
# CHECK-DISASM: f3 22 00 7d   csrrs t0, trace, zero
csrr    t0, trace
