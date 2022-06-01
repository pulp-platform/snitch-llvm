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
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasAnalysisEvaluator.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/DependenceAnalysis.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"

#include <array>
#include <vector>
#include <iostream>
#include <utility>

using namespace llvm;

//================== AffineAcces, helper functions =========================================

namespace {

/// guarantees: 
/// L has 1 preheader and 1 dedicated exit
/// L has 1 backedge and 1 exiting block
/// bt SCEV can be expanded to instructions at insertionsPoint
bool checkLoop(const Loop *L, DominatorTree &DT, ScalarEvolution &SE){
  if (!L->isLCSSAForm(DT) 
    || !L->getLoopPreheader() 
    || !L->getExitingBlock() 
    || !L->getExitBlock() 
    || !L->hasDedicatedExits() 
    || L->getNumBackEdges() != 1) { 
    return false; 
  }
  if (!SE.hasLoopInvariantBackedgeTakenCount(L)){
    return false;
  }
  const SCEV *bt = SE.getBackedgeTakenCount(L);
  if(!isa<SCEVCouldNotCompute>(bt) || !SE.isAvailableAtLoopEntry(bt, L)){
    return false;
  }
  return true;
}

Optional<std::pair<const SCEV *, const SCEV *>> toSameType(const SCEV *LHS, const SCEV *RHS, ScalarEvolution &SE, bool unsafe = false){
  assert(LHS && RHS);
  using PT = std::pair<const SCEV *, const SCEV *>;

  const DataLayout &DL = SE.getDataLayout();
  LLVMContext &ctxt = SE.getContext();

  Type *LT = LHS->getType(), *RT = RHS->getType();
  if (LT == RT) 
    return Optional<PT>(std::make_pair(LHS, RHS)); //trivially the same size
  if (LT->isPointerTy() && RT->isPointerTy()) //if we have pointers to different types
    //PointerType *LTP = cast<PointerType>(LT); PointerType *RTP = cast<PointerType>(RT);
    return Optional<PT>(std::make_pair(
      SE.getPtrToIntExpr(LHS, Type::getIntNTy(ctxt, DL.getMaxPointerSizeInBits())), 
      SE.getPtrToIntExpr(RHS, Type::getIntNTy(ctxt, DL.getMaxPointerSizeInBits()))
    ));

  if (!LT->isSized() || !RT->isSized()) return None;
  if (DL.getTypeSizeInBits(LT).isScalable() || DL.getTypeSizeInBits(RT).isScalable()) return None;

  uint64_t ls = DL.getTypeSizeInBits(LT).getValue(), rs = DL.getTypeSizeInBits(RT).getValue();

  if (ls > rs) {
    if (auto LHSx = dyn_cast<SCEVConstant>(LHS)){
      if (LHSx->getAPInt().getActiveBits() <= rs) 
        return Optional<PT>(std::make_pair(SE.getConstant(RHS->getType(), LHSx->getAPInt().getLimitedValue()), RHS));
    } 
    if (auto RHSx = dyn_cast<SCEVConstant>(RHS)){
      return Optional<PT>(std::make_pair(LHS, SE.getConstant(LHS->getType(), RHSx->getAPInt().getLimitedValue())));
    }
    if (auto LHSx = dyn_cast<SCEVSignExtendExpr>(LHS)) return toSameType(LHSx->getOperand(0), RHS, SE);
    if (auto LHSx = dyn_cast<SCEVZeroExtendExpr>(LHS)) return toSameType(LHSx->getOperand(0), RHS, SE);
    if (auto RHSx = dyn_cast<SCEVTruncateExpr>(RHS)) return toSameType(LHS, RHSx->getOperand(0), SE);
    if (unsafe && LT->isIntegerTy() && RT->isIntegerTy()) return Optional<PT>(std::make_pair(SE.getTruncateExpr(LHS, RHS->getType()), RHS));
    return None;
  }else if (ls < rs){
    auto p = toSameType(RHS, LHS, SE); //swap
    if (!p.hasValue()) return None;
    return Optional<PT>(std::make_pair(p.getValue().second, p.getValue().first)); //swap back
  }
  if (unsafe) return Optional<PT>(std::make_pair(LHS, RHS));
  return None;
}

///checks whether LHS == RHS always holds
bool SCEVEquals(const SCEV *LHS, const SCEV *RHS, ScalarEvolution &SE){
  auto p = toSameType(LHS, RHS, SE);
  if (!p.hasValue()) return false;
  LHS = p.getValue().first;
  RHS = p.getValue().second;
  if (LHS == RHS) return true; //trivially the same if this holds (bc const Ptr)
  else{
    const SCEVPredicate *Peq = SE.getEqualPredicate(LHS, RHS);
    if (Peq->isAlwaysTrue()) return true; //if we arrive at setup addr scev, we are done
  }
  return false;
}

/// check whether BB is on all controlflow paths from header to header
// TODO: could also be done with DT
bool isOnAllControlFlowPaths(BasicBlock *BB, const Loop *L, const DominatorTree &DT){
  BasicBlock *End = L->getHeader();
  std::deque<std::pair<BasicBlock *, bool>> q;
  q.push_back(std::make_pair(End, false)); //start with header (false = BB not yet visited)
  std::set<std::pair<BasicBlock *, bool>> vis; //comp here is less<pair<BasicBlock *, bool>>
  while (!q.empty()){
    auto p = q.front(); q.pop_front();
    if (vis.find(p) != vis.end()) continue;
    vis.insert(p);
    for (BasicBlock *B : successors(p.first)){
      q.push_back(std::make_pair(B, p.second || B == BB));
    }
    //check here whether End is reached with false (not at start of loop bc we also start with End)
    p = q.front();
    if (!p.second && p.first == End) return false; //got to End (header) without ever visiting BB
  }
  return true;
}

//return result of Cmp predicated on Rep > 0 if possible.
Optional<bool> predicatedICmpOutcome(ICmpInst *Cmp, const SCEV *Rep, ScalarEvolution &SE){
  switch (Cmp->getPredicate())
  {
  case CmpInst::Predicate::ICMP_SGT:
  case CmpInst::Predicate::ICMP_UGT:
  {
    const SCEV *LHS = SE.getSCEV(Cmp->getOperand(0));
    const SCEV *RHS = SE.getSCEV(Cmp->getOperand(1));
    //transform: LHS > RHS <==> LHS - RHS > 0
    const SCEV *LHSmRHS = SE.getMinusSCEV(LHS, RHS);
    //then check whether Rep == LHS - RHS in which case we know: Rep > 0 ==> result of Cmp is true
    if (SCEVEquals(Rep, LHSmRHS, SE)) return Optional<bool>(true);
    else return None;
  }
  case CmpInst::Predicate::ICMP_SLT:
  case CmpInst::Predicate::ICMP_ULT:
  {
    //a < b <==> b > a
    const SCEV *LHS = SE.getSCEV(Cmp->getOperand(1)); //b
    const SCEV *RHS = SE.getSCEV(Cmp->getOperand(0)); //a
    //transform: LHS > RHS <==> LHS - RHS > 0
    const SCEV *LHSmRHS = SE.getMinusSCEV(LHS, RHS);
    //then check whether Rep == LHS - RHS in which case we know: Rep > 0 ==> result of Cmp is true
    if (SCEVEquals(Rep, LHSmRHS, SE)) return Optional<bool>(true);
    else return None;
  }
  case CmpInst::Predicate::ICMP_EQ:
  case CmpInst::Predicate::ICMP_NE:
  {
    //Rep > 0 ==> Rep + x != x
    const SCEV *LHS = SE.getSCEV(Cmp->getOperand(0)); //Rep + x (hopefully)
    const SCEV *RHS = SE.getSCEV(Cmp->getOperand(1)); //x 
    const SCEV *LHSmRHS = SE.getMinusSCEV(LHS, RHS);  //Rep (hopefully)
    if (SCEVEquals(Rep, LHSmRHS, SE)) return Optional<bool>(Cmp->getPredicate() == CmpInst::Predicate::ICMP_NE);
    else return None;
  }
  default:
    return None;
  }
}

//conservative! 
//because SCEVComparePredicate is not in this version of LLVM we have to do this manually ==> will not catch all cases (FIXME)
//predicate is that Rep > 0
bool isOnAllPredicatedControlFlowPaths(BasicBlock *BB, const Loop *L, const DominatorTree &DT, const SCEV *Rep, ScalarEvolution &SE){
  if (isOnAllControlFlowPaths(BB, L, DT)) return true; //is on all paths anyway
  Rep->dump();
  DenseSet<BasicBlock *> vis; //visited set
  std::deque<BasicBlock *> q(1U, L->getHeader()); //iterative BFS with queue
  while (!q.empty()){
    BasicBlock *Current = q.front(); q.pop_front();
    if (Current == BB) continue; //do not continue BFS from BB
    if (vis.find(Current) == vis.end()) continue; //already visited this block
    vis.insert(Current);

    Instruction *T = Current->getTerminator();
    T->dump();
    if (BranchInst *BR = dyn_cast<BranchInst>(T)){
      if (BR->isConditional()){
        if (ICmpInst *Cmp = dyn_cast<ICmpInst>(BR->getCondition())){ //FOR NOW: only works with a single ICmpInst as branch condition operand
          Cmp->dump();
          auto r = predicatedICmpOutcome(Cmp, Rep, SE);
          if (r.hasValue()){
            if (r.getValue()) q.push_back(BR->getSuccessor(0));
            else q.push_back(BR->getSuccessor(1));
          }else{
            q.push_back(BR->getSuccessor(0));
            q.push_back(BR->getSuccessor(1));
          }
        }
      }else{
        q.push_back(BR->getSuccessor(0)); //add the only successor to queue
      }
    }else{  
      return false; //unknown jump somewhere else ==> BB not on all predicated paths
    }

    if (q.front() == L->getHeader()) return false; //bfs arrived at Header (again) with a path that never went through BB
  }

  return true;
}

Value *castToSize(Value *R, Type *ty, Instruction *InsPoint){
  const DataLayout &DL = InsPoint->getParent()->getModule()->getDataLayout();
  IRBuilder<> builder(InsPoint);
  Type *rty = R->getType();
  if (rty == ty) return R;
  if (DL.getTypeSizeInBits(rty) > DL.getTypeSizeInBits(ty)) {
    return builder.CreateTruncOrBitCast(R, ty, "scev.trunc");
  }
  if (DL.getTypeSizeInBits(rty) < DL.getTypeSizeInBits(ty)) {
    return builder.CreateSExtOrBitCast(R, ty, "scev.sext");
  }
  return builder.CreateBitOrPointerCast(R, ty, "scev.cast");
}

} //end of namespace

