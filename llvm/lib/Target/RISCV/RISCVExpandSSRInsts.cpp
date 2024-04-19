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
#include "RISCVTargetMachine.h"
#include "RISCVMachineFunctionInfo.h"

#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/LowLevelType.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

#define DEBUG_TYPE "riscv-ssr"

/// Command line options
static cl::opt<bool>
    SSRRegisterMerge("ssr-noregmerge", cl::Hidden,
                    cl::desc("Disable the merging of SSR registers in other instructions"));

#define RISCV_EXPAND_SSR_NAME "RISCV SSR pseudo instruction expansion pass"

#define NUM_SSR 3

namespace {

class RISCVExpandSSR : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  /// Parameters for the register merging pass
  struct RegisterMergingPreferences {
    /// enable the register merging
    bool Enable;
  };

  RISCVExpandSSR() : MachineFunctionPass(ID) {
    initializeRISCVExpandSSRPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_SSR_NAME; }

private:

  const MachineFunction *MF;
  RISCVMachineFunctionInfo *RVFI;
  bool Enabled;

  bool expandMBB(MachineBasicBlock &MBB);
  void mergePushPop(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandSSR_Setup(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_PushPop(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_ReadWrite(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_ReadWriteImm(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_BoundStride(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_EnDis(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_SetupRep(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
  bool expandSSR_Barrier(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI,
                         MachineBasicBlock::iterator &NextMBBI);

  RISCVExpandSSR::RegisterMergingPreferences gatherRegisterMergingPreferences();
};

char RISCVExpandSSR::ID = 0;

static Register getSSRFtReg(unsigned streamer) {
  unsigned AssignedReg = RISCV::F0_D + streamer;
  // Advance the iterator to the assigned register until the valid
  // register is found
  const TargetRegisterClass *RC = &RISCV::FPR64RegClass;
  TargetRegisterClass::iterator I = RC->begin();
  for (; *I != AssignedReg; ++I)
    assert(I != RC->end() && "AssignedReg should be a member of provided RC");
  return Register(*I);
}

bool RISCVExpandSSR::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  this->MF = &MF;
  this->RVFI = MF.getInfo<RISCVMachineFunctionInfo>();
  Enabled = false;

  bool Modified = false;
  for (auto &MBB : MF)
    Modified |= expandMBB(MBB);

  // Run over MF again to merge SSR pops/pushs into instruction uses
  RISCVExpandSSR::RegisterMergingPreferences RMP = gatherRegisterMergingPreferences();
  if(RMP.Enable && RVFI->getUsedSSR())
    for (auto &MBB : MF)
      mergePushPop(MBB);

  /// "Forcefully" add all SSR registers as live-in to all MBB in this MF
  if(Modified) {
    for (auto &MBB : MF) {
      for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no)
        MBB.addLiveIn(getSSRFtReg(ssr_no));
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
    return expandSSR_Setup(MBB, MBBI);
  case RISCV::PseudoSSRPush:
  case RISCV::PseudoSSRPop:
    return expandSSR_PushPop(MBB, MBBI);
  case RISCV::PseudoSSRRead:
  case RISCV::PseudoSSRWrite:
    return expandSSR_ReadWrite(MBB, MBBI);
  case RISCV::PseudoSSRReadImm:
  case RISCV::PseudoSSRWriteImm:
    return expandSSR_ReadWriteImm(MBB, MBBI);
  case RISCV::PseudoSSRSetupBoundStride_1D:
  case RISCV::PseudoSSRSetupBoundStride_2D:
  case RISCV::PseudoSSRSetupBoundStride_3D:
  case RISCV::PseudoSSRSetupBoundStride_4D:
    return expandSSR_BoundStride(MBB, MBBI);
  case RISCV::PseudoSSREnable:
  case RISCV::PseudoSSRDisable:
    return expandSSR_EnDis(MBB, MBBI);
  case RISCV::PseudoSSRSetupRepetition:
    return expandSSR_SetupRep(MBB, MBBI);
  case RISCV::PseudoSSRBarrier:
    return expandSSR_Barrier(MBB, MBBI, NextMBBI);
  }

  // Prevent excessive live-ins, they pose a problem with multiple SSR regions
  // in a single function. Adding SSR regs to live ins in push/pop should suffice
  // for now, but there might be edge cases
  
  // if(Enabled) {
  //   // mark the SSR registers reserved in this BB
  //   unsigned ssrEnabledMask = 0;
  //   for (unsigned n = 0; n < NUM_SSR; ++n)
  //     MBB.addLiveIn(getSSRFtReg(n));
  // }

  return false;
}

bool RISCVExpandSSR::expandSSR_Setup(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  LLVM_DEBUG(dbgs() << "-- Expanding SSR Setup 1D\n");

  // select streamer based on first argument
  int dm_off = (int)MBBI->getOperand(0).getImm();

  // set repeat
  Register RepReg = MBBI->getOperand(1).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(RepReg).addImm(dm_off+(0x1<<5));
  // set bound 0
  Register BoundReg = MBBI->getOperand(2).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(BoundReg).addImm(dm_off+(0x2<<5));
  // set stride 0
  Register StrideReg = MBBI->getOperand(3).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(StrideReg).addImm(dm_off+(0x6<<5));
  // set read/write pointer
  Register PtrReg = MBBI->getOperand(4).getReg();
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetup_1D_R)
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(PtrReg).addImm(dm_off+(0x18<<5));
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetup_1D_W)
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(PtrReg).addImm(dm_off+(0x1c<<5));

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_PushPop(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool isPop = MBBI->getOpcode() == RISCV::PseudoSSRPop;

  // argument index of streamer and dest/source register is switched for push/pop 
  // instructions
  unsigned ssrArgIdx = MBBI->getOpcode() == RISCV::PseudoSSRPush ? 0 : 1;
  unsigned ssrValIdx = ssrArgIdx^1;

  // Get streamer data register from argument
  // ssrX is mapped to floating point temporary ftX
  unsigned streamer = (unsigned)MBBI->getOperand(ssrArgIdx).getImm();
  Register R = getSSRFtReg(streamer);

  LLVM_DEBUG(dbgs() << "-- Expanding SSR " << (isPop?"Pop":"Push") << "\n");
  LLVM_DEBUG(dbgs() << "   Using register " << R << " for SSR streamer "<<streamer<<"\n");

  // Build float move from ssr reg to register provided as argument or vice versa
  // FSGNJ_D is used for FMV.D
  if(isPop) {
    // Insert newly-built instruction and set up first operand as a destination
    // virtual register. A copy instruction is emitted that moves the value
    // from the SSR register (R) to a destination virtual register
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJ_D), MBBI->getOperand(ssrValIdx).getReg())
      .addReg(R, 0)
      .addReg(R, 0);
    // BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::COPY), MBBI->getOperand(ssrValIdx).getReg())
    //   .addReg(R, 0);
  }
  else {
    // Build a copy instruction that moves the value from the register passed as 
    // argument to the ssr data register (R)
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJ_D), R)
      .addReg(MBBI->getOperand(ssrValIdx).getReg())
      .addReg(MBBI->getOperand(ssrValIdx).getReg());
    // BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::COPY), R)
    //   .addReg(MBBI->getOperand(ssrValIdx).getReg());
  }

  MBB.addLiveIn(R);
  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_ReadWrite(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool isRead = MBBI->getOpcode() == RISCV::PseudoSSRRead;
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  
  // select streamer based on first argument
  Register dm_off_reg = (int)MBBI->getOperand(0).getReg();
  // select dimension based on second argument
  Register dim_off_reg = (int)MBBI->getOperand(1).getReg();
  // select read/write based read/write
  int rw_off = isRead ? 0x18 : 0x1c;

  // reg  = (read ? 0x18 : 0x1c) + dim_off_reg
  // addr = reg << 5 + dm_off

  Register dim_off0 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), dim_off0).addReg(dim_off_reg).addImm(rw_off);
  Register dim_off1 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SLLI), dim_off1).addReg(dim_off0).addImm(5);
  Register dim_off2 = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADD), dim_off2).addReg(dim_off1).addReg(dm_off_reg);

  // emit scfgwi at proper location
  Register PtrReg = MBBI->getOperand(2).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW)).addReg(PtrReg).addReg(dim_off2);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_ReadWriteImm(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  bool read = MBBI->getOpcode() == RISCV::PseudoSSRReadImm;

  // select streamer based on first argument
  int dm_off = (int)MBBI->getOperand(0).getImm();
  int dim = (int)MBBI->getOperand(1).getImm();
  int ssr_reg = (((read ? 0x18 : 0x1c) + dim) << 5) + dm_off;
  Register PtrReg = MBBI->getOperand(2).getReg();

  // set read/write pointer
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGWI)).addReg(PtrReg).addImm(ssr_reg);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_SetupRep(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  // select streamer based on first argument
  Register dm_off_reg = MBBI->getOperand(0).getReg();
  Register dm_off = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), dm_off).addReg(dm_off_reg).addImm(1<<5);

  // emit scfgwi at proper location
  Register PtrReg = MBBI->getOperand(1).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW)).addReg(PtrReg).addReg(dm_off, RegState::Kill);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_BoundStride(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  unsigned dim;
  DebugLoc DL = MBBI->getDebugLoc();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_1D) dim = 1;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_2D) dim = 2;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_3D) dim = 3;
  if(MBBI->getOpcode() == RISCV::PseudoSSRSetupBoundStride_4D) dim = 4;

  LLVM_DEBUG(dbgs() << "-- Expanding SSR Bound Stride " << dim << "D\n");

  // select streamer based on first argument
  Register dm_off_reg = MBBI->getOperand(0).getReg();

  // SCFGW rs1 rs2 # rs1=value rs2=addr
  // addr= reg << 5 + dm_off
  // dm_off = 0,1,2,3

  // Build an add to calculate the SSR register
  Register addr_bound = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  Register addr_stride = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), addr_bound).addReg(dm_off_reg).addImm( ((2+(dim-1))<<5));
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::ADDI), addr_stride).addReg(dm_off_reg).addImm( ((6+(dim-1))<<5));

  // set bound
  Register BoundReg = MBBI->getOperand(1).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW)).addReg(BoundReg).addReg(addr_bound, RegState::Kill);
  // set stride
  Register StrideReg = MBBI->getOperand(2).getReg();
  BuildMI(MBB, MBBI, DL, TII->get(RISCV::SCFGW)).addReg(StrideReg).addReg(addr_stride, RegState::Kill);
  
  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_EnDis(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  bool isEnable = MBBI->getOpcode() == RISCV::PseudoSSREnable;
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  LLVM_DEBUG(dbgs() << "-- Expanding SSR " << (isEnable ? "Enable" : "Disable") << "\n");
  Enabled = isEnable;

  // emit a csrsi/csrci call to the SSR location
  if(isEnable) {
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::CSRRSI))
      .addDef(MRI.createVirtualRegister(&RISCV::GPRRegClass), RegState::Dead)
      .addImm(0x7C0).addImm(1);

    // mark the SSR registers reserved in this machine function
    unsigned ssrEnabledMask = 0;
    for (unsigned n = 0; n != 3; ++n) {
      ssrEnabledMask |= 1 << n;
    }
    RVFI->setUsedSSR(ssrEnabledMask);
  }
  else {
    BuildMI(MBB, MBBI, DL, TII->get(RISCV::CSRRCI))
      .addDef(MRI.createVirtualRegister(&RISCV::GPRRegClass), RegState::Dead)
      .addImm(0x7C0).addImm(1);
  }

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

