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

using namespace llvm;

static cl::opt<signed> MaxLoopRangeImm(
    "pulp-loop-range-immediate", cl::Hidden, cl::init(62),
    cl::desc("Restrict range of lp.setupi to N instructions."));

static cl::opt<signed> MaxLoopRangeReg(
    "pulp-loop-range-register", cl::Hidden, cl::init(8190),
    cl::desc("Restrict range of lp.setup to N instructions."));

namespace llvm {
  FunctionPass *createPULPFixupHwLoops();
  void initializePULPFixupHwLoopsPass(PassRegistry&);
}

namespace {
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

bool PULPFixupHwLoops::fixupLoopLatch(MachineFunction &MF) {

  bool changedNow = false;
  bool changedOverall = false;

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
                LastI->getOpcode() == RISCV::BGEU) {
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
