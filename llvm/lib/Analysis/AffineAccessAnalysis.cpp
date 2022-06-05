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
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"

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

struct SCEVUknownSetFinder {
  DenseSet<Value *> values;
  // return true to follow this node.
  bool follow(const SCEV *S) {
    if (S->getSCEVType() == SCEVTypes::scUnknown) {
      values.insert(cast<SCEVUnknown>(S)->getValue());
    }
    return true; //always true
  }
  // return true to terminate the search.
  bool isDone() { return false; /*continue forever*/ }
};

bool shareValues(const SCEV *A, const SCEV *B) {
  SCEVUknownSetFinder finderA;
  SCEVTraversal<SCEVUknownSetFinder> trA(finderA);
  trA.visitAll(A);
  SCEVUknownSetFinder finderB;
  SCEVTraversal<SCEVUknownSetFinder> trB(finderB);
  trB.visitAll(B);
  bool shareValues = false;
  for (Value *V : finderA.values) {
    for (Value *W : finderB.values) {
      shareValues |= V == W;
    }
  }
  return shareValues;
}

bool SCEVContainsCouldNotCompute(const SCEV *S) {
  auto pred = [](const SCEV *X) { return !X || X->getSCEVType() == SCEVTypes::scCouldNotCompute || isa<SCEVCouldNotCompute>(X); };
  return SCEVExprContains(S, std::move(pred));
}

/// guarantees: 
/// L has 1 preheader and 1 dedicated exit
/// L has 1 backedge and 1 exiting block
/// bt SCEV can be expanded to instructions at insertionsPoint
const SCEV *getLoopBTSCEV(const Loop *L, DominatorTree &DT, ScalarEvolution &SE){
  if (!L->isLCSSAForm(DT)
    || !L->getLoopPreheader() 
    || !L->getExitingBlock() 
    || !L->getExitBlock() 
    || !L->hasDedicatedExits() 
    || L->getNumBackEdges() != 1) { 
    return nullptr; 
  }
  if (!SE.hasLoopInvariantBackedgeTakenCount(L)){
    return nullptr;
  }
  const SCEV *bt = SE.getBackedgeTakenCount(L);
  if(!bt || isa<SCEVCouldNotCompute>(bt) || !SE.isAvailableAtLoopEntry(bt, L) || SCEVContainsCouldNotCompute(bt)){
    return nullptr;
  }
  return bt;
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

Value *getAddress(MemoryUseOrDef *MA) {
  assert(MA && "called getAddress on nullptr");
  assert(MA->getMemoryInst());
  Instruction *I = MA->getMemoryInst();
  if (auto *L = dyn_cast<LoadInst>(I)) return L->getPointerOperand();
  if (auto *S = dyn_cast<StoreInst>(I)) return S->getPointerOperand();
  return nullptr;
}

const Loop *findFirstContaining(ArrayRef<const Loop *> loops, BasicBlock *BB){
  for (const Loop *L : loops) {
    if (L && L->contains(BB)) {
      return L;
    }
  }
  return nullptr;
}

Value *goodAnd(IRBuilder<> &builder, ArrayRef<Value *> bools){
  assert(!bools.empty());
  std::vector<Value *> b1, b2;
  for (Value *b : bools) b1.push_back(b);
  while (b1.size() > 1u) {
    unsigned i = 0u;
    for (; i+1 < b1.size(); i += 2) {
      b2.push_back(builder.CreateAnd(b1[i], b1[i+1], "and.tree"));
    }
    if (i < b1.size()) b2.push_back(b1[i]); //add last element if odd nr in b1
    std::swap(b1, b2);
    b2.clear();
  }
  return b1[0]; //return the last value
}

bool hasMemInst(MemoryUseOrDef *MA) { return MA && MA->getMemoryInst(); }

} //end of namespace

//==================  ===========================================================

