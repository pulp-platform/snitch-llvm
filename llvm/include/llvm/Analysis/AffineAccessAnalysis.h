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
class MemoryDef;
struct ExpandedAffAcc;

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
  Value *expandLoopGuard(Instruction *InsertBefore = (Instruction *)nullptr);
};

enum AffAccConflict { NoConflict = 0, MustNotIntersect = 1, Bad = 10};

struct AffAcc{
private:
  ScalarEvolution &SE;
  MemoryUseOrDef *MA;
  SmallVector<Instruction *, 2U> accesses;       //the load/store (or call) instructions 
  SmallVector<const SCEV *, 4U> baseAddresses;   //base addresses depending on loop 
  SmallVector<const SCEV *, 4U> steps;           //steps per loop (0 if loop-inv) 
  SmallVector<LoopRep *, 4U> reps;               //loop reps 
  SmallVector<const Loop *, 4U> containingLoops; //from inner- to outermost
  DenseMap<AffAcc *, std::pair<const Loop *, AffAccConflict>> conflicts;
  void findSteps(const SCEV *A, const SCEV *Factor, unsigned loop);

public:
  AffAcc() = delete;
  //immediately copies the contens of accesses and containingLoops
  AffAcc(ArrayRef<Instruction *> accesses, const SCEV *Addr, MemoryUseOrDef *MA, ArrayRef<const Loop *> containingLoops, ScalarEvolution &SE);
  ArrayRef<Instruction *> getAccesses() const;
  bool isWrite() const;
  unsigned getMaxDimension() const;
  bool isWellFormed(unsigned dimension) const;
  bool isWellFormed(const Loop *L) const;
  bool canExpandBefore(const Loop *L) const;
  void dump() const;
  void dumpInLoop(const Loop *L) const;
  unsigned loopToDimension(const Loop *L) const;
  const SCEV *getBaseAddr(unsigned dim) const;
  const SCEV *getStep(unsigned dim) const;
  const SCEV *getRep(unsigned dim) const;
  const Loop *getLoop(unsigned dim) const;
  ArrayRef<const Loop *> getContainingLoops() const;
  AffAccConflict getConflict(AffAcc *A, const Loop *L) const;

  MemoryUseOrDef *getMemoryAccess();
  void addConflict(AffAcc *A, const Loop *StartL, AffAccConflict kind);
  bool promote(LoopRep *LR); ///does not check whether it is on all CF-paths for LR->getLoop()
  ///code gen:
  Value *expandBaseAddr(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
  Value *expandStep(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
  Value *expandRep(unsigned dimension, Type *ty, Instruction *InsertBefore = (Instruction *)nullptr);
  ExpandedAffAcc expandAt(const Loop *L, Instruction *Point, Type *PtrTy, IntegerType *ParamTy, IntegerType *AgParamTy);
};

struct MemDep {
private:
  DenseMap<MemoryUseOrDef *, DenseSet<MemoryDef *>> clobbers;
  DenseMap<MemoryUseOrDef *, DenseSet<MemoryUseOrDef *>> clobberUsers;
  MemorySSA &MSSA;
  AAResults &AA;
  bool alias(Value *A, Value *B);
  bool alias(MemoryUseOrDef *A, MemoryUseOrDef *B);

public:
  MemDep(MemorySSA &MSSA, AAResults &AA) : MSSA(MSSA), AA(AA) {}
  const DenseSet<MemoryDef *> &findClobbers(MemoryUseOrDef *MA);
  std::vector<MemoryDef *> findClobbersInLoop(MemoryUseOrDef *MA, const Loop *L);
  const DenseSet<MemoryUseOrDef *> &findClobberUsers(MemoryDef *MA);
  std::vector<MemoryUseOrDef *> findClobberUsersInLoop(MemoryDef *MA, const Loop *L);
};

struct ExpandedAffAcc {
public:
  AffAcc *const Access;
  Value *const Addr;
  const SmallVector<Value *, 3U> Steps;
  const SmallVector<Value *, 3U> Reps;
  const SmallVector<Value *, 3U> Ranges;
  const SmallVector<Value *, 3U> PrefixSumRanges;
  Value *const LowerBound;
  Value *const UpperBound;
  unsigned getDimension() const { return Steps.size(); }
  ExpandedAffAcc (AffAcc *A, Value *Addr, ArrayRef<Value *> Steps, ArrayRef<Value *> Reps, 
    ArrayRef<Value *> Ranges, ArrayRef<Value *> PSRanges, Value *LowerBound, Value *UpperBound) 
    : Access(A), Addr(Addr), Steps(Steps.begin(), Steps.end()), Reps(Reps.begin(), Reps.end()), 
      Ranges(Ranges.begin(), Ranges.end()), PrefixSumRanges(PSRanges.begin(), PSRanges.end()), 
      LowerBound(LowerBound), UpperBound(UpperBound) { }
};

class AffineAccess{
private:
  ScalarEvolution &SE;
  DominatorTree &DT;
  LoopInfo &LI;
  MemorySSA &MSSA;
  AAResults &AA;
  MemDep MD;
  DenseMap<MemoryUseOrDef *, AffAcc *> access;
  DenseMap<const Loop *, LoopRep *> reps;
  DenseMap<const Loop *, SmallVector<AffAcc *, 4U>> wellformedAccesses;
  DenseMap<const Loop *, SmallVector<AffAcc *, 3U>> expandableAccesses;

  std::vector<AffAcc *> analyze(const Loop *Parent, ArrayRef<const Loop *> loopPath);
  void addAllConflicts(const std::vector<AffAcc *> &all);
  AffAccConflict getRWConflict(AffAcc *Read, AffAcc *Write, const Loop *L);
  
public:
  AffineAccess(Function &F, ScalarEvolution &SE, DominatorTree &DT, LoopInfo &LI, MemorySSA &MSSA, AAResults &AA);
  AffineAccess() = delete;
  bool accessPatternsMatch(const AffAcc *A, const AffAcc *B, const Loop *L) const;
  bool accessPatternsAndAddressesMatch(const AffAcc *A, const AffAcc *B, const Loop *L) const;
  ScalarEvolution &getSE() const;
  DominatorTree &getDT() const;
  LoopInfo &getLI() const;
  MemorySSA &getMSSA() const;
  AAResults &getAA() const;
  SmallVector<Loop *, 4U> getLoopsInPreorder() const;

  ArrayRef<AffAcc *> getExpandableAccesses(const Loop *L);
  std::vector<ExpandedAffAcc> expandAllAt(ArrayRef<AffAcc *> Accs, const Loop *L, Instruction *Point, 
    Value *&BoundCheck, Type *PtrTy, IntegerType *ParamTy, IntegerType *AgParamTy);
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