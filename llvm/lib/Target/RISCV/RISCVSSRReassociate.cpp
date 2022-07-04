//===- SSRReassociatePass.cpp - Expand atomic instructions ------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass (at IR level) to replace atomic instructions with
// __atomic_* library calls, or target specific instruction which implement the
// same semantics in a way which better fits the target backend.  This can
// include the use of (intrinsic-based) load-linked/store-conditional loops,
// AtomicCmpXchg, or type coercions.
//
//===----------------------------------------------------------------------===//

#include "RISCV.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/RuntimeLibcalls.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include <algorithm>
#include <vector>
#include <limits>

using namespace llvm;

#define DEBUG_TYPE "ssr-inference"

namespace {

  class SSRReassociate: public FunctionPass {
    const TargetLowering *TLI = nullptr;

  public:
    static char ID; // Pass identification, replacement for typeid

    SSRReassociate() : FunctionPass(ID) {
      initializeSSRReassociatePass(*PassRegistry::getPassRegistry());
    }

    bool runOnFunction(Function &F) override;

  private:
    bool runOnBB(BasicBlock &BB);
    // void moveAfterWithMetadata
    // DominatorTreeWrapperPass DTP;
  };

} // end anonymous namespace

bool SSRReassociate::runOnFunction(Function &F) {
  bool Modified = false;

  errs()<<"SSR Reassociate Pass running on: "<<F.getNameOrAsOperand()<<"\n";

  // this->DTP.runOnFunction(F);
  if (!F.hasFnAttribute("SSR")) return false;

  for (auto &BB : F) Modified |= runOnBB(BB);

  return Modified;
}

// static bool BubbleSSRIntrinsics(BasicBlock &BB) {
//   bool Modified = false;
//   auto II = BB.getFirstInsertionPt();
//   auto LastInsertedPop = std::prev(II);
//   auto LastInsertedPush = BB.getTerminator()->getIterator();
//   while (II != BB.end() && II != LastInsertedPush) {
//     auto NII = std::next(II);
//     if (isa<IntrinsicInst>(*II)) {
//       auto &Intr = cast<IntrinsicInst>(*II);
//       if (Intr.getIntrinsicID() == Intrinsic::riscv_ssr_pop) {
//         Intr.removeFromParent();
//         Intr.insertAfter(&*LastInsertedPop);
//         LastInsertedPop = Intr.getIterator();
//         Modified = true;
//       } else if (Intr.getIntrinsicID() == Intrinsic::riscv_ssr_push) {
//         Intr.removeFromParent();
//         Intr.insertBefore(&*LastInsertedPush);
//         LastInsertedPush = Intr.getIterator();
//         Modified = true;
//       }
//     }
//     II = NII;
//   }
//   return Modified;
// }

static bool BubbleSSRIntrinsics(BasicBlock &BB) {
  bool Modified = false;
  auto II = BB.getFirstInsertionPt();
  DenseSet<const Instruction *> vis;
  while (II != BB.end()) {
    auto NII = std::next(II);
    if (vis.find(&*II) != vis.end()) {
      II = NII;
      continue;
    }
    vis.insert(&*II);
    if (isa<IntrinsicInst>(*II)) {
      auto &Intr = cast<IntrinsicInst>(*II);
      if (Intr.getIntrinsicID() == Intrinsic::riscv_ssr_pop) {
        Instruction *UU = nullptr;
        for (User *U : Intr.users()) {
          if (isa<Instruction>(U) && !UU) UU = cast<Instruction>(U);
          else UU = nullptr;
        }
        if (UU) {
          Intr.moveBefore(UU);
          Modified = true;
        }
      } else if (Intr.getIntrinsicID() == Intrinsic::riscv_ssr_push) {
        if (Instruction *D = dyn_cast<Instruction>(Intr.getOperand(1))) {
          Intr.moveAfter(D);
          Modified = true;
        }
      }
    }
    II = NII;
  }
  return Modified;
}

static bool isAssociative(const Value &V) {
  if (!isa<Instruction>(V)) return false;
  const auto &I = cast<Instruction>(V);
  if (I.getType()->isIntegerTy(1u)) return false; //ignore bools
  if(I.isAssociative()) return true;
  if ((I.getType()->isFloatingPointTy() && I.isFast())){ //https://gcc.gnu.org/wiki/FloatingPointMath
    switch (I.getOpcode())
    {
    case Instruction::BinaryOps::FAdd:
    case Instruction::BinaryOps::FMul:
      return true;  
    default:
      return false;
    }
  }
  return false;
}

