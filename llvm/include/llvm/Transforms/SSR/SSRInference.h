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

namespace llvm {

class SSRInferencePass : public PassInfoMixin<SSRInferencePass> {
public:
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM, LoopStandardAnalysisResults &AR, LPMUpdater &);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_SSR_SSRINFERENCE_H
