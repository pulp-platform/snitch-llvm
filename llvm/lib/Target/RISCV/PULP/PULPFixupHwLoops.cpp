//===---- PULPFixupHwLoops.cpp - Fixup HW loops too far from LOOPn. ----===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// FIXME: Add description
//===----------------------------------------------------------------------===//

#include "../RISCVTargetMachine.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Pass.h"

#include <queue>
#include <set>

using namespace llvm;

static cl::opt<signed> MaxLoopRangeImm(
    "pulp-loop-range-immediate", cl::Hidden, cl::init(50),
    cl::desc("Restrict range of lp.setupi to N instructions."));

static cl::opt<signed> MaxLoopRangeReg(
    "pulp-loop-range-register", cl::Hidden, cl::init(8190),
    cl::desc("Restrict range of lp.setup to N instructions."));

namespace llvm {
  FunctionPass *createPULPFixupHwLoops();
  void initializePULPFixupHwLoopsPass(PassRegistry&);
}

namespace {

// TODO: This function is not needed for the latest LLVM versions, as MBB has
//       had the splitAt() method added. This version is adapted from that
//       method, from https://github.com/llvm/llvm-project/blob/
//       d7c219a506ec9aabe7c5d36c0da55656af487b73/llvm/lib/CodeGen/
//       MachineBasicBlock.cpp
//       CAVEAT: The above function will not split, as the loop instruction is
//               a terminator, in which case the above linked version would
//               chicken out.
MachineBasicBlock *splitMBBAt(MachineBasicBlock *OldMBB, MachineInstr &MI) {
  MachineBasicBlock::iterator SplitPoint(&MI);

  MachineFunction *MF = OldMBB->getParent();

  LivePhysRegs LiveRegs;
  // Make sure we add any physregs we define in the block as liveins to the
  // new block.
  MachineBasicBlock::iterator Prev(&MI);
  LiveRegs.init(*MF->getSubtarget().getRegisterInfo());
  LiveRegs.addLiveOuts(*OldMBB);
  for (auto I = OldMBB->rbegin(), E = Prev.getReverse(); I != E; ++I)
    LiveRegs.stepBackward(*I);

  MachineBasicBlock *SplitBB =
      MF->CreateMachineBasicBlock(OldMBB->getBasicBlock());

  MF->insert(++MachineFunction::iterator(OldMBB), SplitBB);
  SplitBB->splice(SplitBB->begin(), OldMBB, SplitPoint, OldMBB->end());

  SplitBB->transferSuccessorsAndUpdatePHIs(OldMBB);
  OldMBB->addSuccessor(SplitBB);

  addLiveIns(*SplitBB, LiveRegs);

  return SplitBB;
}


  struct PULPFixupHwLoops : public MachineFunctionPass {
  public:
    static char ID;

    PULPFixupHwLoops() : MachineFunctionPass(ID) {
      initializePULPFixupHwLoopsPass(*PassRegistry::getPassRegistry());
    }

    bool runOnMachineFunction(MachineFunction &MF) override;

    MachineFunctionProperties getRequiredProperties() const override {
      return MachineFunctionProperties().set(
          MachineFunctionProperties::Property::NoVRegs);
    }

    StringRef getPassName() const override {
      return "PULP Hardware Loop Fixup";
    }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesCFG();
      AU.addRequired<MachineLoopInfo>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }

  private:
    bool fixupLoopPreheader(MachineFunction &MF);
    bool fixupLoopLatch(MachineFunction &MF);
    bool fixupLoopInstrs(MachineFunction &MF);

  };

  char PULPFixupHwLoops::ID = 0;
}

INITIALIZE_PASS(PULPFixupHwLoops, "hwloopsfixup",
                "PULP Hardware Loops Fixup", false, false)

FunctionPass *llvm::createPULPFixupHwLoops() {
  return new PULPFixupHwLoops();
}

/// Returns true if the instruction is a hardware loop instruction.
static bool isHardwareLoop(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LOOP0setup ||
         MI.getOpcode() == RISCV::LOOP1setup ||
         MI.getOpcode() == RISCV::LOOP0setupi ||
         MI.getOpcode() == RISCV::LOOP1setupi;
}

static bool isHardwareLoopZero(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LOOP0setup ||
         MI.getOpcode() == RISCV::LOOP0setupi;
}

