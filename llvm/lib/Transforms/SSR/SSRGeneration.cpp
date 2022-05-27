//===-- SSRGeneration.cpp - Generate SSR --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/SSR/SSRGeneration.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Target/TargetMachine.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/AffineAccessAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/FormatVariadic.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"
#include "llvm/IR/InlineAsm.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/ilist.h"

#include <array>
#include <vector>
#include <map>
#include <utility>
#include <algorithm>
#include <queue>

#define SSR_INFERENCE true

#define NUM_SSR 3U //NOTE: if increased too much, might need to change 1st arguments to clobberRegisters(..)
#define SSR_MAX_DIM 4U
//both are inclusive! 
#define SSR_SCRATCHPAD_BEGIN 0x100000
#define SSR_SCRATCHPAD_END 0x120000
//current state of hw: only allow doubles
#define CHECK_TYPE(T, I) (T == Type::getDoubleTy(I->getParent()->getContext()))

using namespace llvm;

static cl::opt<bool> GenerateSSR("generate-ssr", cl::init(true), cl::Hidden);

namespace{

void clobberRegisters(ArrayRef<std::string> regs, IRBuilder<> &builder){
  //equivalent to asm volatile ("":::regs);
  std::string constraints = "~{dirflag},~{fpsr},~{flags}"; //TODO: what are these doing?
  for (const std::string r : regs){
    constraints = "~{" + r + "}," + constraints; //(formatv("~{{{0}},", r) + Twine(constraints)).str()
  }
  errs()<<constraints<<"\n";
  InlineAsm *IA = InlineAsm::get(
    FunctionType::get(Type::getVoidTy(builder.getContext()), false), 
    "", 
    constraints,
    true
  );
  builder.CreateCall(IA);
}

} //end of namespace

///Wraps an AffineAcc *Access, expands all its SCEVs in constructor
struct GenSSR{
private:
  Value *Base;
  ConstantInt *DMID;
  SmallVector<std::pair<Value *, Value *>, SSR_MAX_DIM> offsets; 
  Value *MemBegin = nullptr;
  Value *MemEnd = nullptr;
  //Instruction *AvailableFrom; //use this and do everything lazy?
  
public:
  ///AffineAcc which is wrapped by this GenSSR
  const AffineAcc *Access;

  ///expand data, bound, and stride
  GenSSR(const AffineAcc *A, unsigned dmid, Instruction *ExpandBefore, AffineAccess &AF) : Access(A) {
    auto &ctxt = ExpandBefore->getParent()->getContext();
    Type *i32 = IntegerType::getInt32Ty(ctxt);
    DMID = cast<ConstantInt>(ConstantInt::get(i32, dmid));
    Base = AF.expandData(A, Type::getInt8PtrTy(ctxt), ExpandBefore);
    for (unsigned i = 0U; i < A->getDimension(); i++){
      offsets.push_back(std::make_pair(AF.expandBound(A, i, i32, ExpandBefore), AF.expandStride(A, i, i32, ExpandBefore)));
    }
  }

  ///generate comparisons 
  Value *GenerateSSRGuard(Instruction *ExpandBefore){
    auto &ctxt = ExpandBefore->getParent()->getContext();
    Type *i64 = IntegerType::getInt64Ty(ctxt);
    IRBuilder<> builder(ExpandBefore);
    std::vector<Value *> checks;
    for (unsigned i = 0U; i < Access->getDimension(); i++){
      /// loop has to be taken at least once (>= 1) ==> bound >= 0
      /// SGE also works for unsigned int: if the bound is unsigned and larger than 2^30 it will be too large for the scratchpad anyway
      checks.push_back(builder.CreateICmpSGE(getBound(i), ConstantInt::get(Type::getInt32Ty(ExpandBefore->getContext()), 0U)));
    }
    Value *BaseInt = builder.CreatePtrToInt(getBase(), i64, "base.to.int");
    this->MemBegin = BaseInt;
    checks.push_back(builder.CreateICmpUGE(BaseInt, ConstantInt::get(i64, SSR_SCRATCHPAD_BEGIN), "scratchpad.begin.check"));
    Value *EndIncl = BaseInt;
    for (unsigned i = 0U; i < Access->getDimension(); i++){
      auto dim = formatv("{0}d", i+1u);
      Value *Range = builder.CreateNUWMul(getBound(i), getStride(i), Twine("range.").concat(dim));
      Value *RangeExt = builder.CreateSExt(Range, i64, Twine("range.sext.").concat(dim));
      EndIncl = builder.CreateAdd(EndIncl, RangeExt, Twine("end.incl.").concat(dim));
    }
    this->MemEnd = EndIncl;
    checks.push_back(builder.CreateICmpULE(EndIncl, ConstantInt::get(i64, SSR_SCRATCHPAD_END), "scratchpad.end.check"));
    return builder.CreateAnd(ArrayRef<Value *>(checks));
  }

