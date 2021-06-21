//====-- SNITCHFrepLoops.cpp - Identify and generate freppable loops ----====//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This pass identifies loops where we can generate the Snitch FPU hardware
// floating point repetition instruction (frep).  The hardware loop can perform 
// floating point instruction repetition with zero-cycle overhead
//
//  This file is based on the lib/Target/Hexagon/HexagonHardwareLoops.cpp file.
//===----------------------------------------------------------------------===//



#include "../RISCVInstrInfo.h"
#include "../RISCVRegisterInfo.h"
#include "../RISCVSubtarget.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "snitch-freploops"
#define SNITCH_FREP_LOOPS_NAME "Snitch frep loops"

static cl::opt<bool> EnableFrepInference("snitch-frep-inference", cl::init(false),
  cl::Hidden, cl::ZeroOrMore, cl::desc("Enable automatic inference of frep loops"));

STATISTIC(NumFrepLoops, "Number of loops converted to frep loops");

namespace {
  class CountValue;
  class FrepLoop;

class SNITCHFrepLoops : public MachineFunctionPass {
  MachineLoopInfo            *MLI;
  MachineRegisterInfo        *MRI;
  MachineDominatorTree       *MDT;
  const RISCVInstrInfo       *TII;
  const RISCVRegisterInfo    *TRI;
  FrepLoop                   *FL;

public:
  static char ID;

  SNITCHFrepLoops() : MachineFunctionPass(ID) {
    initializeSNITCHFrepLoopsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return SNITCH_FREP_LOOPS_NAME; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    AU.addRequired<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  /// Kinds of comparisons in the compare instructions.
  struct Comparison {
    enum Kind {
      EQ  = 0x01,
      NE  = 0x02,
      L   = 0x04,
      G   = 0x08,
      U   = 0x40,
      LTs = L,
      LEs = L | EQ,
      GTs = G,
      GEs = G | EQ,
      LTu = L      | U,
      LEu = L | EQ | U,
      GTu = G      | U,
      GEu = G | EQ | U
    };

    static Kind getSwappedComparison(Kind Cmp) {
      assert ((!((Cmp & L) && (Cmp & G))) && "Malformed comparison operator");
      if ((Cmp & L) || (Cmp & G))
        return (Kind)(Cmp ^ (L|G));
      return Cmp;
    }

    static Kind getNegatedComparison(Kind Cmp) {
      if ((Cmp & L) || (Cmp & G))
        return (Kind)((Cmp ^ (L | G)) ^ EQ);
      if ((Cmp & NE) || (Cmp & EQ))
        return (Kind)(Cmp ^ (EQ | NE));
      return (Kind)0;
    }

    static bool isSigned(Kind Cmp) {
      return (Cmp & (L | G) && !(Cmp & U));
    }

    static bool isUnsigned(Kind Cmp) {
      return (Cmp & U);
    }
  };

  /// speculatively search for preheader
  bool SpecPreheader = false;

  /// keeping track of all hardware loop
  std::set<const MachineInstr *> KnownHardwareLoops;

  /// Given a loop, check if we can convert it to a hardware loop.
  /// If so, then perform the conversion and return true.
  bool convertToHardwareLoop(MachineLoop *L);

  /// Return number of freppable instructions of loop is freppable, zero
  /// if not
  unsigned containsInvalidInstruction(MachineLoop *L, Register *IV, Register *ICV) const;

  /// Return true if the instruction is not valid within a hardware
  /// loop.
  bool isInvalidLoopOperation(const MachineInstr *MI) const;

  /// Scan the loop body and search for a branch instruction that 
  /// leads to the induction variable and trip count
  const MachineInstr * findBranchInstruction(MachineLoop *L);

  /// Analyze the statements in a loop to determine if the loop
  /// has a computable trip count and, if so, return a value that represents
  /// the trip count expression.
  CountValue *getLoopTripCount(MachineLoop *L,
                               SmallVectorImpl<MachineInstr *> &OldInsts,
                               Register *&InductionReg,
                               Register *&IncrementReg);

  /// Find the register that contains the loop controlling
  /// induction variable.
  /// If successful, it will return true and set the \p Reg, \p IVBump
  /// and \p BumpOp arguments.  Otherwise it will return false.
  /// The returned induction register is the register R that follows the
  /// following induction pattern:
  /// IndReg = PHI [InitialValue, entry], [IncReg, Latch]
  /// BumpOp: IncReg = ADDI IndReg, IVBump
  bool findInductionRegister(MachineLoop *L, unsigned &IndReg, unsigned &IncReg,
                             int64_t &IVBump, MachineInstr *&BumpOp) const;

  /// Check if the given operand has a compile-time known constant
  /// value. Return true if yes, and false otherwise. When returning true, set
  /// Val to the corresponding constant value.
  bool checkForImmediate(const MachineOperand &MO, int64_t &Val) const;

  /// Return the comparison kind for the specified opcode.
  Comparison::Kind getComparisonKind(unsigned CondOpc,
                                     MachineOperand *InitialValue,
                                     const MachineOperand *Endvalue,
                                     int64_t IVBump) const;

  /// Return the expression that represents the number of times
  /// a loop iterates.  The function takes the operands that represent the
  /// loop start value, loop end value, and induction value.  Based upon
  /// these operands, the function attempts to compute the trip count.
  /// If the trip count is not directly available (as an immediate value,
  /// or a register), the function will attempt to insert computation of it
  /// to the loop's preheader.
  CountValue *computeCount(MachineLoop *Loop, const MachineOperand *Start,
                           const MachineOperand *End, unsigned IVReg,
                           int64_t IVBump, Comparison::Kind Cmp) const;

  /// removeove the instruction if it is now dead.
  void removeIfDead(MachineInstr *MI);

  /// Return true if the instruction is now dead.
  bool isDead(const MachineInstr *MI,
              SmallVectorImpl<MachineInstr *> &DeadPhis) const;

  /// given the hardware loop L, insert a synchronization barrier after the loop
  void insertFPUBarrier(MachineLoop *L, MachineBasicBlock *MBB);

  /// check for any metadata that suggests that this loop should be frepped
  bool isMarkedForInference(MachineLoop *L);
};

/// Abstraction for a trip count of a loop. A smaller version
/// of the MachineOperand class without the concerns of changing the
/// operand representation.
class CountValue {
public:
  enum CountValueType {
    CV_Register,
    CV_Immediate
  };

private:
  CountValueType Kind;
  union Values {
    struct {
      unsigned Reg;
      unsigned Sub;
    } R;
    unsigned ImmVal;
  } Contents;

public:
  explicit CountValue(CountValueType t, unsigned v, unsigned u = 0) {
    Kind = t;
    if (Kind == CV_Register) {
      Contents.R.Reg = v;
      Contents.R.Sub = u;
    } else {
      Contents.ImmVal = v;
    }
  }

  bool isReg() const { return Kind == CV_Register; }
  bool isImm() const { return Kind == CV_Immediate; }

  unsigned getReg() const {
    assert(isReg() && "Wrong CountValue accessor");
    return Contents.R.Reg;
  }

  unsigned getSubReg() const {
    assert(isReg() && "Wrong CountValue accessor");
    return Contents.R.Sub;
  }

