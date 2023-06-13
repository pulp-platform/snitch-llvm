//===-- RISCVExpandSSRInsts.cpp - Expand SSR pseudo instructions ---------===//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that expands SSR pseudo instructions into target
// instructions. This pass should be run before register allocation
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// The SSR are configured in a memory-mapped address space accessible through 
// the SCGGW(I)/SCGGR(I) instructions. The (I)mmediate instructions take the 
// address as an immediate. The Address map is as follows:
//
// | Word| Hex  | reg        |
// |-----|------|------------|
// | 0   | 0x00 | status     |
// | 1   | 0x01 | repeat     |
// | 2   | 0x02 | Bound 0    |
// | 3   | 0x03 | Bound 1    |
// | 4   | 0x04 | Bound 2    |
// | 5   | 0x05 | Bound 3    |
// | 6   | 0x06 | Stride 0   |
// | 7   | 0x07 | Stride 1   |
// | 8   | 0x08 | Stride 2   |
// | 9   | 0x09 | Stride 3   |
// |     |      | _reserved_ |
// | 24  | 0x18 | Rptr 0     |
// | 25  | 0x19 | Rptr 1     |
// | 26  | 0x1a | Rptr 2     |
// | 27  | 0x1b | Rptr 3     |
// | 28  | 0x1c | Wptr 0     |
// | 29  | 0x1d | Wptr 1     |
// | 30  | 0x1e | Wptr 2     |
// | 31  | 0x1f | Wptr 3     |
// 
// The data mover is selected in the lower 5 bits, the register offset is encoded
// in the upper 7 bits. The value passed to scfgX is therefore
//             addr = dm + reg << 5
//
// scfgw   rs1 rs2 # rs1=value rs2=addr
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
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-ssr"
#define RISCV_EXPAND_SSR_NAME "RISCV SSR pseudo instruction expansion pass"

namespace {

class RISCVExpandSSR : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVExpandSSR() : MachineFunctionPass(ID) {
    initializeRISCVExpandSSRPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_SSR_NAME; }

private:

  MachineFunction *MF;
  RISCVMachineFunctionInfo *RVFI;
  std::vector<MachineInstr *> MoveLoads;
  std::vector<MachineInstr *> MoveStores;

  bool expandMBB(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandSSRSetup(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator MBBI);
  bool expandSSRPushPop(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI);
  bool expandSSRReadWrite(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator MBBI);
  bool expandSSRReadWriteImm(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI);
  bool expandSSRBoundStride(MachineBasicBlock &MBB,
                            MachineBasicBlock::iterator MBBI);
  bool expandSSREnDis(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator MBBI);
  bool expandSSRSetupRep(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSRBarrier(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI,
                        MachineBasicBlock::iterator &NextMBBI);
  bool expandSSRBundle(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI);
};

char RISCVExpandSSR::ID = 0;

static const MachineInstrBuilder& addSSRDefUse(
    const MachineInstrBuilder& Builder) {
  return Builder.addDef(RISCV::FGSSR, RegState::Implicit)
                .addUse(RISCV::FGSSR, RegState::Implicit);
}

static Register getFSSRReg(unsigned Streamer) {
  unsigned AssignedReg = RISCVRegisterInfo::getFSSRD(Streamer);
  // Advance the iterator to the assigned register until the valid
  // register is found
  const TargetRegisterClass *RC = &RISCV::FPR64RegClass;
  TargetRegisterClass::iterator I = RC->begin();
  for (; *I != AssignedReg; ++I)
    assert(I != RC->end() && "AssignedReg should be a member of provided RC");
  return Register(*I);
}

bool RISCVExpandSSR::runOnMachineFunction(MachineFunction &MFunc) {
  TII = static_cast<const RISCVInstrInfo *>(MFunc.getSubtarget().getInstrInfo());
  this->MF = &MFunc;
  this->RVFI = MFunc.getInfo<RISCVMachineFunctionInfo>();
  this->MoveLoads.empty();
  this->MoveStores.empty();

  bool Modified = false;
  for (auto &MBB : MFunc)
    Modified |= expandMBB(MBB);

  // Add all SSR registers and FGSSR as a live-in for correctness
  if(Modified) {
    for (auto &MBB : MFunc) {
      //MBB.addLiveIn(RISCV::FGSSR);
      for(unsigned S = 0; S < RISCVRegisterInfo::NumFSSRs; ++S)
        MBB.addLiveIn(getFSSRReg(S));
      MBB.sortUniqueLiveIns();
    }
  }

  return Modified;
}

bool RISCVExpandSSR::expandMBB(MachineBasicBlock &MBB) {
  bool Modified = false;

  MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
  while (MBBI != E) {
    MachineBasicBlock::iterator NMBBI = std::next(MBBI);
    Modified |= expandMI(MBB, MBBI, NMBBI);
    MBBI = NMBBI;
  }
  MBB.sortUniqueLiveIns();

  return Modified;
}

bool RISCVExpandSSR::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  // RISCVInstrInfo::getInstSizeInBytes hard-codes the number of expanded
  // instructions for each pseudo, and must be updated when adding new pseudos
  // or changing existing ones.
  switch (MBBI->getOpcode()) {
  case RISCV::PseudoSSRSetup_1D_R:
  case RISCV::PseudoSSRSetup_1D_W:
    return expandSSRSetup(MBB, MBBI);
  case RISCV::PseudoSSRPush:
  case RISCV::PseudoSSRPop:
    return expandSSRPushPop(MBB, MBBI);
  case RISCV::PseudoSSRRead:
  case RISCV::PseudoSSRWrite:
    return expandSSRReadWrite(MBB, MBBI);
  case RISCV::PseudoSSRReadImm:
  case RISCV::PseudoSSRWriteImm:
    return expandSSRReadWriteImm(MBB, MBBI);
  case RISCV::PseudoSSRSetupBoundStride_1D:
  case RISCV::PseudoSSRSetupBoundStride_2D:
  case RISCV::PseudoSSRSetupBoundStride_3D:
  case RISCV::PseudoSSRSetupBoundStride_4D:
    return expandSSRBoundStride(MBB, MBBI);
  case RISCV::PseudoSSREnable:
  case RISCV::PseudoSSRDisable:
    return expandSSREnDis(MBB, MBBI);
  case RISCV::PseudoSSRSetupRepetition:
    return expandSSRSetupRep(MBB, MBBI);
  case RISCV::PseudoSSRBarrier:
    return expandSSRBarrier(MBB, MBBI, NextMBBI);
  case llvm::TargetOpcode::BUNDLE:
    return expandSSRBundle(MBB, MBBI);
  }

  return false;
}

bool RISCVExpandSSR::expandSSRSetup(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  LLVM_DEBUG(dbgs() << "-- Expanding SSR Setup 1D\n");

  // select streamer based on first argument
  int DmOffs = (int)MBBI->getOperand(0).getImm();

  // set repeat
  Register RepReg = MBBI->getOperand(1).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                   .addReg(RepReg).addImm(DmOffs +(0x1<<5)));
  // set bound 0
  Register BoundReg = MBBI->getOperand(2).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                   .addReg(BoundReg).addImm(DmOffs +(0x2<<5)));
  // set stride 0
  Register StrideReg = MBBI->getOperand(3).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                   .addReg(StrideReg).addImm(DmOffs +(0x6<<5)));
  // set read/write pointer
  Register PtrReg = MBBI->getOperand(4).getReg();
  if (MBBI->getOpcode() == RISCV::PseudoSSRSetup_1D_R)
    addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                     .addReg(PtrReg).addImm(DmOffs +(0x18<<5)));
  if (MBBI->getOpcode() == RISCV::PseudoSSRSetup_1D_W)
    addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                     .addReg(PtrReg).addImm(DmOffs +(0x1c<<5)));

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSRPushPop(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool IsPop = MBBI->getOpcode() == RISCV::PseudoSSRPop;

  // argument index of streamer and dest/source register is switched for push/pop 
  // instructions
  unsigned SSRArgIdx = MBBI->getOpcode() == RISCV::PseudoSSRPush ? 0 : 1;
  unsigned SSRValIdx = SSRArgIdx ^ 1;

  // Get streamer data register from argument
  // ssrX is mapped to floating point temporary ftX
  unsigned Streamer = (unsigned) MBBI->getOperand(SSRArgIdx).getImm();
  Register R = getFSSRReg(Streamer);

  LLVM_DEBUG(dbgs() << "-- Expanding SSR " << (IsPop ? "Pop" : "Push") << "\n");
  LLVM_DEBUG(dbgs() << "   Using register " << R
                    << " for SSR streamer " << Streamer <<"\n");

  if (IsPop) {
    // Insert a "loading move" this is like a normal move but has side effects
    Register ValR = MBBI->getOperand(SSRValIdx).getReg();
    MachineInstr *MI =
        addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJ_D), ValR)
                         .addReg(R).addReg(R)).getInstr();
    MBBI->eraseFromParent(); // The pseudo instruction is gone now.
    MI->getOperand(0).setIsDef();
    this->MoveLoads.push_back(MI);
  }
  else {
    Register ValR = MBBI->getOperand(SSRValIdx).getReg();
    // Insert a "storing move" this is like a normal move but has side effects
    MachineInstr *MI =
        addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJ_D), R)
                         .addReg(ValR, getRegState(MBBI->getOperand(SSRValIdx)))
                         .addReg(ValR, getRegState(MBBI->getOperand(SSRValIdx)))
                         ).getInstr();
    MBBI->eraseFromParent(); // The pseudo instruction is gone now.
    this->MoveStores.push_back(MI);
  }

  MBB.addLiveIn(R);
  return true;
}

