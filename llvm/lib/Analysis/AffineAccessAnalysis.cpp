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

AffineAcc::AffineAcc(const Loop *L, Instruction *Addr, ArrayRef<Instruction *> accesses,
    const SCEV *data, const SCEV *bound, const SCEV *stride) : data(data), Addr(Addr){
  for (Instruction *I : accesses) this->accesses.push_back(I);
  addLoop(L, bound, stride);
  return;
}

///note: stride can be 0 if access is inv w.r.t. this loop
void AffineAcc::addLoop(const Loop *L, const SCEV *bound, const SCEV *stride){
  this->L = L;
  this->bounds.push_back(bound);
  this->strides.push_back(stride);
  return;
}

unsigned AffineAcc::getDimension() const{
  return this->bounds.size();
}

void AffineAcc::dump() const{
  errs()<<"Affine Access in Loop:\n";
  L->dump();
  errs()<<"With Addr instruction: "; Addr->dump();
  errs()<<"And the following load/store instructions:\n";
  for (Instruction *I : accesses){
    I->dump();
  }
  errs()<<"data pointer: "; data->dump();
  for (unsigned i = 0; i < this->getDimension(); i++){
    errs()<<"dim "<<(i+1)<<" stride: "; strides[i]->dump();
    errs()<<"dim "<<(i+1)<<" bound:  "; bounds[i]->dump();
  }
}

Instruction *AffineAcc::getAddrIns() const{
  return Addr;
}

const Loop *AffineAcc::getLoop() const{
  return L;
}

const SmallVector<Instruction *, 2U> &AffineAcc::getAccesses() const{
  return this->accesses;
}

//================== AffineAcces, Result of Analysis =========================================
void AffineAccess::addAccess(const AffineAcc *a){
  this->accesses.push_back(a);
  return;
}

ArrayRef<const AffineAcc *> AffineAccess::getAll() const{
  ArrayRef<const AffineAcc *> *ar = new ArrayRef<const AffineAcc *>(accesses.begin(), accesses.end());
  return *ar; 
}

  Value *AffineAccess::expandData(const AffineAcc *aa, Type *ty) const{
    SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "data");
    ex.setInsertPoint(aa->L->getLoopPreheader()->getTerminator());
    return ex.expandCodeFor(aa->data, ty);
  }

  Value *AffineAccess::expandBound(const AffineAcc *aa, unsigned i, Type *ty) const{
    SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "bound");
    ex.setInsertPoint(aa->L->getLoopPreheader()->getTerminator());
    return ex.expandCodeFor(aa->bounds[i], ty);
  }

  Value *AffineAccess::expandStride(const AffineAcc *aa, unsigned i, Type *ty) const{
    SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "stride");
    ex.setInsertPoint(aa->L->getLoopPreheader()->getTerminator());
    return ex.expandCodeFor(aa->strides[i], ty);
  }

//================== Affine Acces Analysis ==================================================

