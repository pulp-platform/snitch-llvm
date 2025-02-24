//===-- RISCVISelLowering.h - RISCV DAG Lowering Interface ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the interfaces that RISCV uses to lower LLVM code into a
// selection DAG.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_RISCV_RISCVISELLOWERING_H
#define LLVM_LIB_TARGET_RISCV_RISCVISELLOWERING_H

#include "RISCV.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {
class RISCVSubtarget;
namespace RISCVISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET_FLAG,
  URET_FLAG,
  SRET_FLAG,
  MRET_FLAG,
  CALL,
  /// Select with condition operator - This selects between a true value and
  /// a false value (ops #3 and #4) based on the boolean result of comparing
  /// the lhs and rhs (ops #0 and #1) of a conditional expression with the
  /// condition code in op #2, a XLenVT constant from the ISD::CondCode enum.
  /// The lhs and rhs are XLenVT integers. The true and false values can be
  /// integer or floating point.
  SELECT_CC,
  BuildPairF64,
  SplitF64,
  TAIL,
  // RV64I shifts, directly matching the semantics of the named RISC-V
  // instructions.
  SLLW,
  SRAW,
  SRLW,
  // 32-bit operations from RV64M that can't be simply matched with a pattern
  // at instruction selection time. These have undefined behavior for division
  // by 0 or overflow (divw) like their target independent counterparts.
  DIVW,
  DIVUW,
  REMUW,
  // RV64IB rotates, directly matching the semantics of the named RISC-V
  // instructions.
  ROLW,
  RORW,
  // RV64IB funnel shifts, with the semantics of the named RISC-V instructions,
  // but the same operand order as fshl/fshr intrinsics.
  FSRW,
  FSLW,
  // FPR<->GPR transfer operations when the FPR is smaller than XLEN, needed as
  // XLEN is the only legal integer width.
  //
  // FMV_H_X matches the semantics of the FMV.H.X.
  // FMV_X_ANYEXTH is similar to FMV.X.H but has an any-extended result.
  // FMV_W_X_RV64 matches the semantics of the FMV.W.X.
  // FMV_X_ANYEXTW_RV64 is similar to FMV.X.W but has an any-extended result.
  //
  // This is a more convenient semantic for producing dagcombines that remove
  // unnecessary GPR->FPR->GPR moves.
  FMV_H_X,
  FMV_X_ANYEXTH,
  FMV_W_X_RV64,
  FMV_X_ANYEXTW_RV64,
  // READ_CYCLE_WIDE - A read of the 64-bit cycle CSR on a 32-bit target
  // (returns (Lo, Hi)). It takes a chain operand.
  READ_CYCLE_WIDE,
  // Generalized Reverse and Generalized Or-Combine - directly matching the
  // semantics of the named RISC-V instructions. Lowered as custom nodes as
  // TableGen chokes when faced with commutative permutations in deeply-nested
  // DAGs. Each node takes an input operand and a TargetConstant immediate
  // shift amount, and outputs a bit-manipulated version of input. All operands
  // are of type XLenVT.
  GREVI,
  GREVIW,
  GORCI,
  GORCIW,
  // Vector Extension
  // VMV_X_S matches the semantics of vmv.x.s. The result is always XLenVT
  // sign extended from the vector element size. NOTE: The result size will
  // never be less than the vector element size.
  VMV_X_S,
  // Splats an i64 scalar to a vector type (with element type i64) where the
  // scalar is a sign-extended i32.
  SPLAT_VECTOR_I64,
  // Read VLENB CSR
  READ_VLENB,
  // Truncates a RVV integer vector by one power-of-two.
  TRUNCATE_VECTOR,
  // Unit-stride fault-only-first load
  VLEFF,
  VLEFF_MASK,
  // Unit-stride fault-only-first segment load
  VLSEGFF,
  VLSEGFF_MASK,
  // read vl CSR
  READ_VL,
  // Matches the semantics of vslideup/vslidedown. The first operand is the
  // pass-thru operand, the second is the source vector, and the third is the
  // XLenVT index (either constant or non-constant).
  VSLIDEUP,
  VSLIDEDOWN,
  // Matches the semantics of the unmasked vid.v instruction.
  VID,
};
} // namespace RISCVISD

class RISCVTargetLowering : public TargetLowering {
  const RISCVSubtarget &Subtarget;

public:
  explicit RISCVTargetLowering(const TargetMachine &TM,
                               const RISCVSubtarget &STI);

  const RISCVSubtarget &getSubtarget() const { return Subtarget; }

