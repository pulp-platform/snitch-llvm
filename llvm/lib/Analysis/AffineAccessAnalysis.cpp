#include "llvm/Analysis/AffineAccessAnalysis.h"

#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/InitializePasses.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/Dominators.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasAnalysisEvaluator.h"
#include "llvm/Analysis/AliasSetTracker.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/ilist.h"

#include <array>
#include <vector>
#include <iostream>

using namespace llvm;

AffineAcc::AffineAcc(const Loop *L, ArrayRef<Instruction *> instructions, const SCEV *data, 
  const SCEV *bound, const SCEV *stride) : data(data), L(L){
  this->instructions.append(instructions.begin(), instructions.end());
  this->bounds.push_back(bound);
  this->strides.push_back(stride);
  return;
}

void AffineAcc::addLoop(const Loop *L, const SCEV *bound, const SCEV *stride){
  this->L = L;
  this->bounds.push_back(bound);
  this->strides.push_back(stride);
  return;
}

unsigned AffineAcc::getDimension(){
  return this->bounds.size();
}

void AffineAcc::dump(){
  errs()<<"strd,bd of "; this->data->dump();
  for (unsigned i = 0; i < this->getDimension(); i++){
    this->strides[i]->dump();
    this->bounds[i]->dump();
  }
}

//================== AffineAcces, Result of Analysis =========================================
void AffineAccess::addAccess(AffineAcc &a){
  this->accesses.push_back(a);
  return;
}

ArrayRef<AffineAcc> AffineAccess::getAll(){
  ArrayRef<AffineAcc> ar(accesses.begin(), accesses.end());
  return ar; //copy
}

//================== Affine Acces Analysis ==================================================

namespace {

/// guarantees: 
/// L has 1 preheader and 1 dedicated exit
/// L has 1 backedge and 1 exiting block
/// bt SCEV can be expanded to instructions at insertionsPoint
bool checkLoop(const Loop *L, DominatorTree &DT, ScalarEvolution &SE, Instruction *InsertionPoint){
  if (!L->isLCSSAForm(DT) || !L->getLoopPreheader() || !L->getExitBlock() 
    || !L->getExitBlock() || !L->hasDedicatedExits() || L->getNumBackEdges() != 1){
      errs()<<"malformed loop: "; L->dump();
      return false;
  }
  if (!SE.hasLoopInvariantBackedgeTakenCount(L)){
    errs()<<"cannot calculate backedge taken count\n";
    return false;
  }
  const SCEV *bt = SE.getBackedgeTakenCount(L);
  if(!isSafeToExpandAt(bt, InsertionPoint, SE) /*|| !SE->isAvailableAtLoopEntry(bt, L)*/){
    errs()<<"cannot expand bt SCEV: "; bt->dump();
  }
  errs()<<"loop is well-formed: "; bt->dump();
  return true;
}

/// check whether BB is on all controlflow paths from header to header
bool isOnAllControlFlowPaths(const BasicBlock *BB, const Loop *L, const DominatorTree &DT){
  return DT.dominates(BB, L->getHeader());
}

AffineAccess &runOnFunction(Function &F, LoopInfo &LI, DominatorTree &DT, ScalarEvolution &SE, AAResults &AA){
  AffineAccess *aa = new AffineAccess(SE);

  for (const Loop *L : LI){
    errs()<<"loop: "; L->dump();

    if (!L->getLoopPreheader() || !checkLoop(L, DT, SE, L->getLoopPreheader()->getTerminator())) continue;

    for (const auto &BB : L->getBlocks()){
      //TODO: how to allow inner loops?
      if (!isOnAllControlFlowPaths(BB, L, DT)) continue;

      for (auto &I : *BB){
        Value *Addr;
        if (LoadInst *Load = dyn_cast<LoadInst>(&I)){
          Addr = Load->getPointerOperand();
        }else if (StoreInst *Store = dyn_cast<StoreInst>(&I)){
          Addr = Store->getPointerOperand();
        }else{
          continue; //cannot do anything with this instruction
        }

        const SCEV *AddrSCEV = SE.getSCEVAtScope(Addr, L);
        if (!SE.hasComputableLoopEvolution(AddrSCEV, L)) continue;
        errs()<<"has computable loop evolution: "; AddrSCEV->dump();

        auto split = SE.SplitIntoInitAndPostInc(L, AddrSCEV);
        const SCEV *SetupAddrSCEV = split.first;
        const SCEV *PostIncSCEV = split.second;
        if (!isSafeToExpandAt(SetupAddrSCEV, L->getLoopPreheader()->getTerminator(), SE)) continue;
        errs()<<"can expand setup addr scev in preheader: "; SetupAddrSCEV->dump();
        

      }
    }

  }
  
  return *aa;
}

} //end of namespace

AnalysisKey AffineAccessAnalysis::Key;

AffineAccess AffineAccessAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  
  errs()<<"running AffineAccessAnalysis on "<<F.getName()<<"\n";

  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
  AAResults &AA = FAM.getResult<AAManager>(F);

  AffineAccess aa = runOnFunction(F, LI, DT, SE, AA);  
  return aa;
}

//================== Affine Acces Analysis Pass for opt =======================================
PreservedAnalyses AffineAccessAnalysisPass::run(Function &F, FunctionAnalysisManager &FAM) {
  this->OS<<"enable debugging to see:\n";
  AffineAccess AA = FAM.getResult<AffineAccessAnalysis>(F);
  for (auto A : AA.getAll()){
    A.dump();
  }
  return PreservedAnalyses::all();
}