// ==== LoopRep ====
LoopRep::LoopRep(const Loop *L, ArrayRef<const Loop *> contLoops, ScalarEvolution &SE, DominatorTree &DT) 
  : SE(SE), DT(DT), L(L), containingLoops(contLoops.begin(), contLoops.end()), safeExpandBound(0u)
  {
  RepSCEV = getLoopBTSCEV(L, DT, SE);
  if (RepSCEV) errs()<<"new LoopRep with rep scev: "<<*RepSCEV<<"\n";
  else errs()<<"new LoopRep with rep scev: <nullptr> \n";
  
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
  assert(RepSCEV);
  if (Rep) { //FIXME: currently forces user to call first expand at a point that dominates all possible uses (improvement: could update expand point using DT)
    assert(ty == Rep->getType() && "was already expanded with same type");
    return Rep;
  }
  InsertBefore = InsertBefore ? InsertBefore : L->getLoopPreheader()->getTerminator();
  const SCEV *RepPlusOne = getSCEVPlusOne();
  assert(isSafeToExpandAt(RepPlusOne, InsertBefore, SE) && "bound not expanable here");
  SCEVExpander ex(SE, L->getHeader()->getModule()->getDataLayout(), "rep");
  ex.setInsertPoint(InsertBefore);
  Rep = castToSize(ex.expandCodeFor(RepPlusOne), ty, InsertBefore);
  return Rep;
}

Value *LoopRep::expandLoopGuard(Instruction *InsertBefore) {
  assert(Rep && "expandAt has to be called before this");
  InsertBefore = InsertBefore ? InsertBefore : L->getLoopPreheader()->getTerminator();
  IRBuilder<> builder(InsertBefore);
  return builder.CreateICmpSGT(Rep, ConstantInt::get(Rep->getType(), 0u, true)); //FIXME: this only works for unsigned Rep's that are < 2^30 (for i32)
}

// ==== AffAcc ====
AffAcc::AffAcc(ArrayRef<Instruction *> accesses, const SCEV *Addr, MemoryUseOrDef *MA, ArrayRef<const Loop *> contLoops, ScalarEvolution &SE) 
  : SE(SE), MA(MA), accesses(accesses.begin(), accesses.end())
{
  assert(!accesses.empty());
  assert(MA); 
  if (Addr && SCEVContainsCouldNotCompute(Addr)) Addr = nullptr; //set to null if contains SCEVCouldNotCompute
  baseAddresses.push_back(Addr);
  steps.push_back((const SCEV *)nullptr); //there is no step for dim=0
  reps.push_back((LoopRep *)nullptr); //there is no rep for dim=0
  containingLoops.push_back((const Loop *)nullptr); //there is no loop for dim=0
  containingLoops.append(contLoops.begin(), contLoops.end());
  if (!Addr) return; //do not look for steps or addresses if SCEV of address is unknown
  findSteps(Addr, (const SCEV *)nullptr, 1u); //find steps
  for (unsigned dim = 1; dim < containingLoops.size(); dim++){
    Addr = SE.SplitIntoInitAndPostInc(containingLoops[dim], Addr).first;
    baseAddresses.push_back(Addr);
  }
}

void AffAcc::findSteps(const SCEV *A, const SCEV *Factor, unsigned loop){
  assert(A);
  assert(baseAddresses.size() == 1 && reps.size() == 1 && "we only know dim=0 so far");
  if (loop >= containingLoops.size()) return;  
  if (!SE.containsAddRecurrence(A) && loop < containingLoops.size()){ //A is inv to the rest of the loops
    steps.push_back(SE.getConstant(Type::getInt64Ty(this->accesses[0]->getContext()), 0U));
    findSteps(A, Factor, loop + 1u);
  }
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
      Step = S->getStepRecurrence(SE);
      if (Factor) {
        auto p = toSameType(Factor, Step, SE, true);
        if (!p.hasValue()) return;
        Step = SE.getMulExpr(p.getValue().first, p.getValue().second);
      }
      steps.push_back(Step);
      return findSteps(S->getStart(), Factor, loop+1);
    }else{ //A is loop-invariant to containingLoops[loop]
      bool occursLater = false; //loop needs to occur later 
      for (unsigned i = loop+1; i < containingLoops.size(); i++) occursLater = occursLater || containingLoops[i] == S->getLoop();
      if (!occursLater) return; 
      steps.push_back(SE.getConstant(Type::getInt64Ty(this->accesses[0]->getContext()), 0U));
      return findSteps(S, Factor, loop+1);
    }    
  }
  default:
    return;
  }
}

