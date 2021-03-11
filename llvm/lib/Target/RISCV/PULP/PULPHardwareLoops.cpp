//====-- PULPHardwareLoops.cpp - Identify and generate hardware loops ----====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass identifies loops where we can generate the PULP hardware
// loop instruction.  The hardware loop can perform loop branches with a
// zero-cycle overhead.
//
//  This file is based on the lib/Target/Hexagon/HexagonHardwareLoops.cpp file.
//
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

#define DEBUG_TYPE "pulp-hwloops"

// Turn it off by default. If a preheader block is not created here, the
// software pipeliner may be unable to find a block suitable to serve as
// a preheader. In that case SWP will not run.
static cl::opt<bool> SpecPreheader("pulp-hwloop-spec-preheader", cl::init(false),
  cl::Hidden, cl::ZeroOrMore, cl::desc("Allow speculation of preheader "
  "instructions"));

STATISTIC(NumHWLoops, "Number of loops converted to hardware loops");

namespace llvm {

  FunctionPass *createPULPHardwareLoops();
  void initializePULPHardwareLoopsPass(PassRegistry&);

} // end namespace llvm

namespace {

  class CountValue;

  struct PULPHardwareLoops : public MachineFunctionPass {
    MachineLoopInfo            *MLI;
    MachineRegisterInfo        *MRI;
    MachineDominatorTree       *MDT;
    const RISCVInstrInfo     *TII;
    const RISCVRegisterInfo  *TRI;

    unsigned NumHWLoopsInternal = 0;

  public:
    static char ID;

    PULPHardwareLoops() : MachineFunctionPass(ID) {}

    bool runOnMachineFunction(MachineFunction &MF) override;

    StringRef getPassName() const override { return "PULP Hardware Loops"; }

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.addRequired<MachineDominatorTree>();
      AU.addRequired<MachineLoopInfo>();
      MachineFunctionPass::getAnalysisUsage(AU);
    }

  private:
    using LoopFeederMap = std::map<unsigned, MachineInstr *>;
    std::set<const MachineInstr *> KnownHardwareLoops;

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

    /// Find the register that contains the loop controlling
    /// induction variable.
    /// If successful, it will return true and set the \p Reg, \p IVBump
    /// and \p IVOp arguments.  Otherwise it will return false.
    /// The returned induction register is the register R that follows the
    /// following induction pattern:
    /// loop:
    ///   R = phi ..., [ R.next, LatchBlock ]
    ///   R.next = R + #bump
    ///   if (R.next < #N) goto loop
    /// IVBump is the immediate value added to R, and IVOp is the instruction
    /// "R.next = R + #bump".
    bool findInductionRegister(MachineLoop *L, unsigned &Reg,
                               int64_t &IVBump, MachineInstr *&IVOp) const;

    /// Return the comparison kind for the specified opcode.
    Comparison::Kind getComparisonKind(unsigned CondOpc,
                                       MachineOperand *InitialValue,
                                       const MachineOperand *Endvalue,
                                       int64_t IVBump) const;

    /// Analyze the statements in a loop to determine if the loop
    /// has a computable trip count and, if so, return a value that represents
    /// the trip count expression.
    CountValue *getLoopTripCount(MachineLoop *L,
                                 SmallVectorImpl<MachineInstr *> &OldInsts);

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

    /// Return true if the instruction is not valid within a hardware
    /// loop.
    bool isInvalidLoopOperation(const MachineInstr *MI) const;

    /// Return true if the loop contains an instruction that inhibits
    /// using the hardware loop.
    bool containsInvalidInstruction(MachineLoop *L) const;

    /// Given a loop, check if we can convert it to a hardware loop.
    /// If so, then perform the conversion and return true.
    bool convertToHardwareLoop(MachineLoop *L, bool &L0used, bool &L1used);

    /// Return true if the instruction is now dead.
    bool isDead(const MachineInstr *MI,
                SmallVectorImpl<MachineInstr *> &DeadPhis) const;

    /// Remove the instruction if it is now dead.
    void removeIfDead(MachineInstr *MI);

    /// Return true if MO and MI pair is visited only once. If visited
    /// more than once, this indicates there is recursion. In such a case,
    /// return false.
    bool isLoopFeeder(MachineLoop *L, MachineBasicBlock *A, MachineInstr *MI,
                      const MachineOperand *MO,
                      LoopFeederMap &LoopFeederPhi) const;

    /// Return true if the Phi may generate a value that may underflow,
    /// or may wrap.
    bool phiMayWrapOrUnderflow(MachineInstr *Phi, const MachineOperand *EndVal,
                               MachineBasicBlock *MBB, MachineLoop *L,
                               LoopFeederMap &LoopFeederPhi) const;

    /// Return true if the induction variable may underflow an unsigned
    /// value in the first iteration.
    bool loopCountMayWrapOrUnderFlow(const MachineOperand *InitVal,
                                     const MachineOperand *EndVal,
                                     MachineBasicBlock *MBB, MachineLoop *L,
                                     LoopFeederMap &LoopFeederPhi) const;

