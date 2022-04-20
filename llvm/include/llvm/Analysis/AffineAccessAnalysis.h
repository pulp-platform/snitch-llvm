#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/ADT/SmallVector.h"

#include <iostream>
#include <vector>
#include <utility>

namespace llvm {

class AffineAccess;
class AffineAccessAnalysis;

class AffineAcc{
  friend AffineAccess;
private:
  AffineAcc(Instruction *Addr, ArrayRef<Instruction *> accesses, const SCEV *data);
  
  const SCEV *data;
  SmallVector<const SCEV *, 3U> bounds; //from outer- to innermost loop
  SmallVector<const SCEV *, 3U> strides; //from outer- to innermost loop
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
  unsigned getNStore() const;
  unsigned getNLoad() const;
};

class AffineAccess{
private:
  SmallVector<const AffineAcc *, 0U> accesses; //accesses
  DenseMap<const Loop *, const SCEV *> loopReps; //wellformed loops & their bt counts
  DenseSet<const Instruction *> addresses; //already checked address instructions
  ScalarEvolution &SE;
  DominatorTree &DT;
  LoopInfo &LI;
  
public:
  AffineAccess(ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI);
  AffineAccess() = delete;
  void addAllAccesses(Instruction *Addr, const Loop *L);
  AffineAcc *promoteAccess(const AffineAcc &Acc, const Loop *L, const SCEV *Stride);
  std::pair<const AffineAcc *, const AffineAcc *> splitLoadStore(const AffineAcc *Acc) const;
  ArrayRef<const AffineAcc *> getAccesses() const;
  bool accessPatternsMatch(const AffineAcc *A, const AffineAcc *B) const;
  bool shareInsts(const AffineAcc *A, const AffineAcc *B) const;
  bool conflictWWWR(const AffineAcc *A, const AffineAcc *B) const;
  const SCEV *wellFormedLoopBTCount(const Loop *L) const; //returns bt count if loop is well-formed
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