static bool isHardwareLoopOne(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LOOP1setup ||
         MI.getOpcode() == RISCV::LOOP1setupi;
}

static bool isHardwareLoopReg(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LOOP0setup ||
         MI.getOpcode() == RISCV::LOOP1setup;
}

static bool isHardwareLoopImm(const MachineInstr &MI) {
  return MI.getOpcode() == RISCV::LOOP0setupi ||
         MI.getOpcode() == RISCV::LOOP1setupi;
}

bool PULPFixupHwLoops::runOnMachineFunction(MachineFunction &MF) {


  // FIXME: Cannot add xpulpv2 to march. Problem in Koen's implementation?
  // We can only use hardware loops if we have the PULPv2 extension enabled.
  //if (!MF.getSubtarget<RISCVSubtarget>().hasPULPExtV2()) {
  //  return false;
  //}

  if (skipFunction(MF.getFunction())) {
    return false;
  }
  bool fixedPreHd = fixupLoopPreheader(MF);
  bool fixedLatch = fixupLoopLatch(MF);
  bool fixedInstr = fixupLoopInstrs(MF);

  // TODO Move to own function
  for (MachineBasicBlock &MBB : MF) {
    unsigned instrs = 0;
    for (MachineInstr &MI : MBB) {
      instrs++;
      // If this is a loop instruction that has not been moved to a dedicated
      // block (for alignment of that block), then split and align it.
      if (isHardwareLoop(MI) && instrs > 1) {
        MachineBasicBlock *New = splitMBBAt(&MBB, MI);
        if (New == &MBB) {
          // Nothing changed
          continue;
        }
        New->setAlignment(Align(4));
        break;
      }
    }
  }

  return fixedPreHd || fixedLatch || fixedInstr;
}

// Get the size of an instruction. This function currently only works if
// instruction compression (standard extension C is disabled).
size_t getMISize(const MachineInstr &MI) {
  // Handle fast-path if we are not going to compress instructions.
  const MachineFunction * MF = MI.getMF();
  const RISCVInstrInfo *RII =
      static_cast<const RISCVInstrInfo *>(MF->getSubtarget().getInstrInfo());
  return RII->getInstSizeInBytes(MI);
}

bool PULPFixupHwLoops::fixupLoopPreheader(MachineFunction &MF) {
  bool changed = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (isHardwareLoop(MI)) {
        MachineInstr &Term = *(--MBB.end());
        if (Term.getDesc().isUnconditionalBranch()) {
          assert(MBB.succ_size() == 1 && "Too many successors!");
          MachineBasicBlock *LoopStartMBB = *MBB.succ_begin();
          if (MBB.isLayoutSuccessor(LoopStartMBB)) {
            Term.eraseFromParent();
            changed = true;
            break;
          }
        }
      }
    }
  }
  return changed;
}

