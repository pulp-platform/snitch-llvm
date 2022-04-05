//===-- SSRInference.cpp - Infer SSR usage --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/SSR/SSRInference.h"
#include "llvm/InitializePasses.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include <deque>

using namespace llvm;

namespace{

Value *SCEVtoValues(const SCEV *scev, BasicBlock *block){
  assert(scev && block && "arguments should not be null");
  switch (scev->getSCEVType())
  {
    case SCEVTypes::scConstant: 
    {
      return cast<SCEVConstant>(scev)->getValue();
    }
    case SCEVTypes::scUnknown:
    {
      return cast<SCEVUnknown>(scev)->getValue();
    }
    case SCEVTypes::scTruncate:
    case SCEVTypes::scZeroExtend:
    case SCEVTypes::scSignExtend:
    {
      const SCEVCastExpr *castSCEV = cast<SCEVCastExpr>(scev);
      Value *v = SCEVtoValues(castSCEV->getOperand(0), block);
      if (v){
        Instruction *i;
        switch (scev->getSCEVType())
        {
        case SCEVTypes::scTruncate:
          i = CastInst::CreateTruncOrBitCast(v, castSCEV->getType(), "scev.trunc", block);
          break;
        case SCEVTypes::scZeroExtend:
          i = CastInst::CreateZExtOrBitCast(v, castSCEV->getType(), "scev.zext", block);
          break;
        case SCEVTypes::scSignExtend:
          i = CastInst::CreateSExtOrBitCast(v, castSCEV->getType(), "scev.sext", block);
          break;
        default:
          assert(false && "should not happen!");
          break;
        }
        return i;
      }
      return nullptr;
    }
    case SCEVTypes::scAddExpr:
    case SCEVTypes::scMulExpr:
    {
      const SCEVCommutativeExpr *binopSCEV = cast<SCEVCommutativeExpr>(scev);
      Value *v1 = SCEVtoValues(binopSCEV->getOperand(0), block);
      Value *v2 = SCEVtoValues(binopSCEV->getOperand(1), block);
      if (v1 && v2){
        Instruction *binop;
        if (binopSCEV->getSCEVType() == SCEVTypes::scAddExpr) {
          binop = BinaryOperator::CreateAdd(v1, v2, "rcev.add", block);
        } else {
          binop = BinaryOperator::CreateMul(v1, v2, "rcev.mul", block);
        }
        return binop;
      }
      return nullptr;
    }
    default:
    {
      errs()<<"encountered some weird SCEVType:\n";
      scev->dump();
      return nullptr;
    }
  }
}

bool runOnLoop(
    Loop *L, AAResults *AA, LoopInfo *LI, DominatorTree *DT,
    BlockFrequencyInfo *BFI, TargetLibraryInfo *TLI, TargetTransformInfo *TTI,
    ScalarEvolution *SE, MemorySSA *MSSA) {
  L->dump();

  if (!L->isInnermost() || !L->isLCSSAForm(*DT)){
    errs()<<"loop not innermost or not LCSSA\n";
    return false;
  }

  BasicBlock *preheader = L->getLoopPreheader();
  if (!preheader){
    //TODO: can alleviate this by adding one if SSR setup needed
    errs()<<"loop has no preheader\n";
    return false;
  }

  BasicBlock *exit = L->getExitBlock();
  if (!exit){
    errs()<<"loop has no or multiple exits\n";
    return false;
  }

  BasicBlock *exiting = L->getExitingBlock();
  if (!exiting){
    errs()<<"no, or multiple exiting blocks \n";
    return false;
  }

  if (!L->hasDedicatedExits()){
    //TODO: relatively easily fixable ==> add block before single exit block
    errs()<<"exit block is not dedicated \n";
    return false;
  }

  if (L->getNumBackEdges() != 1){
    errs()<<"# of back-edges is not 1 \n";
    return false;
  }

  if (!SE->hasLoopInvariantBackedgeTakenCount(L)){
    errs()<<"back-edge taken count is not loop-inv\n";
    return false;
  }

  const SCEV *bt = SE->getBackedgeTakenCount(L);
  errs()<<"backedge taken SCEV is:\n";
  bt->dump(); errs()<<"\n";

  Value *v = SCEVtoValues(bt, preheader);
  if (!v){
    errs()<<"SCEV to Value/Instructions conversion failed\n";
    return false;
  }
  v->dump();

  bool Changed = false;
  
  if (Changed){
    SE->forgetLoop(L); //TODO: maybe use SE->forgetValue instead
  }
  return Changed;
}

/*
  InductionDescriptor IndDesc;
  if(!L->getInductionDescriptor(*SE, IndDesc)){
    errs()<<"no loop induction variable found\n";
    return false;
  }
  if(IndDesc.getKind() != InductionDescriptor::IK_IntInduction){
    //TODO: could allow with addresses too
    errs()<<"induction not on integer\n";
    return false;
  }
  Value *tc = findTripCount(L, &IndDesc);
  if (!tc){
    errs()<<"trip count not found\n";
    return false;
  }
*/

/*
Value *findTripCount(Loop *L, InductionDescriptor *IndDesc){
  ConstantInt *step = IndDesc->getConstIntStepValue();
  if (!step){
    errs()<<"step is not const\n";
    return nullptr;
  }
  BasicBlock *exiting = L->getExitingBlock();
  if (!exiting){
    errs()<<"no, or multiple latches \n";
    return nullptr;
  }
  BasicBlock *header = L->getHeader();
  errs()<<"InductionBinOP = "<<IndDesc->getInductionBinOp()->getNameOrAsOperand()<<"\n";
  for (User *U : IndDesc->getInductionBinOp()->users()){ //for all users of induction variable
    if(ICmpInst *cmp = dyn_cast<ICmpInst>(U)){
      if(cmp->getParent() != exiting) continue; //looking for integer-comparisons in exiting block
      for (User *Ucmp : cmp->users()){
        if(BranchInst *br = dyn_cast<BranchInst>(Ucmp)){
          //if (br->isConditional() && ((br->get) || ()))
        }
      }
    }
  }
  return nullptr;
}*/

} //end of namespace

PreservedAnalyses SSRInferencePass::run(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR, LPMUpdater &){
  errs()<<"# =============== SSR Inference =============== #\n";
  if(!runOnLoop(&L, &AR.AA, &AR.LI, &AR.DT, AR.BFI, &AR.TLI, &AR.TTI, &AR.SE, AR.MSSA)){
    return PreservedAnalyses::all();
  }
  return PreservedAnalyses::none();
}

