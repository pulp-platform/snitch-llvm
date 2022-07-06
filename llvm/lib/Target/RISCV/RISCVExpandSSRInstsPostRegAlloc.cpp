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

#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/AntiDepBreaker.h"

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
  // auto &MRI = MF.getRegInfo();
  // auto &TRI = *MRI.getTargetRegisterInfo();
  // RegisterClassInfo RCI;
  // RCI.runOnMachineFunction(MF);
  // auto *ADB = createAggressiveAntiDepBreaker(MF, RCI, )

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

static std::pair<bool, bool> isDefIsUse(MachineInstr &MI, MCRegister R) {
  bool def = false;
  bool use = false;
  for (auto &MOP : MI.operands()) {
    if (MOP.isReg() && MOP.getReg() == R) {
      if (MOP.isDef()) def = true;
      else use = true;
    }
  }
  return std::make_pair(def, use);
}

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
    errs()<<"looing at: "<<*MII;
    if (UseMOP) errs()<<"usemop = "<<*UseMOP<<"\n";
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
  auto *MBB = beg->getParent();
  if (MBB) {
    bool avail_in_all = true;
    MachineRegisterInfo &MRI = MBB->getParent()->getRegInfo();
    for (auto *Succ : MBB->successors()) {
      if (!Succ) continue;
      LivePhysRegs liveness(*MRI.getTargetRegisterInfo());
      liveness.addLiveIns(*Succ);
      avail_in_all &= liveness.available(MRI, valR);
    }
    if (avail_in_all) return UseMOP;
  }
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
          errs()<<"looking for "<<MI->getOperand(0)<<"\n";
          MachineOperand *MO = getUniqueUser(std::next(MI.getReverse()), rangeLimit, MI->getParent()->end().getInstrIterator(), r);
          if (!MO) errs()<<"*** NOT FOUND ***\n";
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
  return Modified;
}

} // end of anonymous namespace

INITIALIZE_PASS(RISCVExpandSSRPostRegAlloc, "riscv-expand-ssr-post-reg-alloc",
                RISCV_EXPAND_SSR_POST_REG_ALLOC_NAME, false, false)
namespace llvm {

FunctionPass *createRISCVExpandSSRPostRegAllocPass() { return new RISCVExpandSSRPostRegAlloc(); }

} // end of namespace llvm


///REGMERGE USING LIVENESS, BUT SOMEHOW WORSE
// static std::pair<bool, bool> isDefIsUse(MachineInstr &MI, MCRegister R) {
//   bool def = false;
//   bool use = false;
//   for (auto &MOP : MI.operands()) {
//     if (MOP.isReg() && MOP.getReg() == R) {
//       if (MOP.isDef()) def = true;
//       else use = true;
//     }
//   }
//   return std::make_pair(def, use);
// }

// struct Liveness {
// public:
//   Liveness(const TargetRegisterInfo &TRI, const MachineRegisterInfo &MRI, MachineBasicBlock &MBB, bool end) : liveness(TRI), MBB(MBB), MRI(MRI) {
//     if (end) {
//       liveness.addLiveOuts(MBB);
//       LiveIn = MBB.end().getInstrIterator();
//     } else {
//       liveness.addLiveIns(MBB);
//       LiveIn = MBB.begin().getInstrIterator();
//     }
//   }

//   void MoveForward(MachineBasicBlock::instr_iterator Point) {
//     if (Point == LiveIn) return;
//     SmallVector<std::pair<MCPhysReg, const MachineOperand *>, 1u> clb;
//     while (LiveIn != Point && LiveIn != MBB.end().getInstrIterator()) {
//       liveness.stepForward(*LiveIn, clb);
//       LiveIn++;
//     }
//     assert(LiveIn == Point && "moved forward to point");
//   }

//   void MoveBackward(MachineBasicBlock::reverse_instr_iterator Point) {
//     assert(Point != MBB.rend().getInstrIterator() && "not rend()");
//     if (Point.getReverse() == LiveIn) return;
//     Point++; //in order to get LiveIN for Point we have to move up to and incl. Point
//     MachineBasicBlock::reverse_instr_iterator LiveInRev = LiveIn.getReverse();
//     LiveInRev++;
//     while (LiveInRev != Point && LiveInRev != MBB.rend().getInstrIterator()) {
//       liveness.stepBackward(*LiveInRev);
//       LiveInRev++;
//     }
//     LiveIn = std::next(LiveInRev.getReverse());
//     assert(LiveInRev == Point && "moved backward to point");
//   }

//   //move forward up to first use of Reg, make sure Reg is not live anymore afterwards
//   MachineBasicBlock::instr_iterator findUniqueUser(MCRegister Reg, MachineBasicBlock::instr_iterator end) {
//     while (LiveIn != end) {
//       auto ut = isDefIsUse(*LiveIn, Reg);
//       if (ut.first && !ut.second) return end; //redefined
//       if (ut.first && ut.second) return LiveIn; //first user redefines himself
//       MoveForward(std::next(LiveIn));
//       if (ut.second) {
//         if (liveness.available(MRI, Reg)) std::prev(LiveIn);
//         else {
//           for (auto x = LiveIn; x != MBB.end().getInstrIterator(); ++x) {
//             auto ut = isDefIsUse(*x, Reg);
//             if (ut.first && !ut.second) return std::prev(LiveIn); //found redef.
//             else if (ut.second) return end; //another use
//           }
//           return end;
//         }
//       }
//     }
//     return end;
//   }

//   MachineBasicBlock::instr_iterator getPoint() const { return LiveIn; }
//   const LivePhysRegs &getLiveness() const { return liveness; }
//   void addReg(MCRegister R) { liveness.addReg(R); }

