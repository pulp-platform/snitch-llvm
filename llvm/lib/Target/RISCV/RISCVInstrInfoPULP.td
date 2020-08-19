//===-- RISCVInstrInfoPULP.td - RISC-V PULP extension ----- *- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the RISC-V instructions from the PULP extension.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Instruction class templates
//===----------------------------------------------------------------------===//

class RVInstPULPBit<bits<7> funct7, bits<3> funct3, RISCVOpcode opcode, dag outs,
                    dag ins, string opcodestr, string argstr>
    : RVInst<outs, ins, opcodestr, argstr, [], InstFormatOther> {
  bits<5> rs1;
  bits<5> ls2;
  bits<5> ls3;
  bits<5> rd;

  let Inst{31-30} = 0b11;
  let Inst{29-25} = ls3;
  let Inst{24-20} = ls2;
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}


//===----------------------------------------------------------------------===//
// Instructions
//===----------------------------------------------------------------------===//

let Predicates = [IsRV32, HasPULPExtV2] in {

let hasSideEffects = 1, mayStore = 1 in
def PULP_ELW : Load_ri<0b110, "p.elw">; // LWU in RV64

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def PULP_INSERT : RVInstPULPBit<0b0001000, 0b010, OPC_OP,
                                (outs GPR:$rd_wb),
                                (ins GPR:$rd, GPR:$rs1, uimm5:$ls3, uimm5:$ls2),
                                "p.insert", "$rd, $rs1, $ls3, $ls2"> {
  let Constraints = "$rd = $rd_wb";
}
let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
def PULP_FL1 : RVInstR<0b0001000, 0b001, OPC_OP,
                            (outs GPR:$rd), (ins GPR:$rs1),
                            "p.fl1", "$rd, $rs1"> {
  let rs2 = 0;
}

}

//===----------------------------------------------------------------------===//
// PULPv2 Hardware Loops
//===----------------------------------------------------------------------===//

def uimm12pcrel : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<12>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<12>;
  let DecoderMethod = "decodeUImmOperand<12>";
}

let Predicates = [IsRV32, HasPULPExtV2] in {
def LOOP0setup : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;
    bits<5> rs1;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = rs1;
    let Inst{14-12} = 0b100;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL, GPR:$rs1);
    dag OutOperandList = (outs);

    let AsmString = "lp.setup\tx0, $rs1, $uimmL";
}

def LOOP1setup : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;
    bits<5> rs1;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = rs1;
    let Inst{14-12} = 0b100;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL, GPR:$rs1);
    dag OutOperandList = (outs);

    let AsmString = "lp.setup\tx1, $rs1, $uimmL";
}

def LOOP0setupi : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;
    bits<5> uimmS;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = uimmS;
    let Inst{14-12} = 0b101;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm5:$uimmS, uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.setupi\tx0, $uimmL, $uimmS";

}

def LOOP1setupi : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;
    bits<5> uimmS;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = uimmS;
    let Inst{14-12} = 0b101;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm5:$uimmS, uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.setupi\tx1, $uimmL, $uimmS";

}

def LOOP0starti : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b000;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.starti\tx0, $uimmL";

}

def LOOP1starti : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b000;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.starti\tx1, $uimmL";

}

def LOOP0endi : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b001;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.endi\tx0, $uimmL";

}

def LOOP1endi : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b001;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.endi\tx1, $uimmL";

}

def LOOP0counti : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b011;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.counti\tx0, $uimmL";

}

def LOOP1counti : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<12> uimmL;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = uimmL;
    let Inst{19-15} = 0b00000;
    let Inst{14-12} = 0b011;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm12pcrel:$uimmL);
    dag OutOperandList = (outs);

    let AsmString = "lp.counti\tx1, $uimmL";

}

def LOOP0count : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<5> uimmS;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = 0b000000000000;
    let Inst{19-15} = uimmS;
    let Inst{14-12} = 0b010;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b0;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm5:$uimmS);
    dag OutOperandList = (outs);

    let AsmString = "lp.count\tx0, $uimmS";

}

def LOOP1count : Instruction {

    bits<32> Inst;
    bits<32> SoftFail = 0;
    bits<5> uimmS;

    let Namespace = "RISCV";
    let hasSideEffects = 1;
    let mayLoad = 0;
    let mayStore = 0;
    let Size = 4;
    let isTerminator = 1;

    let Inst{31-20} = 0b000000000000;
    let Inst{19-15} = uimmS;
    let Inst{14-12} = 0b010;
    let Inst{11-8} = 0b0000;
    let Inst{7} = 0b1;
    let Inst{6-0} = 0b1111011;

    dag InOperandList = (ins uimm5:$uimmS);
    dag OutOperandList = (outs);

    let AsmString = "lp.count\tx1, $uimmS";

}
}


//===----------------------------------------------------------------------===//
// Load/Store + Post Increment
//===----------------------------------------------------------------------===//

// Opcodes
def OPC_PULP_LPI : RISCVOpcode<0b0001011>;
def OPC_PULP_SPI : RISCVOpcode<0b0101011>;

