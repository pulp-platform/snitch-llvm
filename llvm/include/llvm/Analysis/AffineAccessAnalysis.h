#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"

#include <iostream>
#include <vector>
#include <utility>

namespace llvm {

class AffineAccess;
class AffineAccessAnalysis;
class LoopInfo;
class ScalarEvolution;
class MemorySSA;
class MemoryUseOrDef;

struct LoopRep{
private:
  ScalarEvolution &SE;
  DominatorTree &DT;
  const Loop *L;
  const SCEV *RepSCEV;
  Value *Rep = nullptr;
  SmallVector<const Loop *, 4U> containingLoops; //from inner- to outermost
  unsigned safeExpandBound; //exclusive bound

public:
  /// construct rep for this loop, if loop well-formed isAvaliable will give true
  LoopRep(const Loop *L, ArrayRef<const Loop *> contLoops, ScalarEvolution &SE, DominatorTree &DT);
  bool isAvailable() const;
  bool isOnAllCFPathsOfParentIfExecuted() const;
  const Loop *getLoop() const;
  const SCEV *getSCEV() const;
  const SCEV *getSCEVPlusOne() const;
  bool isSafeToExpandBefore(const Loop *L) const;

  ///expands LoopRep::RepSCEV at InsertBefore (if nullptr in preheader of loop)
  Value *expandAt(Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
};

enum ConflictKind {NoConflict = 0, MustNotIntersect, MustBeSame, Bad };

struct AffAcc{
private:
  ScalarEvolution &SE;
  MemoryUseOrDef *MA;
  SmallVector<Instruction *, 2U> accesses;       //the load/store (or call) instructions 
  SmallVector<const SCEV *, 4U> baseAddresses;   //base addresses depending on loop 
  SmallVector<const SCEV *, 4U> steps;           //steps per loop (0 if loop-inv) 
  SmallVector<LoopRep *, 4U> reps;               //loop reps 
  SmallVector<const Loop *, 4U> containingLoops; //from inner- to outermost
  DenseMap<const AffAcc *, std::pair<unsigned, ConflictKind>> conflicts; //conflicts to other Affine Accesses and starting from which dimension
  void findSteps(const SCEV *A, const SCEV *Factor, unsigned loop);

public:
  AffAcc() = delete;
  //immediately copies the contens of accesses and containingLoops
  AffAcc(ArrayRef<Instruction *> accesses, const SCEV *Addr, MemoryUseOrDef *MA, ArrayRef<const Loop *> containingLoops, ScalarEvolution &SE);
  ArrayRef<Instruction *> getAccesses() const;
  bool isWrite() const;
  unsigned getMaxDimension() const;
  bool isWellFormed(unsigned dimension) const;
  bool canExpandBefore(const Loop *L) const;
  void dump() const;
  unsigned loopToDimension(const Loop *L) const;
  ConflictKind getConflictFor(const AffAcc *A, unsigned dimension) const;
  ConflictKind getConflictInLoop(const AffAcc *A, const Loop *L) const;
  const SCEV *getBaseAddr(unsigned dim) const;
  const SCEV *getStep(unsigned dim) const;
  const SCEV *getRep(unsigned dim) const;
  const Loop *getLoop(unsigned dim) const;

  MemoryAccess *getMemoryAccess();
  ///finds all MemoryDefs that clobber this access's memory that prevent it from being prefetched before the loop
  ArrayRef<MemoryDef *> getAllClobberingFor(const Loop *L); 
  void addConflict(const AffAcc *A, unsigned startDimension, ConflictKind kind);
  void addConflictInLoop(const AffAcc *A, const Loop *StartLoop, ConflictKind kind);  
  bool promote(LoopRep *LR); ///does not check whether it is on all CF-paths for LR->getLoop()
  ///code gen:
  Value *expandBaseAddr(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
  Value *expandStep(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
  Value *expandRep(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
};

class AffineAccess{
private:
  ScalarEvolution &SE;
  DominatorTree &DT;
  LoopInfo &LI;
  MemorySSA &MSSA;
  AAResults &AA;
  DenseMap<MemoryUseOrDef *, AffAcc *> access;
  DenseMap<const Loop *, LoopRep *> reps;
  DenseMap<const Loop *, SmallVector<AffAcc *, 4U>> expandableAccesses;

  DenseSet<AffAcc *> analyze(const Loop *Parent, std::vector<const Loop *> &loopPath);
  void addConflictsForUse(AffAcc *A, const Loop *L);
  void addConflictsForDef(AffAcc *A, const Loop *L);
  void addConflict(AffAcc *A, AffAcc *B, const Loop *L, ConflictKind kind);
  
public:
  AffineAccess(Function &F, ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI, MemorySSA &MSSA, AAResults &AA);
  AffineAccess() = delete;
  bool accessPatternsMatch(const AffAcc *A, const AffAcc *B, const Loop *L) const;
  ScalarEvolution &getSE() const;
  DominatorTree &getDT() const;
  LoopInfo &getLI() const;
  MemorySSA &getMSSA() const;
  AAResults &getAA() const;
  ArrayRef<const Loop *> getLoopsInPreorder() const;
  ArrayRef<const AffAcc *> getExpandableAccesses(const Loop *L) const;
  const AffAcc *getAccess(Instruction *I) const;

  static Value *getAddress(Instruction *I);
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