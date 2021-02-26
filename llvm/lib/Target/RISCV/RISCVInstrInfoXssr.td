//===-- RISCVInstrInfoXfrep.td - RISC-V 'Xfrep' instructions ---*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the RISC-V instructions from the custom 'Xssr'
// instruction set extension.
//
//===----------------------------------------------------------------------===//

def OPC_CUSTOM1     : RISCVOpcode<0b0101011>;

let hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {

  class SSRInstImmRead<bits<3> funct3, string opcodestr>
      : RVInst<(outs GPR:$rd), (ins uimm12:$uimm12), opcodestr, "$rd, $uimm12", [], InstFormatOther> {
    bits<12> uimm12;
    bits<5> rd;
    bits<7> funct7;

    let Inst{31-25} = uimm12{11-5};
    let Inst{24-20} = uimm12{4-0};
    let Inst{19-15} = 0b00000; // rs1 = 0
    let Inst{14-12} = funct3; 
    let Inst{11-7}  = rd;
    let Opcode = OPC_CUSTOM1.Value;
  }

  class SSRInstImmWrite<bits<3> funct3, string opcodestr>
      : RVInst<(outs), (ins GPR:$rs1, uimm12:$uimm12), opcodestr, "$rs1, $uimm12", [], InstFormatOther> {
    bits<12> uimm12;
    bits<5> rs1;
    bits<7> funct7;

    let Inst{31-25} = uimm12{11-5};
    let Inst{24-20} = uimm12{4-0};
    let Inst{19-15} = rs1;
    let Inst{14-12} = funct3; 
    let Inst{11-7}  = 0b00000; // rd = 0
    let Opcode = OPC_CUSTOM1.Value;
  }

  class SSRInstRegRead<bits<7> funct7, bits<3> funct3, string opcodestr>
    : RVInstR<funct7, funct3, OPC_CUSTOM1, (outs GPR:$rd),
              (ins GPR:$rs2), opcodestr, "$rd, $rs2"> {
    let Inst{19-15} = 0b00001;  // rs1 = 0b00001
  }
  class SSRInstRegWrite<bits<7> funct7, bits<3> funct3, string opcodestr>
    : RVInstR<funct7, funct3, OPC_CUSTOM1, (outs),
              (ins GPR:$rs1, GPR:$rs2), opcodestr, "$rs1, $rs2"> {
    let Inst{11-7} = 0b00001;  // rd = 0b00001
  }
}// hasSideEffects = 1, mayLoad = 0, mayStore = 0

//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasExtXssr] in {
  def SCFGRI : SSRInstImmRead  <0b001, "scfgri">, Sched<[]>;
  def SCFGWI : SSRInstImmWrite <0b010, "scfgwi">, Sched<[]>;
  def SCFGR  : SSRInstRegRead  <0b0000000, 0b001, "scfgr">,  Sched<[]>;
  def SCFGW  : SSRInstRegWrite <0b0000000, 0b010, "scfgw">,  Sched<[]>;
} // Predicates = [HasExtXssr]