    /// Check if the given operand has a compile-time known constant
    /// value. Return true if yes, and false otherwise. When returning true, set
    /// Val to the corresponding constant value.
    bool checkForImmediate(const MachineOperand &MO, int64_t &Val) const;

  };

  char PULPHardwareLoops::ID = 0;

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
      if (isReg()) { OS << printReg(Contents.R.Reg, TRI, Contents.R.Sub); }
      if (isImm()) { OS << Contents.ImmVal; }
    }
  };

} // end anonymous namespace

INITIALIZE_PASS_BEGIN(PULPHardwareLoops, "pulp-hwloops",
                      "PULP Hardware Loops", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfo)
INITIALIZE_PASS_END(PULPHardwareLoops, "pulp-hwloops",
                    "PULP Hardware Loops", false, false)

FunctionPass *llvm::createPULPHardwareLoops() {
  return new PULPHardwareLoops();
}

bool PULPHardwareLoops::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "********* PULP Hardware Loops *********\n");

  // We can only use hardware loops if we have the PULPv2 extension enabled.
  if (!MF.getSubtarget<RISCVSubtarget>().hasPULPExtV2()) {
    return false;
  }

  if (skipFunction(MF.getFunction())) {
    return false;
  }

  bool Changed = false;
  NumHWLoopsInternal = 0;

  MLI = &getAnalysis<MachineLoopInfo>();
  MRI = &MF.getRegInfo();
  MDT = &getAnalysis<MachineDominatorTree>();
  const RISCVSubtarget &HST = MF.getSubtarget<RISCVSubtarget>();
  TII = HST.getInstrInfo();
  TRI = HST.getRegisterInfo();

  for (auto &L : *MLI)
    if (!L->getParentLoop()) {
      bool L0Used = false;
      bool L1Used = false;
      KnownHardwareLoops.clear();
      Changed |= convertToHardwareLoop(L, L0Used, L1Used);
    }

  if (Changed) {
    errs().changeColor(raw_fd_ostream::Colors::BLUE, true);
    errs() << "Created " << NumHWLoopsInternal << " hardware loops in "
           << MF.getName() << "!\n";
    errs().resetColor();
  }

  return Changed;
}

bool PULPHardwareLoops::findInductionRegister(MachineLoop *L,
                                                 unsigned &Reg,
                                                 int64_t &IVBump,
                                                 MachineInstr *&IVOp
                                                 ) const {
  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  MachineBasicBlock *Header = L->getHeader();
  MachineBasicBlock *Latch = L->getLoopLatch();
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();

  if (!Header || !Preheader || !Latch || !ExitingBlock) {
    return false;
  }

  // This pair represents an induction register together with an immediate
  // value that will be added to it in each loop iteration.
  using RegisterBump = std::pair<unsigned, int64_t>;

  // Mapping:  R.next -> (R, bump), where R, R.next and bump are derived
  // from an induction operation
  //   R.next = R + bump
  // where bump is an immediate value.
  using InductionMap = std::map<unsigned, RegisterBump>;

  InductionMap IndMap;

  using instr_iterator = MachineBasicBlock::instr_iterator;

  for (instr_iterator I = Header->instr_begin(), E = Header->instr_end();
       I != E && I->isPHI(); ++I) {
    MachineInstr *Phi = &*I;

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
        unsigned IndReg = DI->getOperand(1).getReg();
        MachineOperand &Opnd2 = DI->getOperand(2);
        int64_t V;
        if (MRI->getVRegDef(IndReg) == Phi && checkForImmediate(Opnd2, V)) {
          unsigned UpdReg = DI->getOperand(0).getReg();
          IndMap.insert(std::make_pair(UpdReg, std::make_pair(IndReg, V)));
        }
      }
    }
  }

  // If we couldn't find any registers used for the induction, we fail.
  if (IndMap.empty()) {
    return false;
  }

  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  SmallVector<MachineOperand,2> Cond;
  // Check that the exit branch can be analyzed.
  // AnalyzeBranch returns true if it fails to analyze branch.
  bool NotAnalyzed = TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false);
  if (NotAnalyzed || Cond.empty()) {
    return false;
  }
  
  // We now know there are two terminators, one conditional and one
  // unconditional. If the order does not match what we expect, bail out.
  MachineInstr *condTerm = &(*Latch->getFirstTerminator());
  MachineInstr *uncondTerm = &(*std::next(Latch->getFirstTerminator()));
  if (!(condTerm->getDesc().isConditionalBranch() &&
      uncondTerm->getDesc().isUnconditionalBranch())) {
    return false;
  }

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
        return false;
      }
      F = F2;
    }
  }
  if (F == IndMapEnd) {
    return false;
  }

  Reg = F->second.first;
  IVBump = F->second.second;
  IVOp = MRI->getVRegDef(F->first);

  return true;
}

