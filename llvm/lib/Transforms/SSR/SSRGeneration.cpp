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
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"

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

#define NUM_SSR 3U
#define SSR_MAX_DIM 4U

//current state of hw: only allow doubles
#define CHECK_TYPE(I) (I->getType() == Type::getDoubleTy(I->getParent()->getContext()))

using namespace llvm;

namespace{

///generates SSR setup calls
void generateSSR(AffineAccess &AA, const AffineAcc *aa, unsigned dmid, bool isStore){
  BasicBlock *LoopPreheader = aa->getLoop()->getLoopPreheader();
  Module *mod = LoopPreheader->getModule();
  LLVMContext &ctxt = LoopPreheader->getContext();
  IntegerType *i32 = IntegerType::getInt32Ty(ctxt);

  IRBuilder<> builder(LoopPreheader->getTerminator());

  ConstantInt *dm = ConstantInt::get(i32, dmid); //datamover id, ty=i32
  ConstantInt *dim = ConstantInt::get(i32, aa->getDimension() - 1U); //dimension - 1, ty=i32
  Value *data = AA.expandData(aa, Type::getInt8PtrTy(ctxt));
  Function *SSRSetup;
  if (!isStore){
    SSRSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_read_imm); //can take _imm bc dm and dim are constant
  }else{
    SSRSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_write_imm); //can take _imm bc dm and dim are constant
  }
  std::array<Value *, 3> args = {dm, dim, data};
  builder.CreateCall(SSRSetup->getFunctionType(), SSRSetup, ArrayRef<Value *>(args))->dump();

  Intrinsic::RISCVIntrinsics functions[] = {
    Intrinsic::riscv_ssr_setup_bound_stride_1d,
    Intrinsic::riscv_ssr_setup_bound_stride_2d,
    Intrinsic::riscv_ssr_setup_bound_stride_3d,
    Intrinsic::riscv_ssr_setup_bound_stride_4d
  };
  for (unsigned d = 0U; d < aa->getDimension(); d++){
    Value *bound = AA.expandBound(aa, d, i32); //bound - 1, ty=i32 
    Value *stride = AA.expandStride(aa, d, i32); //relative stride, ty=i32

    Function *SSRBoundStrideSetup = Intrinsic::getDeclaration(mod, functions[d]);
    std::array<Value *, 3> bsargs = {dm, bound, stride};
    auto *C = builder.CreateCall(SSRBoundStrideSetup->getFunctionType(), SSRBoundStrideSetup, ArrayRef<Value *>(bsargs));
    C->dump();
  }

  unsigned n_reps = 0U;
  if (isStore){
    Function *SSRPush = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_push);
    std::vector<Instruction *> del;
    for (Instruction *I : aa->getAccesses()){
      std::array<Value *, 2> pusharg = {dm, cast<StoreInst>(I)->getValueOperand()};
      builder.SetInsertPoint(I);
      auto *C = builder.CreateCall(SSRPush->getFunctionType(), SSRPush, ArrayRef<Value *>(pusharg));
      C->dump();
      I->dump();
      del.push_back(I);
      n_reps++;
    }
    for (Instruction *I : del) I->removeFromParent();
  }else{
    Function *SSRPop = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_pop);
    std::array<Value *, 1> poparg = {dm};
    for (Instruction *I : aa->getAccesses()){
      builder.SetInsertPoint(I);
      Instruction *V = builder.CreateCall(SSRPop->getFunctionType(), SSRPop, ArrayRef<Value *>(poparg), "ssr.pop");
      V->dump();
      I->dump();
      BasicBlock::iterator ii(I);
      ReplaceInstWithValue(I->getParent()->getInstList(), ii, V);
    }
  }

  builder.SetInsertPoint(LoopPreheader->getTerminator());
  ConstantInt *rep = ConstantInt::get(i32, n_reps - 1U); //repetition - 1, ty=i32
  Function *SSRRepetitionSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_setup_repetition);
  std::array<Value *, 2> repargs = {dm, rep};
  builder.CreateCall(SSRRepetitionSetup->getFunctionType(), SSRRepetitionSetup, ArrayRef<Value *>(repargs))->dump();
  return;
}

