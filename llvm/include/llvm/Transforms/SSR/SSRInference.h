//===-- SSRInference.h - Infer SSR usage ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_SSR_SSRINFERENCE_H
#define LLVM_TRANSFORMS_SSR_SSRINFERENCE_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

#include "llvm/IR/Value.h"
#include "llvm/IR/Instruction.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/ilist.h"

namespace llvm {

class SSRInferencePass : public PassInfoMixin<SSRInferencePass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR, LPMUpdater &);
};

class SSRStream{
public:
  SSRStream(Loop *L, ilist<Instruction> *setup, ArrayRef<Instruction *> insts, 
    unsigned dim, Value *data, Value *bound, Value *stride, bool isStore);

  int getDM();
  void setDM(int dmId);

  void GenerateSSRInstructions();

private:
  Loop *L;
  ilist<Instruction> *setup;

  SmallVector<Instruction *, 1> moveInsts; //likely to be just one or maybe two load/store insts

  bool isStore;

  bool _isgen;

  unsigned dim;
  Value *data;
  Value *bound;
  Value *stride;

  int dm; //"color"
  SmallVector<SSRStream *> conflicts; //"edges" to conflicting SSRStreams
};


} // namespace llvm

#endif // LLVM_TRANSFORMS_SSR_SSRINFERENCE_H
