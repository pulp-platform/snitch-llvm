#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"

#include <iostream>
#include <vector>

namespace llvm {

class AffineAcc{
public:
  AffineAcc(const Loop *L, ArrayRef<Instruction *> instructions, const SCEV *data, const SCEV *bound, const SCEV *stride);
  void dump();
  unsigned getDimension();
private:
  void addLoop(const Loop *L, const SCEV *bound, const SCEV *stride); //add dimension
  
  const SCEV *data;
  SmallVector<const SCEV *, 2> bounds; //from outer- to innermost loop
  SmallVector<const SCEV *, 2> strides; //from outer- to innermost loop
  SmallVector<Instruction *, 2> instructions; //instructions that are accessing the memory according to data, bounds, and strides.
  const Loop *L; //outermost loop
};

class AffineAccess{
public:
  AffineAccess(ScalarEvolution &SE) :SE(SE) {}
  void addAccess(AffineAcc &a);
  ArrayRef<AffineAcc> getAll();
private:
  SmallVector<AffineAcc> accesses;
  ScalarEvolution &SE;
};

class AffineAccessAnalysis : public AnalysisInfoMixin<AffineAccessAnalysis> {
  friend AnalysisInfoMixin<AffineAccessAnalysis>;
  static AnalysisKey Key;

public:
  using Result = AffineAccess;
  Result run(Function &F, FunctionAnalysisManager &AM);
};

// This is the analysis pass that will be invocable via opt
class AffineAccessAnalysisPass : public AnalysisInfoMixin<AffineAccessAnalysisPass> {
  raw_ostream &OS;

public:
  explicit AffineAccessAnalysisPass(raw_ostream &OS) : OS(OS) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm