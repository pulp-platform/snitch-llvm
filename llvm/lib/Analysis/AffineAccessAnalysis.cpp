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
    const SCEV *data) : data(data), Addr(Addr), L(nullptr){
  for (Instruction *I : accesses) this->accesses.push_back(I);
  return;
}

unsigned AffineAcc::getDimension() const{
  return this->bounds.size();
}

unsigned AffineAcc::getUsedDimension() const{
  unsigned d = 0u;
  for (const SCEV *S : this->strides){
    if (!isa<SCEVConstant>(S) || !cast<SCEVConstant>(S)->isZero()) d++; //only cound a dimension if its stride is non-zero
  }
  return d;
}

void AffineAcc::dump() const{
  errs()<<"Affine Access in Loop:\n";
  if (L) L->dump();
  else errs()<<"nullptr\n";
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
  if (!res.empty()) { errs()<<"stride: "; res.back()->dump(); }
  errs()<<"finding strides in "; Addr->dump();
  if (loops.empty()) return;
  errs()<<"loop header: "<<loops[0]->getHeader()->getNameOrAsOperand()<<"\n";
  switch (Addr->getSCEVType())
  {
  case SCEVTypes::scAddRecExpr:
  {
    auto AddRec = cast<SCEVAddRecExpr>(Addr);
    if (AddRec->getLoop() == *loops.begin()){
      const SCEV *S = AddRec->getStepRecurrence(SE);
      for (const SCEV *x : factors){
        auto p = toSameType(S, x, SE, true);
        if (!p.hasValue()) {
          assert(false && "unsafe toSameType returned None!"); //TODO: change to errs()
          return;
        }
        S = SE.getMulExpr(p.getValue().first, p.getValue().second);
      }
      res.push_back(S);
      findStridesRec(AddRec->getStart(), ArrayRef<const Loop *>(loops.begin()+1, loops.end()), factors, SE, res);
    }else{
      bool occurs = false;
      for (const Loop *L : loops) occurs = occurs || AddRec->getLoop() == L; //loops needs to occur further up, o/w invalid
      if (!occurs) return;
      res.push_back(SE.getConstant(APInt(64U, 0U))); //TODO: this leads to ugly casts
      findStridesRec(AddRec, ArrayRef<const Loop *>(loops.begin()+1, loops.end()), factors, SE, res);
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
    bool lhs = SE.containsAddRecurrence(S->getOperand(0)); //TODO: this does not catch all cases maybe limit SCEV to outermost loop?
    bool rhs = SE.containsAddRecurrence(S->getOperand(1));
    if (lhs && !rhs) findStridesRec(S->getOperand(0), loops, factors, SE, res);
    else if (!lhs && rhs) findStridesRec(S->getOperand(1), loops, factors, SE, res);
    return;
  }
  case SCEVTypes::scMulExpr:
  {
    auto S = cast<SCEVMulExpr>(Addr);
    bool lhs = SE.containsAddRecurrence(S->getOperand(0)); //TODO: this does not catch all cases maybe limit SCEV to outermost loop?
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

SmallVector<const SCEV *, 4U> &findStrides(Instruction *Addr, ArrayRef<const Loop *> loops, ScalarEvolution &SE){
  const SCEV *AddrS = SE.getSCEV(Addr); //SE.getSCEVAtScope(Addr, loops.back()); //we only look at the scev as contained in outermost loop
  SmallVector<const SCEV *, 4U> &strides = *(new SmallVector<const SCEV *, 4U>());
  SmallVector<const SCEV *, 2U> factors;
  findStridesRec(AddrS, loops, factors, SE, strides);
  return strides;
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
  assert(!aa.L || (aa.L && !aa.L->isInvalid()));
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

  errs()<<"passed (2), ";

  const SCEV *Bound = this->loopReps.find(L)->getSecond();
  Instruction *InsPoint = L->getLoopPreheader()->getTerminator();

  if (!SE.hasComputableLoopEvolution(aa.data, L) && !SE.isLoopInvariant(aa.data, L)) return nullptr; //(3.1)
  const SCEV *Data = SE.SplitIntoInitAndPostInc(L, aa.data).first;
  if (!isSafeToExpandAt(Data, InsPoint, SE)) return nullptr;      //(3.2)

  errs()<<"passed (3), ";

  for (const SCEV *Bd : aa.bounds){
    if (!(SE.isLoopInvariant(Bd, L) && isSafeToExpandAt(Bd, InsPoint, SE))) return nullptr; //(4)
  }

  errs()<<"passed (4), ";

  for (const SCEV *Str : aa.strides){
    if (!(SE.isLoopInvariant(Str, L) && isSafeToExpandAt(Str, InsPoint, SE))) return nullptr; //(5)
  }

  errs()<<"passed (5), ";

  if (!isSafeToExpandAt(Bound, InsPoint, SE) || !isSafeToExpandAt(Stride, InsPoint, SE)) return nullptr; //(6)

  errs()<<"passed (6)";

  AffineAcc *A = new AffineAcc(aa);
  A->data = Data;
  A->L = L;
  A->bounds.push_back(Bound);
  A->strides.push_back(Stride);
  return A;
}

/// adds all affine accesses that use Addr in loop L
void AffineAccess::addAllAccesses(Instruction *Addr, const Loop *L){
  if (addresses.find(Addr) != addresses.end()) return; //already called addAllAccesses on this Addr instruction
  addresses.insert(Addr);

  errs()<<"addAllAccesses: start: "<<*Addr<<"\n";

  //find all accesses
  std::vector<Instruction *> accesses;
  for (auto U = Addr->use_begin(); U != Addr->use_end(); ++U){
    Instruction *Acc = dyn_cast<Instruction>(U->getUser());
    if (!Acc) continue; //user is not an instruction
    if (isa<LoadInst>(Acc) || isa<StoreInst>(Acc)){
      if (!isOnAllControlFlowPaths(Acc->getParent(), L, DT)) continue; //access does not occur consistently in loop ==> not suitable
      accesses.push_back(Acc);
    }
  }
  if (accesses.empty()) return; //Addr not used

  errs()<<"adding Access: "; Addr->dump();

  //we are looking at containing loops of all the accesses (guaranteed to be all the same)
  //Addr ins might be outside of loop (licm) if 1D stride is 0
  auto &cloops = getContainingLoops(LI.getLoopsInPreorder(), accesses[0]); 

  errs()<<"has "<<cloops.size()<<" containing loops\n";

  auto &strides = findStrides(Addr, cloops, SE);
  auto Stride = strides.begin();

  AffineAcc dim0(Addr, ArrayRef<Instruction *>(accesses), SE.getSCEV(Addr)); //never needed -> alloc in stack
  AffineAcc *A = &dim0;

  for (auto L = cloops.begin(); L != cloops.end(); ++L){
    if (loopReps.find(*L) == loopReps.end()) break; //this loop is malformed ==> this and all more outer loops cannot be used
    const SCEV *Str;
    if (Stride != strides.end()) Str = *(Stride++);
    else Str = SE.getConstant(IntegerType::getInt32Ty((*L)->getHeader()->getContext()), 0U); //if we run out of strides we can still promote with stride=0

    A = promoteAccess(*A, *L, Str);
    errs()<<"\n";
    if (A){
      errs()<<"found AffineAcc:\n"; A->dump();
      this->accesses.push_back(A);
    }else{
      break; //did not manage to promote ==> cannot promote for loops further out
    }
  }
  errs()<<"we now have "<<this->accesses.size()<<" affine accesses\n";
  return;
}

std::pair<const AffineAcc *, const AffineAcc *> AffineAccess::splitLoadStore(const AffineAcc *Acc) const{
  unsigned nLoad = Acc->getNLoad(), nStore = Acc->getNStore();
  if (nLoad > 0U && nStore == 0U) return std::make_pair(Acc, nullptr);
  if (nLoad == 0U && nStore > 0U) return std::make_pair(nullptr, Acc);
  AffineAcc *L = new AffineAcc(*Acc); //copy
  AffineAcc *S = new AffineAcc(*Acc); //copy
  L->accesses.clear();
  S->accesses.clear();
  for (Instruction *I : Acc->getAccesses()){
    if (isa<LoadInst>(I)) L->accesses.push_back(I);
    else if(isa<StoreInst>(I)) S->accesses.push_back(I);
  }
  return std::make_pair(L, S);
}

ArrayRef<const AffineAcc *> AffineAccess::getAccesses() const{
  ArrayRef<const AffineAcc *> *ar = new ArrayRef<const AffineAcc *>(accesses.begin(), accesses.end());
  return *ar; 
}

bool AffineAccess::accessPatternsMatch(const AffineAcc *A, const AffineAcc *B) const {
  if (!SCEVEquals(A->data, B->data, SE)) return false;
  if (A->getDimension() != B->getDimension()) return false;
  for (unsigned i = 0; i < A->getDimension(); i++){
    if (!SCEVEquals(A->bounds[i], B->bounds[i], SE)) return false;
    if (!SCEVEquals(A->strides[i], B->strides[i], SE)) return false;
  }
  return true;
}

bool AffineAccess::shareInsts(const AffineAcc *A, const AffineAcc *B) const{
  for (Instruction *IA : A->getAccesses()){
    for (Instruction *IB : B->getAccesses()){
      if (IA == IB) return true;
    }
  }
  return false;
}

bool AffineAccess::conflictWWWR(const AffineAcc *A, const AffineAcc *B) const {
  assert(!shareInsts(A, B) && "these AffineAcc's share instructions ==> one of them should be filtered");
  unsigned nstA = A->getNStore(), nstB = B->getNStore();
  if (nstA == 0U && nstB == 0U) return false; //can intersect read streams
  //at this point at least one of them is store

  //special case: no conflict, if
  // - exactly one of them is a store
  // - they have the same access pattern (AffineAccessAnalysis::accessPatternsMatch)
  // - all loads dominate all stores in the loop (ie. read before write)
  if ((nstA && !nstB) || (!nstA && nstB)){
    if (accessPatternsMatch(A, B)){
      const AffineAcc *S = nstA ? A : B; //store
      const AffineAcc *L = nstA ? B : A; //load
      bool check = true;
      for (Instruction *IL : L->getAccesses()){
        for (Instruction *IS : S->getAccesses()){
          check = check && DT.dominates(IL, IS);
        }
      }
      if (check) return false;
    }
  }
  
  if (A->getLoop()->contains(B->getLoop()) || B->getLoop()->contains(A->getLoop())) return true;
  return false;
}

bool AffineAccess::shareLoops(const AffineAcc *A, const AffineAcc *B) const {
  return A->getLoop() == B->getLoop() || A->getLoop()->contains(B->getLoop()) || B->getLoop()->contains(A->getLoop());
}

const SCEV *AffineAccess::wellFormedLoopBTCount(const Loop *L) const {
  auto P = loopReps.find(L);
  if (P == loopReps.end()) return nullptr; //loop not well-formed;
  return P->getSecond();
}

Value *AffineAccess::expandData(const AffineAcc *aa, Type *ty, Instruction *InsertBefore) const {
  InsertBefore = InsertBefore ? InsertBefore : aa->L->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(aa->data, InsertBefore, SE) && "data not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "data");
  ex.setInsertPoint(InsertBefore);
  errs()<<"expandData: scev  "<<*aa->data<<" has type: "<<*aa->data->getType()<<"\n";
  Value *data = ex.expandCodeFor(aa->data);
  errs()<<"expandData: value "<<*data<<" has type: "<<*data->getType()<<"\n";
  return castToSize(data, ty, InsertBefore);
}

Value *AffineAccess::expandBound(const AffineAcc *aa, unsigned i, Type *ty, Instruction *InsertBefore) const {
  InsertBefore = InsertBefore ? InsertBefore : aa->L->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(aa->bounds[i], InsertBefore, SE) && "bound not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "bound");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(aa->bounds[i]), ty, InsertBefore);
}

Value *AffineAccess::expandStride(const AffineAcc *aa, unsigned i, Type *ty, Instruction *InsertBefore) const {
  InsertBefore = InsertBefore ? InsertBefore : aa->L->getLoopPreheader()->getTerminator();
  assert(isSafeToExpandAt(aa->strides[i], InsertBefore, SE) && "bound not expanable here (note: only preheader guaranteed)");
  SCEVExpander ex(SE, aa->L->getHeader()->getModule()->getDataLayout(), "stride");
  ex.setInsertPoint(InsertBefore);
  return castToSize(ex.expandCodeFor(aa->strides[i]), ty, InsertBefore);
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
    assert(L);
    assert(!L->isInvalid());
    if (!A->wellFormedLoopBTCount(L)) continue; //loop not well-formed
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
        errs()<<"run: "<<I<<"\n";
        Instruction *AddrIns;
        if (!(AddrIns = dyn_cast<Instruction>(Addr))) continue; //if Addr is not instruction ==> constant, or sth else (==> leave for other passes to opt)
        A->addAllAccesses(AddrIns, L);
      }
    }
  }

  return std::move(*A);
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

/*
Optional<std::pair<const SCEV *, const SCEV *>> toSameSize(const SCEV *LHS, const SCEV *RHS, ScalarEvolution &SE, bool unsafe = false){
  assert(LHS && RHS);
  errs()<<"toSameSize: LHS="<<*LHS<<" with type"<<*LHS->getType()<<"\n";
  errs()<<"toSameSize: RHS="<<*RHS<<" with type"<<*RHS->getType()<<"\n";
  using PT = std::pair<const SCEV *, const SCEV *>;
  if (LHS->getType() == RHS->getType()) return Optional<PT>(std::make_pair(LHS, RHS)); //trivially the same size
  if (LHS->getType()->isPointerTy() && RHS->getType()->isPointerTy()) return Optional<PT>(std::make_pair(LHS, RHS));
  if (!LHS->getType()->isSized() || !RHS->getType()->isSized()) return None;
  //TODO: use datalayout for size instead
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
*/