  bool getTgtMemIntrinsic(IntrinsicInfo &Info, const CallInst &I,
                          MachineFunction &MF,
                          unsigned Intrinsic) const override;
  bool isLegalAddressingMode(const DataLayout &DL, const AddrMode &AM, Type *Ty,
                             unsigned AS,
                             Instruction *I = nullptr) const override;
  bool isLegalICmpImmediate(int64_t Imm) const override;
  bool isLegalAddImmediate(int64_t Imm) const override;
  bool isTruncateFree(Type *SrcTy, Type *DstTy) const override;
  bool isTruncateFree(EVT SrcVT, EVT DstVT) const override;
  bool isZExtFree(SDValue Val, EVT VT2) const override;
  bool isSExtCheaperThanZExt(EVT SrcVT, EVT DstVT) const override;
  bool isCheapToSpeculateCttz() const override;
  bool isCheapToSpeculateCtlz() const override;
  bool isFPImmLegal(const APFloat &Imm, EVT VT,
                    bool ForCodeSize) const override;

  bool hasBitPreservingFPLogic(EVT VT) const override;

  // Override default pointer type
  MVT getPointerTy(const DataLayout &DL, uint32_t AS = UINT32_MAX) const override;
  MVT getPointerMemTy(const DataLayout &DL, uint32_t AS = UINT32_MAX) const override;
  MVT getScalarShiftAmountTy(const DataLayout &DL, EVT) const override;

  // Provide custom lowering hooks for some operations.
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  void ReplaceNodeResults(SDNode *N, SmallVectorImpl<SDValue> &Results,
                          SelectionDAG &DAG) const override;

  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  bool targetShrinkDemandedConstant(SDValue Op, const APInt &DemandedBits,
                                    const APInt &DemandedElts,
                                    TargetLoweringOpt &TLO) const override;

  void computeKnownBitsForTargetNode(const SDValue Op,
                                     KnownBits &Known,
                                     const APInt &DemandedElts,
                                     const SelectionDAG &DAG,
                                     unsigned Depth) const override;
  unsigned ComputeNumSignBitsForTargetNode(SDValue Op,
                                           const APInt &DemandedElts,
                                           const SelectionDAG &DAG,
                                           unsigned Depth) const override;

  // This method returns the name of a target specific DAG node.
  const char *getTargetNodeName(unsigned Opcode) const override;

  ConstraintType getConstraintType(StringRef Constraint) const override;

  unsigned getInlineAsmMemConstraint(StringRef ConstraintCode) const override;

  std::pair<unsigned, const TargetRegisterClass *>
  getRegForInlineAsmConstraint(const TargetRegisterInfo *TRI,
                               StringRef Constraint, MVT VT) const override;

  void LowerAsmOperandForConstraint(SDValue Op, std::string &Constraint,
                                    std::vector<SDValue> &Ops,
                                    SelectionDAG &DAG) const override;

  MachineBasicBlock *
  EmitInstrWithCustomInserter(MachineInstr &MI,
                              MachineBasicBlock *BB) const override;

  EVT getSetCCResultType(const DataLayout &DL, LLVMContext &Context,
                         EVT VT) const override;

  bool convertSetCCLogicToBitwiseLogic(EVT VT) const override {
    return VT.isScalarInteger();
  }
  bool convertSelectOfConstantsToMath(EVT VT) const override { return true; }

  bool shouldInsertFencesForAtomic(const Instruction *I) const override {
    return isa<LoadInst>(I) || isa<StoreInst>(I);
  }
  Instruction *emitLeadingFence(IRBuilder<> &Builder, Instruction *Inst,
                                AtomicOrdering Ord) const override;
  Instruction *emitTrailingFence(IRBuilder<> &Builder, Instruction *Inst,
                                 AtomicOrdering Ord) const override;

  bool isFMAFasterThanFMulAndFAdd(const MachineFunction &MF,
                                  EVT VT) const override;

  ISD::NodeType getExtendForAtomicOps() const override {
    return ISD::SIGN_EXTEND;
  }

  ISD::NodeType getExtendForAtomicCmpSwapArg() const override {
    return ISD::SIGN_EXTEND;
  }

  bool shouldExpandShift(SelectionDAG &DAG, SDNode *N) const override {
    if (DAG.getMachineFunction().getFunction().hasMinSize())
      return false;
    return true;
  }
  bool isDesirableToCommuteWithShift(const SDNode *N,
                                     CombineLevel Level) const override;

  /// If a physical register, this returns the register that receives the
  /// exception address on entry to an EH pad.
  Register
  getExceptionPointerRegister(const Constant *PersonalityFn) const override;

  /// If a physical register, this returns the register that receives the
  /// exception typeid on entry to a landing pad.
  Register
  getExceptionSelectorRegister(const Constant *PersonalityFn) const override;

  bool shouldExtendTypeInLibCall(EVT Type) const override;
  bool shouldSignExtendTypeInLibCall(EVT Type, bool IsSigned) const override;

