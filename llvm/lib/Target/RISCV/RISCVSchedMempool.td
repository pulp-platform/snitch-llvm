//==- RISCVSchedMempool.td - Mempool Scheduling Definitions -*- tablegen -*-=//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// ===---------------------------------------------------------------------===//
// The following definitions describe the simpler per-operand machine model.
// This works with MachineScheduler. See MCSchedule.h for details.

// Mempool machine model for scheduling and other instruction cost heuristics.
def MempoolModel : SchedMachineModel {
  let MicroOpBufferSize = 0;
  let IssueWidth = 1;
  let LoadLatency = 8;
  let MispredictPenalty = 1;
  let PostRAScheduler = 1;
  let CompleteModel = 0;
}

//===----------------------------------------------------------------------===//
// Define each kind of processor resource and number available.

// Modeling each pipeline as a ProcResource using the BufferSize = 0 since
// Snitch is in-order.

let BufferSize = 0 in {
def MempoolUnitALU        : ProcResource<1>; // Int ALU
def MempoolUnitIMulDiv    : ProcResource<1>; // Int Multiply
def MempoolUnitLSU        : ProcResource<1>; // Load/Store
def MempoolUnitRegFile    : ProcResource<1>; // RegisterFile
}

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedWrite types which both map the ProcResources and
// set the latency.