// Returns true if it is safe to remove uses of bumpReg. This is the case if it
// is not used between setup and the end of lastBlock AND the first use of the
// register after lastBlock is a write-only (no dependency on old value). The
// instructions that operate on the bump register throughout the loop are
// inserted into regUsers.
bool fixupBump(MachineBasicBlock *lastBlock, MachineInstr *setup,
               unsigned bumpReg, std::set<MachineInstr *> &regUsers) {

  std::set<MachineBasicBlock *> visited;
  std::set<MachineInstr *> tmpRegUsers;

  // Track backwards to find all instruction uses. Goal is to ensure bumped
  // register is not used within loop.
  std::queue<MachineBasicBlock *> backtrackQ;
  backtrackQ.push(lastBlock);
  while (!backtrackQ.empty()) {
    // Get next from queue.
    MachineBasicBlock *MBB = backtrackQ.front();
    backtrackQ.pop();
    if (visited.count(MBB) > 0) {
      continue;
    }
    visited.insert(MBB);

    // Loop backwards through block, recording any users of the bump register.
    // If we reach the top of the loop (setup), we will stop our search.
    bool foundSetup = false;
    for (auto I = MBB->instr_rbegin(), E = MBB->instr_rend(); I != E; I++) {
      MachineInstr *MII = &(*I);
      if (MII == setup) {
        foundSetup = true;
        break;
      }
      for (unsigned i = 0; i < MII->getNumOperands(); i++) {
        MachineOperand &MOp = MII->getOperand(i);
        if (MOp.isReg()) {
          if (MOp.getReg() == bumpReg) {
            tmpRegUsers.insert(MII);
          }
        }
      }
    }

    // Enqueue its predecessors.
    if (!foundSetup) {
      for (MachineBasicBlock *Pred : MBB->predecessors()) {
        backtrackQ.push(Pred);
      }
    }
  }

  // Track forwards, bfs, to check that the first use of the instruction is
  // always a write. Backloops are prevented, as the loop blocks are already in
  // the visited set.
  std::queue<MachineBasicBlock *> fwdQ;
  for (MachineBasicBlock *Succ : lastBlock->successors()) {
    fwdQ.push(Succ);
  }
  visited.insert(lastBlock);
  bool alwaysWrite = true;
  while (!fwdQ.empty()) {
    MachineBasicBlock *MBB = fwdQ.front();
    fwdQ.pop();
    if (visited.count(MBB) > 0) {
      continue;
    }
    visited.insert(MBB);

    // Check for uses
    bool found = false;
    for (MachineInstr &MI : *MBB) {
      // If this is a read of the register, and we have not yet encountered a
      // write, then it is not safe to remove this register.
      alwaysWrite &= !MI.readsRegister(bumpReg);
      if (MI.modifiesRegister(bumpReg, nullptr)) {
        // We do not need to continue searching, the old value is killed here.
        found = true;
        break;
      }
    }

    // If we didn't find the first use, continue to next block(s). Quit early
    // if we already know that we don't always write.
    if (!found && alwaysWrite) {
      for (MachineBasicBlock *Succ : MBB->successors()) {
        fwdQ.push(Succ);
      }
    }
  }

  // If we don't always see an overwrite of the register value before the next
  // read after the hardware loop, then it is not safe to remove the bump
  // instruction: Return false.
  // TODO: If we can calculate the end value and assign it to the register after
  //       the loop, then we can just insert that load-immediate and continue.
  if (!alwaysWrite) {
    return false;
  }

  // Get all the users of the bump registers to the referenced set, and then
  // return true.
  regUsers.insert(tmpRegUsers.begin(), tmpRegUsers.end());
  return true;

}