// private:
//   MachineBasicBlock::instr_iterator LiveIn; //INV: this always points to the instr for which liveness has live-in info
//   LivePhysRegs liveness;
//   MachineBasicBlock &MBB;
//   const MachineRegisterInfo &MRI;
// };

// static bool isSSREn(const MachineInstr &MI) {
//   return MI.getOpcode() == RISCV::CSRRSI 
//     && MI.getOperand(1).isImm() 
//     && MI.getOperand(1).getImm() == 0x7C0
//     && MI.getOperand(2).isImm()
//     && MI.getOperand(2).getImm() == 1;
// }

// static bool isSSRDis(const MachineInstr &MI) {
//   return MI.getOpcode() == RISCV::CSRRCI 
//     && MI.getOperand(1).isImm() 
//     && MI.getOperand(1).getImm() == 0x7C0
//     && MI.getOperand(2).isImm()
//     && MI.getOperand(2).getImm() == 1;
// }

// static bool isSSRReg(MCRegister R) {
//   for (unsigned s = 0u; s < NUM_SSR; s++) {
//     if (getSSRFtReg(s).asMCReg() == R) return true;
//   }
//   return false;
// }

// static unsigned getSSRRegIdx(MCRegister R) {
//   return R - MCRegister(RISCV::F0_D);
// }

// bool RISCVExpandSSRPostRegAlloc::mergePushPop(MachineBasicBlock &MBB) {
//   bool Modified;

//   MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
//   const TargetRegisterInfo &TRI = *MRI.getTargetRegisterInfo();

//   recomputeLiveIns(MBB);
//   recomputeLivenessFlags(MBB);

//   SmallSet<const MachineInstr*, 2u> modifiedInsts[NUM_SSR]; //keep track of which insts were merged into to avoid merging two different moves of same stream into one inst
//   MachineBasicBlock::reverse_instr_iterator MII = MBB.rbegin().getInstrIterator();
//   MachineBasicBlock::instr_iterator SearchEnd = MBB.end().getInstrIterator();
//   while (MII != MBB.rend().getInstrIterator()) {
//     auto NMII = std::next(MII);

//     if (isSSRDis(*MII)) {
//       SearchEnd = MII.getReverse();
//       MII = NMII;
//       continue;
//     }

//     if (MII->getOpcode() == RISCV::FSGNJ_D) {
//       auto &MOP0 = MII->getOperand(0);
//       auto &MOP1 = MII->getOperand(1);
//       auto &MOP2 = MII->getOperand(2);
//       if (MOP0.isReg() && MOP1.isReg() && MOP2.isReg() && MOP1.getReg() == MOP2.getReg()) {
//         if (isSSRReg(MOP1.getReg()) && MII != MBB.rbegin().getInstrIterator()) { //this is ssr pop (and there is at least one potential user)
//           MCRegister dest = MOP0.getReg().asMCReg();
//           MCRegister ssr_reg = MOP1.getReg().asMCReg();
//           unsigned dmid = getSSRRegIdx(ssr_reg);
//           //try to find unique user of dest
//           Liveness Live(TRI, MRI, MBB, true);
//           Live.MoveBackward(std::prev(MII)); //increment liveness to past MII
//           auto user = Live.findUniqueUser(dest, SearchEnd);
//           if (user != SearchEnd && modifiedInsts[dmid].find(&*user) == modifiedInsts[dmid].end()) { //found user
//             user->dump();
//             for (auto &MOP : user->operands()) {
//               if (MOP.isReg() && !MOP.isDef() && MOP.getReg() == dest) MOP.setReg(ssr_reg); //replace all non-def uses of r with ssr_reg
//             }
//             user->dump();
//             MII->eraseFromBundle();
//             modifiedInsts[dmid].insert(&*user);
//             Modified = true;
//           }
//         } else if (isSSRReg(MOP0.getReg())) {
//           MCRegister src = MOP1.getReg();
//           MCRegister ssr_reg = MOP0.getReg();
//           unsigned dmid = getSSRRegIdx(ssr_reg);
//           MachineBasicBlock::reverse_instr_iterator beginSearch = std::next(MII);
//           while (beginSearch != MBB.rend().getInstrIterator()) {
//             if (isSSREn(*beginSearch)) break;
//             auto ut = isDefIsUse(*beginSearch, src);
//             if (ut.first) break;
//             beginSearch++;
//           }
//           if (beginSearch != MBB.rend().getInstrIterator() && !isSSREn(*beginSearch)) {
//             assert(isDefIsUse(*beginSearch, src).first && "does define src");
//             Liveness Live(TRI, MRI, MBB, true);
//             Live.MoveBackward(std::prev(beginSearch));
//             auto user = Live.findUniqueUser(src, std::next(MII.getReverse()));
//             if (user == MII.getReverse() && modifiedInsts[dmid].find(&*beginSearch) == modifiedInsts[dmid].end()) {
//               beginSearch->dump();
//               for (auto &MOP : beginSearch->operands()) {
//                 if (MOP.isReg() && MOP.isDef() && MOP.getReg() == src) { 
//                   MOP.setReg(ssr_reg); //replace all defs of R with ssr_reg
//                   MOP.setIsDef(false);
//                 }
//               }
//               beginSearch->dump();
//               MII->eraseFromBundle();
//               modifiedInsts[dmid].insert(&*beginSearch);
//               Modified = true;
//             }
//           }
//         }
//       }
//     }
//     MII = NMII;
//   }
//   return Modified;
// }