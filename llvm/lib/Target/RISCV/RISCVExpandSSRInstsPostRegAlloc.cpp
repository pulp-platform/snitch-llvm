//===-- RISCVExpandSSRPostRegAllocInsts.cpp - Expand SSR pseudo instructions ---------===//
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

namespace llvm {
  /// Command line options
  cl::opt<bool> SSRNoRegisterMerge("ssr-no-regmerge", cl::init(false),
    cl::desc("Disable the merging of SSR registers in other instructions"));
}

#define RISCV_EXPAND_SSR_POST_REG_ALLOC_NAME "RISCV SSR pseudo instruction expansion pass post reg alloc"

#define NUM_SSR 3

namespace {

class RISCVExpandSSRPostRegAlloc : public MachineFunctionPass {
public:
  const RISCVInstrInfo *TII;
  static char ID;

  RISCVExpandSSRPostRegAlloc() : MachineFunctionPass(ID) {
    initializeRISCVExpandSSRPostRegAllocPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return RISCV_EXPAND_SSR_POST_REG_ALLOC_NAME; }

private:

  const MachineFunction *MF;
  RISCVMachineFunctionInfo *RVFI;
  bool Enabled;

  bool expandMBB(MachineBasicBlock &MBB);
  bool mergePushPop(MachineBasicBlock &MBB);
  bool expandMI(MachineBasicBlock &MBB, MachineBasicBlock::iterator MBBI,
                MachineBasicBlock::iterator &NextMBBI);
  bool expandSSR_StoreLoadMove(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);
};

char RISCVExpandSSRPostRegAlloc::ID = 0;

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

bool RISCVExpandSSRPostRegAlloc::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());
  this->MF = &MF;
  this->RVFI = MF.getInfo<RISCVMachineFunctionInfo>();

  bool Modified = false;
  for (auto &MBB : MF) Modified |= expandMBB(MBB);

  if (SSRNoRegisterMerge) errs()<<"regmerge disabled \n";
  if (!SSRNoRegisterMerge && Modified){
    for (auto &MBB : MF) mergePushPop(MBB);
  }

  return Modified;
}

