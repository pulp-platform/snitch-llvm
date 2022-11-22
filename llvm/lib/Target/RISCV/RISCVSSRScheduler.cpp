//===-- RISCVSSRSchedulerInsts.cpp - Expand SDMA pseudo instructions --------===//
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
#include "RISCVMachineFunctionInfo.h"
#include "RISCVTargetMachine.h"

#include "llvm/ADT/SmallSet.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/LowLevelType.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-ssr-scheduler"
#define RISCV_SSR_SCHEDULER_NAME "RISCV SSR machine block rescheduler pass"
// TODO: centralize this, share among SSR passes
#define NUM_SSR 3

namespace {

// Active SSRs require blocking register allocation,
// but no rescheduling unless actually accessed in a block.
enum SSRUseKind {
  None = 0,
  Active = 1,
  Access = 2
};

class RISCVSSRScheduler : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVSSRScheduler() : MachineFunctionPass(ID) {
    initializeRISCVSSRSchedulerPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override {
    return RISCV_SSR_SCHEDULER_NAME;
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  // Helpers
  void unfuseFPDInst(MachineBasicBlock::iterator &MBBI, bool Sub, bool Neg);
  MachineInstr* getEarliestSuccInBlock(MachineInstr& I);
  unsigned fuseFPDInst(MachineBasicBlock::iterator &MBBI);
  // Passes
  SSRUseKind getSSRUseKind(MachineBasicBlock &MBB);
  void unfuseInsts(MachineBasicBlock &MBB);
  void raiseInstsToAsapIssue(MachineBasicBlock &MBB);
  void refuseInsts(MachineBasicBlock &MBB);
  void bundleSSRUsers(MachineBasicBlock &MBB);
  // TODO: reschedule unbundled insts, or does the MISched get this right?
};

char RISCVSSRScheduler::ID = 0;

bool RISCVSSRScheduler::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  bool MFUsesSSRs = false;
  // Visit each block once, but run multiple passes
  for (auto &MBB : MF) {
    auto UseKind = getSSRUseKind(MBB);
    if (UseKind != SSRUseKind::None) MFUsesSSRs = true;
    if (UseKind != SSRUseKind::Access) continue;
    unfuseInsts(MBB);
    raiseInstsToAsapIssue(MBB);
    refuseInsts(MBB);
    bundleSSRUsers(MBB);
  }
  // Declare active SSR regions to later block SSR register allocation
  MF.getInfo<RISCVMachineFunctionInfo>()->setUsesSSRs(MFUsesSSRs);
  return MFUsesSSRs;
}

SSRUseKind RISCVSSRScheduler::getSSRUseKind(llvm::MachineBasicBlock &MBB) {
  auto Ret = SSRUseKind::None;
  for (auto &Inst : MBB) {
    switch (Inst.getOpcode()) {
      case RISCV::PseudoSSRPush:
      case RISCV::PseudoSSRPop:
        {return SSRUseKind::Access;}
      case RISCV::PseudoSSREnable:
      case RISCV::PseudoSSRDisable:
        {Ret = SSRUseKind::Active;}
      default: ;
    }
  }
  return Ret;
}

// TODO: handle S and other datatypes once fully supported
// TODO: Check associativity constraints on FNMADD, FNSUB
void RISCVSSRScheduler::unfuseFPDInst(MachineBasicBlock::iterator &MBBI,
                                      bool Sub, bool Neg) {
  // TODO: Add implicit use of FRM (rounding mode) register!
  // Get symbols needed from iterator
  auto &DL = MBBI->getDebugLoc();
  auto &MBB = *MBBI->getParent();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  auto MIFlags = MBBI->getFlags();
  // Generate constituent instructions
  auto &AccInst = TII->get(Sub ? RISCV::FSUB_D : RISCV::FADD_D);
  auto MulReg = MRI.createVirtualRegister(&RISCV::FPR64RegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::FMUL_D)).setMIFlags(MIFlags)
      .addDef(MulReg).add(MBBI->getOperand(1)).add(MBBI->getOperand(2))
      .add(MBBI->getOperand(4)).addUse(RISCV::FRM, RegState::Implicit);
  if (Neg) {
    auto NegReg = MRI.createVirtualRegister(&RISCV::FPR64RegClass);
    BuildMI(MBB, MBBI, DL, AccInst).setMIFlags(MIFlags)
        .addDef(NegReg).addUse(MulReg).add(MBBI->getOperand(3))
        .add(MBBI->getOperand(4)).addUse(RISCV::FRM, RegState::Implicit);
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJN_D)).setMIFlags(MIFlags)
        .add(MBBI->getOperand(0)).addUse(NegReg).addUse(NegReg)
        .add(MBBI->getOperand(3)).addUse(RISCV::FRM, RegState::Implicit);
  } else {
    BuildMI(MBB, MBBI, DL, AccInst).setMIFlags(MIFlags)
        .add(MBBI->getOperand(0)).addUse(MulReg).add(MBBI->getOperand(3))
        .add(MBBI->getOperand(4)).addUse(RISCV::FRM, RegState::Implicit);
  }
}