ArrayRef<Instruction *> AffAcc::getAccesses() const { return accesses; }
bool AffAcc::isWrite() const { return isa<MemoryDef>(MA); }
unsigned AffAcc::getMaxDimension() const { return reps.size() - 1u; }
bool AffAcc::isWellFormed(unsigned dimension) const { return dimension <= getMaxDimension() && baseAddresses[0]; }
bool AffAcc::isWellFormed(const Loop *L) const { return isWellFormed(loopToDimension(L)); }
unsigned AffAcc::loopToDimension(const Loop *L) const {
  assert(L && "L not nullptr");
  for (unsigned d = 1u; d < containingLoops.size(); d++){ //FIXME: linear search -> improve with a map
    if (containingLoops[d] == L) return d;
  }
  llvm_unreachable("The provided loop does not contain `this`!");
}
bool AffAcc::canExpandBefore(const Loop *L) const { return isWellFormed(loopToDimension(L)); }
const SCEV *AffAcc::getBaseAddr(unsigned dim) const { assert(dim < baseAddresses.size()); return baseAddresses[dim]; }
const SCEV *AffAcc::getStep(unsigned dim) const { assert(dim < steps.size()); return steps[dim]; }
const SCEV *AffAcc::getRep(unsigned dim) const { 
  assert(dim < reps.size()); 
  if (!reps[dim] || !reps[dim]->isAvailable()) return nullptr;
  return reps[dim]->getSCEV(); 
}
const Loop *AffAcc::getLoop(unsigned dim) const { assert(dim < containingLoops.size()); return containingLoops[dim]; }
ArrayRef<const Loop *> AffAcc::getContainingLoops() const { return ArrayRef<const Loop *>(containingLoops); }
void AffAcc::dumpInLoop(const Loop *L) const {
  errs()<<"Affine Access of \n";
  unsigned dimension;
  if (L) dimension = loopToDimension(L);
  else dimension = getMaxDimension();
  for (auto *I : accesses) errs()<<*I<<"\n";
  for (unsigned dim = 0u; dim <= dimension  && dim <= getMaxDimension(); dim++){
    const SCEV *s = getStep(dim);
    const SCEV *r = getRep(dim);
    const SCEV *a = getBaseAddr(dim);
    errs()<<"\tdim = "<<dim<<", step = "; 
    if (s) errs()<<*s;
    else errs()<<"<nullptr>"; 
    errs()<<", rep = ";
    if (r) errs()<<*r;
    else errs()<<"<nullptr>";
    errs()<<", well-formed = "<<this->isWellFormed(dim);
    errs()<<"\n";
    errs()<<"\taddress = ";
    if (a) errs()<<*a;
    else errs()<<"<nullptr>";
    errs()<<"\n";
    errs()<<"\tloop header = ";
    if (getLoop(dim)) errs()<<getLoop(dim)->getHeader()->getNameOrAsOperand();
    else errs()<<"<nullptr>";
    errs()<<"\n";
  }
}
void AffAcc::dump() const { dumpInLoop(nullptr); }
AffAccConflict AffAcc::getConflict(AffAcc *A, const Loop *L) const {
  auto p = conflicts.find(A);
  if (p != conflicts.end()) {
    const Loop *S = p->getSecond().first;
    if (S == L || L->contains(S)) { //if start is L or more "inner" loop
      if (!isWellFormed(L) || !A->isWellFormed(L)) return AffAccConflict::Bad; //if either is not well-formed "demote" the conflict to bad (but only if exists)
      return p->getSecond().second;
    }
  }
  return AffAccConflict::NoConflict;
}