bool RISCVExpandSSR::expandSSR_Barrier(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineBasicBlock::iterator &NextMBBI) {
  DebugLoc DL = MBBI->getDebugLoc();
  MachineInstr &MI = *MBBI;
  MachineFunction *MF = MBB.getParent();
  MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();

  unsigned streamer = (unsigned)MBBI->getOperand(0).getImm();
  
  LLVM_DEBUG(dbgs() << "-- Expanding SSR barrier on DM" << streamer << "\n");

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
  
  // build loop: %0 = scfgri 0 | DM; srli %0, %0, 31; beq %0, zero, loop
  Register R = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(LoopMBB, DL, TII->get(RISCV::SCFGRI), R).addImm(streamer);
  Register Rs = MRI.createVirtualRegister(&RISCV::GPRRegClass);
  BuildMI(LoopMBB, DL, TII->get(RISCV::SRLI), Rs).addReg(R, RegState::Kill).addImm(31);
  BuildMI(LoopMBB, DL, TII->get(RISCV::BEQ)).addReg(Rs, RegState::Kill).addReg(RISCV::X0).addMBB(LoopMBB);

  NextMBBI = MBB.end();
  MI.eraseFromParent();

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *LoopMBB);
  computeAndAddLiveIns(LiveRegs, *DoneMBB);

  return true;
}