//==================  ===========================================================

// ==== LoopRep ====
LoopRep::LoopRep(const Loop *L, ArrayRef<const Loop *> contLoops, ScalarEvolution &SE, DominatorTree &DT) 
  : L(L), containingLoops(contLoops.begin(), contLoops.end()), SE(SE), DT(DT), safeExpandBound(0u)
  {
  if (checkLoop(L, DT, SE)){
    const SCEV *R = SE.getBackedgeTakenCount(L);
    RepSCEV = isa<SCEVCouldNotCompute>(R) ? nullptr : R;
  }else{
    RepSCEV = nullptr;
  }
  
  if (RepSCEV){
    while (safeExpandBound < containingLoops.size() 
      && isSafeToExpandAt(RepSCEV, containingLoops[safeExpandBound]->getLoopPreheader()->getTerminator(), SE)) 
      safeExpandBound++;
  }
}

bool LoopRep::isAvailable() const { return RepSCEV != nullptr; }

const Loop *LoopRep::getLoop() const { return L; }

const SCEV *LoopRep::getSCEV() const { 
  assert(isAvailable() && "SCEV available"); //not necessary, but forces good practice
  return RepSCEV; 
}

const SCEV *LoopRep::getSCEVPlusOne() const {
  assert(isAvailable() && "SCEV available");
  return SE.getAddExpr(getSCEV(), SE.getConstant(getSCEV()->getType(), 1UL));
}

