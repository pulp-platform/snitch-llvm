//===-- SSRInference.cpp - Infer SSR usage --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/SSR/SSRInference.h"
#include "llvm/InitializePasses.h"
#include "llvm/Passes/PassBuilder.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Casting.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/IntrinsicsRISCV.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/ilist.h"

#include <array>
#include <vector>

#define SSR_NUM_DMS 3

using namespace llvm;

static cl::opt<bool> EnableSSRInference("ssr-inference", cl::Hidden, cl::init(false),
                     cl::desc("inference of SSR intrinsics"));

SSRStream::SSRStream(Loop *L, ilist<Instruction> *setup, ArrayRef<Instruction *> insts, 
    unsigned dim, Value *data, Value *bound, Value *stride, bool isStore) 
    : L(L), setup(setup), moveInsts(), isStore(isStore), _isgen(false), 
    dim(dim), data(data), bound(bound), stride(stride), dm(-1), conflicts() 
    {
      moveInsts.append<ArrayRef<Instruction *>::iterator>(insts.begin(), insts.end());
      assert(L && setup && "input not null");
      assert(dim > 0 && "correct dimension");
      assert(data->getType() == Type::getInt8PtrTy(L->getHeader()->getContext()));
      assert(bound->getType() == Type::getInt32Ty(L->getHeader()->getContext()));
      assert(stride->getType() == Type::getInt32Ty(L->getHeader()->getContext()));
    }

int SSRStream::getDM() { return dm; }
void SSRStream::setDM(int dmId) { dm = dmId; }

void SSRStream::GenerateSSRInstructions(){
  assert(!_isgen && "this stream has not generated its instructions yet");
  this->_isgen = true;
  assert(this->dm >= 0 && this->dm < SSR_NUM_DMS && "stream has valid dm id");

  Module *mod = L->getHeader()->getModule(); //module for function declarations, TODO: is this the correct one?
  IntegerType *i32 = IntegerType::getInt32Ty(L->getHeader()->getContext());

  Instruction *point = L->getLoopPreheader()->getTerminator();

  Instruction *i = &setup->front();
  while(i){
    Instruction *iNext = setup->getNextNode(*i);
    i->insertBefore(point);
    i = iNext;
  }

  IRBuilder<> builder(point);

  ConstantInt *dm = ConstantInt::get(i32, this->dm); //datamover id, ty=i32
  ConstantInt *dim = ConstantInt::get(i32, this->dim - 1); //dimension - 1, ty=i32
  // data pointer, ty=i8*
  Function *SSRReadSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_read_imm); //can take _imm bc dm and dim are constant
  std::array<Value *, 3> args = {dm, dim, data};
  builder.CreateCall(SSRReadSetup->getFunctionType(), SSRReadSetup, ArrayRef<Value *>(args));

  errs()<<"generated ssr_read_imm \n";

  ConstantInt *rep; //repetition - 1, ty=i32
  rep = ConstantInt::get(i32, moveInsts.size());
  Function *SSRRepetitionSetup = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_setup_repetition);
  std::array<Value *, 2> repargs = {dm, rep};
  builder.CreateCall(SSRRepetitionSetup->getFunctionType(), SSRRepetitionSetup, ArrayRef<Value *>(repargs));

  errs()<<"generated ssr_setup_repetitions \n";

  //bound - 1, ty=i32, relative stride, ty=i32
  Function *SSRBoundStrideSetup1D = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_setup_bound_stride_1d);
  std::array<Value *, 3> bsargs = {dm, bound, stride};
  builder.CreateCall(SSRBoundStrideSetup1D->getFunctionType(), SSRBoundStrideSetup1D, ArrayRef<Value *>(bsargs));

  errs()<<"generated ssr_setup_bound_stride_1d \n";

  if (isStore){
    errs()<<"store not done yet \n";
  }else{
    Function *SSRPop = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_pop);
    std::array<Value *, 1> poparg = {dm};
    for (Instruction *I : moveInsts){
      builder.SetInsertPoint(I);
      Value *v = builder.CreateCall(SSRPop->getFunctionType(), SSRPop, ArrayRef<Value *>(poparg), "ssr.pop");
      BasicBlock::iterator ii(I);
      ReplaceInstWithValue(I->getParent()->getInstList(), ii, v);
    }
  }

  return;
}