bool PULPFixupHwLoops::fixupLoopLatch(MachineFunction &MF) {

  bool changedNow = false;
  bool changedOverall = false;
  std::set<MachineInstr *> toRemove;

  // Look at each loop instruction in turn, removing the branching instructions
  // at the end of them. The outer do...while loop and the breaks ensure that we
  // are not modifying the instruction stream while iterating over it in the
  // inner for loops: If we have changed the data structure, restart from
  // scratch.
  // TODO We might be able to optimize this a bit, but it shouldn't affect the
  //      total compilation time too much.
  do {
    changedNow = false;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        if (isHardwareLoop(MI)) {
          // Get the ExitingBlock, and from there the ExitBlock
          MachineLoopInfo *MLI = &getAnalysis<MachineLoopInfo>();
          MachineBasicBlock *LastMBB = MI.getOperand(0).getMBB();
          MachineLoop *L = MLI->getLoopFor(LastMBB);
          assert(L->contains(LastMBB) && "Loop does not contain LastMBB");
          if (L->getBottomBlock() != LastMBB) {
            // Update the end of the loop from previous transformations. If the
            // new bottom is reachable from the previous LastMBB, and all blocks
            // are in layout order and are still in the loop, then update.
            MachineBasicBlock *current = LastMBB;
            bool reachedEnd = false;
            while (current != L->getBottomBlock() && !reachedEnd) {
              bool found = false;
              for (MachineBasicBlock &succ : MF) {
                if (current->isLayoutSuccessor(&succ) &&
                    L->getBlocksSet().find(&succ) != L->getBlocksSet().end()) {
                  current = &succ;
                  found = true;
                  break;
                }
              }
              if (!found) {
                reachedEnd = true;
              }
            }
            if (current == L->getBottomBlock()) {
              LastMBB = L->getBottomBlock();
              MachineOperand countMO = MI.getOperand(1);
              MI.RemoveOperand(1);
              MI.RemoveOperand(0);
              MI.addOperand(MachineOperand::CreateMBB(LastMBB));
              MI.addOperand(countMO);
            }
          }
          assert(L->getBottomBlock() == LastMBB && "Last is not Bottom");
          MachineBasicBlock *ExitBlock = L->getExitBlock();
          MachineBasicBlock::iterator LastI = LastMBB->getFirstTerminator();
          DebugLoc LastIDL = LastI->getDebugLoc();
          if (LastI != LastMBB->end()) {
            if (LastI->getOpcode() == RISCV::BEQ ||
                LastI->getOpcode() == RISCV::BNE ||
                LastI->getOpcode() == RISCV::BLT ||
                LastI->getOpcode() == RISCV::BGE ||
                LastI->getOpcode() == RISCV::BLTU ||
                LastI->getOpcode() == RISCV::BGEU ||
                LastI->getOpcode() == RISCV::P_BNEIMM ||
                LastI->getOpcode() == RISCV::P_BEQIMM) {
              // Check if it is safe to remove the bump instruction, in which
              // case the instructions to remove are placed in regUsers.
              for (unsigned i = 0; i < LastI->getNumOperands(); i++) {
                MachineOperand &MO = LastI->getOperand(i);
                if (MO.isReg()) {
                  std::set<MachineInstr *> regUsers;
                  if (fixupBump(LastMBB, &MI, MO.getReg(), regUsers)) {
                    // Remove the LastI from regUsers, since this instruction is
                    // handled separately.
                    if (regUsers.count(&(*LastI)) > 0) {
                      regUsers.erase(&(*LastI));
                    }
                    // If only the bump instruction remains, then it is safe to
                    // remove it. Caveat: With the current implementation we
                    // need to make sure that we are not violating the minimum
                    // length for HW loops.
                    if (regUsers.size() == 1) {
                      MachineInstr *Cand = (*regUsers.begin());
                      if (Cand->getOpcode() == RISCV::ADD ||
                          Cand->getOpcode() == RISCV::ADDI ||
                          Cand->getOpcode() == RISCV::SUB) {
                        if (Cand->getOperand(0).isReg() &&
                            Cand->getOperand(1).isReg() &&
                            Cand->getOperand(0).getReg()
                            == Cand->getOperand(1).getReg()) {
                          if (Cand->getParent()->size() <= 2) {
                            // If this is part of the two last (potentially)
                            // mandatory (hw loops require length of at least 2)
                            // instructions in the block, it would require much
                            // more work to remove it. Either because this is
                            // the instruction that marks the end of the HW
                            // loop, or because the length would change such
                            // that we need to insert more nops.
                            // FIXME: The bump fixup should be done at an
                            //        earlier stage such that we do not have to
                            //        worry about it during this phase of the
                            //        fixup.
                          } else {
                            toRemove.insert(Cand);
                          }
                        }
                      }
                    }
                  }
                }
              }
              // Delete one and change/add an uncond. branch to out of the loop.
              MachineBasicBlock *BranchTarget = LastI->getOperand(2).getMBB();
              LastI = LastMBB->erase(LastI);
              if (BranchTarget == ExitBlock) {
                // This is the branch we want to keep.
                if (LastI != LastMBB->end()) {
                  LastI = LastMBB->erase(LastI);
                }
                SmallVector<MachineOperand, 0> Cond;
                const RISCVSubtarget &HST = MF.getSubtarget<RISCVSubtarget>();
                const RISCVInstrInfo *TII = HST.getInstrInfo();
                TII->insertBranch(*LastMBB, BranchTarget, nullptr, Cond,
                                  LastIDL);
                changedNow = true;
                changedOverall = true;
              }
            } else if (LastI->getOpcode() == RISCV::PseudoBR) {
              MachineBasicBlock *BranchTarget = LastI->getOperand(0).getMBB();
              if (BranchTarget == L->getHeader())  {
                // Unconditional branch to loop start; just delete it.
                LastMBB->erase(LastI);
                changedNow = true;
                changedOverall = true;
              }
            } else {
              llvm_unreachable("Unknown branch type!");
            }
          }
        }
        if (changedNow) {
          break;
        }
      }
      if (changedNow) {
        break;
      }
    }

  } while (changedNow == true);

  for (MachineInstr *MI : toRemove) {
    MI->eraseFromParent();
  }

  return changedOverall;
}