namespace {

/// guarantees: 
/// L has 1 preheader and 1 dedicated exit
/// L has 1 backedge and 1 exiting block
/// bt SCEV can be expanded to instructions at insertionsPoint
bool checkLoop(const Loop *L, DominatorTree &DT, ScalarEvolution &SE, Instruction *InsertionPoint){
  if (!L->isLCSSAForm(DT)) { errs()<<"not LCSSA\n"; return false; }
  if (!L->getLoopPreheader()) { errs()<<"no preheader\n"; return false; }
  if (!L->getExitBlock()) { errs()<<"nr. exit blocks != 1\n"; return false; }
  if (!L->hasDedicatedExits()) { errs()<<"exit is not dedicated\n"; return false; }
  if (L->getNumBackEdges() != 1) { errs()<<"nr. back-edges != 1\n"; return false; }

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

/// Fold over AddrSCEV
/// All AddRecSCEVs are dependent on L or loops contained in L (TODO: and on all paths?)
/// All steps in ADDRecSCEVs can be calculated in preheader of L
bool canFindStrides(ScalarEvolution &SE, const ArrayRef<const Loop *> &loops, const SCEV *AddrSCEV, const SCEV *SetupAddrSCEV){
  errs()<<"finding strides in: "; AddrSCEV->dump();
  if (AddrSCEV == SetupAddrSCEV) return true; //trivially the same if this holds (bc const Ptr)
  else{
    const SCEVPredicate *Peq = SE.getEqualPredicate(AddrSCEV, SetupAddrSCEV);
    if (Peq->isAlwaysTrue()) return true; //if we arrive at setup addr scev, we are done
  }
  
  if (loops.empty()) { errs()<<"not enough loops\n"; return false; } //need at least one more loop here for SCEVAddRecvExpr

  if (const auto *AR = dyn_cast<SCEVAddRecExpr>(AddrSCEV)){
    auto L = loops.begin();
    while (L != loops.end() && AR->getLoop() != *L) ++L; //do header comparison instead?
    if (L == loops.end()) { errs()<<"loops of addRecExpr not found\n"; return false; }

    const SCEV *Stride = AR->getStepRecurrence(SE);
    const SCEV *Rest = AR->getStart();
    if (isSafeToExpandAt(Stride, (*L)->getLoopPreheader()->getTerminator(), SE)) { //if we can expand stride at loop entry
      errs()<<"can expand stride: "; Stride->dump();
      return canFindStrides(SE, ArrayRef<const Loop *>(++L, loops.end()), Rest, SetupAddrSCEV); //check Rest recursively
    }
  }
  return false;
}

SmallVector<const Loop *, 3U> &getContainingLoops(const Loop *Outermost, Instruction *Ins){
  BasicBlock *BB = Ins->getParent();
  const auto &loops = Outermost->getLoopsInPreorder();
  SmallVector<const Loop *, 3U> *r = new SmallVector<const Loop *, 3U>();
  for (auto L = loops.rbegin(); L != loops.rend(); ++L){ //go through loops in reverse order ==> innermost first
    if ((*L)->contains(BB)){
      errs()<<"found containing Loop\n";
      r->push_back(*L);
    }
  }
  return *r;
}

AffineAccess &runOnFunction(Function &F, LoopInfo &LI, DominatorTree &DT, ScalarEvolution &SE, AAResults &AA){
  AffineAccess *aa = new AffineAccess(SE);

  auto loops = LI.getLoopsInPreorder();
  errs()<<"contains "<<loops.size()<<" loops\n";

  for (const Loop *L : loops){
    errs()<<"LOOP:\n";
    L->dump();

    if (!L->isInnermost()) continue; //for now

    if (!L->getLoopPreheader()) { errs()<<"loop has no preheader\n"; continue; }
    if (!checkLoop(L, DT, SE, L->getLoopPreheader()->getTerminator())) continue;

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
        Instruction *AddrIns;
        if (!(AddrIns = dyn_cast<Instruction>(Addr))) continue; //if Addr is not instruction ==> constant, or sth else (==> leave for other passes to opt)
        errs()<<"looking at: "; AddrIns->dump();

        const SCEV *AddrSCEV = SE.getSCEV(Addr);
        if (!SE.hasComputableLoopEvolution(AddrSCEV, L)) continue;
        errs()<<"has computable loop evolution: "; AddrSCEV->dump();

        auto split = SE.SplitIntoInitAndPostInc(L, AddrSCEV);
        const SCEV *SetupAddrSCEV = split.first; //const SCEV *PostIncSCEV = split.second;
        if (!isSafeToExpandAt(SetupAddrSCEV, L->getLoopPreheader()->getTerminator(), SE)) continue;
        errs()<<"can expand setup addr scev in preheader: "; SetupAddrSCEV->dump();
        
        const auto &loops = getContainingLoops(L, AddrIns);
        if (!canFindStrides(SE, ArrayRef<const Loop *>(loops.begin(), loops.end()), AddrSCEV, SetupAddrSCEV)) continue;
        errs()<<"can find loop Strides: "; AddrSCEV->dump();

        const SCEV *TC = SE.getBackedgeTakenCount(L);
        const auto *AddrRecSCEV = cast<SCEVAddRecExpr>(AddrSCEV);
        const SCEV *Str;
        if (AddrRecSCEV->getLoop() == L){
          Str = cast<SCEVAddRecExpr>(AddrSCEV)->getStepRecurrence(SE); //because 1D for now
        }else{
          Str = SE.getConstant(APInt(64U, 0U));
        }

        std::vector<Instruction *> accesses;
        for (auto U = Addr->use_begin(); U != Addr->use_end(); ++U){
          Instruction *Acc = dyn_cast<LoadInst>(U->getUser());
          if (!Acc) Acc = dyn_cast<StoreInst>(U->getUser());
          
          if (!Acc) continue; //both casts failed ==> not a suitable instruction
          if (!isOnAllControlFlowPaths(Acc->getParent(), L, DT)) continue; //access does not occur consitently in loop ==> not suitable

          accesses.push_back(Acc);
        }

        aa->addAccess(new AffineAcc(L, AddrIns, ArrayRef<Instruction *>(accesses), SetupAddrSCEV, TC, Str));
        errs()<<"added new AffineAcc\n";
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
  AffineAccess AA = FAM.getResult<AffineAccessAnalysis>(F);
  for (const auto *A : AA.getAll()){
    A->dump();
  }
  return PreservedAnalyses::all();
}