static bool isBinop(const Value &I) {
  return isa<BinaryOperator>(I);
}

static unsigned getAndUpdateHeight(const Value &V, DenseMap<const Instruction *, unsigned> &heights); //bc mutual recursion

static unsigned updateHeightFromChildren(const BinaryOperator &I, DenseMap<const Instruction *, unsigned> &heights) {
  unsigned this_height = 1u + std::max(
    getAndUpdateHeight(*I.getOperand(0), heights),
    getAndUpdateHeight(*I.getOperand(1), heights)
  );
  auto p = heights.insert(std::make_pair(&I, this_height));
  if (!p.second) p.first->getSecond() = this_height; //update value
  return this_height;
}

static unsigned getAndUpdateHeight(const Value &V, DenseMap<const Instruction *, unsigned> &heights) {
  if (!isa<Instruction>(V)) return 0;
  const Instruction &I = cast<Instruction>(V);
  if (!isBinop(I)) return 0;
  auto d = heights.find(&I);
  if (d != heights.end()) return d->second; //if height is available it is correct
  return updateHeightFromChildren(cast<BinaryOperator>(I), heights);
}

// static void moveAfterWithMetadata(BinaryOperator &OP, Instruction *Point) {
  
// }

static BinaryOperator *rotateCC(BinaryOperator &L, BinaryOperator &I, DenseMap<const Instruction *, unsigned> &heights) {
  errs()<<"rotating CC:"<<L.getNameOrAsOperand()<<" "<<I.getNameOrAsOperand()<<"\n";
  assert(isAssociative(L) && isAssociative(I) && I.getOperand(0) == &L);
  I.setOperand(0, L.getOperand(1));
  I.replaceAllUsesWith(&L);
  L.setOperand(1, &I);
  L.dropDroppableUses();
  L.moveAfter(&I);
  updateHeightFromChildren(I, heights);
  updateHeightFromChildren(L, heights);
  return &L;
}

static BinaryOperator *rotateCW(BinaryOperator &R, BinaryOperator &I, DenseMap<const Instruction *, unsigned> &heights) {
  errs()<<"rotating CW:"<<R.getNameOrAsOperand()<<" "<<I.getNameOrAsOperand()<<"\n";
  assert(isAssociative(R) && isAssociative(I) && I.getOperand(1) == &R);
  I.setOperand(1, R.getOperand(0));
  I.replaceAllUsesWith(&R);
  R.setOperand(0, &I);
  R.dropDroppableUses(); //remove debug insts that would otherwise not be dominated by R anymore
  R.moveAfter(&I);
  updateHeightFromChildren(I, heights);
  updateHeightFromChildren(R, heights);
  return &R;
}

static BinaryOperator *tryRotateL(Value &Left, Value &Root, DenseMap<const Instruction *, unsigned> &heights) {
  if (isBinop(Left) && isBinop(Root) && isAssociative(Left) && isAssociative(Root)) {
    BinaryOperator &L = cast<BinaryOperator>(Left);
    BinaryOperator &I = cast<BinaryOperator>(Root);
    const unsigned opcode = I.getOpcode();
    if (L.getOpcode() != opcode) return nullptr; //cannot do anything
    unsigned lh = getAndUpdateHeight(L, heights);
    if (lh <= 1u) return nullptr; //nothing to do
    auto &L_RChild = *L.getOperand(1);
    if (isBinop(L_RChild) && isAssociative(L_RChild) 
        && getAndUpdateHeight(L_RChild, heights) + 1u == lh) {
      auto &LRC = cast<BinaryOperator>(L_RChild);
      if (LRC.getOpcode() == opcode) {
        auto &newL = *rotateCW(LRC, L, heights);
        return rotateCC(newL, I, heights);
      }
    }
    return rotateCC(L, I, heights);
  }
  return nullptr;
}