  ///generate setup instructions in loop preheader
  void GenerateSetup(){
    Instruction *Point = Access->getLoop()->getLoopPreheader()->getTerminator();
    Module *mod = Point->getModule();
    IRBuilder<> builder(Point);
    Type *i32 = Type::getInt32Ty(Point->getContext());
    Constant *dim = ConstantInt::get(i32, Access->getDimension() - 1U); //dimension - 1, ty=i32
    bool isStore = Access->getNStore() > 0u;

    Intrinsic::RISCVIntrinsics functions[] = {
      Intrinsic::riscv_ssr_setup_bound_stride_1d,
      Intrinsic::riscv_ssr_setup_bound_stride_2d,
      Intrinsic::riscv_ssr_setup_bound_stride_3d,
      Intrinsic::riscv_ssr_setup_bound_stride_4d
    };
    Value *StrideChange = nullptr;
    for (unsigned i = 0u; i < Access->getDimension(); i++){
      Value *Str = getStride(i);
      Value *Bd = getBound(i);
      Value *ChSt;
      if (StrideChange) ChSt = builder.CreateSub(Str, StrideChange, formatv("stride.{0}d.final", i+1));
      else ChSt = Str;
      Function *SSRBoundStrideSetup = Intrinsic::getDeclaration(mod, functions[i]);
      std::array<Value *, 3> bsargs = {getDMID(), Bd, ChSt};
      builder.CreateCall(SSRBoundStrideSetup->getFunctionType(), SSRBoundStrideSetup, ArrayRef<Value *>(bsargs))->dump();
      if (i + 1 != Access->getDimension()){ //only calculate stride change if needed
        Value *bdXstr = builder.CreateMul(Bd, Str, formatv("bdXstd.{0}d", i+1)); 
        if (StrideChange) StrideChange = builder.CreateAdd(StrideChange, bdXstr, formatv("str.change.for{0}d", i+2)); 
        else StrideChange = bdXstr;
      }
    }

    unsigned n_reps = 0U;
    std::string s = formatv("ft{0}", (unsigned)DMID->getValue().getLimitedValue());
    ArrayRef<std::string> regs(s);
    if (isStore){
      Function *SSRPush = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_push);
      for (Instruction *I : Access->getAccesses()){
        std::array<Value *, 2> pusharg = {getDMID(), cast<StoreInst>(I)->getValueOperand()};
        builder.SetInsertPoint(I);
        clobberRegisters(regs, builder);
        auto *C = builder.CreateCall(SSRPush->getFunctionType(), SSRPush, ArrayRef<Value *>(pusharg));
        clobberRegisters(regs, builder);
        C->dump(); I->dump();
        I->eraseFromParent();
        n_reps++;
      }
    }else{
      Function *SSRPop = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_pop);
      std::array<Value *, 1> poparg = {getDMID()};
      for (Instruction *I : Access->getAccesses()){
        builder.SetInsertPoint(I);
        clobberRegisters(regs, builder);
        auto *V = builder.CreateCall(SSRPop->getFunctionType(), SSRPop, ArrayRef<Value *>(poparg), "ssr.pop");
        clobberRegisters(regs, builder);
        V->dump(); I->dump();
        BasicBlock::iterator ii(I);
        ReplaceInstWithValue(I->getParent()->getInstList(), ii, V);
        n_reps++;
      }
    }

    builder.SetInsertPoint(Point);
    Constant *Rep = ConstantInt::get(i32, n_reps - 1U);
    Function *SSRRepetitionSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_setup_repetition);
    std::array<Value *, 2> repargs = {getDMID(), Rep};
    builder.CreateCall(SSRRepetitionSetup->getFunctionType(), SSRRepetitionSetup, ArrayRef<Value *>(repargs))->dump();

    Function *SSRSetup;
    if (!isStore){
      SSRSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_read_imm); //can take _imm bc dm and dim are constant
    }else{
      SSRSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_write_imm); //can take _imm bc dm and dim are constant
    }
    std::array<Value *, 3> args = {getDMID(), dim, getBase()};
    //NOTE: this starts the prefetching ==> always needs to be inserted AFTER bound/stride and repetition setups !!!
    builder.CreateCall(SSRSetup->getFunctionType(), SSRSetup, ArrayRef<Value *>(args))->dump(); 

    //create an SSR barrier in exit block. TODO: needed esp. for write streams?
    builder.SetInsertPoint(Access->getLoop()->getExitBlock()->getFirstNonPHI());
    Function *SSRBarrier = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_barrier);
    std::array<Value *, 1> barrargs = {getDMID()};
    builder.CreateCall(SSRBarrier->getFunctionType(), SSRBarrier, ArrayRef<Value *>(barrargs))->dump();
    return;
  }

  Value *getBase() const { return Base; }
  Value *getBound(unsigned i) const { return offsets[i].first; }
  Value *getStride(unsigned i) const { return offsets[i].second; }
  ConstantInt *getDMID() const { return DMID; }
  Value *getMemBegin() const { return MemBegin; }
  Value *getMemEnd() const { return MemEnd; }
};

namespace{

void copyPHIsFromPred(BasicBlock *BB){
  BasicBlock *Pred = BB->getSinglePredecessor();
  assert(Pred && "BB has single predecessor");
  for (Instruction &I : *Pred){
    if (auto *Phi = dyn_cast<PHINode>(&I)){
      PHINode *PhiC = PHINode::Create(Phi->getType(), 1u, Twine(Phi->getName()).concat(".copy"), BB->getFirstNonPHI());
      Phi->replaceAllUsesWith(PhiC);
      PhiC->addIncoming(Phi, Pred);
      errs()<<"replaced all uses of "<<*Phi<<" with "<<*PhiC<<"\n";
    }
  }
}

///splits block, redirects all predecessor to first half of split, copies phi's
std::pair<BasicBlock *, BasicBlock *> splitAt(Instruction *X, const Twine &name, DomTreeUpdater *DTU){
  BasicBlock *Two = X->getParent();
  BasicBlock *One = splitBlockBefore(Two, X, nullptr, nullptr, nullptr, name);
  for (auto *BB : predecessors(Two)){
    if (BB == One) continue;
    Instruction *T = BB->getTerminator();
    for (unsigned i = 0; i < T->getNumOperands(); i++){
      Value *OP = T->getOperand(i);
      if (dyn_cast<BasicBlock>(OP) == Two){
        T->setOperand(i, One); //if an operand of the terminator of a predecessor of Two points to Two it should now point to One
      }
    }
  }
  DTU->flush();
  copyPHIsFromPred(Two); //copy Phi's from One to Two
  return std::make_pair(One, Two);
}

///clones code from BeginWith up to EndBefore
///assumes all cf-paths from begin lead to end (or return)
///assumes there is a phi node for each value defined in the region that will be cloned in the block of EndBefore that is live after EndBefore
BranchInst *cloneRegion(Instruction *BeginWith, Instruction *EndBefore, DominatorTree &DT, DomTreeUpdater *DTU, LoopInfo *LI, MemorySSAUpdater *MSSAU){
  errs()<<"cloning from "<<*BeginWith<<" up to "<<*EndBefore<<"\n";

  auto p = splitAt(BeginWith, "split.before", DTU);
  BasicBlock *Head = p.first;
  BasicBlock *Begin = p.second;

  p = splitAt(EndBefore, "fuse.prep", DTU);
  BasicBlock *Fuse = p.first;
  BasicBlock *End = p.second;

  std::deque<BasicBlock *> q; //bfs queue
  q.push_back(Begin);
  DenseSet<BasicBlock *> vis; //bfs visited set
  DenseMap<Value *, Value *> clones; //value in orig -> value in clone (INV: orig and clone are of same class)
  std::vector<std::pair<unsigned, Instruction *>> operandsCleanup; //store operands that reference instructions that are not cloned yet
  
  while (!q.empty()){
    BasicBlock *C = q.front(); q.pop_front();
    if (C == End || vis.find(C) != vis.end()) continue;
    vis.insert(C);
    BasicBlock *Cc = BasicBlock::Create(C->getContext(), Twine(C->getName()).concat(".clone"), C->getParent(), C);
    clones.insert(std::make_pair(C, Cc)); //BasicBlock <: Value, needed for branches
    IRBuilder<> builder(Cc);
    for (Instruction &I : *C){
      Instruction *Ic = I.clone();
      assert(Ic->use_empty() && "no uses of clone");
      if (I.getType()->isVoidTy() || I.getType()->isLabelTy()) Ic = builder.Insert(Ic); //insert without name
      else Ic = builder.Insert(Ic, Twine(I.getName()).concat(".clone"));
      for (unsigned i = 0; i < Ic->getNumOperands(); i++){
        auto A = clones.find(Ic->getOperand(i));
        if (A != clones.end()){
          Ic->setOperand(i, A->second); //this also updates uses of A->second
          //check users update in A->second
          bool userUpdate = false; for (User *U : A->second->users()) {userUpdate = userUpdate || U == Ic; } assert(userUpdate && "user is updated on setOperand");
        }else{
          operandsCleanup.push_back(std::make_pair(i, Ic));
        }
      }
      clones.insert(std::make_pair(&I, Ic)); //add Ic as clone of I
    }
    auto succs = successors(C);
    for (auto S = succs.begin(); S != succs.end(); ++S) {
      q.push_back(*S);
    }
  }
  //operandCleanup
  for (const auto &p : operandsCleanup){ //p.first = index of operand that needs to be changed to clone in p.second
    auto A = clones.find(p.second->getOperand(p.first));
    if (A != clones.end()){
      p.second->setOperand(p.first, A->second);
    }//else did not find ==> was defined before region 
  }
  //incoming blocks of phi nodes are not operands ==> handle specially
  for (const auto &p : clones){ //all clones of phi-nodes appear in here
    if (auto *Phi = dyn_cast<PHINode>(p.second)){
      for (auto B = Phi->block_begin(); B != Phi->block_end(); ++B){
        const auto &c = clones.find(*B);
        if (c != clones.end()){
          *B = cast<BasicBlock>(c->second); //overwrite with clone of block if it was cloned
        }
      }
    }
  }
  //change terminator of Head to be CondBr with TakeOrig as cond
  BranchInst *HeadBr = cast<BranchInst>(Head->getTerminator()); //always BranchInst because of splitBlockBefore
  BasicBlock *HeadSucc = HeadBr->getSuccessor(0);
  HeadBr->eraseFromParent();
  HeadBr = BranchInst::Create(
    HeadSucc, //branch-cond = true -> go to non-clone (here SSR will be inserted)
    cast<BasicBlock>(clones.find(HeadSucc)->second),
    ConstantInt::get(Type::getInt1Ty(HeadSucc->getContext()), 0u), 
    Head
  );
  const auto &edge = BasicBlockEdge(std::make_pair(Fuse, End));
  for (auto &p : clones){
    for (User *U : p.first->users()){
      auto *I = dyn_cast<Instruction>(U);
      if (I && DT.dominates(edge, I->getParent())){
        errs()<<*I<<" makes use of "<<*p.first<<" after cloned region ==> add phi node at end!\n";
        assert(true && "did not declare phi node for live-out value");
      }
    }
  }
  //handle phi nodes in End
  for (Instruction &I : *End){
    if (auto *Phi = dyn_cast<PHINode>(&I)){
      for (auto *B : Phi->blocks()){ //yes Phi->blocks() will change during loop ==> does not matter
        auto p = clones.find(B);
        if (p != clones.end()){
          Value *Bval = Phi->getIncomingValueForBlock(B);
          auto v = clones.find(Bval);
          if (v != clones.end()){
            Phi->addIncoming(v->second, cast<BasicBlock>(p->second)); //add clone value & block as input
          }else {
            //v->first is constant or it is defined before cloned region begins
            Phi->addIncoming(Bval, cast<BasicBlock>(p->second));
          }
        }
      }
    }
  }
  errs()<<"done cloning \n";

  return HeadBr;
}

///generates SSR enable & disable calls
void generateSSREnDis(const Loop *L){
  IRBuilder<> builder(L->getLoopPreheader()->getTerminator()); // ----------- in preheader 
  Module *mod = L->getHeader()->getModule();
  Function *SSREnable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_enable);
  builder.CreateCall(SSREnable->getFunctionType(), SSREnable, ArrayRef<Value *>());