  unsigned getImm() const {
    assert(isImm() && "Wrong CountValue accessor");
    return Contents.ImmVal;
  }

  void print(raw_ostream &OS, const TargetRegisterInfo *TRI = nullptr) const {
    if (isReg()) { OS << "register: " << printReg(Contents.R.Reg, TRI, Contents.R.Sub) << '\n'; }
    if (isImm()) { OS << "immediate: " << Contents.ImmVal << '\n'; }
  }
};

class FrepLoop {
public:
  MachineLoop *L;
  MachineBasicBlock *TopBlock = nullptr;
  MachineBasicBlock *ExitBlock = nullptr;
  MachineBasicBlock *LoopStart = nullptr;
  MachineBasicBlock *LoopSucc = nullptr;
  MachineBasicBlock *LatchBlock = nullptr;
  MachineBasicBlock *Header = nullptr;
  MachineBasicBlock *Preheader = nullptr;
  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  bool isFirstOperandEndvalue = false;
  bool isCondExit = false;
  MachineInstr *condTerm = nullptr;
  MachineInstr *uncondTerm = nullptr;

  explicit FrepLoop(MachineLoop *ML) {
    L = ML;
  }
};

char SNITCHFrepLoops::ID = 0;


bool SNITCHFrepLoops::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs()<<"------------ Snitch Frep Loops ------------\n");

  bool Changed = false;

  MLI = &getAnalysis<MachineLoopInfo>();
  MRI = &MF.getRegInfo();
  MDT = &getAnalysis<MachineDominatorTree>();
  const RISCVSubtarget &HST = MF.getSubtarget<RISCVSubtarget>();
  TII = HST.getInstrInfo();
  TRI = HST.getRegisterInfo();

  for (auto &L : *MLI)
    if (L->isOutermost()) {
      FL = new FrepLoop(L);
      Changed |= convertToHardwareLoop(L);
      delete FL;
    }

  // sanity pass: look for infer pseudo and drop it
  for (auto &MBB : MF) {
    MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
    while (MBBI != E) {
      MachineBasicBlock::iterator NMBBI = std::next(MBBI);
      if(MBBI->getOpcode() == RISCV::PseudoFrepInfer)
        MBBI->removeFromParent();
      MBBI = NMBBI;
    }
  }

  return Changed;
}