/// This function makes three passes over the basic blocks.  The first
/// pass labels all loop ends. The second calculates the offset of the
/// instructions between blocks. The third checks the length of the hardware
/// loops, and pads them with NOPs if they are too short, or uses the explicit
/// setup instructions if they are too long.
bool PULPFixupHwLoops::fixupLoopInstrs(MachineFunction &MF) {

  // Offset of the current instruction from the start.
  unsigned InstOffset = 0;
  // Map for each basic block to it's first instruction.
  DenseMap<const MachineBasicBlock *, unsigned> BlockToInstOffset;

  const RISCVInstrInfo *RII =
      static_cast<const RISCVInstrInfo *>(MF.getSubtarget().getInstrInfo());

  // First pass: Label the end of each loop.
  bool Changed = false;
  for (MachineBasicBlock &MBB : MF) {
    // Loop over all the instructions.
    MachineBasicBlock::iterator MII = MBB.begin();
    MachineBasicBlock::iterator MIE = MBB.end();
    while (MII != MIE) {
      if (MII->isMetaInstruction()) {
        ++MII;
        continue;
      }
      if (isHardwareLoop(*MII)) {
        assert(MII->getOperand(0).isMBB() &&
               "Expect a basic block as loop operand");
        // Figure out which MBB that is the last one in the loop.
        MachineBasicBlock *LastMBB = MII->getOperand(0).getMBB();

        // Create a new basic block in the loop, and insert it after the
        // current last. It will be used to label the last instruction in the
        // loop.
        MachineBasicBlock::iterator instrToMove = --LastMBB->getFirstTerminator();

        // Inline ASM can contain multiple instructions, causing the loop to end
        // too early. Work around this by inserting a NOP at the end.
        if (instrToMove->isInlineAsm()) {
          DebugLoc DL = instrToMove->getDebugLoc();
          BuildMI(*LastMBB, LastMBB->getFirstTerminator(), DL,
                  RII->get(RISCV::ADDI)).addReg(RISCV::X0).addReg(RISCV::X0)
                                        .addImm(0);
          instrToMove = --LastMBB->getFirstTerminator();
        }

        MachineBasicBlock::iterator firstPossibleMove = LastMBB->getFirstNonPHI();
        MachineFunction *MF = LastMBB->getParent();
        auto LoopEnd = MF->CreateMachineBasicBlock();
        MF->insert(++LastMBB->getIterator(), LoopEnd);
        // Adopt the control flow.
        LoopEnd->transferSuccessors(LastMBB);
        LastMBB->addSuccessor(LoopEnd);
        // Put the last instruction of the loop in this new block, whose label
        // we will use to calculate the length of the hardware loop.
        while ((instrToMove->isTransient() ||
              instrToMove->isBranch()) && instrToMove != firstPossibleMove) {
          instrToMove--;
        }
        LoopEnd->splice(LoopEnd->begin(), LastMBB, instrToMove, LastMBB->end());
        LoopEnd->setLabelMustBeEmitted();

        // Update the loop setup instruction with the actual loop end.
        DebugLoc DL = MII->getDebugLoc();
        MachineInstrBuilder MIB = BuildMI(*MII->getParent(), MII,
            DL, RII->get(MII->getOpcode()));
        MIB.addMBB(LoopEnd);
        MIB.add(MII->getOperand(1));
        // Remove old
        MII = MII->getParent()->erase(MII);
      } else {
        ++MII;
      }
    }
  }

  // Second pass: Compute offset from start
  for (const MachineBasicBlock &MBB : MF) {
    BlockToInstOffset[&MBB] = InstOffset;
    for (const MachineInstr &MI : MBB) {
      InstOffset += getMISize(MI);
    }
  }
  
  // Third pass: Pad with nops if short, switch to separate instructions if too
  // long.
  for (MachineBasicBlock &MBB : MF) {
    InstOffset = BlockToInstOffset[&MBB];

    // Loop over all the instructions.
    MachineBasicBlock::iterator MII = MBB.begin();
    MachineBasicBlock::iterator MIE = MBB.end();
    while (MII != MIE) {
      unsigned InstSize = getMISize(*MII);
      if (MII->isMetaInstruction()) {
        ++MII;
        continue;
      }
      if (isHardwareLoop(*MII)) {
        bool offsetIncrease = 0;
        assert(MII->getOperand(0).isMBB() &&
               "Expect a basic block as loop operand");
        MachineBasicBlock *TargetBB = MII->getOperand(0).getMBB();

        unsigned Diff = AbsoluteDifference(InstOffset,
                                           BlockToInstOffset[TargetBB]);
        signed LoopLen = Diff - getMISize(*MII);
        // Adjust for too short or too long loops.
        MachineBasicBlock::iterator inspt = MII;
        DebugLoc DL = inspt->getDebugLoc();
        inspt++;

        // Handle loop lengths (address range).
        if (isHardwareLoopImm(*MII) && LoopLen > MaxLoopRangeImm) {
          // If the loop spans a larger address range than what can be supported
          // by lp.setupi (five bits for relative end address), we have to
          // switch to the separate lp.starti, lp.endi, and lp.counti
          // instructions.
          assert(MBB.succ_size() == 1 && "Too many successors!");
          assert(isHardwareLoopZero(*MII) || isHardwareLoopOne(*MII));
          MachineBasicBlock *LoopStartMBB = *MBB.succ_begin();
          MachineInstrBuilder Start;
          MachineInstrBuilder End;
          MachineInstrBuilder Count;
          // Expand into long loop setup instructions, depending on if it is
          // loop 0 or loop 1.
          if (isHardwareLoopZero(*MII)) {
            Start = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP0starti))
                .addMBB(LoopStartMBB);
            End = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP0endi))
                .addMBB(TargetBB);
            Count = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP0counti))
                .addImm(MII->getOperand(1).getImm());
          } else  if (isHardwareLoopOne(*MII)) {
            Start = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP1starti))
                .addMBB(LoopStartMBB);
            End = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP1endi))
                .addMBB(TargetBB);
            Count = BuildMI(MBB, inspt, DL, RII->get(RISCV::LOOP1counti))
                .addImm(MII->getOperand(1).getImm());
          }
          // Compute new loop length
          LoopLen += getMISize(*Start.getInstr());
          offsetIncrease += getMISize(*Start.getInstr());
          LoopLen += getMISize(*End.getInstr());
          offsetIncrease += getMISize(*Start.getInstr());
          LoopLen += getMISize(*Count.getInstr());
          offsetIncrease += getMISize(*Start.getInstr());
          LoopLen -= getMISize(*MII);
          offsetIncrease -= getMISize(*MII);
          // Take address for start block.
          LoopStartMBB->setHasAddressTaken();
          LoopStartMBB->setLabelMustBeEmitted();
          // This line is needed to set the hasAddressTaken flag on the
          // BasicBlock object.
          BlockAddress::get(
              const_cast<BasicBlock *>(LoopStartMBB->getBasicBlock()));
          // Remove old instruction.
          MII->eraseFromParent();
        }
        if (isHardwareLoopReg(*MII) && LoopLen > MaxLoopRangeReg) {
          // FIXME: If the loop spans a larger address range than what can fit
          //        in lp.setup instruction format (twelve bits), I am not sure
          //        what to do. We have no way of storing more information in
          //        any of the "long" instructions. We'd have to roll back,
          //        probably?
          errs().changeColor(raw_fd_ostream::Colors::RED, true);
          errs() << "UNHANDLED: Hardware Loop length " << LoopLen << " is "
                 << "higher than limit for lp.setup: " << MaxLoopRangeReg
                 << "\n";
          errs().resetColor();
          abort();
        }
        if (LoopLen < 4) {
          // Loop length is too small (must be at least two instructions): Fill
          // up with nops.
          while (LoopLen < 4) {
            MachineInstrBuilder NOP = BuildMI(MBB, inspt, DL,
                                              RII->get(RISCV::ADDI));
            NOP.addReg(RISCV::X0);
            NOP.addReg(RISCV::X0);
            NOP.addImm(0);
            LoopLen += getMISize(*NOP.getInstr());
            offsetIncrease += getMISize(*NOP.getInstr());
          }
        }

        // If the changes to the loop instructions caused the length of the
        // basic block to increase, update the data structure to match.
        MII = inspt;
        if (offsetIncrease > 0) {
          bool hasSeenOurselves = false;
          for (const MachineBasicBlock &otherBlock : MF) {
            // Increase the offset for all blocks after the current one.
            // Currently we just go through the basic blocks in order, and once
            // we have found the current basic block (MBB), we apply the change
            // to all blocks following.
            if (&MBB == &otherBlock) {
              hasSeenOurselves = true;
              continue;
            } else if (!hasSeenOurselves) {
              continue;
            }
            BlockToInstOffset[&otherBlock] += offsetIncrease;
          }
        }
      } else {
        ++MII;
      }
      InstOffset += InstSize;
    }
  }

  return Changed;
}
