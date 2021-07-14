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
def OPC_PULP_LPI : RISCVOpcode<"PULP_LPI", 0b0001011>;
def OPC_PULP_SPI : RISCVOpcode<"PULP_SPI", 0b0101011>;

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