/// Check if the loop is a candidate for converting to a hardware
/// loop.  If so, then perform the transformation.
///
/// This function works on innermost loops first.  A loop can be converted
/// if it is a counting loop; either a register value or an immediate.
///
/// The code makes several assumptions about the representation of the loop
/// in llvm.
bool SNITCHFrepLoops::convertToHardwareLoop(MachineLoop *L) {
  // This is just for sanity.
  assert(L->getHeader() && "Loop without a header?");
  
  bool changed = false;

  /// Convert all inner loops firts
  LLVM_DEBUG(L->dump(););
  for (MachineLoop::iterator I = L->begin(), E = L->end(); I != E; ++I) {
    changed |= convertToHardwareLoop(*I);
  }

  // skip if not innermost loop (should be a redundant check)
  if(!L->isInnermost()) return false;
  
  LLVM_DEBUG(dbgs()<<"-------------------\n");
  LLVM_DEBUG(dbgs()<<"Running on\n");
  LLVM_DEBUG(L->dump(););
  LLVM_DEBUG(dbgs()<<"-------------------\n");

  // Check if marked for inference or global inference is enabled
  if(!isMarkedForInference(L) && !EnableFrepInference)
    return changed;

  SmallVector<MachineInstr*, 2> OldInsts, NewInsts;
  Register *IndReg, *IncReg;

  LLVM_DEBUG(dbgs() << ">>>>> in getLoopTripCount()\n");
  CountValue *TripCount = getLoopTripCount(L, OldInsts, IndReg, IncReg);

  if(TripCount == nullptr) {
    LLVM_DEBUG(dbgs() << "trip count not found\n");
    return changed;
  }
  LLVM_DEBUG(dbgs() << "getLoopTripCount found count "; TripCount->print(dbgs(), TRI));

  // Does the loop contain any invalid instructions?
  LLVM_DEBUG(dbgs() << ">>>>> in containsInvalidInstruction()\n");
  unsigned nFlops = containsInvalidInstruction(L, IndReg, IncReg);
  if (nFlops == 0) {
    return changed;
  }
  LLVM_DEBUG(dbgs() << "No invalid instructions found\n");

  // don't proceed if we don't have a control block
  MachineBasicBlock *ControlBlock = L->findLoopControlBlock();
  if (!ControlBlock) {
    return changed;
  }
  LLVM_DEBUG(dbgs()<<"ControlBlock: " << ControlBlock->getName() << "\n");

  // malformed loop, we expect more than one terminators, one to the
  // loop latch, one out of the loop
  MachineBasicBlock::iterator LastI = ControlBlock->getFirstTerminator();
  if (LastI == ControlBlock->end()) {
    return changed;
  }

  // Ensure the loop has a preheader: the loop instruction will be
  // placed there.
  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  if (!Preheader) {
    dbgs() << "No preheader found\n";
    return changed;
  }

  // Is the trip count available in the preheader?
  // Don't worry if it is immediate
  if (TripCount->isReg()) {
    dbgs() << "Don't know yet how to handle register trip counts\n";
    return changed;
    // There will be a use of the register inserted into the preheader,
    // so make sure that the register is actually defined at that point.
    // MachineInstr *TCDef = MRI->getVRegDef(TripCount->getReg());
    // MachineBasicBlock *BBDef = TCDef->getParent();
    // if (!MDT->dominates(BBDef, Preheader)) {
    //   return Changed;
    // }
  }

  // Determine the loop start.
  LLVM_DEBUG(dbgs() << ">>>>> find loop start\n");
  MachineBasicBlock * TopBlock = L->getTopBlock();
  MachineBasicBlock * ExitBlock = L->getExitBlock();
  MachineBasicBlock * LoopStart = nullptr;
  MachineBasicBlock * LoopSucc = nullptr;
  MachineBasicBlock * LatchBlock = L->getLoopLatch();
  MachineBasicBlock * Header = L->getHeader();
  MachineBasicBlock * TB = nullptr, * FB = nullptr;
  SmallVector<MachineOperand, 2> Cond;
  LLVM_DEBUG(dbgs()<<"TopBlock: " << TopBlock->getName() << "\n");
  bool branchNotFound = TII->analyzeBranch(*ControlBlock, TB, FB, Cond, false);
  if (ControlBlock !=  LatchBlock) {
    if (branchNotFound) {
      return changed;
    }
    if (L->contains(TB)) {
      LoopStart = TB;
      LoopSucc = FB;
    }
    else if (L->contains(FB)) {
      LoopStart = FB;
      LoopSucc = TB;
    }
    else {
      return changed;
    }
  }
  else {
    LoopStart = TopBlock;
    LoopSucc = L->contains(TB) ? FB : TB;
  }
  assert(LoopStart != nullptr && "Didn't find loop start!");
  LLVM_DEBUG(dbgs()<<"LoopStart: " << LoopStart->getName()<<" LoopSucc: " << LoopSucc->getName() << "\n");

  // We need a single exit block to make sure that this loop can be simplified
  // to a fixed amount of loop iterations.
  if (!ExitBlock) {
    dbgs() << "no exit block found\n";
    return changed;
  }

  // Ensure the loop length can reasonably fit into 12 bits.  Assume
  // non-compressed instructions as an upper-bound for the length. The length
  // gives the offset which must fit in 12 bits.
  LLVM_DEBUG(dbgs() << ">>>>> find loop size\n");
  unsigned loopSize = 0;
  const unsigned instructionSize = 4;
  for (const MachineBasicBlock *LB : L->getBlocks()) {
    loopSize += instructionSize * LB->size();
    if (loopSize > 0xFFF) {
      dbgs() << "loopsize exceeds limit: "<<loopSize<<"\n";
      return changed;
    }
  }
  LLVM_DEBUG(dbgs() << "loopsize: "<<loopSize/instructionSize<<" instructions "<<nFlops<<" flops\n");
  
  // Convert the loop to a hardware loop.
  LLVM_DEBUG(dbgs() << ">>>>> insert frep\n");
  MachineBasicBlock::iterator InsertPos = TopBlock->getFirstNonPHI();
  LLVM_DEBUG(dbgs() << "Change to hardware loop at "; L->dump());
  DebugLoc DL;
  if (InsertPos != Header->end())
    DL = InsertPos->getDebugLoc();

  if (TripCount->isReg()) {
    // Create a copy of the loop count register.
    unsigned CountReg = MRI->createVirtualRegister(&RISCV::GPRRegClass);
    BuildMI(*TopBlock, InsertPos, DL, TII->get(TargetOpcode::COPY), CountReg)
      .addReg(TripCount->getReg(), 0, TripCount->getSubReg());
    // wat tripcount-1 in the register
    BuildMI(*TopBlock, InsertPos, DL, TII->get(RISCV::ADDI))
      .addReg(CountReg).addReg(CountReg).addImm(-1);
    // Add the Loop instruction to the beginning of the loop.
    auto hwloop = BuildMI(*TopBlock, InsertPos, DL, TII->get(RISCV::FREP_O))
      .addReg(CountReg).addImm(nFlops).addImm(0).addImm(0);
    KnownHardwareLoops.insert(hwloop.getInstr());
  } else {
    assert(TripCount->isImm() && "Expecting immediate value for trip count if not register");
    // Add the Loop immediate instruction to the beginning of the loop,
    // write the immediate to a register
    int64_t CountImm = TripCount->getImm();
    unsigned CountReg = MRI->createVirtualRegister(&RISCV::GPRRegClass);
    BuildMI(*TopBlock, InsertPos, DL, TII->get(RISCV::ADDI), CountReg)
      .addReg(RISCV::X0).addImm(CountImm-1);
    auto hwloop = BuildMI(*TopBlock, InsertPos, DL, TII->get(RISCV::FREP_O))
      .addReg(CountReg).addImm(nFlops).addImm(0).addImm(0);
    KnownHardwareLoops.insert(hwloop.getInstr()); 
  }
  delete TripCount;

  // Make sure the loop start always has a reference in the CFG.  We need
  // to create a BlockAddress operand to get this mechanism to work both the
  // MachineBasicBlock and BasicBlock objects need the flag set.
  ControlBlock->setHasAddressTaken();
  ControlBlock->setLabelMustBeEmitted();
  // This line is needed to set the hasAddressTaken flag on the BasicBlock
  // object.
  BlockAddress::get(const_cast<BasicBlock *>(ControlBlock->getBasicBlock()));

  // The induction operation and the comparison may now be
  // unneeded. If these are unneeded, then remove them.
  LLVM_DEBUG(dbgs() << ">>>>> cleanup\n");

  // if the conditional branch target is true and header, replace it with a 
  // unconditional branch to the exit block
  if(FL->isCondExit && LatchBlock==ControlBlock) {
    assert(FL->condTerm != nullptr && "Expected a reference to the conditional term");
    assert(LoopSucc != nullptr && "Expected a loop successor for unconditional branch");
    LLVM_DEBUG(dbgs()<<"Change conditional exit branch to uncondition with target " << LoopSucc->getName() << "\n");
    LLVM_DEBUG(dbgs()<<"  insert at"; FL->condTerm->dump());
    NewInsts.push_back(BuildMI(*FL->condTerm->getParent(), FL->condTerm, FL->condTerm->getDebugLoc(),
      TII->get(RISCV::PseudoBR)).addMBB(LoopSucc).getInstr());
    FL->condTerm->removeFromParent();
  }
  else {
    OldInsts.push_back(FL->condTerm);
  }

  // original cleanup routine TWICE, becsue first migh remove some uses
  for(unsigned j = 0; j < 2; ++j)
    for (unsigned i = 0; i < OldInsts.size(); ++i) {
      if(OldInsts[i] && OldInsts[i]->getParent()) {
        LLVM_DEBUG(dbgs()<<"Check if can be removed:"; OldInsts[i]->dump());
        removeIfDead(OldInsts[i]);
      }
    }

  // search for all forward/backedges and remove them
  for (MachineBasicBlock *MBB : L->getBlocks()) {
    MachineBasicBlock::iterator term = MBB->getFirstTerminator();

    if(term == MBB->end())
      continue;

    MachineBasicBlock::iterator nextTerm;
    for(;term != MBB->end(); term = nextTerm) {
      nextTerm = std::next(term);

      // ignore if this instruction was added by this pass
      unsigned skip = 0;
      for(unsigned p = 0; p < NewInsts.size(); ++p) {
        if(NewInsts[p] == term) {
          skip = 1;
          LLVM_DEBUG(dbgs()<<"skip removal of terminator"; term->dump());
        }
      } 
      if(skip) continue;

      // unconditional branch from latch to header
      if(term->isUnconditionalBranch() && 
        term->getOperand(0).getMBB() == Header) {
        LLVM_DEBUG(dbgs()<<"removing unconditional backedge to header "; term->dump());
        // replace successors: remove header and add exit
        if(!term->getParent()->isSuccessor(LoopSucc)) term->getParent()->addSuccessor(LoopSucc);
        term->getParent()->removeSuccessor(Header);
        term->removeFromParent();
      }
      // unconditional branch from header to latch
      else if(term->isUnconditionalBranch() && 
        term->getOperand(0).getMBB() == LatchBlock) {
        LLVM_DEBUG(dbgs()<<"removing unconditional backedge to latch "; term->dump());
        // replace successors: remove header and add exit
        term->getParent()->removeSuccessor(LoopSucc);
        term->removeFromParent();
      }
      // unconditional branch from control to successor
      else if(term->isUnconditionalBranch() && 
        term->getOperand(0).getMBB() == LoopSucc) {
        LLVM_DEBUG(dbgs()<<"removing unconditional branch to loop successor "; term->dump());
        // replace successors: remove header and add exit
        // term->getParent()->removeSuccessor(LoopSucc);
        term->removeFromParent();
      }
    }
  }

  // Dead code from old PHI-merging approach where phi's are removed when no longer needed
  // This caused problems with multiple frep loops in a single function
  // Removing it fixed the problem. The phis get correctly eliminated in later stages

  // map which register to what new register
//   LLVM_DEBUG(dbgs() << ">>>>> eliminate PHI\n");
//   const MachineOperand * opRes, * opIn, * opOut;
//   using MergeMap = std::map<Register, Register>;
//   MergeMap mm;
//   SmallVector<MachineInstr*, 2> OldPhis;
//   for (MachineInstr &MI : *Header) {
//     if(MI.isPHI()) {
//       if(MI.getOperand(2).getMBB() == Preheader && MI.getOperand(4).getMBB() == LatchBlock) {
//         opRes = &MI.getOperand(0);
//         opIn = &MI.getOperand(1);
//         opOut = &MI.getOperand(3);
//       }
//       else if(MI.getOperand(4).getMBB() == Preheader && MI.getOperand(2).getMBB() == LatchBlock) {
//         opRes = &MI.getOperand(0);
//         opIn = &MI.getOperand(3);
//         opOut = &MI.getOperand(1);
//       }
//       else 
//         continue;
//       // insert the registers into the merge mapping
//       mm.insert(std::make_pair(opRes->getReg(), opIn->getReg()));
//       mm.insert(std::make_pair(opOut->getReg(), opIn->getReg()));
//       // find definition of register comming from loop body
//       MachineInstr *defMI = MRI->getVRegDef(opOut->getReg());
//       // convert def to use
//       // if(MI.getDesc().OpInfo[0].isOptionalDef())
//         defMI->getOperand(0).setIsDef(false);
//       // and substitute the register with the one comming from the header
//       defMI->substituteRegister(opOut->getReg(), opIn->getReg(), 0, *TRI);
//       // mark the phi to remove
//       OldPhis.push_back(&MI);
//     }
//   }

// #ifndef NDEBUG
//   LLVM_DEBUG(dbgs()<<"  merge map:\n");
//   for(auto key : mm) {
//     LLVM_DEBUG(dbgs()<<"    %"<<key.first.virtRegIndex()<<" |-> %"<<key.second.virtRegIndex()<<"\n");
//   }
// #endif

//   using use_nodbg_iterator = MachineRegisterInfo::use_nodbg_iterator;
//   use_nodbg_iterator nextJ;
//   use_nodbg_iterator End = MRI->use_nodbg_end();
//   // iterate over all uses of the defined reg
//   for(auto key : mm) {
//     for (use_nodbg_iterator J = MRI->use_nodbg_begin(key.first);
//          J != End; J = nextJ) {
//       nextJ = std::next(J);
//       // don't touch phi
//       if(J->getParent()->isPHI()) continue;
//       // substitute the use with the register in the map
//       J->substVirtReg(key.second, 0, *TRI);
//     }
//   }
  
//   // remove all PHIs that we just merged
//   for(auto phi : OldPhis) {
//     LLVM_DEBUG(dbgs()<<"removing\n"; phi->dump());
//     phi->removeFromParent();
//   }

  // insert FPU barrier to synchronize float and integer pipelines after loop
  insertFPUBarrier(L, ExitBlock);

  // statistics
  ++NumFrepLoops;

  FL->TopBlock = TopBlock;
  FL->ExitBlock = ExitBlock;
  FL->LoopStart = LoopStart;
  FL->LoopSucc = LoopSucc;
  FL->LatchBlock = LatchBlock;
  FL->Header = Header;
  FL->TB = TB;
  FL->TB = FB;

  return true;
}