  /// Returns the register with the specified architectural or ABI name. This
  /// method is necessary to lower the llvm.read_register.* and
  /// llvm.write_register.* intrinsics. Allocatable registers must be reserved
  /// with the clang -ffixed-xX flag for access to be allowed.
  Register getRegisterByName(const char *RegName, LLT VT,
                             const MachineFunction &MF) const override;

  bool getPostIndexedAddressParts(SDNode *N, SDNode *Op, SDValue &Base,
                                  SDValue &Offset, ISD::MemIndexedMode &AM,
                                  SelectionDAG &DAG) const override;

  // Lower incoming arguments, copy physregs into vregs
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  bool CanLowerReturn(CallingConv::ID CallConv, MachineFunction &MF,
                      bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      LLVMContext &Context) const override;
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;
  SDValue LowerCall(TargetLowering::CallLoweringInfo &CLI,
                    SmallVectorImpl<SDValue> &InVals) const override;

  bool shouldConvertConstantLoadToIntImm(const APInt &Imm,
                                         Type *Ty) const override {
    return true;
  }
  bool mayBeEmittedAsTailCall(const CallInst *CI) const override;
  bool shouldConsiderGEPOffsetSplit() const override { return true; }

  bool decomposeMulByConstant(LLVMContext &Context, EVT VT,
                              SDValue C) const override;

  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicRMWInIR(AtomicRMWInst *AI) const override;
  Value *emitMaskedAtomicRMWIntrinsic(IRBuilder<> &Builder, AtomicRMWInst *AI,
                                      Value *AlignedAddr, Value *Incr,
                                      Value *Mask, Value *ShiftAmt,
                                      AtomicOrdering Ord) const override;
  TargetLowering::AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(AtomicCmpXchgInst *CI) const override;
  Value *emitMaskedAtomicCmpXchgIntrinsic(IRBuilder<> &Builder,
                                          AtomicCmpXchgInst *CI,
                                          Value *AlignedAddr, Value *CmpVal,
                                          Value *NewVal, Value *Mask,
                                          AtomicOrdering Ord) const override;

private:
  void analyzeInputArgs(MachineFunction &MF, CCState &CCInfo,
                        const SmallVectorImpl<ISD::InputArg> &Ins,
                        bool IsRet) const;
  void analyzeOutputArgs(MachineFunction &MF, CCState &CCInfo,
                         const SmallVectorImpl<ISD::OutputArg> &Outs,
                         bool IsRet, CallLoweringInfo *CLI) const;

  template <class NodeTy>
  SDValue getAddr(NodeTy *N, SelectionDAG &DAG, bool IsLocal = true) const;

  SDValue getStaticTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG,
                           bool UseGOT) const;
  SDValue getDynamicTLSAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;

  SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerBlockAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerConstantPool(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerJumpTable(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalTLSAddress(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerSELECT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVASTART(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerFRAMEADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerRETURNADDR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftLeftParts(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerShiftRightParts(SDValue Op, SelectionDAG &DAG, bool IsSRA) const;
  SDValue lowerSPLATVECTOR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerVectorMaskExt(SDValue Op, SelectionDAG &DAG,
                             int64_t ExtTrueVal) const;
  SDValue lowerVectorMaskTrunc(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerINSERT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerEXTRACT_VECTOR_ELT(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_WO_CHAIN(SDValue Op, SelectionDAG &DAG) const;
  SDValue LowerINTRINSIC_W_CHAIN(SDValue Op, SelectionDAG &DAG) const;

  bool isEligibleForTailCallOptimization(
      CCState &CCInfo, CallLoweringInfo &CLI, MachineFunction &MF,
      const SmallVector<CCValAssign, 16> &ArgLocs) const;

  /// Generate error diagnostics if any register used by CC has been marked
  /// reserved.
  void validateCCReservedRegs(
      const SmallVectorImpl<std::pair<llvm::Register, llvm::SDValue>> &Regs,
      MachineFunction &MF) const;

  bool allowsMisalignedMemoryAccesses(EVT, unsigned AddrSpace, unsigned Align,
                                      MachineMemOperand::Flags Flags,
                                      bool *Fast) const override;
};

namespace RISCVVIntrinsicsTable {

struct RISCVVIntrinsicInfo {
  unsigned int IntrinsicID;
  unsigned int ExtendedOperand;
};

using namespace RISCV;

#define GET_RISCVVIntrinsicsTable_DECL
#include "RISCVGenSearchableTables.inc"

} // end namespace RISCVVIntrinsicsTable

namespace RISCVZvlssegTable {

struct RISCVZvlsseg {
  unsigned int IntrinsicID;
  unsigned int SEW;
  unsigned int LMUL;
  unsigned int IndexLMUL;
  unsigned int Pseudo;
};

using namespace RISCV;

#define GET_RISCVZvlssegTable_DECL
#include "RISCVGenSearchableTables.inc"

} // namespace RISCVZvlssegTable
}

#endif