bool RISCVExpandSSR::expandSSRReadWrite(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool IsRead = MBBI->getOpcode() == RISCV::PseudoSSRRead;
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  
  // select streamer based on first argument
  Register DmOffsReg = (int) MBBI->getOperand(0).getReg();
  // select dimension based on second argument
  Register DimOffsReg = (int) MBBI->getOperand(1).getReg();
  // select read/write based read/write
  int RwOffs = IsRead ? 0x18 : 0x1c;

  // reg  = (read ? 0x18 : 0x1c) + dim_off_reg
  // addr = reg << 5 + dm_off

  Register DimOff0 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), DimOff0).addReg(DimOffsReg)
      .addImm(RwOffs);
  Register DimOff1 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SLLI), DimOff1).addReg(DimOff0)
      .addImm(5);
  Register DimOff2 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADD), DimOff2).addReg(DimOff1)
      .addReg(DmOffsReg);

  // emit scfgwi at proper location
  Register PtrReg = MBBI->getOperand(2).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW))
                   .addReg(PtrReg).addReg(DimOff2));

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSRReadWriteImm(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  bool Read = MBBI->getOpcode() == RISCV::PseudoSSRReadImm;

  // select streamer based on first argument
  int DmOff = (int)MBBI->getOperand(0).getImm();
  int Dim = (int)MBBI->getOperand(1).getImm();
  int SSRReg = (((Read ? 0x18 : 0x1c) + Dim) << 5) + DmOff;
  Register PtrReg = MBBI->getOperand(2).getReg();

  // set read/write pointer
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI))
                   .addReg(PtrReg).addImm(SSRReg));

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSRSetupRep(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  // select streamer based on first argument
  Register DmOffReg = MBBI->getOperand(0).getReg();
  Register DmOff = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), DmOff)
      .addReg(DmOffReg).addImm(1<<5);

  // emit scfgwi at proper location
  Register PtrReg = MBBI->getOperand(1).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW)).addReg(PtrReg)
                   .addReg(DmOff, RegState::Kill));

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSRBoundStride(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  unsigned Dim;
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_1D) Dim = 1;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_2D) Dim = 2;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_3D) Dim = 3;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_4D) Dim = 4;

  LLVM_DEBUG(dbgs() << "-- Expanding SSR Bound Stride " << Dim << "D\n");

  // select streamer based on first argument
  Register DmOffsReg = MBBI->getOperand(0).getReg();

  // SCFGW rs1 rs2 # rs1=value rs2=addr
  // addr= reg << 5 + dm_off
  // dm_off = 0,1,2,3

  // Build an add to calculate the SSR register
  Register AddrBound = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  Register AddrStride = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), AddrBound)
      .addReg(DmOffsReg).addImm( ((2+(Dim -1))<<5));
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), AddrStride)
      .addReg(DmOffsReg).addImm( ((6+(Dim -1))<<5));

  // set bound
  Register BoundReg = MBBI->getOperand(1).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW))
                   .addReg(BoundReg).addReg(AddrBound, RegState::Kill));
  // set stride
  Register StrideReg = MBBI->getOperand(2).getReg();
  addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW))
                   .addReg(StrideReg).addReg(AddrStride, RegState::Kill));
  
  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSREnDis(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool IsEnable = MBBI->getOpcode() == RISCV::PseudoSSREnable;
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  LLVM_DEBUG(dbgs() << "-- Expanding SSR " <<
             (IsEnable ? "Enable" : "Disable") << "\n");

  // emit a csrsi/csrci call to the SSR location
  if(IsEnable) {
    addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::CSRRSI))
      .addDef(MRI.createVirtualRegister(&RISCV::GPRRegClass), RegState::Dead)
      .addImm(0x7C0).addImm(1));
  }
  else {
    addSSRDefUse(BuildMI(MBB, MBBI, DL, TII->get(RISCV::CSRRCI))
      .addDef(MRI.createVirtualRegister(&RISCV::GPRRegClass), RegState::Dead)
      .addImm(0x7C0).addImm(1));
  }

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSRBarrier(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineBasicBlock::iterator &NextMBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineInstr &MI = *MBBI;
  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  unsigned Streamer = (unsigned)MBBI->getOperand(0).getImm();
  
  LLVM_DEBUG(dbgs() << "-- Expanding SSR barrier on DM " << Streamer << "\n");

  auto *LoopMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());
  auto *DoneMBB = MF->CreateMachineBasicBlock(MBB.getBasicBlock());

  // Insert new MBBs.
  MF->insert(++MBB.getIterator(), LoopMBB);
  MF->insert(++LoopMBB->getIterator(), DoneMBB);

  // Set up successors and transfer remaining instructions to DoneMBB.
  LoopMBB->addSuccessor(LoopMBB);
  LoopMBB->addSuccessor(DoneMBB);
  DoneMBB->splice(DoneMBB->end(), &MBB, MI, MBB.end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(&MBB);
  MBB.addSuccessor(LoopMBB);
  
  // build loop: %0 = scfgri 0 | DM; srli %0, %0, 31; beq %0, zero, loop
  Register R = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  addSSRDefUse(BuildMI(LoopMBB, DL, TII->get(RISCV::SCFGRI), R)
                   .addImm(Streamer));
  Register Rs = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(LoopMBB, DL, TII->get(RISCV::SRLI), Rs).addReg(R, RegState::Kill)
      .addImm(31);
  BuildMI(LoopMBB, DL, TII->get(RISCV::BEQ)).addReg(Rs, RegState::Kill)
      .addReg(RISCV::X0).addMBB(LoopMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

bool RISCVExpandSSR::expandSSRBundle(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI) {
  // We only care about SSR bundles
  if (MBBI->getOperand(MBBI->getNumOperands()-1).getReg() != RISCV::FGSSR)
    return false;
  // Parse instructions in bundle (-1 indicates SSR is unused)
  MachineInstr *WrappedInst = nullptr;
  int SSRPushIdx = -1;
  int SSRPushVirt = -1;
  int SSRPopVirts[RISCVRegisterInfo::NumFSSRs];
  std::fill_n(SSRPopVirts, RISCVRegisterInfo::NumFSSRs, -1);
  auto BInst = MBBI.getInstrIterator();
  ++BInst;
  for (; BInst->isInsideBundle(); ++BInst) {
    switch (BInst->getOpcode()) {
      case RISCV::PseudoSSRPop: {
        auto PopDstRegIdx = BInst->getOperand(0).getReg().virtRegIndex();
        auto PopSSRIdx = BInst->getOperand(1).getImm();
        assert(SSRPopVirts[PopSSRIdx] == -1 && "SSR popped twice in bundle.");
        SSRPopVirts[PopSSRIdx] = PopDstRegIdx;
        break;
      }
      case RISCV::PseudoSSRPush: {
        assert(SSRPushIdx == -1 && "Multiple SSR pushes in bundle!");
        SSRPushIdx = BInst->getOperand(0).getImm();
        SSRPushVirt = BInst->getOperand(1).getReg().virtRegIndex();
        break;
      }
      default: {
        assert(!WrappedInst && "Multiple instructions in SSR bundle.");
        WrappedInst = &*BInst;
        break;
      }
    }
  }
  assert(WrappedInst &&  "No instruction in SSR bundle.");
  // Replace instruction destination if there is a push
  if (SSRPushIdx != -1) {
    assert(WrappedInst->getOperand(0).isReg() && SSRPushVirt ==
               int(WrappedInst->getOperand(0).getReg().virtRegIndex()) &&
           "Instruction bundled with SSR push does not write to push.");

    WrappedInst->getOperand(0).setReg(RISCVRegisterInfo::getFSSRD(SSRPushIdx));
  }
  // Replace instruction push for every pop
  for (auto &Use : WrappedInst->operands()) {
    if (!Use.isReg() || !Use.isUse() || Use.isImplicit()) continue;
    // Check if this reg use is an SSR pop
    for (unsigned S = 0; S < RISCVRegisterInfo::NumFSSRs; ++S) {
      if (SSRPopVirts[S] == int(Use.getReg().virtRegIndex())) {
        Use.setReg(RISCVRegisterInfo::getFSSRD(S));
        break;
      }
    }
  }
  // Add Implicit def and use
  auto FGSSRUse = MachineOperand::CreateReg(RISCV::FGSSR, false, true);
  auto FGSSRDef = MachineOperand::CreateReg(RISCV::FGSSR, true, true);
  WrappedInst->addOperand(FGSSRUse);
  WrappedInst->addOperand(FGSSRDef);
  // Move instruction before bundle, then delete bundle
  WrappedInst = WrappedInst->removeFromBundle();
  MBB.insert(MBBI, WrappedInst);
  MBBI->eraseFromParent();
  return true;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandSSR, "riscv-expand-ssr",
                RISCV_EXPAND_SSR_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandSSRPass() { return new RISCVExpandSSR(); }

} // end of namespace llvm
