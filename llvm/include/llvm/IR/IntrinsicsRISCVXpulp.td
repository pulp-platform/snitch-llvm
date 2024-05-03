//===- IntrinsicsRISCVXpulp.td - PULP intrinsics -----------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the PULP vendor intrinsics for RISC-V.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "riscv" in {

  multiclass PulpIntrinsic<list<LLVMType> ret_types,
                           list<LLVMType> param_types,
                           list<IntrinsicProperty> intr_properties> {

    def int_riscv_pulp_ #NAME
        : ClangBuiltin<"__builtin_pulp_" #NAME>,
          Intrinsic<ret_types, param_types, intr_properties>;
  }

  multiclass PulpUnary32
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty], [IntrNoMem]>;
  multiclass PulpBinary32
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
  multiclass PulpTernary32
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem]>;

  defm fl1 : PulpUnary32;
  defm ff1 : PulpUnary32;
  defm clb : PulpUnary32;
  defm cnt : PulpUnary32;
  defm parity : PulpUnary32;
  defm abs : PulpUnary32;

  defm mac : PulpTernary32;
  defm msu : PulpTernary32;

  defm macs : PulpTernary32;
  defm macu : PulpTernary32;

  defm machhs : PulpTernary32;
  defm machhu : PulpTernary32;

  defm rotr : PulpBinary32;

  defm maxsi : PulpBinary32;
  defm minsi : PulpBinary32;
  defm maxusi : PulpBinary32;
  defm minusi : PulpBinary32;

  multiclass PulpBinaryV2
      : PulpIntrinsic<[llvm_v2i16_ty], [llvm_v2i16_ty, llvm_v2i16_ty],
                      [IntrNoMem]>;
  multiclass PulpBinaryV4
      : PulpIntrinsic<[llvm_v4i8_ty], [llvm_v4i8_ty, llvm_v4i8_ty],
                      [IntrNoMem]>;

  multiclass PulpBinaryV {
    defm "2" : PulpBinaryV2;
    defm "4" : PulpBinaryV4;
  }

  multiclass PulpUnaryV2
      : PulpIntrinsic<[llvm_v2i16_ty], [llvm_v2i16_ty], [IntrNoMem]>;
  multiclass PulpUnaryV4
      : PulpIntrinsic<[llvm_v4i8_ty], [llvm_v4i8_ty], [IntrNoMem]>;

  multiclass PulpUnaryV {
    defm "2" : PulpUnaryV2;
    defm "4" : PulpUnaryV4;
  }

  defm add : PulpBinaryV;
  defm sub : PulpBinaryV;
  defm avg : PulpBinaryV;
  defm avgu : PulpBinaryV;
  defm min : PulpBinaryV;
  defm minu : PulpBinaryV;
  defm max : PulpBinaryV;
  defm maxu : PulpBinaryV;
  defm and : PulpBinaryV;
  defm or : PulpBinaryV;
  defm exor : PulpBinaryV;
  defm srl : PulpBinaryV;
  defm sra : PulpBinaryV;
  defm sll : PulpBinaryV;
  defm abs : PulpUnaryV;
  defm neg : PulpUnaryV;

  multiclass PulpDotp {
    defm p2 : PulpIntrinsic<[llvm_i32_ty], [llvm_v2i16_ty, llvm_v2i16_ty],
                            [IntrNoMem]>;
    defm psc2 : PulpIntrinsic<[llvm_i32_ty], [llvm_v2i16_ty, llvm_i32_ty],
                              [IntrNoMem]>;
    defm p4 : PulpIntrinsic<[llvm_i32_ty], [llvm_v4i8_ty, llvm_v4i8_ty],
                            [IntrNoMem]>;
    defm psc4 : PulpIntrinsic<[llvm_i32_ty], [llvm_v4i8_ty, llvm_i32_ty],
                              [IntrNoMem]>;
    defm s #NAME #p2
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_v2i16_ty, llvm_v2i16_ty, llvm_i32_ty],
                        [IntrNoMem]>;
    defm s #NAME #psc2
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_v2i16_ty, llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
    defm s #NAME #p4
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_v4i8_ty, llvm_v4i8_ty, llvm_i32_ty], [IntrNoMem]>;
    defm s #NAME #psc4
        : PulpIntrinsic<[llvm_i32_ty], [llvm_v4i8_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem]>;
  }

  defm dots : PulpDotp;
  defm dotu : PulpDotp;
  defm dotus : PulpDotp;

  defm muls : PulpBinary32;
  defm mulu : PulpBinary32;
  defm mulhhs : PulpBinary32;
  defm mulhhu : PulpBinary32;

  multiclass mul_mac_N_RN {
    defm mul #NAME #N
        : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem, ImmArg<ArgIndex<2>>]>;
    defm mul #NAME #RN
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem, ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;
    defm mac #NAME #N
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem, ImmArg<ArgIndex<3>>]>;
    defm mac #NAME #RN
        : PulpIntrinsic<
              [llvm_i32_ty],
              [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
              [IntrNoMem, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;
  }

  defm s : mul_mac_N_RN;
  defm u : mul_mac_N_RN;
  defm hhs : mul_mac_N_RN;
  defm hhu : mul_mac_N_RN;

  multiclass N_RN_r {
    // <op>N: op with normalization factor as immediate
    defm N
        : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem, ImmArg<ArgIndex<2>>]>;
    // <op>N_r: op with normalization factor
    defm N_r
        : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem]>;
    // <op>RN: op with normalization and round factors as immediates
    defm RN
        : PulpIntrinsic<[llvm_i32_ty],
                        [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem, ImmArg<ArgIndex<2>>, ImmArg<ArgIndex<3>>]>;
    // <op>RN: op with normalization and round factors as immediates
    defm RN_r
        : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                        [IntrNoMem]>;
  }

  defm add : N_RN_r;
  defm addu : N_RN_r;
  defm sub : N_RN_r;
  defm subu : N_RN_r;

  defm pack2 : PulpIntrinsic<[llvm_v2i16_ty], [llvm_i32_ty, llvm_i32_ty], [IntrNoMem]>;
  defm pack4 : PulpIntrinsic<[llvm_v4i8_ty],
                             [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                             [IntrNoMem]>;
  defm shuffleh : PulpBinaryV2;
  defm shuffleb : PulpBinaryV4;
  defm pack4_lo : PulpIntrinsic<[llvm_v4i8_ty], [llvm_i32_ty, llvm_i32_ty, llvm_v4i8_ty],
                      [IntrNoMem]>;
  defm pack4_hi : PulpIntrinsic<[llvm_v4i8_ty], [llvm_i32_ty, llvm_i32_ty, llvm_v4i8_ty],
                      [IntrNoMem]>;
  defm shuffle4b
      : PulpIntrinsic<[llvm_v4i8_ty],
                      [llvm_v4i8_ty, llvm_v4i8_ty, llvm_v4i8_ty], [IntrNoMem]>;
  defm shuffle2h : PulpIntrinsic<[llvm_v2i16_ty],
                                 [llvm_v2i16_ty, llvm_v2i16_ty, llvm_v2i16_ty],
                                 [IntrNoMem]>;

  defm bclr : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty],
                            [IntrNoMem, ImmArg<ArgIndex<1>>]>;
  defm bclr_r : PulpBinary32;
  defm bset : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty],
                            [IntrNoMem, ImmArg<ArgIndex<1>>]>;
  defm bset_r : PulpBinary32;
  defm bextract : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                                [IntrNoMem, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>]>;
  defm bextract_r : PulpBinary32;
  defm bextractu : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                                 [IntrNoMem, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>]>;
  defm bextractu_r : PulpBinary32;
  defm binsert : PulpIntrinsic<[llvm_i32_ty],
                               [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                               [IntrNoMem, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<3>>, ImmArg<ArgIndex<4>>]>;
  defm binsert_r : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                                 [IntrNoMem]>;
  defm clip : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                            [IntrNoMem, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>]>;
  defm clip_r : PulpBinary32;
  defm clipu : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                             [IntrNoMem, ImmArg<ArgIndex<1>>, ImmArg<ArgIndex<2>>]>;
  defm clipu_r : PulpBinary32;

  // PULP Memory Intrinsics

  multiclass PulpReadMemIntrinsic
      : PulpIntrinsic<[llvm_i32_ty],
                      [llvm_ptr_ty, llvm_i32_ty],
                      [IntrReadMem, ReadOnly<ArgIndex<0>>,
                      NoCapture<ArgIndex<0>>]>;

  defm OffsetedRead : PulpReadMemIntrinsic;
  defm OffsetedReadHalf : PulpReadMemIntrinsic;
  defm OffsetedReadByte : PulpReadMemIntrinsic;
  defm read_base_off : PulpReadMemIntrinsic;
  defm read_base_off_v : PulpReadMemIntrinsic;

  multiclass PulpWriteMemIntrinsic
      : PulpIntrinsic<[],
                      [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty],
                      [IntrWriteMem, WriteOnly<ArgIndex<1>>,
                      NoCapture<ArgIndex<1>>]>;

  defm OffsetedWrite : PulpWriteMemIntrinsic;
  defm OffsetedWriteHalf : PulpWriteMemIntrinsic;
  defm OffsetedWriteByte : PulpWriteMemIntrinsic;
  defm write_base_off : PulpWriteMemIntrinsic;
  defm write_base_off_v : PulpWriteMemIntrinsic;

  // PULP SPR Intrinsics

  defm spr_read
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty], [IntrNoMem,
                                                    ImmArg<ArgIndex<0>>]>;
  defm spr_read_vol : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty],
                                    [IntrNoMem, IntrHasSideEffects,
                                    ImmArg<ArgIndex<0>>]>;
  defm spr_write : PulpIntrinsic<[], [llvm_i32_ty, llvm_i32_ty],
                                 [IntrNoMem, IntrHasSideEffects,
                                 ImmArg<ArgIndex<0>>]>;
  defm spr_bit_set
      : PulpIntrinsic<[], [llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<0>>,
                      ImmArg<ArgIndex<1>>]>;
  defm spr_bit_clr
      : PulpIntrinsic<[], [llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<0>>,
                      ImmArg<ArgIndex<1>>]>;
  defm read_then_spr_write
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<0>>]>;
  defm read_then_spr_bit_set
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<0>>,
                      ImmArg<ArgIndex<1>>]>;
  defm read_then_spr_bit_clr
      : PulpIntrinsic<[llvm_i32_ty], [llvm_i32_ty, llvm_i32_ty],
                      [IntrNoMem, IntrHasSideEffects, ImmArg<ArgIndex<0>>,
                      ImmArg<ArgIndex<1>>]>;

  defm CoreId : PulpIntrinsic<[llvm_i32_ty], [], [IntrNoMem]>;
  defm CoreCount : PulpIntrinsic<[llvm_i32_ty], [], [IntrNoMem]>;
  defm ClusterId : PulpIntrinsic<[llvm_i32_ty], [], [IntrNoMem]>;
  defm IsFc : PulpIntrinsic<[llvm_i32_ty], [], [IntrNoMem]>;
  defm HasFc : PulpIntrinsic<[llvm_i32_ty], [], [IntrNoMem]>;

  // FIXME: should probably need a llvm_anyptr_ty here, but that does not work
  def int_riscv_pulp_event_unit_read
    : ClangBuiltin<"__builtin_pulp_event_unit_read">,
      Intrinsic<[llvm_i32_ty],
                [llvm_ptr_ty, llvm_i32_ty],
                [IntrHasSideEffects]>;

  def int_riscv_pulp_bitinsert
    : ClangBuiltin<"__builtin_bitinsert">, // NOTE: GCC has no pulp here
      Intrinsic<[llvm_i32_ty],
                [llvm_i32_ty, llvm_i32_ty, llvm_i32_ty, llvm_i32_ty],
                [IntrNoMem]>;
}