/// Return true if the loop contains an instruction that inhibits
/// the use of the hardware loop instruction.
unsigned SNITCHFrepLoops::containsInvalidInstruction(MachineLoop *L, Register *IV, Register *ICV) const {
  MachineBasicBlock *Header = L->getHeader();
  MachineBasicBlock *Latch = L->getLoopLatch();
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();
  bool skip = false;
  unsigned Flops = 0;
  // TODO: do not check header block, it will be removed later anyway
  
  for (MachineBasicBlock *MBB : L->getBlocks()) {
    for (MachineBasicBlock::iterator
           MII = MBB->begin(), E = MBB->end(); MII != E; ++MII) {
      const MachineInstr *MI = &*MII;
      LLVM_DEBUG(dbgs()<<"Checking"; MI->dump());

      if(MI->getOpcode() ==  TargetOpcode::PHI) {
        LLVM_DEBUG(dbgs() << "  ignoring PHI node\n");
        continue;
      }

      if(MI->getOpcode() ==  TargetOpcode::COPY) {
        LLVM_DEBUG(dbgs() << "  ignoring COPY\n");
        continue;
      }

      if(MI->isUnconditionalBranch()) {
        if(MI->getOperand(0).getMBB() == Latch) {
          LLVM_DEBUG(dbgs() << "  ignoring unconditional branch to latch\n");
          continue;
        }
        if(MI->getOperand(0).getMBB() == Header) {
          LLVM_DEBUG(dbgs() << "  ignoring unconditional branch to header\n");
          continue;
        }
        if(MI->getOperand(0).getMBB() == ExitingBlock) {
          LLVM_DEBUG(dbgs() << "  ignoring unconditional branch to Exit\n");
          continue;
        }
        if(MI->getOperand(0).getMBB() == L->getExitBlock()) {
          LLVM_DEBUG(dbgs() << "  ignoring unconditional branch to Exit Block\n");
          continue;
        }
      }

      if(MI->isDebugValue()) {
        LLVM_DEBUG(dbgs() << "  ignoring debug value\n");
        continue;
      }

      skip = false;
      if(MI->isConditionalBranch()) {
        for(auto MO : MI->operands())
          if(MO.isReg() && (MO.getReg() == *IV || MO.getReg() == *ICV) ) {
            LLVM_DEBUG(dbgs()<<"  ignoring conditional branch with inudction register usage\n");
            skip = true;
            break;
          }
      }
      if(skip) continue;

      // skip if contains induction variable IV
      skip = false;
      for(auto MO : MI->operands())
        if(MO.isReg() && (MO.getReg() == *IV) ) {
          LLVM_DEBUG(dbgs()<<"  skipped due to inudction register usage\n");
          skip = true;
          break;
        }
      if(skip) continue;

      if (isInvalidLoopOperation(MI)) {
        LLVM_DEBUG(dbgs() << "Cannot convert to hw_loop due to:"; MI->dump());
        return 0;
      }
      Flops++;
    }
  }
  return Flops;
}

/// Return true if the operation is invalid within hardware loop.
bool SNITCHFrepLoops::isInvalidLoopOperation(const MachineInstr *MI) const {
  // If all virtual register operands are FPR, this instruction is valid for frep
  bool validInstr = true;
  for(auto MO : MI->operands())
    if(MO.isReg() && MO.getReg().isVirtual())
      if(MRI->getRegClass(MO.getReg()) != &RISCV::FPR64RegClass || 
        MRI->getRegClass(MO.getReg()) != &RISCV::FPR32RegClass)
        validInstr = false;
  return validInstr;
}