  std::vector<std::string> regs;
  for (unsigned r = 0u; r < NUM_SSR; r++){
    regs.push_back(std::string(formatv("ft{0}", r)));
  }
  //create inline asm that clobbers ft0-2 to make sure none of them are reordered to before ssr enable / after ssr disable
  //equivalent to asm volatile ("":::"ft0", "ft1", "ft2");
  clobberRegisters(ArrayRef<std::string>(regs), builder);

  //Function *FREPPragma = Intrinsic::getDeclaration(mod, Intrinsic::riscv_frep_infer);
  //builder.CreateCall(FREPPragma->getFunctionType(), FREPPragma, ArrayRef<Value *>());

  builder.SetInsertPoint(L->getExitBlock()->getTerminator()); // ----------- in exit block
  clobberRegisters(ArrayRef<std::string>(regs), builder);
  Function *SSRDisable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_disable);
  builder.CreateCall(SSRDisable->getFunctionType(), SSRDisable, ArrayRef<Value *>());

  errs()<<"generated ssr_enable and ssr_disable\n";

  L->getLoopPreheader()->getSinglePredecessor()->dump();
  L->getLoopPreheader()->dump();

  return;
}

Value *generateIntersectCheck(IRBuilder<> &builder, GenSSR *G, GenSSR *H){
  Value *Glo = G->getMemBegin();
  Value *Ghi = G->getMemEnd();
  Value *Hlo = H->getMemBegin();
  Value *Hhi = H->getMemEnd();
  Value *GhiLTHlo = builder.CreateICmpULT(Ghi, Hlo, "1st.memrange.check"); //bounds are inclusive, we assume alignment
  Value *HhiLTGlo = builder.CreateICmpULT(Hhi, Glo, "2nd.memrange.check");
  return builder.CreateOr(GhiLTHlo, HhiLTGlo, "or.memrange"); 
}

