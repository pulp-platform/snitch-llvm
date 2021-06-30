target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-linux-gnu"

declare float @g() #1
define i32 @main() #0 {
  call float @g()
  ret i32 0
}

attributes #0 = { nounwind "target-features"="+a,+c,+f,+m,+relax" }
attributes #1 = { nounwind "target-features"="+a,+c,+f,+m,+relax" }

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"target-abi", !"ilp32f"}