bool LoopRep::isOnAllCFPathsOfParentIfExecuted() const { //FIXME: maybe cache this result once calculated?
  assert(isAvailable() && "SCEV available");
  return isOnAllPredicatedControlFlowPaths(L->getHeader(), L->getParentLoop(), DT, getSCEVPlusOne(), SE);
}

bool LoopRep::isSafeToExpandBefore(const Loop *L) const {
  assert(isAvailable() && "SCEV available");
  if (L == getLoop()) return true;
  for (unsigned i = 0u; i < safeExpandBound; i++) { //FIXME: linear search -> use map instead
    if (L == containingLoops[i]) return true;
  }
  return false;
}

Value *LoopRep::expandAt(Type *ty, Instruction *InsertBefore){
  assert(ty);
  if (Rep) { //FIXME: currently forces user to call first expand at a point that dominates all possible uses (improvement: could update expand point using DT)
    assert(ty == Rep->getType() && "was already expanded with same type");
    return Rep;
  }
  InsertBefore = InsertBefore ? InsertBefore : L->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(RepSCEV, InsertBefore, SE) && "bound not expanable here");
  SCEVExpander ex(SE, L->getHeader()->getModule()->getDataLayout(), "rep");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(RepSCEV), ty, InsertBefore);
}

// ==== AffAcc ====
AffAcc::AffAcc(ArrayRef<Instruction *> accesses, const SCEV *Addr, MemoryUseOrDef *MA, ArrayRef<const Loop *> contLoops, ScalarEvolution &SE) 
  : accesses(accesses.begin(), accesses.end()), MA(MA), SE(SE)
{
  baseAddresses.push_back(Addr);
  steps.push_back((const SCEV *)nullptr); //there is no step for dim=0
  reps.push_back((LoopRep *)nullptr); //there is no rep for dim=0
  containingLoops.push_back((const Loop *)nullptr); //there is no loop for dim=0
  for (const Loop *L : contLoops) containingLoops.push_back(L);
  findSteps(Addr, (const SCEV *)nullptr, 1u);
  for (unsigned dim = 1; dim < containingLoops.size(); dim++){
    baseAddresses.push_back(SE.SplitIntoInitAndPostInc(containingLoops[dim], Addr).first);
  }
}