// Return the comparison kind for the specified opcode.
PULPHardwareLoops::Comparison::Kind
PULPHardwareLoops::getComparisonKind(unsigned CondOpc,
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
  case RISCV::P_BNEIMM:
    Cmp = Comparison::NE;
    break;
  case RISCV::P_BEQIMM:
    Cmp = Comparison::EQ;
    break;
  default:
    return (Comparison::Kind)0;
  }
  return Cmp;
}

/// Analyze the statements in a loop to determine if the loop has
/// a computable trip count and, if so, return a value that represents
/// the trip count expression.
///
/// This function iterates over the phi nodes in the loop to check for
/// induction variable patterns that are used in the calculation for
/// the number of time the loop is executed.
CountValue *PULPHardwareLoops::getLoopTripCount(MachineLoop *L,
    SmallVectorImpl<MachineInstr *> &OldInsts) {

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

  // Look for the cmp instruction to determine if we can get a useful trip
  // count.  The trip count can be either a register or an immediate.  The
  // location of the value depends upon the type (reg or imm).
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();
  if (!ExitingBlock) {
    return nullptr;
  }

  // Find the registers used to hold the induction variable.
  unsigned IVReg = 0;
  int64_t IVBump = 0;
  MachineInstr *IVOp;
  bool FoundIV = findInductionRegister(L, IVReg, IVBump, IVOp);
  if (!FoundIV) {
    return nullptr;
  }

  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);

  MachineOperand *InitialValue = nullptr;
  MachineInstr *IV_Phi = MRI->getVRegDef(IVReg);
  MachineBasicBlock *Latch = L->getLoopLatch();
  for (unsigned i = 1, n = IV_Phi->getNumOperands(); i < n; i += 2) {
    MachineBasicBlock *MBB = IV_Phi->getOperand(i+1).getMBB();
    if (MBB == Preheader)
      InitialValue = &IV_Phi->getOperand(i);
    else if (MBB == Latch)
      IVReg = IV_Phi->getOperand(i).getReg();  // Want IV reg after bump.
  }
  if (!InitialValue) {
    return nullptr;
  }

  SmallVector<MachineOperand,2> Cond;
  MachineBasicBlock *TB = nullptr, *FB = nullptr;
  bool NotAnalyzed = TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false);
  if (NotAnalyzed) {
    return nullptr;
  }

  MachineBasicBlock *Header = L->getHeader();
  // TB must be non-null.  If FB is also non-null, one of them must be
  // the header.  Otherwise, branch to TB could be exiting the loop, and
  // the fall through can go to the header.
  assert (TB && "Exit block without a branch?");
  if (ExitingBlock != Latch && (TB == Latch || FB == Latch)) {
    MachineBasicBlock *LTB = nullptr, *LFB = nullptr;
    SmallVector<MachineOperand,2> LCond;
    bool NotAnalyzed = TII->analyzeBranch(*Latch, LTB, LFB, LCond, false);
    if (NotAnalyzed) {
      return nullptr;
    }
    if (TB == Latch)
      TB = (LTB == Header) ? LTB : LFB;
    else
      FB = (LTB == Header) ? LTB: LFB;
  }
  assert ((!FB || TB == Header || FB == Header) && "Branches not to header?");
  if (!TB || (FB && TB != Header && FB != Header)) {
    return nullptr;
  }
  
  // We now know there are two terminators, one conditional and one 
  // unconditional. Double check to be sure.
  MachineBasicBlock::iterator firstTerm = Latch->getFirstTerminator();
  MachineBasicBlock::iterator secondTerm = std::next(firstTerm);
  if (!(firstTerm->getDesc().isConditionalBranch() && 
      secondTerm->getDesc().isUnconditionalBranch())) {
    return nullptr;
  }


  unsigned CondOpc = Cond[0].getImm();


  // The comparison operator type determines how we compute the loop
  // trip count.
  OldInsts.push_back(IVOp);

  // Sadly, the following code gets information based on the position
  // of the operands in the compare instruction.  This has to be done
  // this way, because the comparisons check for a specific relationship
  // between the operands (e.g. is-less-than), rather than to find out
  // what relationship the operands are in (as on PPC).
  Comparison::Kind Cmp;
  bool isSwapped = false;
  const MachineOperand &Op1 = Cond[1];
  const MachineOperand &Op2 = Cond[2];
  const MachineOperand *EndValue = nullptr;

  if (Op1.isReg()) {
    if (Op2.isImm() || Op1.getReg() == IVReg)
      EndValue = &Op2;
    else {
      EndValue = &Op1;
      isSwapped = true;
    }
  }

  if (!EndValue) {
    return nullptr;
  }
  

  Cmp = getComparisonKind(CondOpc, InitialValue, EndValue, IVBump);
  if (!Cmp) {
    return nullptr;
  }
  if (isSwapped)
    Cmp = Comparison::getSwappedComparison(Cmp);

  if (InitialValue->isReg()) {
    unsigned R = InitialValue->getReg();
    MachineBasicBlock *DefBB = MRI->getVRegDef(R)->getParent();
    if (!MDT->properlyDominates(DefBB, Header)) {
      int64_t V;
      if (!checkForImmediate(*InitialValue, V)) {
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
        return nullptr;
      }
    }
    OldInsts.push_back(MRI->getVRegDef(R));
  }

  return computeCount(L, InitialValue, EndValue, IVReg, IVBump, Cmp);
}

