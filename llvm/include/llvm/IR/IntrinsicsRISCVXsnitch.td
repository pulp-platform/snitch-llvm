//===- IntrinsicsRISCVXsnitch.td - Snitch intrinsics --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Snitch vendor intrinsics for RISC-V, a.k.a.
// Xssr, Xfrep and Xsdma.
//
//===----------------------------------------------------------------------===//
class RISCVSSRIntrinsic {}

class RISCVSDMAIntrinsic {}

let TargetPrefix = "riscv" in {

  //===----------------------------------------------------------------------===//
  // Xssr extension

  def int_riscv_ssr_setup_1d_r
      : ClangBuiltin<"__builtin_ssr_setup_1d_r">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_setup_1d_w
      : ClangBuiltin<"__builtin_ssr_setup_1d_w">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_read
      : ClangBuiltin<"__builtin_ssr_read">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_write
      : ClangBuiltin<"__builtin_ssr_write">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_read_imm
      : ClangBuiltin<"__builtin_ssr_read_imm">,
        /* RetTypes, ParamTypes, Properties */
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects, ImmArg<ArgIndex<0>>, ImmArg<ArgIndex<1>>]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_write_imm
      : ClangBuiltin<"__builtin_ssr_write_imm">,
        /* RetTypes, ParamTypes, Properties */
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty, llvm_ptr_ty],
                  [IntrHasSideEffects, ImmArg<ArgIndex<0>>, ImmArg<ArgIndex<1>>]>,
        RISCVSSRIntrinsic;

  // The `Throws` attribute ensures that the push/pop don't get removed from loops
  // by the LICM pass
  // TODO: Is there another way to do this?
  def int_riscv_ssr_push
      : ClangBuiltin<"__builtin_ssr_push">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_double_ty],
                  [IntrWriteMem, IntrHasSideEffects, Throws, ImmArg<ArgIndex<0>>]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_pop
      : ClangBuiltin<"__builtin_ssr_pop">,
        Intrinsic<[llvm_double_ty],
                  [llvm_i32_ty],
                  [IntrReadMem, IntrHasSideEffects, Throws, ImmArg<ArgIndex<0>>]>,
        RISCVSSRIntrinsic;

  def int_riscv_ssr_enable
      : ClangBuiltin<"__builtin_ssr_enable">,
        Intrinsic<[], [], [IntrHasSideEffects]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_disable
      : ClangBuiltin<"__builtin_ssr_disable">,
        Intrinsic<[], [], [IntrHasSideEffects]>,
        RISCVSSRIntrinsic;
  foreach dim = [1, 2, 3, 4] in {
    def int_riscv_ssr_setup_bound_stride_#dim#d
        : ClangBuiltin<"__builtin_ssr_setup_bound_stride_"#dim#"d">,
          Intrinsic<[], 
                    [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                    [IntrHasSideEffects]>,
          RISCVSSRIntrinsic;
  }
  def int_riscv_ssr_setup_repetition
      : ClangBuiltin<"__builtin_ssr_setup_repetition">,
        Intrinsic<[],
                  [llvm_i32_ty, llvm_i32_ty],
                  [IntrHasSideEffects]>,
        RISCVSSRIntrinsic;
  def int_riscv_ssr_barrier
      : ClangBuiltin<"__builtin_ssr_barrier">,
        Intrinsic<[], [llvm_i32_ty], [IntrHasSideEffects, ImmArg<ArgIndex<0>>]>, RISCVSSRIntrinsic;

  //===----------------------------------------------------------------------===//
  // Xsdma extension

  def int_riscv_sdma_start_oned
      : ClangBuiltin<"__builtin_sdma_start_oned">,
        Intrinsic<[llvm_i32_ty],
                  [llvm_i64_ty, llvm_i64_ty, llvm_i32_ty, llvm_i32_ty],
                  [IntrHasSideEffects]>,
        RISCVSDMAIntrinsic;
  // split 64 into 32 hi/low for src/dst addresses which can be inserted during
  // DAG legalization
  def int_riscv_sdma_start_oned_legal
      : Intrinsic<[llvm_i32_ty],
                  [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                  [IntrHasSideEffects]>,
        RISCVSDMAIntrinsic;

  def int_riscv_sdma_start_twod
      : ClangBuiltin<"__builtin_sdma_start_twod">,
        Intrinsic<[llvm_i32_ty],
                  [llvm_i64_ty, llvm_i64_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                  [IntrHasSideEffects]>,
        RISCVSDMAIntrinsic;
  def int_riscv_sdma_start_twod_legal
      : Intrinsic<[llvm_i32_ty],
                  [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                  [IntrHasSideEffects]>,
        RISCVSDMAIntrinsic;
  def int_riscv_sdma_stat
      : ClangBuiltin<"__builtin_sdma_stat">,
        Intrinsic<[llvm_i32_ty], [llvm_i32_ty], [IntrHasSideEffects]>,
        RISCVSDMAIntrinsic;
  def int_riscv_sdma_wait_for_idle
      : ClangBuiltin<"__builtin_sdma_wait_for_idle">,
        Intrinsic<[], [], [IntrHasSideEffects]>, RISCVSDMAIntrinsic;

  //===----------------------------------------------------------------------===//
  // Xfrep extension

  def int_riscv_frep_infer : Intrinsic<[], [], []>;

}
