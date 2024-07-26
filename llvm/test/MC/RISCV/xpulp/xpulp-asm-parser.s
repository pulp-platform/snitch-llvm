# RUN: llvm-mc %s -filetype=obj -triple=riscv32 -mattr=+xpulpv \
# RUN:   | llvm-objdump -M no-aliases -d -r --no-print-imm-hex --mattr=+xpulpv - \
# RUN:   | FileCheck %s

# CHECK: p.lw    ra, 0(sp!)
p.lw x1, 0(x2!)

# CHECK: p.lw    ra, sp(gp) 
p.lw x1, x2(x3)