const MachineInstr * SNITCHFrepLoops::findBranchInstruction(MachineLoop *L) {

  using namespace RISCV;
  static const unsigned Bops[] = {BNE};
  unsigned insInBops = 0;
  const MachineInstr *BI = nullptr;

  for (MachineBasicBlock *MBB : L->getBlocks()) {
    for (MachineBasicBlock::iterator
           MII = MBB->begin(), E = MBB->end(); MII != E; ++MII) {
      const MachineInstr *MI = &*MII;
      for(unsigned i = 0; i < sizeof(Bops)/sizeof(Bops[0]); ++i)
        if(MI->getOpcode() == Bops[i]) {
          BI = MI;
          ++insInBops;
        }
    }
  }
  if(insInBops != 1) {
    LLVM_DEBUG(dbgs() << "Multiple or none ("<<insInBops<<") branch insns found\n");
    return nullptr;
  }
  return BI;
}

/// Analyze the statements in a loop to determine if the loop has
/// a computable trip count and, if so, return a value that represents
/// the trip count expression.
///
/// This function iterates over the phi nodes in the loop to check for
/// induction variable patterns that are used in the calculation for
/// the number of time the loop is executed.
CountValue *SNITCHFrepLoops::getLoopTripCount(MachineLoop *L,
    SmallVectorImpl<MachineInstr *> &OldInsts,
    Register *&InductionReg,
    Register *&IncrementReg) {

  MachineBasicBlock *TopMBB = L->getTopBlock();
  MachineBasicBlock::pred_iterator PI = TopMBB->pred_begin();
  assert(PI != TopMBB->pred_end() &&
         "Loop must have more than one incoming edge!");
  MachineBasicBlock *Backedge = *PI++;
  if (PI == TopMBB->pred_end()) { // dead loop?
    return nullptr;
  }
  MachineBasicBlock *Incoming = *PI++;
  if (PI != TopMBB->pred_end()) {  // multiple backedges?
    return nullptr;
  }

  // Make sure there is one incoming and one backedge and determine which
  // is which.
  if (L->contains(Incoming)) {
    if (L->contains(Backedge)) {
      return nullptr;
    }
    std::swap(Incoming, Backedge);
  } else if (!L->contains(Backedge)) {
    return nullptr;
  }

  LLVM_DEBUG(dbgs()<<"Contains Incoming: " << L->contains(Incoming)<<" Concains Backedge: " << L->contains(Backedge)<<"\n");
  
  // Look for the cmp instruction to determine if we can get a useful trip
  // count.  The trip count can be either a register or an immediate.  The
  // location of the value depends upon the type (reg or imm).
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();
  if (!ExitingBlock) {
    return nullptr;
  }

  LLVM_DEBUG(dbgs()<<"ExitingBlock (contains: "<<L->contains(ExitingBlock)<<")\n");

  // Find the registers used to hold the induction variable.
  unsigned IndReg = 0, IncReg = 0;
  int64_t IVBump = 0;
  MachineInstr *BumpOp; // instruction that bumps the induction variable
  bool FoundIV = findInductionRegister(L, IndReg, IncReg, IVBump, BumpOp);
  if (!FoundIV) {
    return nullptr;
  }

  // mark the PHI to be deleted
  OldInsts.push_back(BumpOp);

  // Find the InitialValue using the loop header PHI node
  // the value comming from the BB that points to the Preheader is the initial value
  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  MachineOperand *InitialValue = nullptr;
  MachineInstr *IV_Phi = MRI->getVRegDef(IndReg);
  MachineBasicBlock *Latch = L->getLoopLatch();
  LLVM_DEBUG(dbgs()<<"IV_Phi: ";IV_Phi->dump());
  for (unsigned i = 1, n = IV_Phi->getNumOperands(); i < n; i += 2) {
    MachineBasicBlock *MBB = IV_Phi->getOperand(i+1).getMBB();
    if (MBB == Preheader)
      InitialValue = &IV_Phi->getOperand(i);
    // else if (MBB == Latch)
    //   IndReg = IV_Phi->getOperand(i).getReg();  // Want IV reg after bump.
  }
  if (!InitialValue) {
    return nullptr;
  }
  LLVM_DEBUG(dbgs()<<"initial value     : "; InitialValue->dump());
  LLVM_DEBUG(dbgs()<<"IndReg def : "; MRI->getVRegDef(IndReg)->dump());
  LLVM_DEBUG(dbgs()<<"IncReg def : "; MRI->getVRegDef(IncReg)->dump());

  // Get loop branch condition to determine trip count later
  SmallVector<MachineOperand,2> Cond;
  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  bool NotAnalyzed = TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false);
  if (NotAnalyzed) {
    return nullptr;
  }

  // find instruction that checks the condition Cond and mark it for removal
  // for (MachineBasicBlock *MBB : L->getBlocks()) {
  //   for (MachineBasicBlock::iterator
  //          MII = MBB->begin(), E = MBB->end(); MII != E; ++MII) {
  //     MachineInstr *MI = &*MII;
  //     if(MI->getOpcode() == Cond[0].getImm() && MI->isBranch()) {
  //       LLVM_DEBUG(dbgs()<<"found branch instruction: "; MI->dump());
  //       OldInsts.push_back(MI);
  //     }
  //   }
  // }

  // if the true branch points to header or latch, the condition in Cond
  // is true if the loop should continue. Else, if the condition is true
  // the loop is exited
  bool isCondExit = true;
  MachineBasicBlock *Header = L->getHeader();
  if((TB == Latch) || (TB == Header)) isCondExit = false;

  // search for the end value
  // Sadly, the following code gets information based on the position
  // of the operands in the compare instruction.  This has to be done
  // this way, because the comparisons check for a specific relationship
  // between the operands (e.g. is-less-than), rather than to find out
  // what relationship the operands are in (as on PPC).
  bool isFirstOperandEndvalue = false;
  const MachineOperand &Op1 = Cond[1];
  const MachineOperand &Op2 = Cond[2];
  const MachineOperand *EndValue = nullptr;

  isFirstOperandEndvalue = Op1.getReg() != IndReg && Op1.getReg() != IncReg;
  EndValue  = (isFirstOperandEndvalue ? &Op1 : &Op2);

  LLVM_DEBUG(dbgs() << "isFirstOperandEndvalue: " << isFirstOperandEndvalue << " isCondExit: " << isCondExit << "\n");

  // can remove definition of end value
  OldInsts.push_back(MRI->getVRegDef(isFirstOperandEndvalue ? Op1.getReg() : Op2.getReg()));

  LLVM_DEBUG(dbgs()<<"Op1     : "; Op1.dump());
  LLVM_DEBUG(dbgs()<<"Op2     : "; Op2.dump());
  
  LLVM_DEBUG(dbgs()<<"EndValue reg def : "; MRI->getVRegDef(EndValue->getReg())->dump());
  Comparison::Kind Cmp;
  unsigned CondOpc = Cond[0].getImm();
  Cmp = getComparisonKind(CondOpc, InitialValue, EndValue, IVBump);

  if (!Cmp) {
    return nullptr;
  }

  // modify comparison to get the it of the form
  // [induction variable] <condition> [end value] -> Continue loop
  if (isFirstOperandEndvalue)
    Cmp = Comparison::getSwappedComparison(Cmp);
  if (isCondExit)
    Cmp = Comparison::getNegatedComparison(Cmp);

  LLVM_DEBUG(dbgs()<<"Condition: [induction] "
      << (Cmp & Comparison::Kind::L  ? "L" : "")
      << (Cmp & Comparison::Kind::G  ? "G" : "")
      << (Cmp & Comparison::Kind::EQ ? "EQ" : "")
      << (Cmp & Comparison::Kind::NE ? "NE" : "")
      << (Cmp & Comparison::Kind::U  ? "U" : "") << " [end] -> continue\n");

  // Check if we can fully qualify start and end value
  if (InitialValue->isReg()) {
    unsigned R = InitialValue->getReg();
    MachineBasicBlock *DefBB = MRI->getVRegDef(R)->getParent();
    if (!MDT->properlyDominates(DefBB, Header)) {
      int64_t V;
      if (!checkForImmediate(*InitialValue, V)) {
        LLVM_DEBUG(dbgs()<<"can't determine initial value\n");
        return nullptr;
      }
    }
    OldInsts.push_back(MRI->getVRegDef(R));
  }
  if (EndValue->isReg()) {
    unsigned R = EndValue->getReg();
    MachineBasicBlock *DefBB = MRI->getVRegDef(R)->getParent();
    if (!MDT->properlyDominates(DefBB, Header)) {
      int64_t V;
      if (!checkForImmediate(*EndValue, V)) {
        LLVM_DEBUG(dbgs()<<"can't determine end value\n");
        return nullptr;
      }
    }
    OldInsts.push_back(MRI->getVRegDef(R));
  }

  FL->isFirstOperandEndvalue = isFirstOperandEndvalue;
  FL->isCondExit = isCondExit;

  InductionReg = new Register(IndReg);
  IncrementReg = new Register(IncReg);
  return computeCount(L, InitialValue, EndValue, IndReg, IVBump, Cmp);
}


  bool SNITCHFrepLoops::findInductionRegister(MachineLoop *L,
                                              unsigned &IndReg, 
                                              unsigned &IncReg,
                                              int64_t &IVBump, 
                                              MachineInstr *&BumpOp) const {
  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  MachineBasicBlock *Header = L->getHeader();
  MachineBasicBlock *Latch = L->getLoopLatch();
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();

  if (!Header || !Preheader || !Latch || !ExitingBlock) {
    return false;
  }
  LLVM_DEBUG(dbgs()<<"Found Header ["<<Header->getName()<<"], Preheader ["<<Preheader->getName()<<"], Latch ["<<Latch->getName()<<"], ExitingBlock ["<<ExitingBlock->getName()<<"]\n");

  bool latchExitOneBlock = (Latch == ExitingBlock);
  LLVM_DEBUG(dbgs() << "latchExitOneBlock = "<<latchExitOneBlock<<"\n");

  // This pair represents an induction register together with an immediate
  // value that will be added to it in each loop iteration.
  using RegisterBump = std::pair<unsigned, int64_t>;

  // Mapping:  R -> (R.next, bump), where R, R.next and bump are derived
  // from an induction operation
  //   R.next = R + bump
  // where bump is an immediate value.
  using InductionMap = std::map<unsigned, RegisterBump>;

  InductionMap IndMap;

  using instr_iterator = MachineBasicBlock::instr_iterator;

  for (instr_iterator I = Header->instr_begin(), E = Header->instr_end();
       I != E && I->isPHI(); ++I) {
    MachineInstr *Phi = &*I;

    LLVM_DEBUG(dbgs()<<"found PHI: "; Phi->dump());

    // Have a PHI instruction.  Get the operand that corresponds to the
    // latch block, and see if is a result of an addition of form "reg+imm",
    // where the "reg" is defined by the PHI node we are looking at.
    for (unsigned i = 1, n = Phi->getNumOperands(); i < n; i += 2) {
      if (Phi->getOperand(i+1).getMBB() != Latch)
        continue;

      // If the PHI is the register operand to an ADDI, it corresponds to the
      // induction pattern we are looking for.
      unsigned PhiOpReg = Phi->getOperand(i).getReg();
      MachineInstr *DI = MRI->getVRegDef(PhiOpReg);
      if (DI->getDesc().getOpcode() == RISCV::ADDI) {
        unsigned InducReg = DI->getOperand(1).getReg();
        MachineOperand &Opnd2 = DI->getOperand(2);
        int64_t V;
        // if induction reg is defined by the Phi and the add value is immediate
        if (MRI->getVRegDef(InducReg) == Phi && checkForImmediate(Opnd2, V)) {
          // we found the induction register and the bump value
          unsigned UpdReg = DI->getOperand(0).getReg();
          if(latchExitOneBlock)
            IndMap.insert(std::make_pair(UpdReg, std::make_pair(InducReg, V)));
          else
            IndMap.insert(std::make_pair(InducReg, std::make_pair(UpdReg, V)));
        }
      }
    }
  }

  // If we couldn't find any registers used for the induction, we fail.
  if (IndMap.empty()) {
    LLVM_DEBUG(dbgs()<<"Couldn't find any PHI nodes\n");
    return false;
  }

#ifndef NDEBUG
  LLVM_DEBUG(dbgs() << "IndMap:\n");
  for(auto it = IndMap.begin(); it != IndMap.end(); ++it) {
    LLVM_DEBUG(MRI->getVRegDef(it->first)->dump());
    LLVM_DEBUG(dbgs() << "   \\--> ");
    LLVM_DEBUG(MRI->getVRegDef(it->second.first)->dump());
    LLVM_DEBUG(dbgs() << "    bump = " << it->second.second << "\n");
  }
#endif

  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  SmallVector<MachineOperand,2> Cond;
  // Check that the exit branch can be analyzed.
  // AnalyzeBranch returns true if it fails to analyze branch.
  bool NotAnalyzed = TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false);
  if (NotAnalyzed || Cond.empty()) {
    LLVM_DEBUG(dbgs()<<"Couldn't analyze block ("<<NotAnalyzed<<") or got no conditions\n");
    return false;
  }
  LLVM_DEBUG(dbgs()<<"True dst: ["<<TB->getName()<<"] False dst: ["<<FB->getName()<<"]\n");

  // We now know there are two terminators, one conditional and one
  // unconditional. If the order does not match what we expect, bail out.
  MachineInstr *condTerm = &(*ExitingBlock->getFirstTerminator());
  MachineInstr *uncondTerm = &(*std::next(ExitingBlock->getFirstTerminator()));
  if (!(condTerm->getDesc().isConditionalBranch() &&
      uncondTerm->getDesc().isUnconditionalBranch())) {
    return false;
  }
  LLVM_DEBUG(dbgs()<<"condTerm    :"; condTerm->dump());
  LLVM_DEBUG(dbgs()<<"uncondTerm  :"; uncondTerm->dump());
  LLVM_DEBUG(dbgs()<<"Cond[1]     : "; Cond[1].dump());
  LLVM_DEBUG(dbgs()<<"Cond[2]     : "; Cond[2].dump());

  FL->condTerm = condTerm;
  FL->uncondTerm = uncondTerm;

  // Get the register numbers
  unsigned CmpReg1 = Cond[1].isReg() ? (unsigned) Cond[1].getReg() : 0;
  unsigned CmpReg2 = Cond[2].isReg() ? (unsigned) Cond[2].getReg() : 0;

  // Exactly one of the input registers to the comparison should be among
  // the induction registers.
  InductionMap::iterator IndMapEnd = IndMap.end();
  InductionMap::iterator F = IndMapEnd;
  if (CmpReg1 != 0) {
    InductionMap::iterator F1 = IndMap.find(CmpReg1);
    if (F1 != IndMapEnd)
      F = F1;
  }
  if (CmpReg2 != 0) {
    InductionMap::iterator F2 = IndMap.find(CmpReg2);
    if (F2 != IndMapEnd) {
      if (F != IndMapEnd) {
        LLVM_DEBUG(dbgs() << "both PHI registers found in conditional, abort\n");
        return false;
      }
      F = F2;
    }
  }
  if (F == IndMapEnd) {
    LLVM_DEBUG(dbgs() << "no PHI registers found in conditional, abort\n");
    return false;
  }

  if(!latchExitOneBlock) {
    IndReg = F->first;
    IncReg = F->second.first;
    IVBump = F->second.second;
    BumpOp = MRI->getVRegDef(F->second.first);
  }
  else {
    IndReg = F->second.first;
    IncReg = F->first;
    IVBump = F->second.second;
    BumpOp = MRI->getVRegDef(F->first);
  }

  LLVM_DEBUG(dbgs()<< "Found induction! IndReg ["<<IndReg<<"] IncReg ["<<IncReg<<"] Bump ["<<IVBump<<"] BumpOp ["<<BumpOp<<"]\n");
  return true;
}