// Instruction class: Stores with Immediate increment
let Predicates = [IsRV32, HasPULPExtV2] in {
let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in
class PULP_Store_rri_postinc<bits<3> funct3, string opcodestr>
    : RVInstS<funct3, OPC_PULP_SPI, (outs GPR:$rd),
              (ins GPR:$rs2, GPR:$rs1, simm12:$imm12),
              opcodestr, "$rs2, ${imm12}(${rs1}!)">;

// Instruction class: Stores with Register increment
let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in
class PULP_Store_rrr_postinc<bits<7> funct7, bits<3> funct3, string opcodestr>
    : RVInstR<funct7, funct3, OPC_PULP_SPI, (outs GPR:$rd),
              (ins GPR:$rs2, GPR:$rs1, GPR:$rs3),
              opcodestr, "$rs2, ${rs3}(${rs1}!)">;

// Instruction class: Loads with Immediate increment
let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
class PULP_Load_ri_postinc<bits<3> funct3, string opcodestr>
    : RVInstI<funct3, OPC_PULP_LPI, (outs GPR:$rd, GPR:$rs1_wb),
              (ins GPR:$rs1, simm12:$imm12),
              opcodestr, "$rd, ${imm12}(${rs1}!)">;

// Instruction class: Loads with Register increment
let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
class PULP_Load_rr_postinc<bits<7> funct7, bits<3> funct3, string opcodestr>
    : RVInstR<funct7, funct3, OPC_PULP_LPI, (outs GPR:$rd, GPR:$rs1_wb),
              (ins GPR:$rs1, GPR:$rs2),
              opcodestr, "$rd, ${rs2}(${rs1}!)">;
}

// Instructions: Stores with Immediate increment
def PULP_SB_IMM_PI : PULP_Store_rri_postinc<0b000, "p.sb">;
def PULP_SH_IMM_PI : PULP_Store_rri_postinc<0b001, "p.sh">;
def PULP_SW_IMM_PI : PULP_Store_rri_postinc<0b010, "p.sw">;

// Instructions: Stores with Register increment
def PULP_SB_REG_PI : PULP_Store_rrr_postinc<0b0000000, 0b100, "p.sb">;
def PULP_SH_REG_PI : PULP_Store_rrr_postinc<0b0000000, 0b101, "p.sh">;
def PULP_SW_REG_PI : PULP_Store_rrr_postinc<0b0000000, 0b110, "p.sw">;

// Instructions: Loads with Immediate increment
def PULP_LB_IMM_PI : PULP_Load_ri_postinc<0b000, "p.lb">;
def PULP_LBU_IMM_PI : PULP_Load_ri_postinc<0b100, "p.lbu">;
def PULP_LH_IMM_PI : PULP_Load_ri_postinc<0b001, "p.lh">;
def PULP_LHU_IMM_PI : PULP_Load_ri_postinc<0b101, "p.lhu">;
def PULP_LW_IMM_PI : PULP_Load_ri_postinc<0b010, "p.lw">;

// Instructions: Loads with Register increment
def PULP_LB_REG_PI : PULP_Load_rr_postinc<0b0000000, 0b111, "p.lb">;
def PULP_LBU_REG_PI : PULP_Load_rr_postinc<0b0100000, 0b111, "p.lbu">;
def PULP_LH_REG_PI : PULP_Load_rr_postinc<0b0001000, 0b111, "p.lh">;
def PULP_LHU_REG_PI : PULP_Load_rr_postinc<0b0101000, 0b111, "p.lhu">;
def PULP_LW_REG_PI : PULP_Load_rr_postinc<0b0010000, 0b111, "p.lw">;

// Patterns: Stores with Immediate increment
def : Pat<(post_truncsti8 GPR:$Rs2, GPR:$Rs1, simm12:$imm12),
          (PULP_SB_IMM_PI GPR:$Rs2, GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_truncsti16 GPR:$Rs2, GPR:$Rs1, simm12:$imm12),
          (PULP_SH_IMM_PI GPR:$Rs2, GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_store i32:$Rs2, GPR:$Rs1, simm12:$imm12),
          (PULP_SW_IMM_PI GPR:$Rs2, GPR:$Rs1, simm12:$imm12)>;

// Patterns: Stores with Register increment
def : Pat<(post_truncsti8 GPR:$Rs2, GPR:$Rs1, GPR:$rs3),
          (PULP_SB_REG_PI GPR:$Rs2, GPR:$Rs1, GPR:$rs3)>;
def : Pat<(post_truncsti16 GPR:$Rs2, GPR:$Rs1, GPR:$rs3),
          (PULP_SH_REG_PI GPR:$Rs2, GPR:$Rs1, GPR:$rs3)>;
def : Pat<(post_store i32:$Rs2, i32:$Rs1, GPR:$rs3),
          (PULP_SW_REG_PI GPR:$Rs2, GPR:$Rs1, GPR:$rs3)>;

