//===-- RISCVInstrInfoXsmallfloat.td - RISC-V smallfloat instructions ---*- tablegen -*-===//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Formats
//===----------------------------------------------------------------------===//

// vector float format
class RISCVVFFormat<bits<2> val> {
  bits<2> Value = val;
}
def VFMT_FP32     : RISCVVFFormat<0b00>;
def VFMT_FP16     : RISCVVFFormat<0b10>;
def VFMT_FP8      : RISCVVFFormat<0b11>;

//===----------------------------------------------------------------------===//
// Instruction formats
//===----------------------------------------------------------------------===//

class RVInstRVf<bits<2> funct2, bits<5> vecfltop, bits<1> r, RISCVVFFormat vfmt, RISCVOpcode opcode, dag outs, dag ins,
                 string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatR> {
  bits<5> rs2;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-30} = funct2;
  let Inst{29-25} = vecfltop;
  let Inst{24-20} = rs2;
  let Inst{19-15} = rs1;
  let Inst{14} = r;
  let Inst{13-12} = vfmt.Value;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

include "RISCVInstrInfoXsmallfloatGen.td"

//===----------------------------------------------------------------------===//
// Assembler Pseudo Instructions
//===----------------------------------------------------------------------===//

// load/stores with zero offset
let Predicates = [HasExtXfalthalf] in {
def : InstAlias<"flah $rd, (${rs1})",  (FLH FPR16:$rd,  GPR:$rs1, 0), 0>;
def : InstAlias<"fsah $rs2, (${rs1})", (FSH FPR16:$rs2, GPR:$rs1, 0), 0>;
}
let Predicates = [HasExtXfquarter] in {
def : InstAlias<"flb $rd, (${rs1})",  (FLB FPR16:$rd,  GPR:$rs1, 0), 0>;
def : InstAlias<"fsb $rs2, (${rs1})", (FSB FPR16:$rs2, GPR:$rs1, 0), 0>;
}
let Predicates = [HasExtXfaltquarter] in {
def : InstAlias<"flab $rd, (${rs1})",  (FLB FPR16:$rd,  GPR:$rs1, 0), 0>;
def : InstAlias<"fsab $rs2, (${rs1})", (FSB FPR16:$rs2, GPR:$rs1, 0), 0>;
}

// fmv, fabs, fneg
let Predicates = [HasExtXfalthalf] in {
def : InstAlias<"fmv.ah $rd, $rs",  (FSGNJ_H  FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fabs.ah $rd, $rs", (FSGNJX_H FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fneg.ah $rd, $rs", (FSGNJN_H FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
}
let Predicates = [HasExtXfquarter] in {
def : InstAlias<"fmv.b $rd, $rs",  (FSGNJ_B  FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fabs.b $rd, $rs", (FSGNJX_B FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fneg.b $rd, $rs", (FSGNJN_B FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
}
let Predicates = [HasExtXfaltquarter] in {
def : InstAlias<"fmv.ab $rd, $rs",  (FSGNJ_B  FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fabs.ab $rd, $rs", (FSGNJX_B FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
def : InstAlias<"fneg.ab $rd, $rs", (FSGNJN_B FPR16:$rd, FPR16:$rs, FPR16:$rs)>;
}

// pack
let Predicates = [HasExtXfvecsingle] in {
def : InstAlias<"vfcpk.s.s.0 $rd, $rs1, $rs2", (VFCPKA_S_S FPR32:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.s.d.0 $rd, $rs1, $rs2", (VFCPKA_S_D FPR32:$rd, FPR64:$rs1, FPR64:$rs2)>;
}
let Predicates = [HasExtXfvechalf] in {
def : InstAlias<"vfcpk.h.s.0 $rd, $rs1, $rs2", (VFCPKA_H_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.h.s.1 $rd, $rs1, $rs2", (VFCPKB_H_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.h.d.0 $rd, $rs1, $rs2", (VFCPKA_H_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
def : InstAlias<"vfcpk.h.d.1 $rd, $rs1, $rs2", (VFCPKB_H_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
}
let Predicates = [HasExtXfvecalthalf] in {
def : InstAlias<"vfcpk.ah.s.0 $rd, $rs1, $rs2", (VFCPKA_H_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.ah.s.1 $rd, $rs1, $rs2", (VFCPKB_H_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.ah.d.0 $rd, $rs1, $rs2", (VFCPKA_H_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
def : InstAlias<"vfcpk.ah.d.1 $rd, $rs1, $rs2", (VFCPKB_H_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
}
let Predicates = [HasExtXfvecquarter] in {
def : InstAlias<"vfcpk.b.s.0 $rd, $rs1, $rs2", (VFCPKA_B_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.b.s.1 $rd, $rs1, $rs2", (VFCPKB_B_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.b.s.2 $rd, $rs1, $rs2", (VFCPKC_B_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.b.s.3 $rd, $rs1, $rs2", (VFCPKD_B_S FPR16:$rd, FPR32:$rs1, FPR32:$rs2)>;
def : InstAlias<"vfcpk.b.d.0 $rd, $rs1, $rs2", (VFCPKA_B_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
def : InstAlias<"vfcpk.b.d.1 $rd, $rs1, $rs2", (VFCPKB_B_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
def : InstAlias<"vfcpk.b.d.2 $rd, $rs1, $rs2", (VFCPKC_B_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
def : InstAlias<"vfcpk.b.d.3 $rd, $rs1, $rs2", (VFCPKD_B_D FPR16:$rd, FPR64:$rs1, FPR64:$rs2)>;
}