bool SNITCHFrepLoops::checkForImmediate(const MachineOperand &MO,
                                             int64_t &Val) const {

  if (MO.isImm()) {
    Val = MO.getImm();
    return true;
  }
  if (!MO.isReg()) {
    return false;
  }

  // MO is a register. Check whether it is defined as an immediate value,
  // and if so, get the value of it in TV. That value will then need to be
  // processed to handle potential subregisters in MO.
  int64_t TV;

  unsigned R = MO.getReg();
  if (!Register::isVirtualRegister(R)) {
    if (R == RISCV::X0) {
      // This is the zero register!
      Val = 0;
      return true;
    }
    return false;
  }
  MachineInstr *DI = MRI->getVRegDef(R);
  unsigned DOpc = DI->getOpcode();
  switch (DOpc) {
    case TargetOpcode::COPY:
      // Call recursively to avoid an extra check whether operand(1) is
      // indeed an immediate (it could be a global address, for example),
      // plus we can handle COPY at the same time.
      if (!checkForImmediate(DI->getOperand(1), TV)) {
        return false;
      } else {
        Val = TV;
      }
      break;
    case RISCV::ADDI:
      if (DI->getOperand(1).isReg() && 
          DI->getOperand(1).getReg() == RISCV::X0) {
        // Load an immediate into a register
        if (!checkForImmediate(DI->getOperand(2), TV)) {
          return false;
        } else {
          Val = TV;
        }
      } else if (DI->getOperand(2).isImm() && DI->getOperand(2).getImm() == 0) {
        // Move a value from Op1 to Op0.
        if (!checkForImmediate(DI->getOperand(1), TV)) {
          return false;
        } else {
          Val = TV;
        }
      } else {
        // This ADDI is not used to LOAD IMM or to MOVE a value.
        return false;
      }
      break;
    default:
      return false;
  }

  return true;
}