void AffAcc::findSteps(const SCEV *A, const SCEV *Factor, unsigned loop){
  assert(baseAddresses.size() == 1 && reps.size() == 1 && "we only know dim=0 so far");
  if (loop >= containingLoops.size() || !A) return;
  switch (A->getSCEVType())
  {
  //case SCEVTypes::scZeroExtend: FIXME: this is unsafe, right?
  case SCEVTypes::scSignExtend:
  case SCEVTypes::scTruncate:
    return findSteps(cast<SCEVCastExpr>(A)->getOperand(0), Factor, loop);
  case SCEVTypes::scAddExpr: {
    const SCEV *L = cast<SCEVAddExpr>(A)->getOperand(0);
    const SCEV *R = cast<SCEVAddExpr>(A)->getOperand(1);
    bool l = SE.containsAddRecurrence(L);
    bool r = SE.containsAddRecurrence(R);
    if (l && !r) return findSteps(L, Factor, loop);
    else if(!l && r) return findSteps(R, Factor, loop);
    return;
  }
  case SCEVTypes::scMulExpr: {
    const SCEV *L = cast<SCEVMulExpr>(A)->getOperand(0);
    const SCEV *R = cast<SCEVMulExpr>(A)->getOperand(1);
    bool l = SE.containsAddRecurrence(L);
    bool r = SE.containsAddRecurrence(R);
    if (l == r) return;
    if (!l && r) std::swap(L, R); 
    assert(SE.containsAddRecurrence(L) && !SE.containsAddRecurrence(R));
    if (Factor) {
      auto p = toSameType(Factor, R, SE, true);
      if (!p.hasValue()) return;
      Factor = SE.getMulExpr(p.getValue().first, p.getValue().second);
    }else Factor = R;
    return findSteps(L, Factor, loop);
  }
  case SCEVTypes::scAddRecExpr: {
    const auto *S = cast<SCEVAddRecExpr>(A);
    const SCEV *Step;
    if (S->getLoop() == containingLoops[loop]){
      auto p = toSameType(Factor, S->getStepRecurrence(SE), SE, true);
      if (!p.hasValue()) return;
      Step = SE.getMulExpr(p.getValue().first, p.getValue().second);
    }else{ //A is loop-invariant to containingLoops[loop]
      bool occursLater = false; //loop needs to occur later 
      for (unsigned i = loop+1; i < containingLoops.size(); i++) occursLater = occursLater || containingLoops[i] == S->getLoop();
      if (!occursLater) return; 
      Step = SE.getConstant(APInt(1u, 0UL, false));
    }
    steps.push_back(Step);
    return findSteps(S->getStart(), Factor, loop+1);
    
  }
  default:
    return;
  }
}