/// Helper function that returns the expression that represents the
/// number of times a loop iterates.  The function takes the operands that
/// represent the loop start value, loop end value, and induction value.
/// Based upon these operands, the function attempts to compute the trip count.
CountValue *PULPHardwareLoops::computeCount(MachineLoop *Loop,
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

    if(!CmpHasEqual) {
      return nullptr;
    }

    // Both, start and end are immediates.
    int64_t Dist = std::max(immStart, immEnd) - std::min(immStart, immEnd);
    if (Dist == 0) {
      return nullptr;
    }

    if (Cmp != Comparison::EQ) {
      return nullptr;
    }
    
    bool Exact = (Dist % IVBump) == 0;
    if (!Exact) {
      return nullptr;
    }

    // Normalize distance to step
    uint64_t Count = Dist / std::abs(IVBump);

    if (Count > 0xFFFFFFFFULL) {
      return nullptr;
    }

    return new CountValue(CountValue::CV_Immediate, Count);
  }

  // Below here, we know that at least one of Start and End is a register.

  // Phis that may feed into the loop.
  LoopFeederMap LoopFeederPhi;

  // Check if the initial value may be zero and can be decremented in the first
  // iteration. If the value is zero, the endloop instruction will not decrement
  // the loop counter, so we shouldn't generate a hardware loop in this case.
  if (loopCountMayWrapOrUnderFlow(Start, End, Loop->getLoopPreheader(), Loop,
                                  LoopFeederPhi)) {
    return nullptr;
  }

  // A general case: Start and End are some values, but the actual
  // iteration count may not be available.  If it is not, insert
  // a computation of it into the preheader.

  // If the induction variable bump is not a power of 2, quit.
  // Othwerise we'd need a general integer division.
  if (!isPowerOf2_64(std::abs(IVBump))) {
    return nullptr;
  }


  // If Start is an immediate and End is a register, the trip count
  // will be "reg - imm".  PULP's "subtract immediate" instruction
  // is actually "reg + -imm".

  // If the loop IV is going downwards, i.e. if the bump is negative,
  // then the iteration count (computed as End-Start) will need to be
  // negated.  To avoid the negation, just swap Start and End.
  if (IVBump < 0) {
    std::swap(Start, End);
    IVBump = -IVBump;
  }
  // Cmp may now have a wrong direction, e.g.  LEs may now be GEs.
  // Signedness, and "including equality" are preserved.

  bool RegToImm = Start->isReg() && End->isImm(); // for (reg..imm)
  bool RegToReg = Start->isReg() && End->isReg(); // for (reg..reg)

  int64_t StartV = 0, EndV = 0;
  if (Start->isImm()) {
    StartV = Start->getImm();
  }
  if (End->isImm()) {
    EndV = End->getImm();
  }

  int64_t AdjV = 0;
  // FIXME: We need some logic based on the comparison to figure out if we need
  //        to add or remove some iterations from DistR (which we will put in
  //        adjusted distance (AdjR/V).

  // Compute DistR (register with the distance between Start and End).
  unsigned DistR, DistSR;

  // Check if the start is zero.
  // First check if it is an immediate zero.
  bool startIsImmZeroOrZeroReg = (Start->isImm() && StartV == 0);
  // If it wasn't, check if it is a register
  if (!startIsImmZeroOrZeroReg && Start->isReg()) {
    // If it is a register, check if it is the zero register.
    int64_t startImm;
    if (checkForImmediate(*Start, startImm)) {
      startIsImmZeroOrZeroReg = (startImm == 0);
    }
  }

  // Avoid special case, where the start value is an imm(0).
  if (startIsImmZeroOrZeroReg) {
    DistR = End->getReg();
    DistSR = End->getSubReg();
  } else {
    const MCInstrDesc &SubD = RegToReg ? TII->get(RISCV::SUB) :
                              (RegToImm ? TII->get(RISCV::SUB) /* TODO */ :
                                          TII->get(RISCV::ADDI));
    if (RegToReg || RegToImm) {
      unsigned SubR = MRI->createVirtualRegister(IntRC);
      MachineInstrBuilder SubIB =
        BuildMI(*PH, InsertPos, DL, SubD, SubR);
      if (RegToReg) {
        SubIB.addReg(End->getReg(), 0, End->getSubReg())
          .addReg(Start->getReg(), 0, Start->getSubReg());
      } else {
        MachineBasicBlock::iterator ThisInsertPos = InsertPos;
        ThisInsertPos--;

        unsigned ImmToRegReg = MRI->createVirtualRegister(IntRC);
        MachineInstrBuilder ImmToReg = BuildMI(*PH, ThisInsertPos, DL,
                                               TII->get(RISCV::ADDI),
                                               ImmToRegReg);
        ImmToReg.addReg(RISCV::X0).addImm(EndV);
        SubIB.addReg(ImmToReg->getOperand(0).getReg())
          .addReg(Start->getReg(), 0, Start->getSubReg());
      }
      DistR = SubR;
    } else {
      // If the loop has been unrolled, we should use the original loop count
      // instead of recalculating the value. This will avoid additional
      // 'Add' instruction.
      const MachineInstr *EndValInstr = MRI->getVRegDef(End->getReg());
      if (EndValInstr->getOpcode() == RISCV::ADDI &&
          EndValInstr->getOperand(1).getSubReg() == 0 &&
          EndValInstr->getOperand(2).getImm() == StartV) {
        DistR = EndValInstr->getOperand(1).getReg();
      } else {
        unsigned SubR = MRI->createVirtualRegister(IntRC);
        MachineInstrBuilder SubIB =
          BuildMI(*PH, InsertPos, DL, SubD, SubR);
        SubIB.addReg(End->getReg(), 0, End->getSubReg())
             .addImm(-StartV);
        DistR = SubR;
      }
    }
    DistSR = 0;
  }

  // From DistR, compute AdjR (register with the adjusted distance).
  unsigned AdjR, AdjSR;

  if (AdjV == 0) {
    AdjR = DistR;
    AdjSR = DistSR;
  } else {
    // Generate CountR = ADD DistR, AdjVal
    unsigned AddR = MRI->createVirtualRegister(IntRC);
    MCInstrDesc const &AddD = TII->get(RISCV::ADDI);
    BuildMI(*PH, InsertPos, DL, AddD, AddR)
      .addReg(DistR, 0, DistSR)
      .addImm(AdjV);

    AdjR = AddR;
    AdjSR = 0;
  }

  // From AdjR, compute CountR (register with the final count).
  unsigned CountR, CountSR;

  if (IVBump == 1) {
    CountR = AdjR;
    CountSR = AdjSR;
  } else {
    // The IV bump is a power of two. Log_2(IV bump) is the shift amount.
    unsigned Shift = Log2_32(IVBump);

    // Generate NormR = LSR DistR, Shift.
    unsigned LsrR = MRI->createVirtualRegister(IntRC);
    const MCInstrDesc &LsrD = TII->get(RISCV::SRLI);
    BuildMI(*PH, InsertPos, DL, LsrD, LsrR)
      .addReg(AdjR, 0, AdjSR)
      .addImm(Shift);

    CountR = LsrR;
    CountSR = 0;
  }

  return new CountValue(CountValue::CV_Register, CountR, CountSR);
}

