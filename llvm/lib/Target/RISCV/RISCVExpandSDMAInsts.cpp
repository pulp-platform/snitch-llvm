//===-- RISCVExpandSDMAInsts.cpp - Expand SDMA pseudo instructions --------===//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands SDMA pseudo instructions into target
// instructions. This pass should be run before register allocation
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "RISCVInstrInfo.h"
#include "RISCVTargetMachine.h"
#include "RISCVMachineFunctionInfo.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/LowLevelType.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-sdma"

#define RISCV_EXPAND_SDMA_NAME "RISCV SDMA pseudo instruction expansion pass"

namespace {

class RISCVExpandSDMA : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVExpandSDMA() : MachineFunctionPass(ID) {
    initializeRISCVExpandSDMAPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_SDMA_NAME; }

private:
  bool expandMBB(MachineBasicBlock &MBB);
  void mergePushPop(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandOned(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandTwod(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandStat(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandWaitForIdle(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI,
                         MachineBasicBlock::iterator &NextMBBI);
};

char RISCVExpandSDMA::ID = 0;

bool RISCVExpandSDMA::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  // this->MF = &MF;
  // this->RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  // this->MRI = &MF.getRegInfo();

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);
  return Modified;
}

bool RISCVExpandSDMA::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }

  return Modified;
}

bool RISCVExpandSDMA::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  // RISCVInstrInfo::getInstSizeInBytes hard-codes the number of expanded
  // instructions for each pseudo, and must be updated when adding new pseudos
  // or changing existing ones.
  switch (MBBI->getOpcode()) {
  case RISCV::PseudoSDMAOned:
    return expandOned(MBB, MBBI);
  case RISCV::PseudoSDMATwod:
    return expandTwod(MBB, MBBI);
  case RISCV::PseudoSDMAStat:
    return expandStat(MBB, MBBI);
  case RISCV::PseudoSDMAWaitForIdle:
    return expandWaitForIdle(MBB, MBBI, NextMBBI);
  }

  return false;
}

bool RISCVExpandSDMA::expandOned(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  // expected arguments:
  // 0 -> rd: destination register for the transfer ID
  // 1/2 -> source pointer hi/lo in registers
  // 3/4 -> destination pointer hi/lo in registers
  // 5 -> size: transfer size stored in a register
  // 6 -> cfg: config bits in a register

  LLVM_DEBUG(dbgs() << "-- Expanding SDMA Oned\n");

  // Mask the config bit 1 to ensure 1D transfer
  Register R = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ANDI), R)
    .addReg(MBBI->getOperand(6).getReg(), 0)
    .addImm(~(0x1<<1));
  // Build DMSRC
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMSRC))
    .addReg(MBBI->getOperand(2).getReg(), 0)  // ptrlo
    .addReg(MBBI->getOperand(1).getReg(), 0); // ptrhi
  // Build DMDST
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMDST))
    .addReg(MBBI->getOperand(4).getReg(), 0)  // ptrlo
    .addReg(MBBI->getOperand(3).getReg(), 0); // ptrhi
  // Build DMCPY
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMCPY), MBBI->getOperand(0).getReg()) // rd = transfer id
    .addReg(MBBI->getOperand(5).getReg(), 0)  // rs1 = size
    .addReg(R, RegState::Kill); // rs2 = config

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSDMA::expandTwod(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  // expected arguments:
  // 0 -> rd: destination register for the transfer ID
  // 1/2 -> source pointer hi/lo in registers
  // 3/4 -> destination pointer hi/lo in registers
  // 5 -> size: transfer size stored in a register
  // 6 -> source stride
  // 7 -> destination stride
  // 8 -> n reps
  // 9 -> cfg: config bits in a register

  LLVM_DEBUG(dbgs() << "-- Expanding SDMA Twod\n");

  // Set the config bit 1 to ensure 2D transfer
  Register R = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ORI), R)
    .addReg(MBBI->getOperand(9).getReg(), 0)
    .addImm(0x1<<1);
  // Build DMSRC
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMSRC))
    .addReg(MBBI->getOperand(2).getReg(), 0)  // ptrlo
    .addReg(MBBI->getOperand(1).getReg(), 0); // ptrhi
  // Build DMDST
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMDST))
    .addReg(MBBI->getOperand(4).getReg(), 0)  // ptrlo
    .addReg(MBBI->getOperand(3).getReg(), 0); // ptrhi
  // Build DMSTR
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMSTR))
    .addReg(MBBI->getOperand(6).getReg(), 0)  // srcstrd
    .addReg(MBBI->getOperand(7).getReg(), 0); // dststrd
  // Build DMREP
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMREP))
    .addReg(MBBI->getOperand(8).getReg(), 0); // reps
  // Build DMCPY
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMCPY), MBBI->getOperand(0).getReg()) // rd = transfer id
    .addReg(MBBI->getOperand(5).getReg(), 0)  // rs1 = size
    .addReg(R, RegState::Kill); // rs2 = config

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSDMA::expandStat(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  LLVM_DEBUG(dbgs() << "-- Expanding SDMA Stat\n");
  // Build DMSTAT
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::DMSTAT), MBBI->getOperand(0).getReg()) // rd = stat
    .addReg(MBBI->getOperand(1).getReg(), 0);  // rs1 = tid

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSDMA::expandWaitForIdle(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI,
                                        MachineBasicBlock::iterator &NextMBBI) {
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  MachineInstr &MI = *MBBI;
  MachineFunction *MF = MBB.getParent();
  DebugLoc DL = MI.getDebugLoc();

  LLVM_DEBUG(dbgs() << "-- Expanding SDMA WaitForIdle\n");

  auto LoopMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopMBB);
  MF->insert(++LoopMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopMBB->addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(DoneMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  MBB.addSuccessor(LoopMBB);
  
  // build loop: %0 = dmstati 2; bne %0, zero, loop
  Register R = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(LoopMBB, DL, TII->get(RISCV::DMSTATI), R).addImm(2);
  BuildMI(LoopMBB, DL, TII->get(RISCV::BNE)).addReg(R, RegState::Kill).addReg(RISCV::X0).addMBB(LoopMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandSDMA, "riscv-expand-sdma",
                RISCV_EXPAND_SDMA_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandSDMAPass() { return new RISCVExpandSDMA(); }

} // end of namespace llvm