// Return the comparison kind for the specified opcode.
SNITCHFrepLoops::Comparison::Kind
SNITCHFrepLoops::getComparisonKind(unsigned CondOpc,
                                        MachineOperand *InitialValue,
                                        const MachineOperand *EndValue,
                                        int64_t IVBump) const {

  Comparison::Kind Cmp = (Comparison::Kind)0;
  switch (CondOpc) {
  case RISCV::BEQ:
    Cmp = Comparison::EQ;
    break;
  case RISCV::BNE:
    Cmp = Comparison::NE;
    break;
  case RISCV::BLT:
    Cmp = Comparison::LTs;
    break;
  case RISCV::BLTU:
    Cmp = Comparison::LTu;
    break;
  case RISCV::BGE:
    Cmp = Comparison::GEs;
    break;
  case RISCV::BGEU:
    Cmp = Comparison::GEs;
    break;
  default:
    return (Comparison::Kind)0;
  }
  return Cmp;
}


/// Helper function that returns the expression that represents the
/// number of times a loop iterates.  The function takes the operands that
/// represent the loop start value, loop end value, and induction value.
/// Based upon these operands, the function attempts to compute the trip count.
CountValue *SNITCHFrepLoops::computeCount(MachineLoop *Loop,
                                               const MachineOperand *Start,
                                               const MachineOperand *End,
                                               unsigned IVReg,
                                               int64_t IVBump,
                                               Comparison::Kind Cmp) const {
    
  // Get the preheader
  MachineBasicBlock *PH = MLI->findLoopPreheader(Loop, SpecPreheader);
  assert (PH && "Should have a preheader by now");
  MachineBasicBlock::iterator InsertPos = PH->getFirstTerminator();
  DebugLoc DL;
  if (InsertPos != PH->end())
    DL = InsertPos->getDebugLoc();
  const TargetRegisterClass *IntRC = &RISCV::GPRRegClass;

  bool startIsImm = false, endIsImm = false;
  int64_t immStart, immEnd;
  startIsImm = checkForImmediate(*Start, immStart);
  endIsImm = checkForImmediate(*End, immEnd);
  if (endIsImm && !End->isImm()) {
    if(immEnd == 0) {
      unsigned VGPR = MRI->createVirtualRegister(IntRC);
      MachineInstrBuilder ZeroInit =
          BuildMI(*PH, InsertPos, DL, TII->get(TargetOpcode::COPY), VGPR);
      ZeroInit.addReg(RISCV::X0);
      End = &ZeroInit->getOperand(0);
    }
  }

  bool CmpLess =     Cmp & Comparison::L;
  bool CmpGreater =  Cmp & Comparison::G;
  bool CmpHasEqual = Cmp & Comparison::EQ;
  
  // Sanity check
  if (!Start->isReg() && !startIsImm) {
    return nullptr;
  }
  if (!End->isReg() && !endIsImm) {
    return nullptr;
  }
  
  // Avoid certain wrap-arounds.  This doesn't detect all wrap-arounds.
  if (CmpLess && IVBump < 0) {
    // Loop going while iv is "less" with the iv value going down.  Must wrap.
    return nullptr;
  }

  if (CmpGreater && IVBump > 0) {
    // Loop going while iv is "greater" with the iv value going up.  Must wrap.
    return nullptr;
  }

  if (IVBump == 0) {
    return nullptr;
  }

  if (startIsImm && endIsImm) {
    // Both, start and end are immediates.
    int64_t Dist = std::max(immStart, immEnd) - std::min(immStart, immEnd);
    bool Exact = (Dist % IVBump) == 0;

    if (Dist == 0) 
      return nullptr;
    if (!Exact)
      return nullptr;
    
    // Normalize distance to step
    uint64_t Count = Dist / std::abs(IVBump);

    if (Count > 0xFFFFFFFFULL)
      return nullptr;

    if(CmpHasEqual) {
      Count++;
    }
    
    LLVM_DEBUG(dbgs() << "Found trip count = "<<Count<<"\n");
    return new CountValue(CountValue::CV_Immediate, Count);
  }
  return nullptr;
}