void generateSSRGuard(BranchInst *BR, ArrayRef<GenSSR *> streams, AffineAccess &AF){
  assert(BR->isConditional());
  if (streams.empty()) return;
  IRBuilder<> builder(BR);
  std::vector<Value *> checks;
  for (auto *G : streams){
    checks.push_back(G->GenerateSSRGuard(BR)); //means getMemBegin() and getMemEnd() do not return nullptr
  }
  for (unsigned i = 0; i < streams.size(); i++){
    GenSSR *G = streams[i];
    for (unsigned j = 0; j < i; j++){
      GenSSR *H = streams[j];
      if (AF.conflictWWWR(G->Access, H->Access)){
        checks.push_back(generateIntersectCheck(builder, G, H));
      }
    }
  }

  Value *TakeSSR = builder.CreateAnd(checks);
  BR->setCondition(TakeSSR);
}

bool isValid(const AffineAcc *A){
  if (!A) return false;
  if (A->getDimension() > SSR_MAX_DIM) return false;
  unsigned n_store = 0U;
  unsigned n_load = 0U;
  bool valid = true;
  for (auto *I : A->getAccesses()){
    if (dyn_cast<LoadInst>(I)) {
      n_load++;
      valid = valid && CHECK_TYPE(I->getType(), I);
    }else if(auto St = dyn_cast<StoreInst>(I)) {
      n_store++;
      valid = valid && CHECK_TYPE(St->getValueOperand()->getType(), I);
    }else assert(false && "non load/store instruction in AffineAcc::accesses ?");
    if(!valid) break;
  }
  return valid && ((n_store > 0U && n_load == 0U) || (n_store == 0U && n_load > 0U)); 
}

