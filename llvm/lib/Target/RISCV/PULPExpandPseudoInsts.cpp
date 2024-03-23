//===-- RISCVExpandPseudoInsts.cpp - Expand pseudo instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands PULP-specific pseudo instructions into target
// instructions.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"

using namespace llvm;

#define PULP_EXPAND_PSEUDO_NAME "PULP pseudo instruction expansion pass"

namespace {

class PULPExpandPseudo : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  PULPExpandPseudo() : MachineFunctionPass(ID) {
    initializePULPExpandPseudoPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return PULP_EXPAND_PSEUDO_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);  
  bool expandPV_ADD_VL(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI);
};

char PULPExpandPseudo::ID = 0;

bool PULPExpandPseudo::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool PULPExpandPseudo::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool PULPExpandPseudo::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  switch (MBBI->getOpcode()) {
  case RISCV::PseudoPV_ADD_SC_H_VL:
  case RISCV::PseudoPV_ADD_SC_B_VL:	  
  case RISCV::PseudoPV_ADD_SCI_H_VL:
  case RISCV::PseudoPV_ADD_SCI_B_VL:
    return expandPV_ADD_VL(MBB, MBBI);
  }

  return false;
}


bool PULPExpandPseudo::expandPV_ADD_VL(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  assert(MBBI->getNumOperands() == 4 &&
         "Unexpected instruction format");

  DebugLoc DL = MBBI->getDebugLoc();

  assert((MBBI->getOpcode() == RISCV::PseudoPV_ADD_SC_H_VL ||
          MBBI->getOpcode() == RISCV::PseudoPV_ADD_SC_B_VL ||
	  MBBI->getOpcode() == RISCV::PseudoPV_ADD_SCI_H_VL ||
          MBBI->getOpcode() == RISCV::PseudoPV_ADD_SCI_B_VL) &&
         "Unexpected pseudo instruction");

  unsigned Opcode;
  switch (MBBI->getOpcode())
  {
  case RISCV::PseudoPV_ADD_SC_H_VL:
    Opcode = RISCV::PV_ADD_SC_H;
    break;
  case RISCV::PseudoPV_ADD_SC_B_VL:
    Opcode = RISCV::PV_ADD_SC_B;
    break;
  case RISCV::PseudoPV_ADD_SCI_H_VL:
    Opcode = RISCV::PV_ADD_SCI_H;
    break;
  case RISCV::PseudoPV_ADD_SCI_B_VL:
    Opcode = RISCV::PV_ADD_SCI_B;
    break;
  }
  
  const MCInstrDesc &Desc = TII->get(Opcode);
  assert(Desc.getNumOperands() == 3 && "Unexpected instruction format");

  BuildMI(MBB, MBBI, DL, Desc)
      .add(MBBI->getOperand(0))  // Dst
      .add(MBBI->getOperand(1))  // Src1
      .add(MBBI->getOperand(2)); // Src2/Imm

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(PULPExpandPseudo, "pulp-expand-pseudo",
                PULP_EXPAND_PSEUDO_NAME, false, false)
namespace llvm {

FunctionPass *createPULPExpandPseudoPass() { return new PULPExpandPseudo(); }

} // end of namespace llvm
