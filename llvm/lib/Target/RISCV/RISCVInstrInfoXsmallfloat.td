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

let Predicates = [HasExtXsmallfloat] in {
include "RISCVInstrInfoXsmallfloatGen.td"
} // Predicates = [HasExtXsmallfloat]


//===----------------------------------------------------------------------===//
// Pseudo Instructions
//===----------------------------------------------------------------------===//

//let Predicates = [HasExtXsmallfloat] in {
//  let Predicates = [HasStdExtZfh] in {
//  }
//}
