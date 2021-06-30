; REQUIRES: ld_emu_elf32lriscv

; RUN: rm -f %t*.o
;; Test mixed-mode LTO (mix of regular and thin LTO objects)
; RUN: opt %s -o %t.o
; RUN: opt -module-summary %p/Inputs/mixed_lto.ll -o %t2.o

; RUN: %gold -m elf32lriscv -plugin %llvmshlibdir/LLVMgold%shlibext \
; RUN:     --plugin-opt=thinlto \
; RUN:     -shared --plugin-opt=-import-instr-limit=0 \
; RUN:     -o %t3.o %t2.o %t.o
; RUN: llvm-nm %t3.o | FileCheck %s
;; ChecK target ABI info
; RUN: llvm-readelf -h %t3.o | FileCheck %s --check-prefix=CHECK-ABI

; RUN: opt %s -module-summary -o %thin0.o
; RUN: opt %p/Inputs/mixed_lto.ll -module-summary -o %thin1.o
; RUN: %gold -m elf32lriscv -plugin %llvmshlibdir/LLVMgold%shlibext \
; RUN:     --plugin-opt=thinlto \
; RUN:     -shared --plugin-opt=-import-instr-limit=0 \
; RUN:     -o %thin2.o %thin0.o %thin1.o
; RUN: llvm-nm %t3.o | FileCheck %s
;; ChecK target ABI info
; RUN: llvm-readelf -h %t3.o | FileCheck %s --check-prefix=CHECK-ABI
; RUN: llvm-readelf -h %thin2.o | FileCheck %s --check-prefix=CHECK-ABI

;; Test ThinLTO
; CHECK-DAG: T main
; CHECK-DAG: T g

; CHECK-ABI: Flags: 0x2, single-float ABI


target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-linux-gnu"
define i32 @g() #0 {
  ret i32 0
}

attributes #0 = { nounwind "target-features"="+a,+c,+f,+m,+relax" }

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"target-abi", !"ilp32f"}