ArrayRef<Instruction *> AffAcc::getAccesses() const { return ArrayRef<Instruction *> (accesses.begin(), accesses.end()); }
bool AffAcc::isWrite() const { return isa<MemoryDef>(MA); }
unsigned AffAcc::getMaxDimension() const { return reps.size() - 1u; }
bool AffAcc::isWellFormed(unsigned dimension) const { return dimension <= getMaxDimension() && baseAddresses[0]; }
unsigned AffAcc::loopToDimension(const Loop *L) const {
  assert(L);
  for (unsigned d = 1u; d < containingLoops.size(); d++){ //FIXME: linear search -> improve with a map
    if (containingLoops[d] == L) return d;
  }
  llvm_unreachable("The provided loop does not contain `this`!");
}
bool AffAcc::canExpandBefore(const Loop *L) const { return isWellFormed(loopToDimension(L)); }
ConflictKind AffAcc::getConflictFor(const AffAcc *A, unsigned dimension) const { 
  auto r = conflicts.find(A);
  if (r == conflicts.end() || r->getSecond().first > dimension) return ConflictKind::None;
  return r->getSecond().second;
}
ConflictKind AffAcc::getConflictInLoop(const AffAcc *A, const Loop *L) const {
  return getConflictFor(A, loopToDimension(L) - 1u);
}
const SCEV *AffAcc::getBaseAddr(unsigned dim) const { assert(dim < baseAddresses.size()); return baseAddresses[dim]; }
const SCEV *AffAcc::getStep(unsigned dim) const { assert(dim < steps.size()); return steps[dim]; }
const SCEV *AffAcc::getRep(unsigned dim) const { assert(dim < reps.size()); return reps[dim]->getSCEV(); }
const Loop *AffAcc::getLoop(unsigned dim) const { assert(dim < containingLoops.size()); return containingLoops[dim]; }
void AffAcc::dump() const {
  errs()<<"Affine Access of \n";
  for (auto *I : accesses) errs()<<*I<<"\n";
  for (unsigned dim = 0u; dim <= getMaxDimension(); dim++){
    errs()<<"\tdim = "<<dim<<", step = "<<*getStep(dim)<<", rep = "<<*getRep(dim)<<"\n";
    errs()<<"\taddress = "<<*getBaseAddr(dim)<<"\n";
  }
}

