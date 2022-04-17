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
#include "llvm/Target/TargetMachine.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/IRBuilder.h"

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Scalar/ADCE.h"

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/AffineAccessAnalysis.h"
#include "llvm/Transforms/Utils/ScalarEvolutionExpander.h"
#include "llvm/Transforms/Scalar/LoopStrengthReduce.h"
#include "llvm/Transforms/SSR/SSRGeneration.h"

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

using namespace llvm;

PreservedAnalyses SSRInferencePass::run(Function &F, FunctionAnalysisManager &FAM){
  errs()<<"SSR Inference Pass on function: "<<F.getNameOrAsOperand()<<"\n";
  FunctionPassManager FPM(true);
  FPM.addPass(LoopSimplifyPass()); //canonicalize loops
  FPM.addPass(LCSSAPass());        //put loops into LCSSA-form
  FPM.addPass(createFunctionToLoopPassAdaptor(LoopStrengthReducePass()));
  FPM.addPass(SSRGenerationPass());//runs AffineAccess analysis and generates SSR intrinsics
  FPM.addPass(ADCEPass());         //remove potential dead instructions that result from SSR replacement (dead code elim)
  return FPM.run(F, FAM);
}