void RISCVSSRScheduler::unfuseInsts(llvm::MachineBasicBlock &MBB) {
  std::vector<MachineInstr*> InstsToRemove;
  // Insert unfused instructions
  for (auto MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    switch (MBBI->getOpcode()) {
      case RISCV::FMADD_D: {unfuseFPDInst(MBBI, false, false); break;}
      case RISCV::FMSUB_D: {unfuseFPDInst(MBBI, true, false); break;}
      case RISCV::FNMADD_D: {unfuseFPDInst(MBBI, false, true); break;}
      case RISCV::FNMSUB_D: {unfuseFPDInst(MBBI, true, true); break;}
      default: {continue;}
    }
    InstsToRemove.push_back(&*MBBI);
  }
  // Remove original instructions
  for (auto *Inst : InstsToRemove)
    Inst->eraseFromParent();
}

// Returns the earliest block instruction before which we can insert `I` without
// violating its use dependencies, considering only virtual registers.
MachineInstr* RISCVSSRScheduler::getEarliestSuccInBlock(MachineInstr& I) {
  auto *MBB = I.getParent();
  auto MBBI = MBB->begin();
  // Skip PHI instructions
  while (MBBI->isPHI()) ++MBBI;
  auto Ret = MBBI;
  MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
  for (auto& Use : I.uses()) {
    if (!Use.isReg() || Use.isDef() || !Use.isUse()) continue;
    auto *UseInst = MRI.getVRegDef(Use.getReg());
    if (!UseInst || UseInst->getParent() != MBB) continue;
    // Consume block instructions until we find this def
    while (MBBI != MBB->end() && &*MBBI != UseInst) {++MBBI;}
    // Unless we hit the end of the block (i.e. we are past this def), update ret
    if (MBBI != MBB->end()) Ret = ++MBBI;
    // If we did hit the end, resume search from last result
    else MBBI = Ret;
  }
  return &*Ret;
}

void RISCVSSRScheduler::raiseInstsToAsapIssue(llvm::MachineBasicBlock &MBB) {
  std::vector<MachineInstr*> MovableInsts;
  // Collect movable instructions first (we cannot move them while iterating)
  for (auto &Inst : MBB) {
    bool SawStore;
    if (Inst.isSafeToMove(nullptr, SawStore))
      MovableInsts.push_back(&Inst);
  }
  // Move instructions
  for (auto *Inst : MovableInsts)
    Inst->moveBefore(getEarliestSuccInBlock(*Inst));
}

// TODO: handle S and other datatypes once fully supported
// TODO: respect FP flags exactly here
unsigned RISCVSSRScheduler::fuseFPDInst(MachineBasicBlock::iterator &MBBI) {
  // Currently, only FMUL_D-based chains with contraction enabled can be fused
  if (MBBI->getOpcode() != RISCV::FMUL_D ||
      !MBBI->getFlag(MachineInstr::MIFlag::FmContract)) return 0;
  // Get symbols needed from iterator
  auto &DL = MBBI->getDebugLoc();
  auto &MBB = *MBBI->getParent();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  auto MIFlags = MBBI->getFlags();
  // We know that the first instruction is an FMUL_D -> no need to check
  auto &MBBIMul = *MBBI;
  auto *MBBIAcc = MBBIMul.getNextNode();
  // Check whether fusable and how
  bool Sub = (MBBIAcc->getOpcode() == RISCV::FSUB_D);
  unsigned AccOperand = 2;
  if (Sub || MBBIAcc->getOpcode() == RISCV::FADD_D) {
    auto *Rs1Def = MRI.getVRegDef(MBBIAcc->getOperand(1).getReg());
    if (Rs1Def == &MBBIMul) ;
    else if (MBBI->getFlag(MachineInstr::MIFlag::FmReassoc)) {
      auto *Rs2Def = MRI.getVRegDef(MBBIAcc->getOperand(2).getReg());
      if (Rs2Def != &MBBIMul) return 0;
      // The *first* operand is accumulated and the *second* fused
      AccOperand = 1;
    }
    else return 0;
  }
  else return 0;
  // Check whether the fused instruction should be negated
  auto *InstNext = MBBI->getNextNode();
  bool Neg = (InstNext && InstNext->getOpcode() == RISCV::FSGNJN_D &&
              InstNext->getOperand(1).getReg().virtRegIndex() ==
                  InstNext->getOperand(2).getReg().virtRegIndex());
  // Generate instruction
  unsigned Ret = Neg ? 3 : 2;   // How many predecessors to later remove
  auto &FusedInst = TII->get(Neg ? (Sub ? RISCV::FNMSUB_D : RISCV::FNMADD_D)
                                 : (Sub ? RISCV::FMSUB_D : RISCV::FMADD_D));
  BuildMI(MBB, MBBI, DL, FusedInst).setMIFlags(MIFlags)
      .add(MBBIAcc->getOperand(0)).add(MBBIMul.getOperand(1))
      .add(MBBIMul.getOperand(2)).add(MBBIAcc->getOperand(AccOperand))
      .add(MBBIAcc->getOperand(3)).addUse(RISCV::FRM, RegState::Implicit);
  return Ret;
}

void RISCVSSRScheduler::refuseInsts(llvm::MachineBasicBlock &MBB) {
  std::vector<MachineInstr*> InstsToRemove;
  // Insert unfused instructions
  for (auto MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    auto RemCnt = fuseFPDInst(MBBI);
    for (unsigned N = 0; N < RemCnt; ++N, ++MBBI)
      InstsToRemove.push_back(&*(MBBI));
  }
  // Remove original instructions
  for (auto *Inst : InstsToRemove)
    Inst->eraseFromParent();
}

void RISCVSSRScheduler::bundleSSRUsers(MachineBasicBlock &MBB) {
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  std::vector<std::pair<MachineBasicBlock::iterator,
                        MachineBasicBlock::iterator>> RangesToBundle;
  // Insert unfused instructions
  bool BundleSSRs [NUM_SSR] = {0};
  auto BundleOngoing = false;
  auto BundleStart = MBB.begin();
  for (auto MBBI = MBB.begin(); MBBI != MBB.end(); ++MBBI) {
    auto BundleCommit = false;
    switch (MBBI->getOpcode()) {
      // If we did not yet start an ongoing bundle, do so now
      case RISCV::PseudoSSRPop: {
        // Check if this SSR was popped before. If so, reset block start;
        // we can only use each SSR once.
        // TODO: optimize this! we can profit through reordering here
        auto SSRIdx = MBBI->getOperand(1).getImm();
        if (!BundleOngoing || BundleSSRs[SSRIdx]) {
          BundleStart = MBBI;
          BundleOngoing = true;
          BundleSSRs[SSRIdx] = true;
        }
        break;
      }
      // The previous instruction vetted us to be a bundleable push.
      // We assume only one push can ever be bundled.
      case RISCV::PseudoSSRPush: {
        BundleCommit = BundleOngoing;
        break;
      }
      // For regular instructions, we must investigate both preceding
      // pops and any single following push for mergeability.
      default: {
        // If a bundle was started, keep it going iff it is only mergeable pops.
        // With every unmergeable pop, move forward our window start.
        if (BundleOngoing) {
          for (auto BunInst = BundleStart; BunInst != MBBI;) {
            // There should only be pops in the bundle so far
            assert(BunInst->getOpcode() == RISCV::PseudoSSRPop &&
                   "Bundle before non-SSR-pseudo should contain only pops.");
            // Scan Uses for this bundled instruction; mergeable if found.
            bool BunInstMergeable = false;
            for (auto Use : MBBI->uses()) {
              if (!Use.isReg() || Use.isDef() || !Use.isUse()) continue;
              if (MRI.getVRegDef(Use.getReg()) == BunInst) {
                BunInstMergeable = true;
                break;
              }
            }
            // Iterate here
            ++BunInst;
            // If this bundle instruction did not match,
            // Discard this pop and all previous ones.
            // TODO: how to optimize for tangled pops/pushes? Can this happen?
            if (!BunInstMergeable) BundleStart = BunInst;
          }
          // If we found no matches: abort bundling of preceding pops.
          // Following pushes may be bundled below.
            BundleOngoing = (BundleStart != MBBI);
        }
        // If the next instruction is a bundleable push, bundle it.
        // Start a new bundle if no bundle is ongoing.
        auto *MBBINext = MBBI->getNextNode();
        if (MBBINext && MBBINext->getOpcode() == RISCV::PseudoSSRPush &&
            MRI.getVRegDef(MBBINext->getOperand(1).getReg()) == &*MBBI) {
          if (!BundleOngoing) BundleStart = MBBI;
          BundleOngoing = true;
        }
        // Otherwise, commit iff we are in a bundle
        else BundleCommit = BundleOngoing;
      }
    }
    // Commit a bundle and reset our state
    if (BundleCommit) {
      std::fill_n(BundleSSRs, NUM_SSR, false);
      BundleOngoing = false;
      BundleCommit = false;
      RangesToBundle.emplace_back(BundleStart, std::next(MBBI));
    }
  }
  // Create Bundle for found ranges; add FGSSR def for later retrieval.
  for (auto &Range : RangesToBundle) {
    auto FGSSROp = MachineOperand::CreateReg(RISCV::FGSSR, true, true);
    finalizeBundle(MBB, Range.first.getInstrIterator(),
                   Range.second.getInstrIterator());
    getBundleStart(Range.first.getInstrIterator())->addOperand(FGSSROp);
  }
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVSSRScheduler, DEBUG_TYPE,
                RISCV_SSR_SCHEDULER_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVSSRSchedulerPass() { return new RISCVSSRScheduler(); }

}  // end of namespace llvm