/// Return true if the operation is invalid within hardware loop.
bool PULPHardwareLoops::isInvalidLoopOperation(const MachineInstr *MI) const {
  // Call is not allowed because the callee may use a hardware loop.
  // Furthermore, calls may be inlined during LTO, and we do not yet support
  // fixups for this after LTO.
  if (MI->getDesc().isCall())
    return true;

  // If this code for some reason (e.g., inline ASM) already contains a hw loop
  // we don't currently have this represented in our book keeping. It is a
  // corner case, so for now we just don't create HW loops if this happens.
  if (MI->getOpcode() == RISCV::LOOP0setup ||
      MI->getOpcode() == RISCV::LOOP1setup ||
      MI->getOpcode() == RISCV::LOOP0setupi ||
      MI->getOpcode() == RISCV::LOOP1setupi) {
    return KnownHardwareLoops.count(MI) == 0;
  }

  // FIXME: We should probably not allow Inline ASM at all, except for the
  //        HERO 64-bit loads.

  return false;
}

/// Return true if the loop contains an instruction that inhibits
/// the use of the hardware loop instruction.
bool PULPHardwareLoops::containsInvalidInstruction(MachineLoop *L) const {
  for (MachineBasicBlock *MBB : L->getBlocks()) {
    for (MachineInstr &MI : *MBB) {
      if (isInvalidLoopOperation(&MI)) {
        LLVM_DEBUG(dbgs() << "\nCannot convert to hwloop due to:"; MI.dump(););
        return true;
      }
    }
  }
  return false;
}