static BinaryOperator *tryRotateR(Value &Right, Value &Root, DenseMap<const Instruction *, unsigned> &heights) {
  if (isBinop(Right) && isBinop(Root) && isAssociative(Right) && isAssociative(Root)) {
    BinaryOperator &R = cast<BinaryOperator>(Right);
    BinaryOperator &I = cast<BinaryOperator>(Root);
    const unsigned opcode = I.getOpcode();
    if (R.getOpcode() != opcode) return nullptr; //cannot do anything
    unsigned rh = getAndUpdateHeight(R, heights);
    if (rh <= 1u) return nullptr; //nothing to do
    auto &R_LChild = *R.getOperand(0);
    if (isBinop(R_LChild) && isAssociative(R_LChild)
      && getAndUpdateHeight(R_LChild, heights) + 1u == rh) {
        auto &RLC = cast<BinaryOperator>(R_LChild);
        if (RLC.getOpcode() == opcode) {
          auto &newR = *rotateCC(RLC, R, heights);
          return rotateCW(newR, I, heights);
        }
      }
    return rotateCW(R, I, heights);
  }
  return nullptr;
}

static bool Reassociate(Value &Inst, DenseMap<const Instruction *, unsigned> &heights) {
  bool Modified = false;
  if (isBinop(Inst) && isAssociative(Inst)) {
    BinaryOperator *I = cast<BinaryOperator>(&Inst);
    bool improved_root = true;
    while (improved_root) {
      improved_root = false;
      int lminusr = 
        (int)getAndUpdateHeight(*I->getOperand(0), heights) 
        - (int)getAndUpdateHeight(*I->getOperand(1), heights);
      BinaryOperator *NewRoot = nullptr;
      if (lminusr >= 2) {
        NewRoot = tryRotateL(*I->getOperand(0), *I, heights); //try to fix at this height
      } else if (lminusr <= -2) {
        NewRoot = tryRotateR(*I->getOperand(1), *I, heights); //try to fix at this height
      }
      if (NewRoot) {
        I = NewRoot;
        improved_root = true;
        Modified = true;
      }
    }

    bool improved_left = Reassociate(*I->getOperand(0), heights); //fix left subtree
    bool improved_right = Reassociate(*I->getOperand(1), heights); //fix right subtree
    Modified = Modified || improved_left || improved_right;

    updateHeightFromChildren(*I, heights);
  }
  return Modified;
}

static void printDep(Value &I, unsigned lvl, DenseMap<const Instruction *, unsigned> &heights, DenseSet<const Value *> &vis) {
  if (vis.find(&I) != vis.end()) return;
  vis.insert(&I);
  for (unsigned i = 0; i < lvl; i++) errs()<<"| \t";
  unsigned h = 0;
  if (isa<Instruction>(I)) {
    auto p = heights.find(&cast<Instruction>(I));
    if (p != heights.end()) h = p->second;
  }
  errs()<<" h = "<<h<<" ";
  errs()<<I.getNameOrAsOperand()<<"\n";
  if (isBinop(I) && isAssociative(I)){
    auto &X = cast<BinaryOperator>(I);
    for (unsigned i = 0; i < X.getNumOperands(); i++) {
      auto *V = X.getOperand(i);
      if (V) printDep(*V, lvl+1, heights, vis);
    }
  }
}

static bool Reassociate(BasicBlock &BB) {
  bool Modified = false;

  DenseMap<const Instruction *, unsigned> heights;

  auto RI = BB.rbegin();
  while (RI != BB.rend()) {
    Modified |= Reassociate(*RI, heights);
    RI++; //yes, this means we miss some instructions, but those are optimized already anyway
  }

  if (Modified) {
    errs()<<"Reassociate in BB: "<<Modified<<"\n";
    DenseSet<const Value *> vis;
    for (auto RI = BB.rbegin(); RI != BB.rend(); RI++) {
      printDep(*RI, 0, heights, vis);
    }
    BB.dump();
  }

  return Modified;
}

bool SSRReassociate::runOnBB(BasicBlock &BB) {
  bool Modified = false;

  Modified |= Reassociate(BB);

  Modified |= BubbleSSRIntrinsics(BB);

  return Modified;
}


char SSRReassociate::ID = 0;

INITIALIZE_PASS(SSRReassociate, DEBUG_TYPE, "SSR Reassociate Pass", false, false)

FunctionPass *llvm::createSSRReassociatePass() { return new SSRReassociate(); }