struct ConflictGraph{
  using NodeT = const AffineAcc *;

  ///accs assumed to be valid
  ConflictGraph(const AffineAccess &AF, ArrayRef<NodeT> accesses)  : AF(AF){ 
    errs()<<"conflict graph with "<<accesses.size()<<" nodes\n";
    for (auto A = accesses.begin(); A != accesses.end(); A++){
      conflicts.insert(std::make_pair(*A, std::vector<NodeT>()));
      mutexs.insert(std::make_pair(*A, std::vector<NodeT>()));
      for (auto B = accesses.begin(); B != A; B++){
        if (AF.shareInsts(*A, *B)){ //AF.conflictWWWR(*A, *B) 
          mutexs.find(*A)->second.push_back(*B);
          mutexs.find(*B)->second.push_back(*A);
        }else if (AF.shareLoops(*A, *B)){ //here we assume that the accessed memory region do not intersect and check this at runtime
          conflicts.find(*A)->second.push_back(*B);
          conflicts.find(*B)->second.push_back(*A);
        }
      }
    }
  }

  ///currently done greedily according to isBetter
  std::map<NodeT, Optional<unsigned>> &color(unsigned nColors) {
    std::map<NodeT, Optional<unsigned>> &color = *(new std::map<NodeT, Optional<unsigned>>());
    std::vector<NodeT> accs;
    for (const auto &A : conflicts) accs.push_back(A.first);
    auto isBetter = [](NodeT A, NodeT B){ 
      unsigned a = A->getLoop()->getLoopDepth() + 2 * A->getDimension() + (A->getNLoad() > 0u);
      unsigned b = B->getLoop()->getLoopDepth() + 2 * B->getDimension() + (B->getNLoad() > 0u);
      return a > b; 
    };
    std::sort(accs.begin(), accs.end(), isBetter);
    for (const auto &A : accs){
      bool done = false;
      for (const auto &M : mutexs.find(A)->second){
        auto c = color.find(M);
        if (c != color.end() && c->second.hasValue()) {//one mutex neighbour has color => A cannot get one
          color.insert(std::make_pair(A, None));
          done = true;
          break;
        }
      }
      if (done) continue; //done with this A ==> go to next

      BitVector cs(nColors);
      for (const auto &M : conflicts.find(A)->second){
        auto mc = color.find(M);
        if (mc != color.end() && mc->second.hasValue()){ //neighbour has some color mc ==> A cannot get mc
          cs[mc->second.getValue()] = 1u;
        }
      }
      int c = cs.find_first_unset();
      if (c >= 0) color.insert(std::make_pair(A, (unsigned)c));
      else color.insert(std::make_pair(A, None));
    }
    return color;
  }

private:
  const AffineAccess &AF;
  std::map<NodeT, std::vector<NodeT>> conflicts; //cannot get same color
  std::map<NodeT, std::vector<NodeT>> mutexs; //if one gets a color the other cannot get any color
};

void addChangedLoop(const Loop *NewL, SmallPtrSet<const Loop *, 4U> &loops){
  //check whether L or any of its predecessors (parents, parents of parents, etc) are already marked for SSRenable & -disable
  const Loop *L = NewL;
  bool contained = false;
  while (L && !contained){
    contained = contained || (loops.find(L) != loops.end());
    L = L->getParentLoop();
  }
  if (!contained){
    //check for all loops in loops whether NewL contains them
    std::vector<const Loop *> dels; //cannot directly delete loops in foreach loops ==> store here first
    for (const Loop *L : loops){
      if (NewL->contains(L)) dels.push_back(L);
    }
    for (const Loop *L : dels) loops.erase(L);
    loops.insert(NewL);
  }
}

} //end of namespace