let SchedModel = MempoolModel in {

def : WriteRes<WriteJmp, [MempoolUnitALU]>;
def : WriteRes<WriteJal, [MempoolUnitALU]>;
def : WriteRes<WriteJalr, [MempoolUnitALU]>;
def : WriteRes<WriteJmpReg, [MempoolUnitALU]>;

def : WriteRes<WriteIALU, [MempoolUnitALU]>;
def : WriteRes<WriteShift, [MempoolUnitALU]>;

// Multiplies on Snitch are pipelines and take four cycles
def : WriteRes<WriteIMul, [MempoolUnitIMulDiv]> { let Latency = 4; }
def : WriteRes<WriteIDiv, [MempoolUnitIMulDiv]> { let Latency = 5; }

// Memory
def : WriteRes<WriteSTB, [MempoolUnitLSU]>;
def : WriteRes<WriteSTH, [MempoolUnitLSU]>;
def : WriteRes<WriteSTW, [MempoolUnitLSU]>;

let Latency = 8 in {
def : WriteRes<WriteLDB, [MempoolUnitLSU]>;
def : WriteRes<WriteLDH, [MempoolUnitLSU]>;
def : WriteRes<WriteLDW, [MempoolUnitLSU]>;
def : WriteRes<WriteAtomicW, [MempoolUnitLSU]>;
def : WriteRes<WriteAtomicLDW, [MempoolUnitLSU]>;
}

def : WriteRes<WriteCSR, [MempoolUnitALU]>;
def : WriteRes<WriteAtomicSTW, [MempoolUnitLSU]>;

def : WriteRes<WriteNop, []>;

def : InstRW<[WriteIALU], (instrs COPY)>;

let Unsupported = 1 in {
def : WriteRes<WriteIALU32, []>;
def : WriteRes<WriteShift32, []>;
def : WriteRes<WriteIMul32, []>;
def : WriteRes<WriteIDiv32, []>;
def : WriteRes<WriteSTD, []>;
def : WriteRes<WriteFST32, []>;
def : WriteRes<WriteFST64, []>;
def : WriteRes<WriteLDWU, []>;
def : WriteRes<WriteLDD, []>;
def : WriteRes<WriteFLD32, []>;
def : WriteRes<WriteFLD64, []>;
def : WriteRes<WriteAtomicD, []>;
def : WriteRes<WriteAtomicLDD, []>;
def : WriteRes<WriteAtomicSTD, []>;
def : WriteRes<WriteFALU32, []>;
def : WriteRes<WriteFALU64, []>;
def : WriteRes<WriteFDiv32, []>;
def : WriteRes<WriteFDiv64, []>;
def : WriteRes<WriteFCvtI64ToF32, []>;
def : WriteRes<WriteFCvtI64ToF64, []>;
def : WriteRes<WriteFCvtF64ToI64, []>;
def : WriteRes<WriteFCvtF32ToI64, []>;
def : WriteRes<WriteFMovI64ToF64, []>;
def : WriteRes<WriteFMovF64ToI64, []>;
def : WriteRes<WriteFCvtI32ToF32, []>;
def : WriteRes<WriteFCvtI32ToF64, []>;
def : WriteRes<WriteFCvtF32ToI32, []>;
def : WriteRes<WriteFCvtF64ToI32, []>;
def : WriteRes<WriteFCvtF32ToF64, []>;
def : WriteRes<WriteFCvtF64ToF32, []>;
def : WriteRes<WriteFClass32, []>;
def : WriteRes<WriteFClass64, []>;
def : WriteRes<WriteFCmp32, []>;
def : WriteRes<WriteFCmp64, []>;
def : WriteRes<WriteFMovF32ToI32, []>;
def : WriteRes<WriteFMovI32ToF32, []>;
def : WriteRes<WriteFMul32, []>;
def : WriteRes<WriteFMulAdd32, []>;
def : WriteRes<WriteFMulSub32, []>;
def : WriteRes<WriteFMul64, []>;
def : WriteRes<WriteFMulAdd64, []>;
def : WriteRes<WriteFMulSub64, []>;
def : WriteRes<WriteFSqrt32, []>;
def : WriteRes<WriteFSqrt64, []>;
}

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedRead types with cycles.
// Dummy definitions for MempoolCore.
def : ReadAdvance<ReadJmp, 0>;
def : ReadAdvance<ReadJalr, 0>;
def : ReadAdvance<ReadCSR, 0>;
def : ReadAdvance<ReadStoreData, 0>;
def : ReadAdvance<ReadMemBase, 0>;
def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadIALU32, 0>;
def : ReadAdvance<ReadShift, 0>;
def : ReadAdvance<ReadShift32, 0>;
def : ReadAdvance<ReadIDiv, 0>;
def : ReadAdvance<ReadIDiv32, 0>;
def : ReadAdvance<ReadIMul, 0>;
def : ReadAdvance<ReadIMul32, 0>;
def : ReadAdvance<ReadAtomicWA, 0>;
def : ReadAdvance<ReadAtomicWD, 0>;
def : ReadAdvance<ReadAtomicDA, 0>;
def : ReadAdvance<ReadAtomicDD, 0>;
def : ReadAdvance<ReadAtomicLDW, 0>;
def : ReadAdvance<ReadAtomicLDD, 0>;
def : ReadAdvance<ReadAtomicSTW, 0>;
def : ReadAdvance<ReadAtomicSTD, 0>;
def : ReadAdvance<ReadFALU32, 0>;
def : ReadAdvance<ReadFALU64, 0>;
def : ReadAdvance<ReadFMul32, 0>;
def : ReadAdvance<ReadFMulAdd32, 0>;
def : ReadAdvance<ReadFMulSub32, 0>;
def : ReadAdvance<ReadFMul64, 0>;
def : ReadAdvance<ReadFMulAdd64, 0>;
def : ReadAdvance<ReadFMulSub64, 0>;
def : ReadAdvance<ReadFDiv32, 0>;
def : ReadAdvance<ReadFDiv64, 0>;
def : ReadAdvance<ReadFSqrt32, 0>;
def : ReadAdvance<ReadFSqrt64, 0>;
def : ReadAdvance<ReadFCmp32, 0>;
def : ReadAdvance<ReadFCmp64, 0>;
def : ReadAdvance<ReadFCvtF32ToI32, 0>;
def : ReadAdvance<ReadFCvtF32ToI64, 0>;
def : ReadAdvance<ReadFCvtF64ToI32, 0>;
def : ReadAdvance<ReadFCvtF64ToI64, 0>;
def : ReadAdvance<ReadFCvtI32ToF32, 0>;
def : ReadAdvance<ReadFCvtI32ToF64, 0>;
def : ReadAdvance<ReadFCvtI64ToF32, 0>;
def : ReadAdvance<ReadFCvtI64ToF64, 0>;
def : ReadAdvance<ReadFCvtF32ToF64, 0>;
def : ReadAdvance<ReadFCvtF64ToF32, 0>;
def : ReadAdvance<ReadFMovF32ToI32, 0>;
def : ReadAdvance<ReadFMovI32ToF32, 0>;
def : ReadAdvance<ReadFMovF64ToI64, 0>;
def : ReadAdvance<ReadFMovI64ToF64, 0>;
def : ReadAdvance<ReadFClass32, 0>;
def : ReadAdvance<ReadFClass64, 0>;
}
