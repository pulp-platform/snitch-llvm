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

#define NUM_SSR 3U

//current state of hw: only allow doubles
#define CHECK_TYPE(I) (I->getType() == Type::getDoubleTy(I->getParent()->getContext()))

using namespace llvm;

namespace{

void generateSSR(AffineAccess &AA, const AffineAcc *aa, unsigned dmid, unsigned n_insts, bool isStore){
  BasicBlock *LoopPreheader = aa->getLoop()->getLoopPreheader();
  Module *mod = LoopPreheader->getModule();
  LLVMContext &ctxt = LoopPreheader->getContext();
  IntegerType *i32 = IntegerType::getInt32Ty(ctxt);

  IRBuilder<> builder(LoopPreheader->getTerminator());

  ConstantInt *dm = ConstantInt::get(i32, dmid); //datamover id, ty=i32
  ConstantInt *dim = ConstantInt::get(i32, aa->getDimension() - 1U); //dimension - 1, ty=i32
  Value *data = AA.expandData(aa, Type::getInt8PtrTy(ctxt));
  Function *SSRReadSetup;
  if (!isStore){
    SSRReadSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_read_imm); //can take _imm bc dm and dim are constant
  }else{
    SSRReadSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_write_imm); //can take _imm bc dm and dim are constant
  }
  std::array<Value *, 3> args = {dm, dim, data};
  builder.CreateCall(SSRReadSetup->getFunctionType(), SSRReadSetup, ArrayRef<Value *>(args));

  errs()<<"generated ssr_read/write_imm \n";

  ConstantInt *rep = ConstantInt::get(i32, n_insts - 1U); //repetition - 1, ty=i32
  Function *SSRRepetitionSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_setup_repetition);
  std::array<Value *, 2> repargs = {dm, rep};
  builder.CreateCall(SSRRepetitionSetup->getFunctionType(), SSRRepetitionSetup, ArrayRef<Value *>(repargs));

  errs()<<"generated ssr_setup_repetitions \n";

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
    builder.CreateCall(SSRBoundStrideSetup->getFunctionType(), SSRBoundStrideSetup, ArrayRef<Value *>(bsargs));

    errs()<<"generated ssr_setup_bound_stride_"<<(d+1)<<"d \n";
  }

  if (isStore){
    Function *SSRPush = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_push);
    for (Instruction *I : aa->getAccesses()){
      std::array<Value *, 2> pusharg = {dm, cast<StoreInst>(I)->getValueOperand()};
      builder.SetInsertPoint(I);
      builder.CreateCall(SSRPush->getFunctionType(), SSRPush, ArrayRef<Value *>(pusharg));
      I->removeFromParent();
    }
  }else{
    Function *SSRPop = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_pop);
    std::array<Value *, 1> poparg = {dm};
    for (Instruction *I : aa->getAccesses()){
      builder.SetInsertPoint(I);
      Value *v = builder.CreateCall(SSRPop->getFunctionType(), SSRPop, ArrayRef<Value *>(poparg), "ssr.pop");
      BasicBlock::iterator ii(I);
      ReplaceInstWithValue(I->getParent()->getInstList(), ii, v);
    }
  }
  errs()<<"placed push/pop calls\n";
  return;
}

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

} //end of namespace

PreservedAnalyses SSRGenerationPass::run(Function &F, FunctionAnalysisManager &FAM){
  errs()<<"SSR Generation Pass on function: "<<F.getNameOrAsOperand()<<"\n";

  AffineAccess &AF = FAM.getResult<AffineAccessAnalysis>(F);


  SmallPtrSet<const Loop *, 8U> changedLoops;

  unsigned dmid = 0U;
  for (const auto *A : AF.getAll()){
    if (dmid >= NUM_SSR) break;
    
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

    errs()<<"current aa is "<<valid<<" with #st="<<n_store<<" #ld="<<n_load<<"\n";
    if(!valid || (n_store == 0U && n_load == 0U)) continue; 
    //all uses are valid load/stores and there is at least one of them

    generateSSR(AF, A, dmid, n_store + n_load, n_store > 0U);
    changedLoops.insert(A->getLoop());
    dmid++;
  }

  for (const Loop *L : changedLoops) generateSSREnDis(L);

  return changedLoops.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
}