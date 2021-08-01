# REQUIRES: x86
# RUN: llvm-mc -filetype=obj -triple=x86_64-pc-linux %s -o %t.o
# RUN: echo "SECTIONS { \
# RUN:   . = SIZEOF_HEADERS; \
# RUN:   .text : { *(.text) } \
# RUN:   foo : { __tbss_start = .; *(foo) __tbss_end = .; } \
# RUN:   bar : { *(bar) } \
# RUN: }" > %t.script
# RUN: ld.lld -T %t.script %t.o -o %t
# RUN: llvm-readelf -S -s %t | FileCheck %s

## Test that a tbss section doesn't affect the start address of the next section.

# CHECK: Name  Type     Address              Off                Size   ES Flg
# CHECK: foo   NOBITS   [[#%x,ADDR:]]        [[#%x,OFF:]]       000004 00 WAT
# CHECK: bar   PROGBITS {{0+}}[[#%x,ADDR]]   {{0+}}[[#%x,OFF]]  000004 00  WA

## Test that . in a tbss section represents the current location, even if the
## address will be reset.

# CHECK: Value                {{.*}} Name
# CHECK: {{0+}}[[#%x,ADDR]]   {{.*}} __tbss_start
# CHECK: {{0+}}[[#%x,ADDR+4]] {{.*}} __tbss_end

        .section foo,"awT",@nobits
        .long   0
        .section bar, "aw"
        .long 0
