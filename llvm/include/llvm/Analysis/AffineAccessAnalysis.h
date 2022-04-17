#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"

#include <iostream>
#include <vector>

namespace llvm {

class AffineAccess;
class AffineAccessAnalysis;

class AffineAcc{
  friend AffineAccess;
private:
  AffineAcc(Instruction *Addr, ArrayRef<Instruction *> accesses, const SCEV *data);
  
  const SCEV *data;
  SmallVector<const SCEV *, 2U> bounds; //from outer- to innermost loop
  SmallVector<const SCEV *, 2U> strides; //from outer- to innermost loop
  Instruction *Addr;
  SmallVector<Instruction *, 2U> accesses; //load/store instructions that use address (guaranteed to be in same loop)
  const Loop *L; //outermost loop

public:
  AffineAcc() = delete;
  void dump() const;
  unsigned getDimension() const;
  const Loop *getLoop() const;
  Instruction *getAddrIns() const;
  const SmallVector<Instruction *, 2U> &getAccesses() const;
};

class AffineAccess{
private:
  SmallVector<const AffineAcc *> accesses;
  DenseMap<const Loop *, const SCEV *> loopReps;
  ScalarEvolution &SE;
  DominatorTree &DT;
  LoopInfo &LI;
public:
  AffineAccess(ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI);
  AffineAccess() = delete;
  void addAllAccesses(Instruction *Addr, const Loop *L);
  AffineAcc *promoteAccess(const AffineAcc &Acc, const Loop *L, const SCEV *Stride);
  ArrayRef<const AffineAcc *> getAccesses() const;
  
  Value *expandData(const AffineAcc *aa, Type *ty = (Type *)nullptr) const;
  Value *expandBound(const AffineAcc *aa, unsigned i, Type *ty = (Type *)nullptr) const;
  Value *expandStride(const AffineAcc *aa, unsigned i, Type *ty = (Type *)nullptr) const;
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
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm