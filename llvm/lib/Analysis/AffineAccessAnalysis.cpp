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
#include "llvm/ADT/DenseMap.h"

#include <array>
#include <vector>
#include <iostream>
#include <utility>

using namespace llvm;

AffineAcc::AffineAcc(Instruction *Addr, ArrayRef<Instruction *> accesses,
    const SCEV *data) : data(data), Addr(Addr){
  for (Instruction *I : accesses) this->accesses.push_back(I);
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

unsigned AffineAcc::getNStore() const {
  unsigned r = 0U;
  for (Instruction *I : this->accesses) r += isa<StoreInst>(I);
  return r;
}

unsigned AffineAcc::getNLoad() const {
  unsigned r = 0U;
  for (Instruction *I : this->accesses) r += isa<LoadInst>(I);
  return r;
}

//================== AffineAcces, helper functions =========================================

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
    errs()<<"checkLoop: cannot calculate backedge taken count\n";
    return false;
  }
  const SCEV *bt = SE.getBackedgeTakenCount(L);
  if(!isSafeToExpandAt(bt, InsertionPoint, SE) /*|| !SE->isAvailableAtLoopEntry(bt, L)*/){
    errs()<<"cannot expand bt SCEV: "; bt->dump();
  }
  errs()<<"loop is well-formed: "; bt->dump();
  return true;
}

Optional<std::pair<const SCEV *, const SCEV *>> toSameSize(const SCEV *LHS, const SCEV *RHS, ScalarEvolution &SE, bool unsafe = false){
  using PT = std::pair<const SCEV *, const SCEV *>;
  if (LHS->getType()->getIntegerBitWidth() > RHS->getType()->getIntegerBitWidth()) {
    if (auto LHSx = dyn_cast<SCEVConstant>(LHS)){
      if (LHSx->getAPInt().getActiveBits() <= RHS->getType()->getIntegerBitWidth()) {}
        return Optional<PT>(std::make_pair(SE.getConstant(RHS->getType(), LHSx->getAPInt().getLimitedValue()), RHS));
    } 
    if (auto RHSx = dyn_cast<SCEVConstant>(RHS)){
      if (RHSx->getAPInt().getActiveBits() <= LHS->getType()->getIntegerBitWidth())
        return Optional<PT>(std::make_pair(LHS, SE.getConstant(LHS->getType(), RHSx->getAPInt().getLimitedValue())));
    }
    if (auto LHSx = dyn_cast<SCEVSignExtendExpr>(LHS)) return toSameSize(LHSx->getOperand(0), RHS, SE);
    if (auto LHSx = dyn_cast<SCEVZeroExtendExpr>(LHS)) return toSameSize(LHSx->getOperand(0), RHS, SE);
    if (auto RHSx = dyn_cast<SCEVTruncateExpr>(RHS)) return toSameSize(LHS, RHSx->getOperand(0), SE);
    if (unsafe) return Optional<PT>(std::make_pair(SE.getTruncateExpr(LHS, RHS->getType()), RHS));
    return None;
  }else if (LHS->getType()->getIntegerBitWidth() < RHS->getType()->getIntegerBitWidth()){
    auto p = toSameSize(RHS, LHS, SE, unsafe);
    if (!p.hasValue()) return None;
    return Optional<PT>(std::make_pair(p.getValue().second, p.getValue().first));
  }
  return Optional<PT>(std::make_pair(LHS, RHS));
}

///checks whether LHS == RHS always holds
bool SCEVEquals(const SCEV *LHS, const SCEV *RHS, ScalarEvolution &SE){
  auto p = toSameSize(LHS, RHS, SE);
  if (!p.hasValue()) return false;
  LHS = p.getValue().first;
  RHS = p.getValue().second;
  errs()<<"SCEVEquals:\n\t"; LHS->dump();
  errs()<<"\t"; RHS->dump();
  if (LHS == RHS) return true; //trivially the same if this holds (bc const Ptr)
  else{
    const SCEVPredicate *Peq = SE.getEqualPredicate(LHS, RHS);
    if (Peq->isAlwaysTrue()) return true; //if we arrive at setup addr scev, we are done
  }
  errs()<<"false\n";
  return false;
}

/// check whether BB is on all controlflow paths from header to header
bool isOnAllControlFlowPaths(const BasicBlock *BB, const Loop *L, const DominatorTree &DT){
  return DT.dominates(BB, L->getHeader());
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
//because SCEVComparePredicate is not in this version of LLVM we have to do this manually ==> will not catch all cases
//predicate is that Rep > 0
bool isOnAllPredicatedControlFlowPaths(const BasicBlock *BB, const Loop *L, const DominatorTree &DT, const SCEV *Rep, ScalarEvolution &SE){
  if (isOnAllControlFlowPaths(BB, L, DT)) return true; //is on all paths anyway
  Rep->dump();

  std::deque<BasicBlock *> q(1U, L->getHeader()); //iterative BFS with queue
  while (!q.empty()){
    BasicBlock *Current = q.front(); q.pop_front();
    if (Current == BB) continue; //do not continue BFS from BB

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

/// get Loops containing Ins from innermost to outermost
SmallVector<const Loop *, 3U> &getContainingLoops(ArrayRef<const Loop *> loopsPreorder, Instruction *Ins){
  BasicBlock *BB = Ins->getParent();
  SmallVector<const Loop *, 3U> *r = new SmallVector<const Loop *, 3U>();
  for (auto L = loopsPreorder.rbegin(); L != loopsPreorder.rend(); ++L){ //go through loops in reverse order ==> innermost first
    if ((*L)->contains(BB)){
      r->push_back(*L);
    }
  }
  return *r;
}

void findStridesRec(const SCEV *Addr, ArrayRef<const Loop *> loops, SmallVector<const SCEV *, 2U> &factors, ScalarEvolution &SE, SmallVector<const SCEV *, 4U> &res){
  errs()<<"finding strides in "; Addr->dump();
  if (loops.empty()) return;
  switch (Addr->getSCEVType())
  {
  case SCEVTypes::scAddRecExpr:
  {
    auto AddRec = cast<SCEVAddRecExpr>(Addr);
    if (AddRec->getLoop() == *loops.begin()){
      const SCEV *S = AddRec->getStepRecurrence(SE);
      for (const SCEV *x : factors){
        auto p = toSameSize(S, x, SE, true);
        S = SE.getMulExpr(p.getValue().first, p.getValue().second);
      }
      res.push_back(S);
      findStridesRec(AddRec->getStart(), ArrayRef<const Loop *>(loops.begin()+1, loops.end()), factors, SE, res);
    }else{
      bool occurs = false;
      for (const Loop *L : loops) occurs = occurs || AddRec->getLoop() == L; //loops needs to occur further up, o/w invalid
      if (!occurs) return;
      res.push_back(SE.getConstant(APInt(64U, 0U)));
      findStridesRec(AddRec->getStart(), loops, factors, SE, res);
    }
    return;
  }
  //case SCEVTypes::scTruncate: TODO: is unsafe here, right?
  case SCEVTypes::scSignExtend:
  case SCEVTypes::scZeroExtend:
    findStridesRec(cast<SCEVIntegralCastExpr>(Addr)->getOperand(0), loops, factors, SE, res);
    return;

  case SCEVTypes::scAddExpr:
  {
    auto S = cast<SCEVAddExpr>(Addr);
    bool lhs = SE.containsAddRecurrence(S->getOperand(0));
    bool rhs = SE.containsAddRecurrence(S->getOperand(1));
    if (lhs && !rhs) findStridesRec(S->getOperand(0), loops, factors, SE, res);
    else if (!lhs && rhs) findStridesRec(S->getOperand(1), loops, factors, SE, res);
    return;
  }
  case SCEVTypes::scMulExpr:
  {
    auto S = cast<SCEVMulExpr>(Addr);
    bool lhs = SE.containsAddRecurrence(S->getOperand(0));
    bool rhs = SE.containsAddRecurrence(S->getOperand(1));
    if (lhs && !rhs) {
      factors.push_back(S->getOperand(1));
      findStridesRec(S->getOperand(0), loops, factors, SE, res);
    }else if (!lhs && rhs) {
      factors.push_back(S->getOperand(0));
      findStridesRec(S->getOperand(1), loops, factors, SE, res);
    }
    return;
  }

  default:
    return;
  }
}

SmallVector<const SCEV *, 4U> &findStrides(const SCEV *Addr, ArrayRef<const Loop *> loops, ScalarEvolution &SE){
  SmallVector<const SCEV *, 4U> &strides = *(new SmallVector<const SCEV *, 4U>());
  SmallVector<const SCEV *, 2U> factors;
  findStridesRec(Addr, loops, factors, SE, strides);
  errs()<<"found strides: \n";
  for (const SCEV *S : strides) S->dump();
  return strides;
}

} //end of namespace

//================== AffineAcces, Result of Analysis =========================================
AffineAccess::AffineAccess(ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI) : SE(SE), DT(DT), LI(LI){
  auto loops = LI.getLoopsInPreorder();
  unsigned l = 0u;
  for (const Loop *L : loops){
    L->dump();

    if (!L->getLoopPreheader()) continue;
    if (!checkLoop(L, DT, SE, L->getLoopPreheader()->getTerminator())) continue;

    loopReps.insert(std::make_pair(L, SE.getBackedgeTakenCount(L))); l++;
  }
  errs()<<"FOUND "<<l<<" LOOPS\n";
  return;
}

/// promotes a (potentially dim=0) AffineAcc to one more dimension 
///  (1) parent loop L satisfies checkLoop
///  (2) child loop (in particular all in aa.accesses) is on all paths in L where the inner bound + 1 > 0 
///  (3) data ptr can be computed outside of parent loop
///  (4) forall bd : aa.bounds. SE.isLoopInvariant(bd, L) && isSafeToExpandAt(bd, LPreheader->getTerminator(), SE)
///  (5) forall st : aa.strides. SE.isLoopInvariant(st, L) && isSafeToExpandAt(st, LPreheader->getTerminator(), SE)
///  (6) isSafeToExpandAt(Bound / Stride, LPreheader->getTerminator(), SE)
AffineAcc *AffineAccess::promoteAccess(const AffineAcc &aa, const Loop *L, const SCEV *Stride){
  assert((!aa.L) || (aa.L && aa.L->getParentLoop() == L && "can only promote to parent loop")); //(1)
  assert(this->loopReps.find(L) != this->loopReps.end() && "L is well formed"); //(1)

  errs()<<"Trying to promote AA of dim="<<aa.getDimension()<<"\n";

  if (aa.L){
    const SCEV *Bd = loopReps.find(aa.L)->getSecond();
    const SCEV *Rep = SE.getAddExpr(Bd, SE.getConstant(Bd->getType(), 1U));
    if (!isOnAllPredicatedControlFlowPaths(aa.L->getHeader(), L, this->DT, Rep, this->SE)) return nullptr; //(2.1)
  }else{
    for (Instruction *I : aa.accesses){
      if (!isOnAllControlFlowPaths(I->getParent(), L, DT)) return nullptr; //(2.2)
    }
  }

  errs()<<"passed (2)\n";

  const SCEV *Bound = this->loopReps.find(L)->getSecond();
  Instruction *InsPoint = L->getLoopPreheader()->getTerminator();

  if (!SE.hasComputableLoopEvolution(aa.data, L)) return nullptr; //(3.1)
  const SCEV *Data = SE.SplitIntoInitAndPostInc(L, aa.data).first;
  if (!isSafeToExpandAt(Data, InsPoint, SE)) return nullptr;      //(3.2)

  errs()<<"passed (3)\n";

  for (const SCEV *Bd : aa.bounds){
    if (!(SE.isLoopInvariant(Bd, L) && isSafeToExpandAt(Bd, InsPoint, SE))) return nullptr; //(4)
  }

  errs()<<"passed (4)\n";

  for (const SCEV *Str : aa.strides){
    if (!(SE.isLoopInvariant(Str, L) && isSafeToExpandAt(Str, InsPoint, SE))) return nullptr; //(5)
  }

  errs()<<"passed (5)\n";

  if (!isSafeToExpandAt(Bound, InsPoint, SE) || !isSafeToExpandAt(Stride, InsPoint, SE)) return nullptr; //(6)

  errs()<<"passed (6)\n";

  AffineAcc *A = new AffineAcc(aa);
  A->data = Data;
  A->L = L;
  A->bounds.push_back(Bound);
  A->strides.push_back(Stride);
  return A;
}

/// adds all affine accesses that use Addr in loop L
void AffineAccess::addAllAccesses(Instruction *Addr, const Loop *L){
  std::vector<Instruction *> accesses;
  for (auto U = Addr->use_begin(); U != Addr->use_end(); ++U){
    Instruction *Acc = dyn_cast<LoadInst>(U->getUser());
    if (!Acc) Acc = dyn_cast<StoreInst>(U->getUser());
    if (!Acc) continue; //both casts failed ==> not a suitable instruction
    if (!isOnAllControlFlowPaths(Acc->getParent(), L, DT)) continue; //access does not occur consistently in loop ==> not suitable
    accesses.push_back(Acc);
  }
  if (accesses.empty()) return; //Addr not used

  errs()<<"adding Access: "; Addr->dump();

  const SCEV *AddrS = SE.getSCEV(Addr);

  auto &cloops = getContainingLoops(LI.getLoopsInPreorder(), Addr);

  errs()<<"has "<<cloops.size()<<" containing loops\n";

  auto &strides = findStrides(AddrS, cloops, SE);
  auto Stride = strides.begin();

  AffineAcc dim0(Addr, ArrayRef<Instruction *>(accesses), AddrS); //never needed -> alloc in stack
  AffineAcc *A = &dim0;

  for (auto L = cloops.begin(); L != cloops.end(); ++L){
    if (loopReps.find(*L) == loopReps.end()) break; //this loop is malformed ==> this and all more outer loops cannot be used
    if (Stride == strides.end()) break; //ran out of strides  

    A = promoteAccess(*A, *L, *Stride);
    if (A){
      errs()<<"found AffineAcc:\n"; A->dump();
      this->accesses.push_back(A);
    }else{
      break; //did not manage to promote ==> cannot promote for loops further out
    }

    ++Stride;
  }
  errs()<<"\n";
  return;
}

ArrayRef<const AffineAcc *> AffineAccess::getAccesses() const{
  ArrayRef<const AffineAcc *> *ar = new ArrayRef<const AffineAcc *>(accesses.begin(), accesses.end());
  return *ar; 
}

Value *castToSize(Value *R, Type *ty, Instruction *InsPoint){
  const DataLayout &DL = InsPoint->getParent()->getModule()->getDataLayout();
  Type *rty = R->getType();
  if (rty == ty) return R;
  if (DL.getTypeSizeInBits(rty) > DL.getTypeSizeInBits(ty)) {
    return CastInst::CreateTruncOrBitCast(R, ty, "scev.cast", InsPoint);
  }
  if (DL.getTypeSizeInBits(rty) < DL.getTypeSizeInBits(ty)) {
    return CastInst::CreateZExtOrBitCast(R, ty, "scev.cast", InsPoint);
  }
  return CastInst::CreateBitOrPointerCast(R, ty, "scev.cast", InsPoint);
}

Value *AffineAccess::expandData(const AffineAcc *aa, Type *ty) const{
  Instruction *InsPoint = aa->L->getLoopPreheader()->getTerminator();
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "data");
  ex.setInsertPoint(InsPoint);
  return castToSize(ex.expandCodeFor(aa->data), ty, InsPoint);
}

Value *AffineAccess::expandBound(const AffineAcc *aa, unsigned i, Type *ty) const{
  Instruction *InsPoint = aa->L->getLoopPreheader()->getTerminator();
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "bound");
  ex.setInsertPoint(InsPoint);
  return castToSize(ex.expandCodeFor(aa->bounds[i]), ty, InsPoint);
}

Value *AffineAccess::expandStride(const AffineAcc *aa, unsigned i, Type *ty) const{
  Instruction *InsPoint = aa->L->getLoopPreheader()->getTerminator();
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "stride");
  ex.setInsertPoint(InsPoint);
  return castToSize(ex.expandCodeFor(aa->strides[i]), ty, InsPoint);
}

//================== Affine Acces Analysis ==================================================

AnalysisKey AffineAccessAnalysis::Key;

AffineAccess AffineAccessAnalysis::run(Function &F, FunctionAnalysisManager &FAM) {
  
  errs()<<"running AffineAccessAnalysis on "<<F.getName()<<"\n";

  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  ScalarEvolution &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
  //AAResults &AA = FAM.getResult<AAManager>(F);

  AffineAccess *A = new AffineAccess(SE, DT, LI);

  for (const Loop *L : LI.getLoopsInPreorder()){
    for (BasicBlock *BB : L->getBlocks()){
      if (!isOnAllControlFlowPaths(BB, L, DT)) continue;
      for (Instruction &I : *BB){
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
        
        A->addAllAccesses(AddrIns, L);
      }
    }
  }

  return *A;
}

//================== Affine Acces Analysis Pass for opt =======================================
PreservedAnalyses AffineAccessAnalysisPass::run(Function &F, FunctionAnalysisManager &FAM) {
  AffineAccess AA = FAM.getResult<AffineAccessAnalysis>(F);
  for (const auto *A : AA.getAccesses()){
    A->dump();
  }
  return PreservedAnalyses::all();
}