// Patterns: Loads with Immediate increment
def : Pat<(post_loadi8 GPR:$Rs1, simm12:$imm12),
          (PULP_LB_IMM_PI GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_loadi8u GPR:$Rs1, simm12:$imm12),
          (PULP_LBU_IMM_PI GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_loadi16 GPR:$Rs1, simm12:$imm12),
          (PULP_LH_IMM_PI GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_loadi16u GPR:$Rs1, simm12:$imm12),
          (PULP_LHU_IMM_PI GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_loadi32 GPR:$Rs1, simm12:$imm12),
          (PULP_LW_IMM_PI GPR:$Rs1, simm12:$imm12)>;
def : Pat<(post_loadi32u GPR:$Rs1, simm12:$imm12),
          (PULP_LW_IMM_PI GPR:$Rs1, simm12:$imm12)>;

// Patterns: Loads with Register increment
def : Pat<(post_loadi8 GPR:$Rs1, GPR:$Rs2),
          (PULP_LB_REG_PI GPR:$Rs1, GPR:$Rs2)>;
def : Pat<(post_loadi8u GPR:$Rs1, GPR:$Rs2),
          (PULP_LBU_REG_PI GPR:$Rs1, GPR:$Rs2)>;
def : Pat<(post_loadi16 GPR:$Rs1, GPR:$Rs2),
          (PULP_LH_REG_PI GPR:$Rs1, GPR:$Rs2)>;
def : Pat<(post_loadi16u GPR:$Rs1, GPR:$Rs2),
          (PULP_LHU_REG_PI GPR:$Rs1, GPR:$Rs2)>;
def : Pat<(post_loadi32 GPR:$Rs1, GPR:$Rs2),
          (PULP_LW_REG_PI GPR:$Rs1, GPR:$Rs2)>;
def : Pat<(post_loadi32u GPR:$Rs1, GPR:$Rs2),
          (PULP_LW_REG_PI GPR:$Rs1, GPR:$Rs2)>;

//===----------------------------------------------------------------------===//
// Pseudo-instructions and codegen patterns
//===----------------------------------------------------------------------===//

def : Pat<(int_riscv_pulp_event_unit_read GPR:$rs, simm12:$simm12),
          (PULP_ELW GPR:$rs, simm12:$simm12)>;
def : Pat<(int_riscv_pulp_event_unit_read GPR:$rs, GPR:$off),
          (PULP_ELW (ADD GPR:$rs, GPR:$off), 0)>;
def : Pat<(int_riscv_pulp_fl1 GPR:$rs),
          (PULP_FL1 GPR:$rs)>;
def : Pat<(int_riscv_pulp_bitinsert GPR:$rd, GPR:$rs1, uimm5:$ls2, uimm5:$ls3),
          (PULP_INSERT GPR:$rd, GPR:$rs1, uimm5:$ls2, uimm5:$ls3)>;