bool RISCVExpandSSRPostRegAlloc::expandMBB(MachineBasicBlock &MBB) {
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

bool RISCVExpandSSRPostRegAlloc::expandMI(MachineBasicBlock &MBB,
                                 MachineBasicBlock::iterator MBBI,
                                 MachineBasicBlock::iterator &NextMBBI) {
  switch (MBBI->getOpcode()) {
  case RISCV::PseudoStoreMove:
  case RISCV::PseudoLoadMove:
    return expandSSR_StoreLoadMove(MBB, MBBI);
  default:
    return false;
  }
}

bool RISCVExpandSSRPostRegAlloc::expandSSR_StoreLoadMove(MachineBasicBlock &MBB,
                                          MachineBasicBlock::iterator MBBI) {
  DebugLoc DL = MBBI->getDebugLoc();

  Register src = MBBI->getOperand(0).getReg();
  Register dest = MBBI->getOperand(1).getReg();

  BuildMI(MBB, MBBI, DL, TII->get(RISCV::FSGNJ_D), src)
    .addReg(dest)
    .addReg(dest);

  MBBI->eraseFromParent(); // The pseudo instruction is gone now.
  return true;
}

/*
static MachineOperand *getUniqueUser(MachineBasicBlock::instr_iterator beg, MachineBasicBlock::instr_iterator end, Register valR) {
  for (auto MII = beg; MII != end; ++MII) {
    if (MII->isDebugInstr()) continue;
    for (auto &MOP : MII->operands()){
      if (!MOP.isReg() || MOP.getReg() != valR) continue;
      if (MOP.isKill()) return &MOP;
      else return nullptr;
    }
  }
  return nullptr; //cannot be sure, maybe there is a user in a later block?
} */

static MachineOperand *getUniqueUser (
    MachineBasicBlock::instr_iterator beg, 
    MachineBasicBlock::instr_iterator end, 
    MachineBasicBlock::instr_iterator realend, 
    Register valR) 
  {
  MachineOperand *UseMOP = nullptr;
  bool isPastEnd = false;
  for (auto MII = beg; MII != realend; ++MII) {
    isPastEnd |= MII == end;
    if (MII->isDebugInstr()) continue; //skip debug instructions
    bool definesValR = false;
    for (auto &MOP : MII->operands()) {
      if (!MOP.isReg() || MOP.getReg() != valR) continue;
      //at this point we know MII accesses valR, with MOP, but maybe also other operands
      definesValR |= MOP.isDef();
      if (!isPastEnd && !UseMOP && !MOP.isDef()) {
        UseMOP = &MOP; //if UseMOP is not yet found and MOP does not redefine valR then MOP is the first Use
        if (MOP.isKill()) return UseMOP; //if MOP kills valR then we can stop looking further and return
      }
    }
    if (definesValR) {
      return UseMOP; //if MII (re-)defines valR then we must have already found the Use before, (or we haven't in which case we return null)
    }
  }
  //if we arrive at the end and have not found a redefinition or a kill, then we cannot be sure, whether valR is used after the realend ==> have to return nullptr
  return nullptr;
}

bool RISCVExpandSSRPostRegAlloc::mergePushPop(MachineBasicBlock &MBB) {
  const TargetRegisterInfo *TRI = MBB.getParent()->getRegInfo().getTargetRegisterInfo();

  Register ssr_regs[NUM_SSR];
  for(unsigned ssr_no = 0; ssr_no < NUM_SSR; ++ssr_no) ssr_regs[ssr_no] = getSSRFtReg(ssr_no);

  bool Modified = false;
  
  for (auto ssr_reg : ssr_regs){
    SmallSet<MachineInstr *, 2u> modified;
    for (auto MI = MBB.rbegin().getInstrIterator(); MI != MBB.rend().getInstrIterator(); ){ //go from back to front
      auto PMI = std::next(MI); //this is prev bc reverse iterator
      if(MI->getOpcode() == RISCV::FSGNJ_D){
        if (MI->getOperand(1).getReg() == ssr_reg && MI->getOperand(2).getReg() == ssr_reg && MI->getOperand(0).isReg()){ //this was an SSR pop
          //limit search range for regmerge if there is an ssr disable
          MachineBasicBlock::instr_iterator rangeLimit = MI.getReverse();
          for (; rangeLimit != MBB.end().getInstrIterator(); ++rangeLimit){
            if (rangeLimit->getOpcode() == RISCV::CSRRCI 
              && rangeLimit->getOperand(1).isImm() 
              && rangeLimit->getOperand(1).getImm() == 0x7C0
              && rangeLimit->getOperand(2).getImm() == 1)
            {
              break;
            } 
          }
          Register r = MI->getOperand(0).getReg(); //register to replace
          MachineOperand *MO = getUniqueUser(std::next(MI.getReverse()), rangeLimit, MI->getParent()->end().getInstrIterator(), r);
          if (MO) { //if unique user exists
            MachineInstr *MIUser = MO->getParent();
            if (MIUser && modified.find(MIUser) == modified.end()){ //if unique user exists and was not yet modified
              MIUser->dump();
              for (auto &MOP : MIUser->operands()) {
                if (MOP.isReg() && !MOP.isDef() && MOP.getReg() == r) MOP.setReg(ssr_reg); //replace all non-def uses of r with ssr_reg
              }
              MIUser->dump();
              MI->eraseFromBundle();
              modified.insert(MIUser);
            }
          }
        }else if(MI->getOperand(0).getReg() == ssr_reg){
          if (MI->getOperand(1).isReg() 
            && MI->getOperand(2).isReg() 
            && MI->getOperand(1).getReg() == MI->getOperand(2).getReg())
          { //FIXME: use liveness analysis instead of .isKill()
            Register R = MI->getOperand(1).getReg();
            MachineInstr *Pred = MI->getPrevNode();
            if (Pred && modified.find(Pred) == modified.end()){ //if Pred exists and is unmodified
              bool predDefsR = false;
              for (auto &MOP : Pred->defs()) {
                predDefsR |= MOP.isReg() && MOP.isDef() && MOP.getReg() == R;
              }
              if (predDefsR) { //if Pred defines R
                auto end = MI->getParent()->end().getInstrIterator();
                MachineOperand *MO = getUniqueUser(Pred->getIterator(), end, end, R);
                if (MO && MO->getParent() == &*MI) { //if MI is unique user of R
                  Pred->dump();
                  for (auto &MOP : Pred->operands()) {
                    if (MOP.isReg() && MOP.isDef() && MOP.getReg() == R) { 
                      MOP.setReg(ssr_reg); //replace all defs of R with ssr_reg
                      MOP.setIsDef(false);
                    }
                  }
                  Pred->dump();
                  MI->eraseFromBundle();
                  modified.insert(Pred);
                }
              }
            }
          }
        }
      }
      MI = PMI;
    }
  }
  MBB.sortUniqueLiveIns();
  return Modified;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandSSRPostRegAlloc, "riscv-expand-ssr-post-reg-alloc",
                RISCV_EXPAND_SSR_POST_REG_ALLOC_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandSSRPostRegAllocPass() { return new RISCVExpandSSRPostRegAlloc(); }

} // end of namespace llvm


/* OLD VERSION OF REGMERGE
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
  */