namespace{

/// guarantees: 
/// L has 1 preheader and 1 dedicated exit
/// L has 1 backedge and 1 exiting block
/// bt SCEV can be expanded to instructions at insertionsPoint
bool checkLoop(const Loop *L, DominatorTree *DT, ScalarEvolution *SE, Instruction *InsertionPoint){
  if (!L->isLCSSAForm(*DT) || !L->getLoopPreheader() || !L->getExitBlock() 
    || !L->getExitBlock() || !L->hasDedicatedExits() || L->getNumBackEdges() != 1){
      errs()<<"malformed loop: "; L->dump();
      return false;
  }
  if (!SE->hasLoopInvariantBackedgeTakenCount(L)){
    errs()<<"cannot calculate backedge taken count\n";
    return false;
  }
  const SCEV *bt = SE->getBackedgeTakenCount(L);
  if(!isSafeToExpandAt(bt, InsertionPoint, *SE) /*|| !SE->isAvailableAtLoopEntry(bt, L)*/){
    errs()<<"cannot expand bt SCEV: "; bt->dump();
  }
  errs()<<"loop is well-formed: "; bt->dump();
  return true;
}

/// check whether BB is on all controlflow paths from header to header
bool isOnAllControlFlowPaths(const BasicBlock *BB, const Loop *L, const DominatorTree *DT){
  return DT->dominates(BB, L->getHeader());
}

bool runOnLoop(
    const Loop *L, AAResults *AA, LoopInfo *LI, DominatorTree *DT,
    BlockFrequencyInfo *BFI, TargetLibraryInfo *TLI, TargetTransformInfo *TTI,
    ScalarEvolution *SE, MemorySSA *MSSA) {
  L->dump();

  if (!L->getLoopPreheader() || !checkLoop(L, DT, SE, L->getLoopPreheader()->getTerminator())) return true;

  SmallVector<SSRStream *, 3>  streams;

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

      const SCEV *AddrSCEV = SE->getSCEVAtScope(Addr, L);
      if (!SE->hasComputableLoopEvolution(AddrSCEV, L)) continue;
      errs()<<"has computable loop evolution: "; AddrSCEV->dump();

      auto split = SE->SplitIntoInitAndPostInc(L, AddrSCEV);
      const SCEV *SetupAddrSCEV = split.first;
      const SCEV *PostIncSCEV = split.second;
      if (!isSafeToExpandAt(SetupAddrSCEV, L->getLoopPreheader()->getTerminator(), *SE)) continue;
      errs()<<"can expand setup addr scev in preheader: "; SetupAddrSCEV->dump();
      if (!isSafeToExpandAt(PostIncSCEV, L->getLoopPreheader()->getTerminator(), *SE)) continue;
      errs()<<"can expand post inc addr scev in preheader: "; PostIncSCEV->dump();


    }
  }

  return true;
}

} //end of namespace

PreservedAnalyses SSRInferencePass::run(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR, LPMUpdater &){
  //if (!EnableSSRInference) return PreservedAnalyses::all(); //if flag is not set, skip
  errs()<<"# =============== SSR Inference =============== #\n";
  runOnLoop(&L, &AR.AA, &AR.LI, &AR.DT, AR.BFI, &AR.TLI, &AR.TTI, &AR.SE, AR.MSSA);
  errs()<<"# =============== SSR done      =============== #\n";
  return PreservedAnalyses::none();
}