MemoryAccess *AffAcc::getMemoryAccess() { return MA; }
void AffAcc::addConflict(const AffAcc *A, unsigned startDimension, ConflictKind kind){
  conflicts.insert(std::make_pair(A, std::make_pair(startDimension, kind)));
}
void AffAcc::addConflictInLoop(const AffAcc *A, const Loop *StartLoop, ConflictKind kind){
  return addConflict(A, loopToDimension(StartLoop) - 1u, kind);
}
bool AffAcc::promote(LoopRep *LR){
  unsigned newDim = getMaxDimension() + 1u;
  if (getLoop(newDim) != LR->getLoop()) return false;
  bool possible = true;
  Instruction *Point = LR->getLoop()->getLoopPreheader()->getTerminator();
  //check all current reps and steps
  for (unsigned dim = 1u; possible && dim < newDim; dim++){ 
    possible = possible && isSafeToExpandAt(getStep(dim), Point, SE);
    possible = possible && reps[dim]->isSafeToExpandBefore(LR->getLoop());
  }
  //check rep and step of new dimension
  possible &= steps.size() > newDim && isSafeToExpandAt(getStep(newDim), Point, SE);
  possible &= LR->isSafeToExpandBefore(LR->getLoop());
  //check base address
  possible &= isSafeToExpandAt(getBaseAddr(newDim), Point, SE);
  if (!possible) return false;

  reps.push_back(LR); //changes getMaxDimension()
  return true;
}
Value *AffAcc::expandBaseAddr(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension));
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(getBaseAddr(dimension), InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, reps[dimension]->getLoop()->getHeader()->getModule()->getDataLayout(), "addr");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(getBaseAddr(dimension)), ty, InsertBefore);
}
Value *AffAcc::expandStep(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension) && dimension > 0u);
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(getStep(dimension), InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, reps[dimension]->getLoop()->getHeader()->getModule()->getDataLayout(), "addr");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(getStep(dimension)), ty, InsertBefore);
}
Value *AffAcc::expandRep(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension) && dimension > 0u);
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(getRep(dimension), InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  return reps[dimension]->expandAt(ty, InsertBefore);
}

//================== Affine Access ===========================================================

AffineAccess::AffineAccess(Function &F, ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI, MemorySSA &MSSA, AAResults &AA) 
  : SE(SE), DT(DT), LI(LI), MSSA(MSSA), AA(AA){
  for (const Loop *L : LI.getTopLevelLoops()){
    std::vector<const Loop *> p;
    analyze(L, p);
    assert(p.empty());
  }

}

DenseSet<AffAcc *> AffineAccess::analyze(const Loop *Parent, std::vector<const Loop *> &loopPath){
  LoopRep *ParentLR = new LoopRep(Parent, ArrayRef<const Loop *>(loopPath), SE, DT);
  reps.insert(std::make_pair(Parent, ParentLR)); //add Parent to LoopReps
  loopPath.push_back(Parent); //add Parent to path
  expandableAccesses.insert(std::make_pair(Parent, SmallVector<AffAcc *, 4U>()));
  DenseSet<AffAcc *> all;

  for (const Loop *L : Parent->getSubLoops()){
    DenseSet<AffAcc *> accs = analyze(L, loopPath);
    LoopRep *LR = reps.find(L)->second; //guaranteed to exist, no check needed
    if (LR->isAvailable() && LR->isOnAllCFPathsOfParentIfExecuted()){ //L is well-formed and on all CF-paths if its rep is >0 at run-time
      for (AffAcc *A : accs){
        all.insert(A);
        if (ParentLR->isAvailable() && A->promote(ParentLR)){
          expandableAccesses.find(Parent)->getSecond().push_back(A); //guaranteed to exist
        }
      }
    }
  }

  std::vector<AffAcc *> toAdd;
  for (BasicBlock *BB : Parent->getBlocks()){
    for (Instruction &I : *BB){
      MemoryUseOrDef *MA = MSSA.getMemoryAccess(&I);
      AffAcc *A;
      if (MA && access.find(MA) == access.end()){ //no AffAcc for this memory access yet!
        if (isa<LoadInst>(I)){
          A = new AffAcc(ArrayRef<Instruction *>(&I), SE.getSCEV(cast<LoadInst>(I).getPointerOperand()), MA, ArrayRef<const Loop *>(loopPath), SE);
        } else if (isa<StoreInst>(I)) {
          A = new AffAcc(ArrayRef<Instruction *>(&I), SE.getSCEV(cast<StoreInst>(I).getPointerOperand()), MA, ArrayRef<const Loop *>(loopPath), SE);
        } else {
          //this is probably a call in the loop that modifies memory or sth like that
          A = new AffAcc(ArrayRef<Instruction *>(&I), nullptr, MA, ArrayRef<const Loop *>(loopPath), SE);
        }
        access.insert(std::make_pair(MA, A));
        toAdd.push_back(A);
        if (ParentLR->isAvailable()){
          bool onAllCFPaths = true;
          for (Instruction *I : A->getAccesses()) onAllCFPaths &= isOnAllControlFlowPaths(I->getParent(), Parent, DT);
          if (onAllCFPaths && A->promote(ParentLR)){
            expandableAccesses.find(Parent)->getSecond().push_back(A); //guaranteed to exist
          }
        }
      }
    }
  }

  for (AffAcc *A : toAdd){
    if (A->isWrite()) addConflictsForDef(A, Parent);
    else addConflictsForUse(A, Parent);
    all.insert(A);
  }

  assert(loopPath.back() == Parent);
  loopPath.pop_back(); //remove Parent again
  
  return std::move(all);
}

///we can prefetch a use before the loop iff its MemoryUse only depends on MemoryDefs that dominate the loop
///this adds conflicts between A and all MemoryDefs that stand in the way of that
void AffineAccess::addConflictsForUse(AffAcc *A, const Loop *L){
  Value *AAddr = getAddress(A->getAccesses()[0]);
  auto *W = MSSA.getSkipSelfWalker();
  std::deque<MemoryAccess *> worklist;
  worklist.push_back(W->getClobberingMemoryAccess(A->getMemoryAccess()));
  while (!worklist.empty()){
    MemoryAccess *C = worklist.front(); worklist.pop_front();
    if (!C) continue;
    if (isa<MemoryDef>(C)){
      MemoryDef *MA = cast<MemoryDef>(C);
      Value *MAAddr = getAddress(MA->getMemoryInst());
      if (L->contains(cast<MemoryDef>(MA)->getMemoryInst()) && (!AAddr || !MAAddr || AA.alias(AAddr, MAAddr))) { //we have a conflict inside loop
        auto p = access.find(cast<MemoryUseOrDef>(MA));
        assert(p != access.end() && "by this point all accesses in L should have an AffAcc!");
        AffAcc *O = p->second;
        //FIXME: should only consider cf-paths where the reps are > 0?
        if (!A->isWellFormed(A->loopToDimension(L)) || !O->isWellFormed(O->loopToDimension(L))){
          addConflict(A, O, L, ConflictKind::Bad); //not well formed ==> cannot generate intersection checks
        }else if (!MSSA.dominates(A->getMemoryAccess(), MA)){ //O might happen before A! 
          addConflict(A, O, L, ConflictKind::MustNotIntersect); //RaW
        } else { //A always happens before O
          bool sameBaseAddrSCEV = SCEVEquals(A->getBaseAddr(A->loopToDimension(L)), O->getBaseAddr(O->loopToDimension(L)), SE);
          if (accessPatternsMatch(A, O, L)){ 
            if (!sameBaseAddrSCEV){
              //TODO: use baseAddrSCEV to catch cases where they are for sure not the same
              addConflict(A, O, L, ConflictKind::MustBeSame); //WaR
            }
          } else {
            //if (sameBaseAddrSCEV) addConflict(A, O, L, ConflictKind::Bad); // this might not hold but might be useful 
            addConflict(A, O, L, ConflictKind::MustNotIntersect); //WaR
          }
        }
        worklist.push_back(W->getClobberingMemoryAccess(C));
        //aliasing is transitive and once an memory def is before loop it will not depend on other defs inside loop
        //so we only add more defs inside the `if`
      }
    } else if (isa<MemoryPhi>(C)){
      MemoryPhi *P = cast<MemoryPhi>(C);
      for (unsigned i = 0u; i < P->getNumOperands(); i++){
        worklist.push_back(P->getOperand(i)); //this adds MemoryDefs that do not alias, but will be be removed when pop-ed
      }
    }
  }
}