/// Returns true if the instruction is dead.  This was essentially
/// copied from DeadMachineInstructionElim::isDead, but with special cases
/// for inline asm, physical registers and instructions with side effects
/// removed.
bool PULPHardwareLoops::isDead(const MachineInstr *MI,
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
    if (std::next(I) != End || !I->getParent()->isPHI())
      return false;

    MachineInstr *OnePhi = I->getParent();
    for (unsigned j = 0, f = OnePhi->getNumOperands(); j != f; ++j) {
      const MachineOperand &OPO = OnePhi->getOperand(j);
      if (!OPO.isReg() || !OPO.isDef())
        continue;

      unsigned OPReg = OPO.getReg();
      use_nodbg_iterator nextJ;
      for (use_nodbg_iterator J = MRI->use_nodbg_begin(OPReg);
           J != End; J = nextJ) {
        nextJ = std::next(J);
        MachineOperand &Use = *J;
        MachineInstr *UseMI = Use.getParent();

        // If the phi node has a user that is not MI, bail.
        if (MI != UseMI)
          return false;
      }
    }
    DeadPhis.push_back(OnePhi);
  }

  // If there are no defs with uses, the instruction is dead.
  return true;
}

void PULPHardwareLoops::removeIfDead(MachineInstr *MI) {
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

/// Check if the loop is a candidate for converting to a hardware
/// loop.  If so, then perform the transformation.
///
/// This function works on innermost loops first.  A loop can be converted
/// if it is a counting loop; either a register value or an immediate.
///
/// The code makes several assumptions about the representation of the loop
/// in llvm.
bool PULPHardwareLoops::convertToHardwareLoop(MachineLoop *L,
                                                 bool &RecL0used,
                                                 bool &RecL1used) {
  // This is just for sanity.
  assert(L->getHeader() && "Loop without a header?");

  bool Changed = false;
  bool L0Used = false;
  bool L1Used = false;

  // Process nested loops first.
  for (MachineLoop::iterator I = L->begin(), E = L->end(); I != E; ++I) {
    Changed |= convertToHardwareLoop(*I, RecL0used, RecL1used);
    L0Used |= RecL0used;
    L1Used |= RecL1used;
  }

  // If a nested loop has been converted, then we can't convert this loop.
  if (Changed && L0Used && L1Used) {
    return Changed;
  }

  // The instructions that are available to use at this level. If L0 is already
  // used we have to use L1.
  unsigned LOOP_i;
  unsigned LOOP_r;
  if (L0Used) {
    LOOP_i = RISCV::LOOP1setupi;
    LOOP_r = RISCV::LOOP1setup;
  } else {
    LOOP_i = RISCV::LOOP0setupi;
    LOOP_r = RISCV::LOOP0setup;
  }

  // Does the loop contain any invalid instructions?
  if (containsInvalidInstruction(L)) {
    return Changed;
  }

  MachineBasicBlock *LastMBB = L->findLoopControlBlock();
  // Don't generate hw loop if the loop has more than one exit.
  if (!LastMBB) {
    return Changed;
  }

  MachineBasicBlock::iterator LastI = LastMBB->getFirstTerminator();
  if (LastI == LastMBB->end()) {
    return Changed;
  }

  // Ensure the loop has a preheader: the loop instruction will be
  // placed there.
  MachineBasicBlock *Preheader = MLI->findLoopPreheader(L, SpecPreheader);
  if (!Preheader) {

    // FIXME: The HEXAGON pass upon which this is based tried to create a new
    //        preheader for the loop here. Instead we just return false, as I am
    //        not sure how common this is on PULP. Perhaps it is better to
    //        re-implement it based on sample code that GCC manages to make into
    //        hardware loops, but we fail.

    return Changed;
  }

  MachineBasicBlock::iterator InsertPos = Preheader->getFirstTerminator();

  SmallVector<MachineInstr*, 2> OldInsts;
  // Are we able to determine the trip count for the loop?
  CountValue *TripCount = getLoopTripCount(L, OldInsts);
  if (!TripCount) {
    return Changed;
  }

  // Is the trip count available in the preheader?
  if (TripCount->isReg()) {
    // There will be a use of the register inserted into the preheader,
    // so make sure that the register is actually defined at that point.
    MachineInstr *TCDef = MRI->getVRegDef(TripCount->getReg());
    MachineBasicBlock *BBDef = TCDef->getParent();
    if (!MDT->dominates(BBDef, Preheader)) {
      return Changed;
    }
  }

  // Determine the loop start.
  MachineBasicBlock *TopBlock = L->getTopBlock();
  MachineBasicBlock *ExitingBlock = L->findLoopControlBlock();
  MachineBasicBlock *ExitBlock = L->getExitBlock();
  MachineBasicBlock *LoopStart = nullptr;
  if (ExitingBlock !=  L->getLoopLatch()) {
    MachineBasicBlock *TB = nullptr, *FB = nullptr;
    SmallVector<MachineOperand, 2> Cond;
    if (TII->analyzeBranch(*ExitingBlock, TB, FB, Cond, false)) {
      return Changed;
    }
    if (L->contains(TB))
      LoopStart = TB;
    else if (L->contains(FB))
      LoopStart = FB;
    else {
      return Changed;
    }
  }
  else {
    LoopStart = TopBlock;
  }
  assert(LoopStart != nullptr && "Didn't find loop start!");

  // We need a single exit block to make sure that this loop can be simplified
  // to a fixed amount of loop iterations.
  if (!ExitBlock) {
    return Changed;
  }

  // Ensure the loop length can reasonably fit into 12 bits.  Assume
  // non-compressed instructions as an upper-bound for the length. The length
  // gives the offset which must fit in 12 bits.
  unsigned loopSize = 0;
  const unsigned instructionSize = 4;
  for (const MachineBasicBlock *LB : L->getBlocks()) {
    loopSize += instructionSize * LB->size();
    if (loopSize > 0xFFF) {
      return Changed;
    }
  }
  
  // Convert the loop to a hardware loop.
  LLVM_DEBUG(dbgs() << "Change to hardware loop at "; L->dump());
  DebugLoc DL;
  if (InsertPos != Preheader->end())
    DL = InsertPos->getDebugLoc();

  if (TripCount->isReg()) {
    // Create a copy of the loop count register.
    unsigned CountReg = MRI->createVirtualRegister(&RISCV::GPRRegClass);
    BuildMI(*Preheader, InsertPos, DL, TII->get(TargetOpcode::COPY), CountReg)
      .addReg(TripCount->getReg(), 0, TripCount->getSubReg());
    // Add the Loop instruction to the beginning of the loop.
    auto hwloop = BuildMI(*Preheader, InsertPos, DL, TII->get(LOOP_r))
      .addMBB(ExitingBlock).addReg(CountReg);
    KnownHardwareLoops.insert(hwloop.getInstr());
  } else {
    assert(TripCount->isImm() && "Expecting immediate value for trip count");
    // Add the Loop immediate instruction to the beginning of the loop,
    // if the immediate fits in the instructions.  Otherwise, we need to
    // create a new virtual register.
    int64_t CountImm = TripCount->getImm();
    if (static_cast<uint64_t>(CountImm) >
        APInt::getMaxValue(12).getLimitedValue()) {
      unsigned CountReg = MRI->createVirtualRegister(&RISCV::GPRRegClass);
      BuildMI(*Preheader, InsertPos, DL, TII->get(RISCV::ADDI), CountReg)
        .addReg(RISCV::X0).addImm(CountImm);
      auto hwloop = BuildMI(*Preheader, InsertPos, DL, TII->get(LOOP_r))
        .addMBB(ExitingBlock).addReg(CountReg);
      KnownHardwareLoops.insert(hwloop.getInstr());
    } else {
      auto hwloop = BuildMI(*Preheader, InsertPos, DL, TII->get(LOOP_i))
        .addMBB(ExitingBlock).addImm(CountImm);
      KnownHardwareLoops.insert(hwloop.getInstr());
    }
  }
  delete TripCount;

  // Make sure the loop start always has a reference in the CFG.  We need
  // to create a BlockAddress operand to get this mechanism to work both the
  // MachineBasicBlock and BasicBlock objects need the flag set.
  ExitingBlock->setHasAddressTaken();
  ExitingBlock->setLabelMustBeEmitted();
  // This line is needed to set the hasAddressTaken flag on the BasicBlock
  // object.
  BlockAddress::get(const_cast<BasicBlock *>(ExitingBlock->getBasicBlock()));

  // The induction operation and the comparison may now be
  // unneeded. If these are unneeded, then remove them.
  for (unsigned i = 0; i < OldInsts.size(); ++i) {
    removeIfDead(OldInsts[i]);
  }

  ++NumHWLoops;
  ++NumHWLoopsInternal;

  // Set RecL1used and RecL0used only after hardware loop has been
  // successfully generated. Doing it earlier can cause wrong loop instruction
  // to be used.
  if (L0Used) // Loop0 was already used. So, the correct loop must be loop1.
    RecL1used = true;
  else
    RecL0used = true;

  return true;
}

/// This function is required to break recursion. Visiting phis in a loop may
/// result in recursion during compilation. We break the recursion by making
/// sure that we visit a MachineOperand and its definition in a
/// MachineInstruction only once. If we attempt to visit more than once, then
/// there is recursion, and will return false.
bool PULPHardwareLoops::isLoopFeeder(MachineLoop *L, MachineBasicBlock *A,
                                        MachineInstr *MI,
                                        const MachineOperand *MO,
                                        LoopFeederMap &LoopFeederPhi) const {
  if (LoopFeederPhi.find(MO->getReg()) == LoopFeederPhi.end()) {
    LLVM_DEBUG(dbgs() << "\nhw_loop head, "
                      << printMBBReference(**L->block_begin()));
    // Ignore all BBs that form Loop.
    for (MachineBasicBlock *MBB : L->getBlocks()) {
      if (A == MBB)
        return false;
    }
    MachineInstr *Def = MRI->getVRegDef(MO->getReg());
    LoopFeederPhi.insert(std::make_pair(MO->getReg(), Def));
    return true;
  } else
    // Already visited node.
    return false;
}

/// Return true if a Phi may generate a value that can underflow.
/// This function calls loopCountMayWrapOrUnderFlow for each Phi operand.
bool PULPHardwareLoops::phiMayWrapOrUnderflow(
    MachineInstr *Phi, const MachineOperand *EndVal, MachineBasicBlock *MBB,
    MachineLoop *L, LoopFeederMap &LoopFeederPhi) const {
  assert(Phi->isPHI() && "Expecting a Phi.");
  // Walk through each Phi, and its used operands. Make sure that
  // if there is recursion in Phi, we won't generate hardware loops.
  for (int i = 1, n = Phi->getNumOperands(); i < n; i += 2)
    if (isLoopFeeder(L, MBB, Phi, &(Phi->getOperand(i)), LoopFeederPhi))
      if (loopCountMayWrapOrUnderFlow(&(Phi->getOperand(i)), EndVal,
                                      Phi->getParent(), L, LoopFeederPhi))
        return true;
  return false;
}

/// Return true if the induction variable can underflow in the first iteration.
/// An example, is an initial unsigned value that is 0 and is decrement in the
/// first itertion of a do-while loop.  In this case, we cannot generate a
/// hardware loop because the endloop instruction does not decrement the loop
/// counter if it is <= 1. We only need to perform this analysis if the
/// initial value is a register.
///
/// This function assumes the initial value may underfow unless proven
/// otherwise. If the type is signed, then we don't care because signed
/// underflow is undefined. We attempt to prove the initial value is not
/// zero by perfoming a crude analysis of the loop counter. This function
/// checks if the initial value is used in any comparison prior to the loop
/// and, if so, assumes the comparison is a range check. This is inexact,
/// but will catch the simple cases.
bool PULPHardwareLoops::loopCountMayWrapOrUnderFlow(
    const MachineOperand *InitVal, const MachineOperand *EndVal,
    MachineBasicBlock *MBB, MachineLoop *L,
    LoopFeederMap &LoopFeederPhi) const {
  // Only check register values since they are unknown.
  if (!InitVal->isReg())
    return false;

  if (!EndVal->isImm())
    return false;

  // A register value that is assigned an immediate is a known value, and it
  // won't underflow in the first iteration.
  int64_t Imm;
  if (checkForImmediate(*InitVal, Imm))
    return (EndVal->getImm() == Imm);

  Register Reg = InitVal->getReg();

  // We don't know the value of a physical register.
  if (!Reg.isVirtual())
    return true;

  MachineInstr *Def = MRI->getVRegDef(Reg);
  if (!Def)
    return true;

  // If the initial value is a Phi or copy and the operands may not underflow,
  // then the definition cannot be underflow either.
  if (Def->isPHI() && !phiMayWrapOrUnderflow(Def, EndVal, Def->getParent(),
                                             L, LoopFeederPhi))
    return false;
  if (Def->isCopy() && !loopCountMayWrapOrUnderFlow(&(Def->getOperand(1)),
                                                    EndVal, Def->getParent(),
                                                    L, LoopFeederPhi))
    return false;

  // Iterate over the uses of the initial value. If the initial value is used
  // in a compare, then we assume this is a range check that ensures the loop
  // doesn't underflow. This is not an exact test and should be improved.
  for (MachineRegisterInfo::use_instr_nodbg_iterator I = MRI->use_instr_nodbg_begin(Reg),
         E = MRI->use_instr_nodbg_end(); I != E; ++I) {
    MachineInstr *MI = &*I;
    Register CmpReg1 = 0, CmpReg2 = 0;
    int64_t CmpMask = 0, CmpValue = 0;

    if (!TII->analyzeCompare(*MI, CmpReg1, CmpReg2, CmpMask, CmpValue))
      continue;

    MachineBasicBlock *TBB = nullptr, *FBB = nullptr;
    SmallVector<MachineOperand, 2> Cond;
    if (TII->analyzeBranch(*MI->getParent(), TBB, FBB, Cond, false))
      continue;

    Comparison::Kind Cmp =
        getComparisonKind(MI->getOpcode(), nullptr, nullptr, 0);
    if (Cmp == 0)
      continue;
    //if (TII->predOpcodeHasNot(Cond) ^ (TBB != MBB)) // TODO
    //  Cmp = Comparison::getNegatedComparison(Cmp);
    if (CmpReg2 != 0 && CmpReg2 == Reg)
      Cmp = Comparison::getSwappedComparison(Cmp);

    // Signed underflow is undefined.
    if (Comparison::isSigned(Cmp))
      return false;

    // Check if there is a comparison of the initial value. If the initial value
    // is greater than or not equal to another value, then assume this is a
    // range check.
    if ((Cmp & Comparison::G) || Cmp == Comparison::NE)
      return false;
  }

  // OK - this is a hack that needs to be improved. We really need to analyze
  // the instructions performed on the initial value. This works on the simplest
  // cases only.
  if (!Def->isCopy() && !Def->isPHI())
    return false;

  return true;
}

bool PULPHardwareLoops::checkForImmediate(const MachineOperand &MO,
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

  Register R = MO.getReg();
  if (!R.isVirtual()) {
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