PreservedAnalyses SSRGenerationPass::run(Function &F, FunctionAnalysisManager &FAM){
  
  if (!SSR_INFERENCE || !GenerateSSR) return PreservedAnalyses::all();

  AffineAccess &AF = FAM.getResult<AffineAccessAnalysis>(F);
  LoopInfo &LI = FAM.getResult<LoopAnalysis>(F);
  DominatorTree &DT = FAM.getResult<DominatorTreeAnalysis>(F);
  DomTreeUpdater DTU(&DT, DomTreeUpdater::UpdateStrategy::Eager);
  auto &MA = FAM.getResult<MemorySSAAnalysis>(F);
  MemorySSAUpdater MSSAU(&MA.getMSSA());

  errs()<<"SSR Generation Pass on function: "<<F.getNameOrAsOperand()<<" ---------------------------------------------------\n";

  SmallPtrSet<const Loop *, 4U> changedLoops;

  auto accs = AF.getAccesses();

  std::vector<const AffineAcc *> goodAccs;
  for (const AffineAcc *A : accs){
    auto p = AF.splitLoadStore(A);
    if (p.first && isValid(p.first)) goodAccs.push_back(p.first);
    if (p.second && isValid(p.second)) goodAccs.push_back(p.second);
  }

  if (goodAccs.empty()) return PreservedAnalyses::all(); //early exit

  ConflictGraph g(AF, ArrayRef<const AffineAcc *>(goodAccs));
  const auto &clr = g.color(NUM_SSR);
  errs()<<"computed coloring\n";

  DenseMap<const Loop *, SmallVector<GenSSR *, NUM_SSR>> ssrs;

  for (const auto C : clr){
    if (C.second.hasValue()){ //not None
      //add to ssrs
      auto p = ssrs.find(C.first->getLoop());
      GenSSR *G = new GenSSR(C.first, C.second.getValue(), C.first->getLoop()->getLoopPreheader()->getTerminator(), AF);
      if (p != ssrs.end()) p->getSecond().push_back(G);
      else ssrs.insert(std::make_pair(C.first->getLoop(), SmallVector<GenSSR *, NUM_SSR>(1u, G)));

      addChangedLoop(C.first->getLoop(), changedLoops); //update set of changed loops
    }
  }

  errs()<<"expanded all SSR bases, bounds, and strides\n";

  //generate clones
  for (const Loop *L : LI.getLoopsInPreorder()){
    auto p = ssrs.find(L);
    if (p != ssrs.end()){
      BranchInst *BR = cloneRegion(L->getLoopPreheader()->getTerminator(), &*L->getExitBlock()->getFirstInsertionPt(), DT, &DTU, &LI, &MSSAU);
      generateSSRGuard(BR, ArrayRef<GenSSR *>(p->getSecond().begin(), p->getSecond().end()), AF); //generate "SSR guard"
    }
  }

  errs()<<"generated all SSR guards\n";

  //generate ssr setups
  for (const auto &p : ssrs){
    for (GenSSR *G : p.getSecond()){
      G->GenerateSetup();
    }
  }

  errs()<<"generated all SSR setups\n";

  //generate enable / disable
  for (const Loop *L : changedLoops) generateSSREnDis(L);

  errs()<<"generated all SSR enable & disable \n";

  //TODO: merge loops
  //TODO: frep pragmas

  F.addFnAttr(Attribute::AttrKind::NoInline); //mark function as no-inline, because there can be intersecting streams if function is inlined!
  return PreservedAnalyses::none();
}



/*
for (unsigned i = 0; i < streams.size(); i++){
    GenSSR *G = streams[i];
    for (unsigned j = 0; j < streams.size(); j++){
      if (G->Access->getNStore() > 0u){
        GenSSR *H = streams[j];
        if (j < i || (j > i && H->Access->getNStore() == 0u)){ //true if H is before G OR H is after G and a load
          checks.push_back(generateIntersectCheck(builder, G, H));
        }
      }
    }
  }
*/