/*
/// Fold over AddrSCEV
/// All AddRecSCEVs are dependent on L or loops contained in L (TODO: and on all paths?)
/// All steps in ADDRecSCEVs can be calculated in preheader of L
bool canFindStrides(ScalarEvolution &SE, const ArrayRef<const Loop *> &loops, const SCEV *AddrSCEV, const SCEV *SetupAddrSCEV){
  errs()<<"finding strides in: "; AddrSCEV->dump();
  if (SCEVEquals(AddrSCEV, SetupAddrSCEV, SE)) return true;
  
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
*/

/*
/// can promote if: 
  (1) parent loop Outer satisfies checkLoop
  (2) child loop Inner is on all paths in Outer where Inner.backedgetakencount +1 > 0
  (3) Stride for Outer can be found
  (4) forall bd : aa.bounds. SE.isLoopInvariant(bd, Outer) && isSafeToExpandAt(bd, OuterPreheader->getTerminator(), SE)
  (5) forall st : aa.strides. SE.isLoopInvariant(st, Outer) && isSafeToExpandAt(st, OuterPreheader->getTerminator(), SE)
bool promoteAffineAccess(AffineAcc &aa, ScalarEvolution &SE, DominatorTree &DT, DenseMap<const Loop *, const SCEV *> &LR){
  const Loop *Inner = aa.getLoop();
  const Loop *Outer = Inner->getParentLoop();
  const auto &R = LR.find_as(Outer); 
  if (R == LR.end()) return false; //Outer violates (1)
  const SCEV *Bound = R->getSecond();
  BasicBlock *OuterPreheader = Outer->getLoopPreheader();
  BasicBlock *InnerPreheader = Inner->getLoopPreheader();
  const SCEV *Rep = SE.getAddExpr(SE.getBackedgeTakenCount(Inner), SE.getConstant(APInt(64U, 1U))); //trip count of Inner loop
  if (!isOnAllPredicatedControlFlowPaths(InnerPreheader, Outer, DT, Rep, SE)) return false;  //violates (2)
  
}
*/

/*
AffineAccess &runOnFunction(Function &F, LoopInfo &LI, DominatorTree &DT, ScalarEvolution &SE, AAResults &AA){
  AffineAccess *aa = new AffineAccess(SE, DT, LI);

  auto loops = LI.getLoopsInPreorder();
  errs()<<"contains "<<loops.size()<<" loops\n";

  //loops contained in this are guatanteed to have passed checkLoop
  DenseMap<const Loop *, const SCEV *> loopReps;

  for (const Loop *L : loops){
    errs()<<"LOOP:\n";
    L->dump();

    if (!L->getLoopPreheader()) continue;
    if (!checkLoop(L, DT, SE, L->getLoopPreheader()->getTerminator())) continue;

    loopReps.insert(std::make_pair(L, SE.getBackedgeTakenCount(L)));

    for (const auto &BB : L->getBlocks()){
      
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

        aa->addAllAccesses(AddrIns);

        //Address SCEV
        const SCEV *AddrSCEV = SE.getSCEV(Addr);
        if (!SE.hasComputableLoopEvolution(AddrSCEV, L)) continue;
        errs()<<"has computable loop evolution: "; AddrSCEV->dump();

        //Base Pointer (=data) SCEV
        auto split = SE.SplitIntoInitAndPostInc(L, AddrSCEV);
        const SCEV *SetupAddrSCEV = split.first; //const SCEV *PostIncSCEV = split.second;
        if (!isSafeToExpandAt(SetupAddrSCEV, L->getLoopPreheader()->getTerminator(), SE)) continue;
        errs()<<"can expand setup addr scev in preheader: "; SetupAddrSCEV->dump();
        
        //Stride Check
        if (!canFindStride(L, AddrSCEV, SetupAddrSCEV, SE)) continue;
        errs()<<"can find loop Stride: "; AddrSCEV->dump();

        std::vector<Instruction *> accesses;
        for (auto U = Addr->use_begin(); U != Addr->use_end(); ++U){
          Instruction *Acc = dyn_cast<LoadInst>(U->getUser());
          if (!Acc) Acc = dyn_cast<StoreInst>(U->getUser());
          
          if (!Acc) continue; //both casts failed ==> not a suitable instruction
          if (!isOnAllControlFlowPaths(Acc->getParent(), L, DT)) continue; //access does not occur consitently in loop ==> not suitable

          accesses.push_back(Acc);
        }

        const SCEV *TC = loopReps.find(L)->getSecond();
        const auto *AddrRecSCEV = cast<SCEVAddRecExpr>(AddrSCEV);
        const SCEV *Str;
        if (AddrRecSCEV->getLoop() == L){
          Str = cast<SCEVAddRecExpr>(AddrSCEV)->getStepRecurrence(SE); //because 1D for now
        }else{
          Str = SE.getConstant(APInt(64U, 0U));
        }

        aa->addAccess(new AffineAcc(L, AddrIns, ArrayRef<Instruction *>(accesses), SetupAddrSCEV, TC, Str));
        errs()<<"added new AffineAcc\n";

        //TODO: dimension promotion: if preheader has only one predecessor -> if cond for "skipping loop" is bt+1 == 0 -> if parent loop passes checks -> promote
      }
    }

  }
  
  return *aa;
}

*/

/*
errs()<<"finding stride in: "; CS->dump();
    const SCEV *Stride;
    if (const auto *Rec = dyn_cast<SCEVAddRecExpr>(CS)){
      if (Rec->getLoop() == *L) {
        CS = Rec->getStart();
        Stride = Rec->getStepRecurrence(SE);
      }else{
        bool occurs = false;
        for (auto L_ = L; L_ != cloops.end(); ++L_) occurs = occurs && Rec->getLoop() == *L_;
        if (!occurs) break; //AddRecExpr references a loop that is not a containing loop ==> cannot guarantee anything
        Stride = SE.getConstant(APInt(64U, 0U)); //addrSCEV does not step in this loop ==> stride is 0
      }
    }else{
      break; //did not manage to compute stride
    }
    assert(Stride);
    errs()<<"found stride: "; Stride->dump();
*/