//===----------------------------------------------------------------------===//
// Unary
//===----------------------------------------------------------------------===//

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_r<bits<7> funct7, bits<3> funct3,
                 RISCVOpcode opcode, string opcodestr>
    : RVInstR<funct7, funct3, opcode, (outs GPR:$rd), (ins GPR:$rs1),
              opcodestr, "$rd, $rs1">,
      Sched<[WriteIALU, ReadIALU]> {
  let rs2 = 0;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_rr<bits<7> funct7, bits<3> funct3, RISCVOpcode opcode, string opcodestr>
    : RVInstR<funct7, funct3, opcode, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2),
              opcodestr, "$rd, $rs1, $rs2">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]>;

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_ri<bits<7> funct7, bits<3> funct3, string opcodestr>
    : RVInstR<funct7, funct3, OPC_OP, (outs GPR:$rd), (ins GPR:$rs1, uimm5:$Is2),
              opcodestr, "$rd, $rs1, $Is2">,
      Sched<[WriteIALU, ReadIALU]> {
  bits<5> Is2;
  let Inst{24-20} = Is2;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_rii<bits<2> funct2, bits<3> funct3, string opcodestr>
    : RVInstI<funct3, OPC_OP, (outs GPR:$rd), (ins GPR:$rs1, uimm5:$Is3, uimm5:$Is2),
              opcodestr, "$rd, $rs1, $Is3, $Is2">,
      Sched<[WriteIALU, ReadIALU]> {
  bits<5> Is3;
  bits<5> Is2;

  let Inst{31-30} = funct2;
  let Inst{29-25} = Is3;
  let Inst{24-20} = Is2;
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_rri<bits<2> funct2, bits<3> funct3, RISCVOpcode opcode, string opcodestr>
    : RVInstI<funct3, opcode, (outs GPR:$rd), (ins GPR:$rs1, GPR:$rs2, uimm5:$Is3),
              opcodestr, "$rd, $rs1, $rs2, $Is3">,
      Sched<[WriteIALU, ReadIALU, ReadIALU]> {
  bits<5> rs2;
  bits<5> Is3;

  let Inst{31-30} = funct2;
  let Inst{29-25} = Is3;
  let Inst{24-20} = rs2;
}


let Predicates = [HasNonStdExtPulp] in {

// Bit manipulation operations
def P_EXTRACT :   Pulp_ALU_rii<0b11, 0b000, "p.extract">;
def P_EXTRACTU :  Pulp_ALU_rii<0b11, 0b001, "p.extractu">;
def P_INSERT :    Pulp_ALU_rii<0b11, 0b010, "p.insert">;
def P_BCLR :      Pulp_ALU_rii<0b11, 0b011, "p.bclr">;
def P_BSET :      Pulp_ALU_rii<0b11, 0b100, "p.bset">;

def P_EXTRACTR :  Pulp_ALU_rr<0b1000000, 0b000, OPC_OP, "p.extractr">;
def P_EXTRACTUR : Pulp_ALU_rr<0b1000000, 0b001, OPC_OP, "p.extractur">;
def P_INSERTR :   Pulp_ALU_rr<0b1000000, 0b010, OPC_OP, "p.insertr">;
def P_BCLRR :     Pulp_ALU_rr<0b1000000, 0b011, OPC_OP, "p.bclrr">;
def P_BSETR :     Pulp_ALU_rr<0b1000000, 0b100, OPC_OP, "p.bsetr">;

def P_ROR :       Pulp_ALU_rr<0b0000100, 0b101, OPC_OP, "p.ror">;
def P_FF1 :       Pulp_ALU_r<0b0001000, 0b000, OPC_OP, "p.ff1">;
def P_FL1 :       Pulp_ALU_r<0b0001000, 0b001, OPC_OP, "p.fl1">;
def P_CLB :       Pulp_ALU_r<0b0001000, 0b010, OPC_OP, "p.clb">;
def P_CNT :       Pulp_ALU_r<0b0001000, 0b011, OPC_OP, "p.cnt">;


// General ALU operations
def P_ABS :    Pulp_ALU_r<0b0000010, 0b000, OPC_OP, "p.abs">;
def P_SLET :   Pulp_ALU_rr<0b0000010, 0b010, OPC_OP, "p.slet">;
def P_SLETU :  Pulp_ALU_rr<0b0000010, 0b011, OPC_OP, "p.sletu">;
def P_MIN :    Pulp_ALU_rr<0b0000010, 0b100, OPC_OP, "p.min">;
def P_MINU :   Pulp_ALU_rr<0b0000010, 0b101, OPC_OP, "p.minu">;
def P_MAX :    Pulp_ALU_rr<0b0000010, 0b110, OPC_OP, "p.max">;
def P_MAXU :   Pulp_ALU_rr<0b0000010, 0b111, OPC_OP, "p.maxu">;
def P_EXTHS :  Pulp_ALU_r<0b0001000, 0b100, OPC_OP, "p.exths">;
def P_EXTHZ :  Pulp_ALU_r<0b0001000, 0b101, OPC_OP, "p.exthz">;
def P_EXTBS :  Pulp_ALU_r<0b0001000, 0b110, OPC_OP, "p.extbs">;
def P_EXTBZ :  Pulp_ALU_r<0b0001000, 0b111, OPC_OP, "p.extbz">;

def P_CLIP :   Pulp_ALU_ri<0b0001010, 0b001, "p.clip">;
def P_CLIPU :  Pulp_ALU_ri<0b0001010, 0b010, "p.clipu">;
def P_CLIPR :  Pulp_ALU_rr<0b0001010, 0b101, OPC_OP, "p.clipr">;
def P_CLIPUR : Pulp_ALU_rr<0b0001010, 0b110, OPC_OP, "p.clipur">;

def P_ADDN :    Pulp_ALU_rri<0b00, 0b010, RISCVOpcode<0b1011011>, "p.addn">;
def P_ADDUN :   Pulp_ALU_rri<0b10, 0b010, RISCVOpcode<0b1011011>, "p.addun">;
def P_ADDRN :   Pulp_ALU_rri<0b00, 0b110, RISCVOpcode<0b1011011>, "p.addrn">;
def P_ADDURN :  Pulp_ALU_rri<0b10, 0b110, RISCVOpcode<0b1011011>, "p.addurn">;
def P_SUBN :    Pulp_ALU_rri<0b00, 0b011, RISCVOpcode<0b1011011>, "p.subn">;
def P_SUBUN :   Pulp_ALU_rri<0b10, 0b011, RISCVOpcode<0b1011011>, "p.subun">;
def P_SUBRN :   Pulp_ALU_rri<0b00, 0b111, RISCVOpcode<0b1011011>, "p.subrn">;
def P_SUBURN :  Pulp_ALU_rri<0b10, 0b111, RISCVOpcode<0b1011011>, "p.suburn">;
def P_ADDNR :   Pulp_ALU_rr<0b0100000, 0b010, RISCVOpcode<0b1011011>, "p.addnr">;
def P_ADDUNR :  Pulp_ALU_rr<0b1100000, 0b010, RISCVOpcode<0b1011011>, "p.addunr">;
def P_ADDRNR :  Pulp_ALU_rr<0b0100000, 0b110, RISCVOpcode<0b1011011>, "p.addrnr">;
def P_ADDURNR : Pulp_ALU_rr<0b1100000, 0b110, RISCVOpcode<0b1011011>, "p.addurnr">;
def P_SUBNR :   Pulp_ALU_rr<0b0100000, 0b011, RISCVOpcode<0b1011011>, "p.subnr">;
def P_SUBUNR :  Pulp_ALU_rr<0b1100000, 0b011, RISCVOpcode<0b1011011>, "p.subunr">;
def P_SUBRNR :  Pulp_ALU_rr<0b0100000, 0b111, RISCVOpcode<0b1011011>, "p.subrnr">;
def P_SUBURNR : Pulp_ALU_rr<0b1100000, 0b111, RISCVOpcode<0b1011011>, "p.suburnr">;
}


def simm5 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isInt<5>(Imm);}]> {
  let ParserMatchClass = SImmAsmOperand<5>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeSImmOperand<5>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isInt<5>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_Immediate_Branch<bits<3> funct3, string opcodestr>
      : RVInstB<funct3, OPC_BRANCH, (outs), (ins GPR:$rs1, simm5:$imm5, simm13_lsb0:$imm12),
                opcodestr, "$rs1, $imm5, $imm12">,
        Sched<[WriteJmp, ReadJmp, ReadJmp]> {
  bits<5> imm5;
  let Inst{24-20} = imm5;

  let isBranch = 1;
  let isTerminator = 1;
}

def P_BEQIMM : Pulp_Immediate_Branch<0b010, "p.beqimm">;
def P_BNEIMM : Pulp_Immediate_Branch<0b011, "p.bneimm">;

// MAC Operations
let Predicates = [HasNonStdExtPulp] in {

def P_MAC :   Pulp_ALU_rr<0b0100001, 0b000, OPC_OP, "p.mac">;
def P_MSU :   Pulp_ALU_rr<0b0100001, 0b001, OPC_OP, "p.msu">;

def P_MULS :   Pulp_ALU_rr<0b1000000, 0b000, RISCVOpcode<0b1011011>, "p.muls">;
def P_MULHHS : Pulp_ALU_rr<0b1100000, 0b000, RISCVOpcode<0b1011011>, "p.mulhhs">;
def P_MULSN :   Pulp_ALU_rri<0b10, 0b000, RISCVOpcode<0b1011011>, "p.mulsn">;
def P_MULHHSN :   Pulp_ALU_rri<0b11, 0b000, RISCVOpcode<0b1011011>, "p.mulhhsn">;
def P_MULSRN :   Pulp_ALU_rri<0b10, 0b100, RISCVOpcode<0b1011011>, "p.mulsrn">;
def P_MULHHSRN :   Pulp_ALU_rri<0b11, 0b100, RISCVOpcode<0b1011011>, "p.mulhhsrn">;
def P_MULU :   Pulp_ALU_rr<0b0000000, 0b000, RISCVOpcode<0b1011011>, "p.mulu">;
def P_MULHHU : Pulp_ALU_rr<0b0100000, 0b000, RISCVOpcode<0b1011011>, "p.mulhhu">;
def P_MULUN :   Pulp_ALU_rri<0b00, 0b000, RISCVOpcode<0b1011011>, "p.mulun">;
def P_MULHHUN :   Pulp_ALU_rri<0b01, 0b000, RISCVOpcode<0b1011011>, "p.mulhhun">;
def P_MULURN :   Pulp_ALU_rri<0b00, 0b100, RISCVOpcode<0b1011011>, "p.mulurn">;
def P_MULHHURN :   Pulp_ALU_rri<0b01, 0b100, RISCVOpcode<0b1011011>, "p.mulhhurn">;
def P_MACSN :   Pulp_ALU_rri<0b10, 0b001, RISCVOpcode<0b1011011>, "p.macsn">;
def P_MACHHSN :   Pulp_ALU_rri<0b11, 0b001, RISCVOpcode<0b1011011>, "p.machhsn">;
def P_MACSRN :   Pulp_ALU_rri<0b10, 0b101, RISCVOpcode<0b1011011>, "p.macsrn">;
def P_MACHHSRN :   Pulp_ALU_rri<0b11, 0b101, RISCVOpcode<0b1011011>, "p.machhsrn">;
def P_MACUN :   Pulp_ALU_rri<0b00, 0b001, RISCVOpcode<0b1011011>, "p.macun">;
def P_MACHHUN :   Pulp_ALU_rri<0b01, 0b001, RISCVOpcode<0b1011011>, "p.machhun">;
def P_MACURN :   Pulp_ALU_rri<0b00, 0b101, RISCVOpcode<0b1011011>, "p.macurn">;
def P_MACHHURN :   Pulp_ALU_rri<0b01, 0b101, RISCVOpcode<0b1011011>, "p.machhurn">;
}

def uimm6 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<6>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<6>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<6>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<6>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

let hasSideEffects = 0, mayLoad = 0, mayStore = 0 in
class Pulp_ALU_sci<bits<5> funct5, bit F, bits<3> funct3, RISCVOpcode opcode,
                   bit signedImm, string opcodestr>
    : RVInst<(outs GPR:$rd), (ins GPR:$rs1, !if(signedImm, simm6, uimm6):$Imm6), opcodestr, "$rd, $rs1, $Imm6", [], InstFormatR>,
      Sched<[WriteIALU, ReadIALU]>  {
  bits<6> Imm6;
  bits<5> rs1;
  bits<5> rd;

  let Inst{31-27} = funct5;
  let Inst{26} = F;
  let Inst{25} = Imm6{0};
  let Inst{24-20} = Imm6{5-1};
  let Inst{19-15} = rs1;
  let Inst{14-12} = funct3;
  let Inst{11-7} = rd;
  let Opcode = opcode.Value;
}

multiclass SC_SCI_H_B<bits<5> funct5, bit F, bit signedImm, string opcodestr> {
  def _H     : Pulp_ALU_rr<{funct5, F, 0}, 0b000, RISCVOpcode<0b1010111>,            "pv." # opcodestr # ".h">;
  def _SC_H  : Pulp_ALU_rr<{funct5, F, 0}, 0b100, RISCVOpcode<0b1010111>,            "pv." # opcodestr # ".sc.h">;
  def _SCI_H : Pulp_ALU_sci<funct5, F,     0b110, RISCVOpcode<0b1010111>, signedImm, "pv." # opcodestr # ".sci.h">;
  def _B     : Pulp_ALU_rr<{funct5, F, 0}, 0b001, RISCVOpcode<0b1010111>,            "pv." # opcodestr # ".b">;
  def _SC_B  : Pulp_ALU_rr<{funct5, F, 0}, 0b101, RISCVOpcode<0b1010111>,            "pv." # opcodestr # ".sc.b">;
  def _SCI_B : Pulp_ALU_sci<funct5, F,     0b111, RISCVOpcode<0b1010111>, signedImm, "pv." # opcodestr # ".sci.b">;
}

// Vectorial ALU Operations
let Predicates = [HasNonStdExtPulp] in {

defm PV_ADD  : SC_SCI_H_B<0b00000, 0, 1, "add">;
defm PV_SUB  : SC_SCI_H_B<0b00001, 0, 1, "sub">;
defm PV_AVG  : SC_SCI_H_B<0b00010, 0, 1, "avg">;
defm PV_AVGU : SC_SCI_H_B<0b00011, 0, 0, "avgu">;
defm PV_MIN  : SC_SCI_H_B<0b00100, 0, 1, "min">;
defm PV_MINU : SC_SCI_H_B<0b00101, 0, 0, "minu">;
defm PV_MAX  : SC_SCI_H_B<0b00110, 0, 1, "max">;
defm PV_MAXU : SC_SCI_H_B<0b00111, 0, 0, "maxu">;
defm PV_SRL  : SC_SCI_H_B<0b01000, 0, 0, "srl">;
defm PV_SRA  : SC_SCI_H_B<0b01001, 0, 0, "sra">;
defm PV_SLL  : SC_SCI_H_B<0b01010, 0, 0, "sll">;
defm PV_OR   : SC_SCI_H_B<0b01011, 0, 1, "or">;
defm PV_XOR  : SC_SCI_H_B<0b01100, 0, 1, "xor">;
defm PV_AND  : SC_SCI_H_B<0b01101, 0, 1, "and">;

def PV_ABS_H      : Pulp_ALU_r  <0b0111000,  0b000, RISCVOpcode<0b1010111>,    "pv.abs.h">;
def PV_ABS_B      : Pulp_ALU_r  <0b0111000,  0b001, RISCVOpcode<0b1010111>,    "pv.abs.b">;
def PV_EXTRACT_H  : Pulp_ALU_sci<0b01111, 0, 0b110, RISCVOpcode<0b1010111>, 1, "pv.extract.h">;
def PV_EXTRACT_B  : Pulp_ALU_sci<0b01111, 0, 0b111, RISCVOpcode<0b1010111>, 1, "pv.extract.b">;
def PV_EXTRACTU_H : Pulp_ALU_sci<0b10010, 0, 0b110, RISCVOpcode<0b1010111>, 1, "pv.extractu.h">;
def PV_EXTRACTU_B : Pulp_ALU_sci<0b10010, 0, 0b111, RISCVOpcode<0b1010111>, 1, "pv.extractu.b">;
def PV_INSERT_H   : Pulp_ALU_sci<0b10110, 0, 0b110, RISCVOpcode<0b1010111>, 1, "pv.insert.h">;
def PV_INSERT_B   : Pulp_ALU_sci<0b10110, 0, 0b111, RISCVOpcode<0b1010111>, 1, "pv.insert.b">;

defm PV_DOTUP   : SC_SCI_H_B<0b10000, 0, 0, "dotup">;
defm PV_DOTUSP  : SC_SCI_H_B<0b10001, 0, 1, "dotusp">;
defm PV_DOTSP   : SC_SCI_H_B<0b10011, 0, 1, "dotsp">;
defm PV_SDOTUP  : SC_SCI_H_B<0b10100, 0, 0, "sdotup">;
defm PV_SDOTUSP : SC_SCI_H_B<0b10101, 0, 1, "sdotusp">;
defm PV_SDOTSP  : SC_SCI_H_B<0b10111, 0, 1, "sdotsp">;

def PV_SHUFFLE_H       : Pulp_ALU_rr <0b1100000,  0b000, RISCVOpcode<0b1010111>,    "pv.shuffle.h">;
def PV_SHUFFLE_SCI_H   : Pulp_ALU_sci<0b11000, 0, 0b110, RISCVOpcode<0b1010111>, 0, "pv.shuffle.sci.h">;
def PV_SHUFFLE_B       : Pulp_ALU_rr <0b1100000,  0b001, RISCVOpcode<0b1010111>,    "pv.shuffle.b">;
def PV_SHUFFLEI0_SCI_B : Pulp_ALU_sci<0b11000, 0, 0b111, RISCVOpcode<0b1010111>, 0, "pv.shufflei0.sci.b">;
def PV_SHUFFLEI1_SCI_B : Pulp_ALU_sci<0b11101, 0, 0b111, RISCVOpcode<0b1010111>, 0, "pv.shufflei1.sci.b">;
def PV_SHUFFLEI2_SCI_B : Pulp_ALU_sci<0b11110, 0, 0b111, RISCVOpcode<0b1010111>, 0, "pv.shufflei2.sci.b">;
def PV_SHUFFLEI3_SCI_B : Pulp_ALU_sci<0b11111, 0, 0b111, RISCVOpcode<0b1010111>, 0, "pv.shufflei3.sci.b">;
def PV_SHUFFLE2_H      : Pulp_ALU_rr <0b1100100,  0b000, RISCVOpcode<0b1010111>,    "pv.shuffle2.h">;
def PV_SHUFFLE2_B      : Pulp_ALU_rr <0b1100100,  0b001, RISCVOpcode<0b1010111>,    "pv.shuffle2.b">;
def PV_PACK_H          : Pulp_ALU_rr <0b1101000,  0b000, RISCVOpcode<0b1010111>,    "pv.pack.h">;
def PV_PACKHI_B        : Pulp_ALU_rr <0b1101100,  0b001, RISCVOpcode<0b1010111>,    "pv.packhi.b">;
def PV_PACKLO_B        : Pulp_ALU_rr <0b1110000,  0b001, RISCVOpcode<0b1010111>,    "pv.packlo.b">;


// Vectorial comparison operations
defm PV_CMPEQ  : SC_SCI_H_B<0b00000, 1, 1, "cmpeq">;
defm PV_CMPNE  : SC_SCI_H_B<0b00001, 1, 1, "cmpne">;
defm PV_CMPGT  : SC_SCI_H_B<0b00010, 1, 1, "cmpgt">;
defm PV_CMPGE  : SC_SCI_H_B<0b00011, 1, 1, "cmpge">;
defm PV_CMPLT  : SC_SCI_H_B<0b00100, 1, 1, "cmplt">;
defm PV_CMPLE  : SC_SCI_H_B<0b00101, 1, 1, "cmple">;
defm PV_CMPGTU : SC_SCI_H_B<0b00110, 1, 0, "cmpgtu">;
defm PV_CMPGEU : SC_SCI_H_B<0b00111, 1, 0, "cmpgeu">;
defm PV_CMPLTU : SC_SCI_H_B<0b01000, 1, 0, "cmpltu">;
defm PV_CMPLEU : SC_SCI_H_B<0b01001, 1, 0, "cmpleu">;

}

let hasSideEffects = 0, mayLoad = 1, mayStore = 0 in
multiclass Pulp_Load<bits<7> funct7, bits<3> funct3, string opcodestr> {

  def _ri_PostIncrement : RVInstI<funct3, RISCVOpcode<0b0001011>, (outs GPR:$rd),
                                  (ins GPR:$rs1, simm12:$imm12), opcodestr, "$rd, ${imm12}(${rs1}!)">;

  def _rr_PostIncrement : RVInstR<funct7, 0b111, RISCVOpcode<0b0001011>, (outs GPR:$rd),
                                  (ins GPR:$rs1, GPR:$rs2), opcodestr, "$rd, ${rs2}(${rs1}!)">;

  def _rr               : RVInstR<funct7, 0b111, RISCVOpcode<0b0000011>, (outs GPR:$rd),
                                  (ins GPR:$rs1, GPR:$rs2), opcodestr, "$rd, ${rs2}(${rs1})">;


}


let Predicates = [HasNonStdExtPulp] in {
  defm P_LB  : Pulp_Load<0b0000000, 0b000, "p.lb">,  Sched<[WriteLDB, ReadMemBase]>;
  defm P_LBU : Pulp_Load<0b0100000, 0b100, "p.lbu">, Sched<[WriteLDB, ReadMemBase]>;
  defm P_LH  : Pulp_Load<0b0001000, 0b001, "p.lh">,  Sched<[WriteLDH, ReadMemBase]>;
  defm P_LHU : Pulp_Load<0b0101000, 0b101, "p.lhu">, Sched<[WriteLDH, ReadMemBase]>;
  defm P_LW  : Pulp_Load<0b0010000, 0b010, "p.lw">,  Sched<[WriteLDW, ReadMemBase]>;
}


let hasSideEffects = 0, mayLoad = 0, mayStore = 1 in
multiclass Pulp_Save<bits<2> funct2, string opcodestr> {

    def _ri_PostIncrement : RVInstS<{0, funct2}, RISCVOpcode<0b0101011>, (outs), (ins GPR:$rs2, GPR:$rs1, simm12:$imm12),
                                    opcodestr, "$rs2, ${imm12}(${rs1}!)">;

    def _rr_PostIncrement : RVInstR<0b0000000, {1, funct2}, RISCVOpcode<0b0101011>, (outs),
                                    (ins GPR:$rs2, GPR:$rd, GPR:$rs1), opcodestr, "$rs2, ${rd}(${rs1}!)">;

    def _rr               : RVInstR<0b0000000, {1, funct2}, RISCVOpcode<0b0100011>, (outs),
                                    (ins GPR:$rs2, GPR:$rd, GPR:$rs1), opcodestr, "$rs2, ${rd}(${rs1})">;


}

let Predicates = [HasNonStdExtPulp] in {
  defm P_SB : Pulp_Save<0b00, "p.sb">, Sched<[WriteSTB, ReadStoreData, ReadMemBase]>;
  defm P_SH : Pulp_Save<0b01, "p.sh">, Sched<[WriteSTH, ReadStoreData, ReadMemBase]>;
  defm P_SW : Pulp_Save<0b10, "p.sw">, Sched<[WriteSTW, ReadStoreData, ReadMemBase]>;
}

def uimm1 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<1>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<1>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<1>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def uimm12 : Operand<XLenVT>, ImmLeaf<XLenVT, [{return isUInt<12>(Imm);}]> {
  let ParserMatchClass = UImmAsmOperand<12>;
  let EncoderMethod = "getImmOpValue";
  let DecoderMethod = "decodeUImmOperand<12>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isUInt<12>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

def uimm13_lsb0 : Operand<OtherVT>,
                  ImmLeaf<XLenVT, [{return isShiftedUInt<12, 1>(Imm);}]>  {
  let ParserMatchClass = UImmAsmOperand<13, "Lsb0">;
  let EncoderMethod = "getImmOpValueAsr1";
  let DecoderMethod = "decodeUImmOperand<13>";
  let MCOperandPredicate = [{
    int64_t Imm;
    if (MCOp.evaluateAsConstantImm(Imm))
      return isShiftedInt<12, 1>(Imm);
    return MCOp.isBareSymbolRef();
  }];
}

let Predicates = [HasNonStdExtPulp],
    hasSideEffects = 1, mayLoad = 0, mayStore = 0 in {
  def LP_STARTI : RVInstI<0b000, RISCVOpcode<0b1111011>, (outs),
                          (ins uimm1:$L, uimm13_lsb0:$imm12),
                          "lp.starti", "$L, $imm12">, Sched<[WriteCSR]> {
    bit L;

    let rs1 = 0;
    let rd{4-1} = 0;
    let rd{0} = L;
  }

  def LP_ENDI : RVInstI<0b001, RISCVOpcode<0b1111011>, (outs),
                        (ins uimm1:$L, uimm13_lsb0:$imm12),
                        "lp.endi", "$L, $imm12">, Sched<[WriteCSR]> {
    bit L;

    let rs1 = 0;
    let rd{4-1} = 0;
    let rd{0} = L;
  }

  def LP_COUNT : RVInstI<0b010, RISCVOpcode<0b1111011>, (outs),
                         (ins uimm1:$L, GPR:$rs1), "lp.count", "$L, $rs1">,
                         Sched<[WriteCSR]> {
    bit L;

    let imm12 = 0;
    let rd{4-1} = 0;
    let rd{0} = L;
  }

  def LP_COUNTI : RVInstI<0b011, RISCVOpcode<0b1111011>, (outs),
                          (ins uimm1:$L, uimm12:$imm12),
                          "lp.counti", "$L, $imm12">, Sched<[WriteCSR]> {
    bit L;

    let rs1 = 0;
    let rd{4-1} = 0;
    let rd{0} = L;
  }

  def LP_SETUP : RVInstI<0b100, RISCVOpcode<0b1111011>, (outs),
                         (ins uimm1:$L, GPR:$rs1, uimm13_lsb0:$imm12),
                         "lp.setup", "$L, $rs1, $imm12">,
                         Sched<[WriteCSR, WriteCSR, WriteCSR]> {
    bit L;

    let rd{4-1} = 0;
    let rd{0} = L;
  }

  def LP_SETUPI : RVInstI<0b101, RISCVOpcode<0b1111011>, (outs),
                          (ins uimm1:$L, uimm5:$imm5, uimm13_lsb0:$imm12),
                          "lp.setupi", "$L, $imm5, $imm12">,
                          Sched<[WriteCSR, WriteCSR, WriteCSR]> {
    bit L;
    bits<5> imm5;

    let rs1 = imm5;
    let rd{4-1} = 0;
    let rd{0} = L;
  }
}
