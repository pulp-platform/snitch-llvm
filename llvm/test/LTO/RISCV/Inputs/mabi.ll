target datalayout = "e-m:e-p:32:32-i64:64-n32-S128"
target triple = "riscv32-unknown-linux-gnu"

define float @foo(float %x) #0 {
  %conv = fpext float %x to double
  %add = fadd double %conv, 0x400921FD80C9BEFB
  %conv1 = fptrunc double %add to float
  ret float %conv1
}

attributes #0 = { nounwind "target-features"="+a,+c,+f,+m,+relax" }

!llvm.module.flags = !{!0}
!0 = !{i32 1, !"target-abi", !"ilp32f"}