/*
  const SCEV *bt = SE->getBackedgeTakenCount(L);
  SCEVExpander ex(*SE, L->getHeader()->getModule()->getDataLayout(), "backedge.taken");
  ex.setInsertPoint(L->getLoopPreheader()->getTerminator());
  Value *v = ex.expandCodeFor(bt);
  v->dump();

Value *SCEVtoValues(const SCEV *scev, ilist<Instruction> *insns){
  assert(scev && insns && "arguments should not be null");
  errs()<<"\t";
  scev->dump();
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
      Value *v = SCEVtoValues(castSCEV->getOperand(0), insns);
      if (v){
        Instruction *i;
        switch (scev->getSCEVType())
        {
        case SCEVTypes::scTruncate:
          i = CastInst::CreateTruncOrBitCast(v, castSCEV->getType(), "scev,trunc");
          break;
        case SCEVTypes::scZeroExtend:
          i = CastInst::CreateZExtOrBitCast(v, castSCEV->getType(), "scev.zext");
          break;
        case SCEVTypes::scSignExtend:
          i = CastInst::CreateSExtOrBitCast(v, castSCEV->getType(), "scev.sext");
          break;
        default:
          assert(false && "should not happen!");
          break;
        }
        insns->push_back(i);
        return i;
      }
      return nullptr;
    }
    case SCEVTypes::scAddExpr:
    case SCEVTypes::scMulExpr:
    {
      const SCEVCommutativeExpr *binopSCEV = cast<SCEVCommutativeExpr>(scev);
      Value *v1 = SCEVtoValues(binopSCEV->getOperand(0), insns);
      Value *v2 = SCEVtoValues(binopSCEV->getOperand(1), insns);
      if (v1 && v2){
        Instruction *binop;
        if (binopSCEV->getSCEVType() == SCEVTypes::scAddExpr) {
          binop = BinaryOperator::CreateAdd(v1, v2, "rcev.add");
        } else {
          binop = BinaryOperator::CreateMul(v1, v2, "rcev.mul");
        }
        insns->push_back(binop);
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

ConstantInt *SCEVtoConstStep(const SCEV *scev, const SCEV *init, Loop *L){
  //FIXME: lots to do better here
  errs()<<"trying to find stepsize\n";
  scev->dump();
  if(const SCEVAddRecExpr *rec = dyn_cast<SCEVAddRecExpr>(scev)){
    errs()<<"add-rec-expr at root\n";
    if (rec->getLoop() == L && rec->getOperand(0) == init){
      errs()<<"loop and init match\n";
      if (const SCEVConstant *c = dyn_cast<SCEVConstant>(rec->getOperand(1))){
        return dyn_cast<ConstantInt>(c->getValue());
      }
    }
  }
  return nullptr;
}


std::vector<SSRStream *> streams;
  
  errs()<<"instructions with SSR replacement potential\n";
  for (BasicBlock *BB : L->blocks()){
    //FIXME: check whether block is on all paths from header to itself
    for (auto &I : *BB){
      if (LoadInst *load = dyn_cast<LoadInst>(&I)){
        if (load->getType() != Type::getDoubleTy(preheader->getContext())) continue;
        Value *addr = load->getOperand(0);
        const SCEV *addrScev = SE->getSCEV(addr);
        if (!SE->hasComputableLoopEvolution(addrScev, L)) {
          errs()<<"addrScev has no computable loop evolution:\n"; 
          addrScev->dump();
          continue;
        }
        errs()<<"load instr, addr instr, and scev of address:\n";
        load->dump(); addr->dump(); addrScev->dump();

        auto split = SE->SplitIntoInitAndPostInc(L, addrScev);
        const SCEV *init = split.first;
        //const SCEV *step = split.second;
        
        ilist<Instruction> *setup = new iplist<Instruction>();
        
        Value *baseAddr = SCEVtoValues(init, setup);
        
        assert(baseAddr && "some weird SCEV in init SCEV");
        errs()<<"init and it's value:\n";
        init->dump(); baseAddr->dump();

        ConstantInt *stepsize = SCEVtoConstStep(addrScev, init, L);
        if (!stepsize){
          errs()<<"failed to compute stepsize\n";
          return false;
        }
        errs()<<"step value:\n";
        stepsize->dump();

        Instruction *data = CastInst::CreatePointerCast(baseAddr, Type::getInt8PtrTy(preheader->getContext()), "data.cast");
        setup->push_back(data);
        Instruction *bound = CastInst::CreateIntegerCast(repcount, IntegerType::getInt32Ty(preheader->getContext()), false, "bound.cast");
        setup->push_back(bound);
        Instruction *stride = CastInst::CreateIntegerCast(stepsize, IntegerType::getInt32Ty(preheader->getContext()), false, "stride.cast");
        setup->push_back(stride);
        SSRStream *s = new SSRStream(L, setup, ArrayRef<Instruction *>(load), 1, data, bound, stride, false);        
        streams.push_back(s);
        errs()<<"constructed SSRStream \n";
      }else if(StoreInst *store = dyn_cast<StoreInst>(&I)){
        store->dump();
      }
    }
  }

  bool Changed = false;

  unsigned dmid = 0;

  for (SSRStream *s : streams){
    if (dmid >= SSR_NUM_DMS) break;
    s->setDM(dmid++);
    s->GenerateSSRInstructions();
    Changed = true;
  }

  if (Changed){
    //SE->forgetLoop(L); //TODO: maybe use SE->forgetValue instead
    errs()<<"inserting insns into preheader:\n";
    Instruction *c = &insns->back();
    while (c) {
      Instruction *c_ = insns->getPrevNode(*c);
      c->dump();
      c->insertBefore(&*preheader->begin());
      c = c_;
    }

    //add SSRenable and -disable calls in preheader and exit
    IRBuilder<> builder(preheader->getTerminator());
    Module *mod = preheader->getModule();
    std::array<Value *, 0> emptyargs = {};
    Function *SSREnable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_enable);
    builder.CreateCall(SSREnable->getFunctionType(), SSREnable, ArrayRef<Value *>(emptyargs));
    Function *SSRDisable = Intrinsic::getDeclaration(mod, Intrinsic::riscv_ssr_disable);
    builder.SetInsertPoint(L->getExitBlock()->getTerminator());
    builder.CreateCall(SSRDisable->getFunctionType(), SSRDisable, ArrayRef<Value *>(emptyargs));

  }else{
    while(!insns->empty()){
      insns->pop_back(); //delete instructions from back to from to not get live Use when Def is deleted
    }
  }

  errs()<<"done with loop:\n";
  L->dump();


*/