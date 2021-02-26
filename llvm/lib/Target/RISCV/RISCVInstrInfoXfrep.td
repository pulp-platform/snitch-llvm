//===-- RISCVInstrInfoXfrep.td - RISC-V 'Xfrep' instructions ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the RISC-V instructions from the custom 'Xfrep'
// instruction set extension.
//
//===----------------------------------------------------------------------===//

class RVInstFrep<bit is_outer, dag outs, dag ins, string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatOther> {
  bits<12> num_inst;
  bits<3> stagger_max;
  bits<4> stagger_mask;
  bits<5> rs1;

  let Inst{31-20} = num_inst;
  let Inst{19-15} = rs1;
  let Inst{14-12} = stagger_max;
  let Inst{11-8} = stagger_mask;
  let Inst{7} = is_outer;
  let Opcode = 0b0001011;
}

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasExtXfrep] in
let hasSideEffects = 0 in
let mayLoad = 0 in
let mayStore = 0 in {
  def FREP_O : RVInstFrep<0b1,  (outs),
                (ins GPR:$rs1, uimm12m1:$num_inst, uimm3:$stagger_max, uimm4:$stagger_mask),
                          "frep.o", "$rs1, $num_inst, ${stagger_max}, ${stagger_mask}">, Sched<[]>;
  def FREP_I : RVInstFrep<0b0, (outs),
                (ins GPR:$rs1, uimm12m1:$num_inst, uimm3:$stagger_max, uimm4:$stagger_mask),
                          "frep.i", "$rs1, $num_inst, ${stagger_max}, ${stagger_mask}">, Sched<[]>;
}