//we can delay a store up to after the loop if it is not redefined or used in the loop anymore
void AffineAccess::addConflictsForDef(AffAcc *A, const Loop *L){
  
}

void AffineAccess::addConflict(AffAcc *A, AffAcc *B, const Loop *L, ConflictKind kind){
  if (!A->canExpandBefore(L) || !B->canExpandBefore(L)) {
    kind = ConflictKind::Bad;
  }
  A->addConflictInLoop(B, L, kind);
  B->addConflictInLoop(A, L, kind);
}

bool AffineAccess::accessPatternsMatch(const AffAcc *A, const AffAcc *B, const Loop *L) const {
  unsigned dimA = A->loopToDimension(L);
  unsigned dimB = B->loopToDimension(L);
  if (dimA != dimB) return false;
  for (unsigned i = 1u; i <= dimA; i++){
    if (A->getLoop(i) != B->getLoop(i)) return false;
    if (!SCEVEquals(A->getRep(i), B->getRep(i), SE)) return false;
    if (!SCEVEquals(A->getStep(i), B->getStep(i), SE)) return false;
  }
  return true;
}

ScalarEvolution &AffineAccess::getSE() const { return this->SE; }

DominatorTree &AffineAccess::getDT()const { return this->DT; }

LoopInfo &AffineAccess::getLI() const { return this->LI; }

MemorySSA &AffineAccess::getMSSA() const { return this->MSSA; }

AAResults &AffineAccess::getAA() const { return this->AA; }

ArrayRef<const Loop *> AffineAccess::getLoopsInPreorder() const { return this->LI.getLoopsInPreorder(); }

Value *AffineAccess::getAddress(Instruction *I) {
  if (auto *L = dyn_cast<LoadInst>(I)) return L->getPointerOperand();
  if (auto *S = dyn_cast<StoreInst>(I)) return S->getPointerOperand();
  return nullptr;
}

ArrayRef<const AffAcc *> AffineAccess::getExpandableAccesses(const Loop *L) const {
  return ArrayRef<const AffAcc *>(expandableAccesses.find(L)->getSecond());
}

const AffAcc *AffineAccess::getAccess(Instruction *I) const {
  MemoryUseOrDef *MA = MSSA.getMemoryAccess(I);
  if (!MA) return nullptr;
  auto p = access.find(MA);
  if (p == access.end()) return nullptr;
  return p->second;
}

//================== Affine Access Analysis ==================================================

AnalysisKey AffineAccessAnalysis::Key;

AffineAccess AffineAccessAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  
  errs()<<"running AffineAccessAnalysis on "<<F.getName()<<"\n";

  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
  auto &MSSAA = FAM.getResult<MemorySSAAnalysis>(F);
  MemorySSA &MSSA = MSSAA.getMSSA();
  AAResults &AA = FAM.getResult<AAManager>(F);
  //DependenceInfo &DI = FAM.getResult<DependenceAnalysis>(F);
  
  return AffineAccess(F, SE, DT, LI, MSSA, AA);
}

//================== Affine Acces Analysis Pass for opt =======================================
PreservedAnalyses AffineAccessAnalysisPass::run(Function &F, FunctionAnalysisManager &FAM) {
  AffineAccess AA = FAM.getResult<AffineAccessAnalysis>(F);
  for (const Loop *L : AA.getLI().getLoopsInPreorder()){
    L->dump();
    for (const AffAcc *A : AA.getAccesses(L)){
      A->dump();
    }
  }
  return PreservedAnalyses::all();
}