MemoryUseOrDef *AffAcc::getMemoryAccess() { return MA; }
void AffAcc::addConflict(AffAcc *A, const Loop *StartL, AffAccConflict kind){
  assert(conflicts.find(A) == conflicts.end() && "no conflict for A yet");
  assert(kind == AffAccConflict::Bad || (isWellFormed(StartL) && A->isWellFormed(StartL)));
  conflicts.insert(std::make_pair(A, std::make_pair(StartL, kind)));
  /*errs()<<"conflict for:\n"; dumpInLoop(StartL); errs()<<"with:\n"; A->dumpInLoop(StartL); errs()<<"is ===========>";
  switch (kind)
  {
  case AffAccConflict::Bad:
    errs()<<"Bad";
    break;
  case AffAccConflict::MustNotIntersect:
    errs()<<"MustNotIntersect";
    break;
  case AffAccConflict::NoConflict:
    errs()<<"NoConflict";
    break;
  default:
    break;
  }
  errs()<<"\n"; */
}
bool AffAcc::promote(LoopRep *LR){
  if (!LR->isAvailable()) return false;
  unsigned newDim = getMaxDimension() + 1u;
  if (!isWellFormed(getMaxDimension())) return false;
  if (getLoop(newDim) != LR->getLoop()) return false;
  errs()<<"promote: (1) loops match, ";
  bool possible = true;
  Instruction *Point = LR->getLoop()->getLoopPreheader()->getTerminator();
  //check all current reps and steps
  for (unsigned dim = 1u; possible && dim < newDim; dim++){ 
    possible = possible && isSafeToExpandAt(getStep(dim), Point, SE);
    possible = possible && reps[dim]->isSafeToExpandBefore(LR->getLoop());
  }
  if (possible) errs()<<"(2) current rep & step can be expanded, ";
  //check rep and step of new dimension
  possible &= steps.size() > newDim && isSafeToExpandAt(getStep(newDim), Point, SE);
  possible &= LR->isSafeToExpandBefore(LR->getLoop());
  if (possible) errs()<<"(3) new rep & step can be expanded, ";
  //check base address
  possible &= !SCEVContainsCouldNotCompute(getBaseAddr(newDim)) && isSafeToExpandAt(getBaseAddr(newDim), Point, SE);
  if (possible) errs()<<"(4) new base addr can be expanded";
  errs()<<"\n";
  if (!possible) return false;

  reps.push_back(LR); //changes getMaxDimension()
  return true;
}