///generates SSR enable & disable calls
void generateSSREnDis(const Loop *L){
  IRBuilder<> builder(L->getLoopPreheader()->getTerminator());
  Module *mod = L->getHeader()->getModule();
  Function *SSREnable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_enable);
  builder.CreateCall(SSREnable->getFunctionType(), SSREnable, ArrayRef<Value *>());
  builder.SetInsertPoint(L->getExitBlock()->getTerminator());
  Function *SSRDisable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_disable);
  builder.CreateCall(SSRDisable->getFunctionType(), SSRDisable, ArrayRef<Value *>());

  errs()<<"generated ssr_enable and ssr_disable\n";
  return;
}

bool isValid(const AffineAcc *A){
  if (!A) return false;
  if (A->getDimension() > SSR_MAX_DIM) return false;
  unsigned n_store = 0U;
  unsigned n_load = 0U;
  bool valid = true;
  for (auto *I : A->getAccesses()){
    valid = valid && CHECK_TYPE(I);
    if (dyn_cast<LoadInst>(I)) n_load++;
    else if(dyn_cast<StoreInst>(I)) n_store++;
    else assert(false && "non load/store instruction in AffineAcc::accesses ?");
    if(!valid) break;
  }
  return valid && ((n_store > 0U && n_load == 0U) || (n_store == 0U && n_load > 0U)); 
}

struct ConflictGraph{
  using NodeT = const AffineAcc *;

  ///accs assumed to be valid
  ConflictGraph(const AffineAccess &AF, ArrayRef<NodeT> accesses)  : AF(AF){ 
    for (auto A = accesses.begin(); A != accesses.end(); A++){
      conflicts.insert(std::make_pair(*A, std::vector<NodeT>()));
      mutexs.insert(std::make_pair(*A, std::vector<NodeT>()));
      for (auto B = accesses.begin(); B != A; B++){
        assert(conflicts.find(*B) != conflicts.end());
        assert(mutexs.find(*B) != mutexs.end());
        if (AF.shareInsts(*A, *B) || AF.conflictWWWR(*A, *B)){
          mutexs.find(*A)->second.push_back(*B);
          mutexs.find(*B)->second.push_back(*A);
        }else if (AF.shareLoops(*A, *B)){
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
    auto isBetter = [](NodeT A, NodeT B){ return A->getDimension() > B->getDimension(); };
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
  
  AffineAccess &AF = FAM.getResult<AffineAccessAnalysis>(F);

  errs()<<"SSR Generation Pass on function: "<<F.getNameOrAsOperand()<<"\n";

  SmallPtrSet<const Loop *, 4U> changedLoops;

  auto accs = AF.getAccesses();

  std::vector<const AffineAcc *> goodAccs;
  for (const AffineAcc *A : accs){
    auto p = AF.splitLoadStore(A);
    if (p.first && isValid(p.first)) goodAccs.push_back(p.first);
    if (p.second && isValid(p.second)) goodAccs.push_back(p.second);
  }

  ConflictGraph g(AF, ArrayRef<const AffineAcc *>(goodAccs));
  const auto &clr = g.color(NUM_SSR);
  errs()<<"computed coloring\n";

  for (const auto &C : clr){
    if (C.second.hasValue()){ //not None
      generateSSR(AF, C.first, C.second.getValue(), C.first->getNStore() > 0U);
      errs()<<"generated ssr insts \n";

      addChangedLoop(C.first->getLoop(), changedLoops);
    }
  }

  for (const Loop *L : changedLoops) generateSSREnDis(L);

  return changedLoops.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
}


/*
std::vector<const AffineAcc *> allaccesses;
  for (const AffineAcc *A : accs) allaccesses.push_back(A); 

  //sort by dimension ascending
  std::sort(allaccesses.begin(), allaccesses.end(), [](const AffineAcc *A, const AffineAcc *B){return A->getDimension() <= B->getDimension();});

  errs()<<"total of "<<allaccesses.size()<<" AffineAcc\n";
  
  std::vector<const AffineAcc *> accesses;
  while (!allaccesses.empty()){
    auto A = allaccesses.back(); allaccesses.pop_back();
    if (!isValid(A)) continue;
    bool conflict = false;
    for (auto B : accesses){
      conflict = conflict || shareInsts(A, B);
    }
    if (!conflict) accesses.push_back(A);
  }

  errs()<<accesses.size()<<" AffineAcc useful\n";

  unsigned dmid = 0U;
  for (const AffineAcc *A : accesses){
    if (dmid >= NUM_SSR) break;
    unsigned n_store = A->getNStore(), n_load = A->getNLoad();
    generateSSR(AF, A, dmid, n_store + n_load, n_store > 0U);
    changedLoops.insert(A->getLoop());
    dmid++;
  }
  */