/// Returns true if the instruction is dead.  This was essentially
/// copied from DeadMachineInstructionElim::isDead, but with special cases
/// for inline asm, physical registers and instructions with side effects
/// removed.
bool SNITCHFrepLoops::isDead(const MachineInstr *MI,
                              SmallVectorImpl<MachineInstr *> &DeadPhis) const {
  // Examine each operand.
  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    if (!MO.isReg() || !MO.isDef())
      continue;

    unsigned Reg = MO.getReg();
    if (MRI->use_nodbg_empty(Reg))
      continue;
    using use_nodbg_iterator = MachineRegisterInfo::use_nodbg_iterator;

    // This instruction has users, but if the only user is the phi node for the
    // parent block, and the only use of that phi node is this instruction, then
    // this instruction is dead: both it (and the phi node) can be removed.
    use_nodbg_iterator I = MRI->use_nodbg_begin(Reg);
    use_nodbg_iterator End = MRI->use_nodbg_end();
    if (std::next(I) != End) {
      // uses without debug: more than two uses
      LLVM_DEBUG(dbgs() << "  not dead because >2 uses\n");
      return false;
    }
    if(!I->getParent()->isPHI()) {
      LLVM_DEBUG(dbgs() << "  not dead because use is not PHI\n");
      return false;
    }

    MachineInstr *UsePhi = I->getParent();
    // iterate over all operands of the PHI that uses this definition
    for (unsigned j = 0, f = UsePhi->getNumOperands(); j != f; ++j) {
      const MachineOperand &OPO = UsePhi->getOperand(j);
      // don't check if it is not a register or not a definition
      if (!OPO.isReg() || !OPO.isDef())
        continue;

      // check the uses of the reg defined by the PHI
      unsigned OPReg = OPO.getReg();
      use_nodbg_iterator nextJ;
      // iterate over all uses of the defined reg
      for (use_nodbg_iterator J = MRI->use_nodbg_begin(OPReg);
           J != End; J = nextJ) {
        nextJ = std::next(J);
        MachineOperand &Use = *J;
        MachineInstr *UseMI = Use.getParent();

        // If the phi node has a user that is not MI, bail.
        LLVM_DEBUG(dbgs()<<"  checking use:\n"; UseMI->dump());
        if (MI != UseMI) {
          LLVM_DEBUG(dbgs() << "  not dead because the phi node has a user that is not MI\n");
          return false;
        }
      }
    }
    DeadPhis.push_back(UsePhi);
  }

  // If there are no defs with uses, the instruction is dead.
  return true;
}

void SNITCHFrepLoops::removeIfDead(MachineInstr *MI) {
  // This procedure was essentially copied from DeadMachineInstructionElim.

  SmallVector<MachineInstr*, 1> DeadPhis;
  if (isDead(MI, DeadPhis)) {
    LLVM_DEBUG(dbgs() << "HW looping will remove: " << *MI);

    // It is possible that some DBG_VALUE instructions refer to this
    // instruction.  Examine each def operand for such references;
    // if found, mark the DBG_VALUE as undef (but don't delete it).
    for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
      const MachineOperand &MO = MI->getOperand(i);
      if (!MO.isReg() || !MO.isDef())
        continue;
      unsigned Reg = MO.getReg();
      MachineRegisterInfo::use_iterator nextI;
      for (MachineRegisterInfo::use_iterator I = MRI->use_begin(Reg),
           E = MRI->use_end(); I != E; I = nextI) {
        nextI = std::next(I);  // I is invalidated by the setReg
        MachineOperand &Use = *I;
        MachineInstr *UseMI = I->getParent();
        if (UseMI == MI)
          continue;
        if (Use.isDebug())
          UseMI->getOperand(0).setReg(0U);
      }
    }

    MI->eraseFromParent();
    for (unsigned i = 0; i < DeadPhis.size(); ++i) {
      DeadPhis[i]->eraseFromParent();
    }
  }
}

void SNITCHFrepLoops::insertFPUBarrier(MachineLoop *L, MachineBasicBlock *MBB) {
  MachineBasicBlock::iterator InsertPos = MBB->getFirstNonPHI();
  LLVM_DEBUG(dbgs() << "inserting FPU barrier at "; InsertPos->dump());
  DebugLoc DL;
  DL = InsertPos->getDebugLoc();
  MachineFunction *MF = MBB->getParent();
  
  auto DoneMBB = MF->CreateMachineBasicBlock(MBB->getBasicBlock());

  // %[tmp] = fmv.x.w fa0
  unsigned ScratchReg = MRI->createVirtualRegister(&RISCV::GPRRegClass);
  unsigned ScratchFPReg = MRI->createVirtualRegister(&RISCV::FPR64RegClass);
  BuildMI(*MBB, InsertPos, DL, TII->get(RISCV::FMV_X_W), ScratchReg)
    .addReg(ScratchFPReg, RegState::Define);
  // blt %[tmp], %[tmp], 1f
  BuildMI(*MBB, InsertPos, DL, TII->get(RISCV::BLT))
    .addReg(ScratchReg, 0).addReg(ScratchReg, RegState::Kill)
    .addMBB(DoneMBB);
  
  // Insert new MBBs.
  MF->insert(++MBB->getIterator(), DoneMBB);
  DoneMBB->splice(DoneMBB->end(), MBB, InsertPos, MBB->end());
  DoneMBB->transferSuccessorsAndUpdatePHIs(MBB);
  if(!MBB->isSuccessor(DoneMBB)) MBB->addSuccessor(DoneMBB);

  LivePhysRegs LiveRegs;
  computeAndAddLiveIns(LiveRegs, *DoneMBB);
}

bool SNITCHFrepLoops::isMarkedForInference(MachineLoop *L) {

  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  if (!Preheader) {
    return false;
  }

  // search preheader for any "frep" meta
  for (MachineInstr &MI : *Preheader) {
    if( MI.getOpcode() == RISCV::PseudoFrepInfer) {
      // found it!
      LLVM_DEBUG(dbgs()<<"found frep infer pseudo\n");
      MI.removeFromParent();
      return true;
    }
  }

  LLVM_DEBUG(dbgs()<<"  inference disabled\n");
  return false;
}

} // end of anonymous namespace

INITIALIZE_PASS_BEGIN(SNITCHFrepLoops, DEBUG_TYPE,
                      SNITCH_FREP_LOOPS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(SNITCHFrepLoops, DEBUG_TYPE,
                    SNITCH_FREP_LOOPS_NAME, false, false)

namespace llvm {
  FunctionPass *createSNITCHFrepLoopsPass() { return new SNITCHFrepLoops(); }
} // end of namespace llvm