Value *AffAcc::expandBaseAddr(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension));
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  if (!isSafeToExpandAt(getBaseAddr(dimension), InsertBefore, SE)){
    errs()<<"data not expanable here (note: only preheader guaranteed)\n";
    errs()<<"SCEV (dim = "<<dimension<<")= "<<*getBaseAddr(dimension)<<"\n";
    errs()<<"in block:\n"; InsertBefore->getParent()->dump();
    errs()<<"before inst: "<<*InsertBefore<<"\n";
    this->dump();
    llvm_unreachable("cannot expand SCEV at desired location");
  }
  SCEVExpander ex(SE, reps[dimension]->getLoop()->getHeader()->getModule()->getDataLayout(), "addr");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(getBaseAddr(dimension)), ty, InsertBefore);
}
Value *AffAcc::expandStep(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension) && dimension > 0u);
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(getStep(dimension), InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, reps[dimension]->getLoop()->getHeader()->getModule()->getDataLayout(), "step");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(getStep(dimension)), ty, InsertBefore);
}
Value *AffAcc::expandRep(unsigned dimension, Type *ty, Instruction *InsertBefore){
  assert(isWellFormed(dimension) && dimension > 0u);
  InsertBefore = InsertBefore ? InsertBefore : reps[dimension]->getLoop()->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(getRep(dimension), InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  return reps[dimension]->expandAt(ty, InsertBefore);
}
ExpandedAffAcc AffAcc::expandAt(const Loop *L, Instruction *Point, 
  Type *PtrTy, IntegerType *ParamTy, IntegerType *AgParamTy) 
{
  errs()<<"expanding for Loop with header: "<<L->getHeader()->getNameOrAsOperand()<<"\n";
  if (!Point) Point = L->getLoopPreheader()->getTerminator();
  IRBuilder<> builder(Point);
  assert(isWellFormed(L));
  std::vector<Value *> reps, steps, ranges, prefixsum_ranges;
  unsigned dim = loopToDimension(L);
  Value *Addr = expandBaseAddr(dim, PtrTy, Point);
  Value *psum = nullptr;
  for (unsigned i = 1u; i <= dim; i++) {
    reps.push_back(expandRep(i, ParamTy, Point));
    steps.push_back(expandStep(i, ParamTy, Point));
    Value *r = reps.back();
    Value *st = steps.back();
    ranges.push_back(builder.CreateMul(r, st, formatv("range.{0}d", i)));
    if (psum) psum = builder.CreateAdd(psum, ranges.back(), formatv("prefsum.range.{0}d", i));
    else psum = ranges.back();
    prefixsum_ranges.push_back(psum);
  }
  Value *LowerBound = builder.CreatePtrToInt(Addr, AgParamTy, "lb");
  Value *r = builder.CreateZExtOrTrunc(prefixsum_ranges.back(), AgParamTy, "prefsum.cast");
  Value *UpperBound = builder.CreateAdd(LowerBound, r, "ub");
  ExpandedAffAcc Aexp(this, Addr, steps, reps, ranges, prefixsum_ranges, LowerBound, UpperBound);
  return Aexp;
}

// ================= MemDep ==============

bool MemDep::alias(Value *A, Value *B) { return !A || !B || AA.alias(A, B) != AliasResult::NoAlias; }
bool MemDep::alias(MemoryUseOrDef *A, MemoryUseOrDef *B) { 
  if (!hasMemInst(A) || !hasMemInst(B)) return false; //the memoryUseOrDef does not correspond to an instruction => no problem
  else return alias(getAddress(A), getAddress(B)); 
}

DenseSet<MemoryDef *> MemDep::findClobbers(MemoryUseOrDef *MA){
  DenseSet<MemoryDef *> res;
  std::deque<MemoryAccess *> worklist;
  DenseSet<MemoryAccess *> vis;
  worklist.push_back(MA->getDefiningAccess());
  while (!worklist.empty()) {
    MemoryAccess *A = worklist.front(); worklist.pop_front();
    if (!A) continue;
    if (vis.find(A) != vis.end()) continue;
    if (A == MA) continue;
    vis.insert(A);
    if (MemoryDef *D = dyn_cast<MemoryDef>(A)) {
      if (alias(D, MA)) {
        res.insert(D);
      }
      worklist.push_back(D);
    } else {
      MemoryPhi *P = cast<MemoryPhi>(A);
      for (unsigned i = 0u; i < P->getNumOperands(); i++) {
        worklist.push_back(P->getOperand(i));
      }
    }
  }
  return res;
}

DenseSet<MemoryUseOrDef *> MemDep::findClobberUsers(MemoryDef *MA) {
  DenseSet<MemoryUseOrDef *> res;
  std::deque<MemoryAccess *> worklist;
  DenseSet<MemoryAccess *> vis;
  for (auto U = MA->use_begin(); U != MA->use_end(); ++U) {
    worklist.push_back(cast<MemoryAccess>(U->getUser()));
  }
  while (!worklist.empty()){
    MemoryAccess *A = worklist.front(); worklist.pop_front();
    if (!A) continue;
    if (vis.find(A) != vis.end()) continue;
    vis.insert(A);
    if (MemoryUse *U = dyn_cast<MemoryUse>(A)) {
      if (alias(U, MA)) res.insert(U);
    } else if (MemoryDef *D = dyn_cast<MemoryDef>(A)) {
      if (alias(D, MA)) {
        res.insert(D);
      }
      worklist.push_back(D);
    } else {
      assert(isa<MemoryPhi>(A));
      for (auto U = A->use_begin(); U != A->use_end(); ++U) {
        worklist.push_back(cast<MemoryAccess>(U->getUser()));
      }
    }
  }
  return res;
}

//================== Affine Access ===========================================================

AffineAccess::AffineAccess(Function &F, ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI, MemorySSA &MSSA, AAResults &AA) 
  : SE(SE), DT(DT), LI(LI), MSSA(MSSA), AA(AA), MD(MSSA, AA){
  for (const Loop *L : LI.getTopLevelLoops()){
    std::vector<AffAcc *> all = analyze(L, ArrayRef<const Loop *>());
    addAllConflicts(all);
  }
}

std::vector<AffAcc *> AffineAccess::analyze(const Loop *Parent, ArrayRef<const Loop *> loopPath){
  errs()<<"analyze: loop          : "<<Parent->getHeader()->getNameOrAsOperand()<<"\n";
  LoopRep *ParentLR = new LoopRep(Parent, loopPath, SE, DT);
  reps.insert(std::make_pair(Parent, ParentLR)); //add Parent to LoopReps
  std::vector<const Loop *> path; 
  path.push_back(Parent); //add Parent to path
  for (auto *L : loopPath) path.push_back(L);
  wellformedAccesses.insert(std::make_pair(Parent, SmallVector<AffAcc *, 4U>()));
  std::vector<AffAcc *> all;

  for (const Loop *L : Parent->getSubLoops()){
    std::vector<AffAcc *> accs = analyze(L, ArrayRef<const Loop *>(path));
    LoopRep *LR = reps.find(L)->second; //guaranteed to exist, no check needed
    bool canPromote = LR->isAvailable() && ParentLR->isAvailable() && LR->isOnAllCFPathsOfParentIfExecuted();
    for (AffAcc *A : accs){
      all.push_back(A);
      if (canPromote){ //L is well-formed and on all CF-paths if its rep is >0 at run-time
        auto &l = wellformedAccesses.find(Parent)->getSecond();
        if (A->promote(ParentLR)){
          l.push_back(A); //guaranteed to exist
        }
      }
    }
  }

  for (BasicBlock *BB : Parent->getBlocks()){
    for (Instruction &I : *BB){
      MemoryUseOrDef *MA = MSSA.getMemoryAccess(&I);
      if (MA && hasMemInst(MA) && access.find(MA) == access.end()){ //no AffAcc for this memory access yet!
        Value *Addr = getAddress(MA);
        const SCEV *AddrSCEV = nullptr;
        if (Addr) AddrSCEV = SE.getSCEV(Addr);
        AffAcc *A = new AffAcc(ArrayRef<Instruction *>(&I), AddrSCEV, MA, ArrayRef<const Loop *>(path), SE);
        all.push_back(A);
        access.insert(std::make_pair(MA, A));
        if (ParentLR->isAvailable()){
          bool onAllCFPaths = true;
          for (Instruction *I : A->getAccesses()) onAllCFPaths &= isOnAllControlFlowPaths(I->getParent(), Parent, DT);
          if (onAllCFPaths && A->promote(ParentLR)){
            wellformedAccesses.find(Parent)->getSecond().push_back(A); //guaranteed to exist
          }
        }
      }
    }
  }
  errs()<<"analyze: done with loop: "<<Parent->getHeader()->getNameOrAsOperand()<<"\n";
  return all;
}

void AffineAccess::addAllConflicts(const std::vector<AffAcc *> &all) {
  for (AffAcc *A : all) {
    assert(A);
    ArrayRef<const Loop *> loops = A->getContainingLoops();
    const Loop *outerMostExpandableExl = nullptr;
    if (A->isWrite()){
      MemoryDef *MA = cast<MemoryDef>(A->getMemoryAccess());
      const DenseSet<MemoryUseOrDef *> &cu = MD.findClobberUsers(MA);
      for (MemoryUseOrDef *D : cu) {
        if (MA == D || !hasMemInst(D)) continue;
        const Loop *innermostCommon = findFirstContaining(loops, D->getBlock());
        if (!innermostCommon) continue;
        auto p = access.find(D);
        if (p == access.end()) continue; //no AffAcc for D ==> skip
        AffAcc *B = p->second;
        AffAccConflict kind = AffAccConflict::Bad;
        if (A->isWellFormed(innermostCommon) && B->isWellFormed(innermostCommon)) {
          if (B->isWrite()) kind = AffAccConflict::MustNotIntersect; //WaW
          else kind = getRWConflict(B, A, innermostCommon);
        }
        //at this point, even if the two do not alias, we assume the chance is high that they do at runtime 
        //if their base addresses share some SCEVUnknowns (ie. some Value's) (FIXME: CONSERVATIVE)
        if (kind == AffAccConflict::MustNotIntersect){
          if (shareValues(A->getBaseAddr(A->loopToDimension(innermostCommon)), B->getBaseAddr(B->loopToDimension(innermostCommon))))
            kind = AffAccConflict::Bad;
        }
        if (kind != AffAccConflict::NoConflict) A->addConflict(B, innermostCommon, kind);
        if (kind == AffAccConflict::Bad) outerMostExpandableExl = innermostCommon;
      }
    } else {
      MemoryUseOrDef *MA = A->getMemoryAccess();
      const DenseSet<MemoryDef *> &cs = MD.findClobbers(MA);
      for (MemoryDef *D : cs) {
        if (MA == D || !hasMemInst(D)) continue;
        const Loop *innermostCommon = findFirstContaining(loops, D->getBlock());
        if (!innermostCommon) continue;
        auto p = access.find(D);
        if (p == access.end()) continue; //no AffAcc for D ==> skip
        AffAcc *B = p->second;
        AffAccConflict kind = getRWConflict(A, B, innermostCommon);
        //at this point, even if the two do not alias, we assume the chance is high that they do at runtime 
        //if their base addresses share some SCEVUnknowns (ie. some Value's) (FIXME: CONSERVATIVE)
        if (kind == AffAccConflict::MustNotIntersect){
          if (shareValues(A->getBaseAddr(A->loopToDimension(innermostCommon)), B->getBaseAddr(B->loopToDimension(innermostCommon))))
            kind = AffAccConflict::Bad;
        }
        if (kind != AffAccConflict::NoConflict) A->addConflict(B, innermostCommon, kind);
        if (kind == AffAccConflict::Bad) outerMostExpandableExl = innermostCommon;
      }
    }
    for (const Loop *L : loops) {
      if (!L) continue;
      if (L == outerMostExpandableExl) break;
      if (!A->isWellFormed(L)) break;
      auto p = expandableAccesses.find(L);
      if (p == expandableAccesses.end()){
        SmallVector<AffAcc *, 3U> l;
        l.push_back(A);
        expandableAccesses.insert(std::make_pair(L, std::move(l)));
      } else {
        p->getSecond().push_back(A);
      }
    }
  }
}

AffAccConflict AffineAccess::getRWConflict(AffAcc *Read, AffAcc *Write, const Loop *L) {
  assert(!Read->isWrite());
  assert(Write->isWrite());
  if (!L->contains(Read->getMemoryAccess()->getBlock()) || !L->contains(Write->getMemoryAccess()->getBlock())) return AffAccConflict::NoConflict;
  if (!Read->isWellFormed(L) || !Write->isWellFormed(L)) return AffAccConflict::Bad;
  Value *Addr = getAddress(Read->getMemoryAccess());
  Value *DAddr = getAddress(Write->getMemoryAccess());
  if (Addr && DAddr && AA.alias(Addr, DAddr) == NoAlias) return AffAccConflict::NoConflict;
  AffAccConflict kind = AffAccConflict::Bad;
  if (!MSSA.dominates(Read->getMemoryAccess(), Write->getMemoryAccess())) { //read does not dominate write ==> RaW
    kind = AffAccConflict::MustNotIntersect;
  } else { //read dominates write ==> WaR
    kind = AffAccConflict::MustNotIntersect;
    //exception we know that the store always happens to a position already written from if the store is to same address as write (FIXME: CONSERVATIVE)
    if ((Addr && DAddr && AA.alias(Addr, DAddr) == MustAlias)
      || accessPatternsAndAddressesMatch(Read, Write, L)) 
    {
      kind = AffAccConflict::NoConflict;
    }
  }
  
  return kind;
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

bool AffineAccess::accessPatternsAndAddressesMatch(const AffAcc *A, const AffAcc *B, const Loop *L) const {
  if (!accessPatternsMatch(A, B, L)) return false;
  return SCEVEquals(A->getBaseAddr(A->loopToDimension(L)), B->getBaseAddr(B->loopToDimension(L)), SE);
}

ScalarEvolution &AffineAccess::getSE() const { return this->SE; }
DominatorTree &AffineAccess::getDT()const { return this->DT; }
LoopInfo &AffineAccess::getLI() const { return this->LI; }
MemorySSA &AffineAccess::getMSSA() const { return this->MSSA; }
AAResults &AffineAccess::getAA() const { return this->AA; }
SmallVector<Loop *, 4U> AffineAccess::getLoopsInPreorder() const { return this->LI.getLoopsInPreorder(); }

ArrayRef<AffAcc *> AffineAccess::getExpandableAccesses(const Loop *L) {
  auto p = expandableAccesses.find(L);
  if (p == expandableAccesses.end()) return ArrayRef<AffAcc *>();
  return ArrayRef<AffAcc *>(p->getSecond());
}


std::vector<ExpandedAffAcc> 
AffineAccess::expandAllAt(ArrayRef<AffAcc *> Accs, const Loop *L, 
  Instruction *Point, Value *&BoundCheck, 
  Type *PtrTy, IntegerType *ParamTy, IntegerType *AgParamTy) 
{
  assert(Point);
  std::vector<ExpandedAffAcc> res;
  IRBuilder<> builder(Point);
  for (AffAcc *A : Accs) {
    res.push_back(std::move(A->expandAt(L, Point, PtrTy, ParamTy, AgParamTy)));
  }
  std::vector<Value *> checks;
  for (auto A = res.begin(); A != res.end(); ++A) {
    for (auto B = res.begin(); B != A; ++B){
      AffAccConflict kind = std::max(A->Access->getConflict(B->Access, L), B->Access->getConflict(A->Access, L));
      switch (kind)
      {
      case AffAccConflict::NoConflict:
        break; //nothing to add
      case AffAccConflict::MustNotIntersect: {
        Value *x = builder.CreateICmpULT(A->UpperBound, B->LowerBound, "no.inter.ab");
        Value *y = builder.CreateICmpULT(B->UpperBound, A->LowerBound, "no.inter.ba");
        checks.push_back(builder.CreateOr(x, y, "no.intersect"));
        break;
      }
      case AffAccConflict::Bad:
        assert(false && "cannot expand the given access because some of them have a bad conflict!");
        break;
      default:
        llvm_unreachable("unknown conflict type");
      }
    }
  }
  DenseSet<const Loop *> loops; //find all relevant loops
  for (AffAcc *A : Accs) {
    for (unsigned d = 0u; d < A->loopToDimension(L); d++) {
      const Loop *x = A->getLoop(d);
      if (x) loops.insert(x);
    }
  }
  for (const Loop *M : loops) { //generate checks for the loops
    auto p = reps.find(M);
    assert(p != reps.end());
    checks.push_back(p->second->expandLoopGuard(Point));
  }
  if (checks.empty()) BoundCheck = builder.getTrue();
  else BoundCheck = builder.CreateAnd(checks);
  return res;
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
    for (const AffAcc *A : AA.getExpandableAccesses(L)){
      A->dumpInLoop(L);
    }
  }
  return PreservedAnalyses::all();
}