void RISCVExpandSSR::mergePushPop(MachineBasicBlock &MBB) {
  SmallSet<Register, 8> virtRegs[NUM_SSR];
  const TargetRegisterInfo *TRI = MBB.getParent()->getRegInfo().getTargetRegisterInfo();

  Register ssr_regs[NUM_SSR];
  for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no) ssr_regs[ssr_no] = getSSRFtReg(ssr_no);

  // First pass: Detect moves to or from SSR registers
  for (auto MI = MBB.begin() ; MI != MBB.end() ; ) {
    MachineBasicBlock::iterator NMI = std::next(MI);

    LLVM_DEBUG(dbgs()<<"Analyzing: "<<*MI<<"\n");

    // detect an emitted pop and add assignment (virtual_reg, ssr_read) to list
    if(MI->getOpcode() == RISCV::FSGNJ_D) {
      LLVM_DEBUG(dbgs()<<"Found FSGNJ_D, Op 0: " << MI->getOperand(1).getReg() << " Op1: " << MI->getOperand(2).getReg() << "\n");
      
      // look for each streamer register
      for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no) {
        // check for pop
        if(MI->getOperand(1).getReg() == ssr_regs[ssr_no] && MI->getOperand(2).getReg() == ssr_regs[ssr_no]) {
          LLVM_DEBUG(dbgs()<<"  pop: both operands from SSR"<< ssr_no <<"\n");
          // append virtual register to list of assigned virtuals
          LLVM_DEBUG(dbgs()<<"  append: "<< MI->getOperand(0).getReg() <<"\n");
          virtRegs[ssr_no].insert(MI->getOperand(0).getReg());
          // remove operation
          MI->eraseFromParent();
          break;
        }
        // TODO: check for push
        else if(MI->getOperand(0).getReg() == ssr_regs[ssr_no]) {
          // This is non-trivial because a register might be used elsewhere, therefore the entire MBB
          // must be analyzed and a merge can only be made, if the register is written once
          // LLVM_DEBUG(dbgs()<<"  push: operand 0 from SSR"<< ssr_no <<"\n");
          // // append virtual register to list of assigned virtuals
          // LLVM_DEBUG(dbgs()<<"  append: "<< MI->getOperand(1).getReg() <<"\n");
          // virtRegs[ssr_no].insert(MI->getOperand(1).getReg());
          // // remove operation
          // MI->eraseFromParent();
          break;
        }
      }
    } 
    MI = NMI;
  }

  // DBG
  for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no) {
    for (auto iter = virtRegs[ssr_no].begin() ; iter != virtRegs[ssr_no].end() ; ++iter)
      LLVM_DEBUG(dbgs() << "virtregs["<<ssr_no<<"] = " << *iter << "\n");
  }

  // Second pass: Replace uses of virtual registers corresponding to DMs with FT registers
  for (auto MI = MBB.begin() ; MI != MBB.end() ; ) {
    MachineBasicBlock::iterator NMI = std::next(MI);

    // look for usage of any of the virtual registers assigned to SSRs
    for (auto operand = MI->operands_begin() ; operand != MI->operands_end() ; ++operand) {
      if(!operand->isReg()) continue;
      // check if operand is in any SSR list
      for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no) {
        if(virtRegs[ssr_no].contains(operand->getReg())) {
          LLVM_DEBUG(dbgs() << "Found use of operand " << operand->getReg() << " ssr: " << ssr_no << " in inst " <<  MI->getOpcode() << "\n");
          // substitute with SSR register
          MI->substituteRegister(operand->getReg(), ssr_regs[ssr_no], 0, *TRI);
          // guard this block and add ssr regs to live in
          MBB.addLiveIn(ssr_regs[ssr_no]);
        }
      }
    }
    MI = NMI;    
  }
  MBB.sortUniqueLiveIns();
}

/// Gather parameters for the register merging
RISCVExpandSSR::RegisterMergingPreferences RISCVExpandSSR::gatherRegisterMergingPreferences() {
  RISCVExpandSSR::RegisterMergingPreferences RMP;

  // set up defaults
  RMP.Enable = true;

  // read user 
  if (SSRRegisterMerge.getNumOccurrences() > 0)
    RMP.Enable = !SSRRegisterMerge;

  LLVM_DEBUG(dbgs() << "RMP Enable "<<RMP.Enable<<"\n");

  return RMP;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandSSR, "riscv-expand-ssr",
                RISCV_EXPAND_SSR_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandSSRPass() { return new RISCVExpandSSR(); }

} // end of namespace llvm
