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
  let CompleteModel = 0;
  let UnsupportedFeatures = [HasStdExtZbkb, HasStdExtZbkc, HasStdExtZbkx,
                             HasStdExtZknd, HasStdExtZkne, HasStdExtZknh,
                             HasStdExtZksed, HasStdExtZksh, HasStdExtZkr,
                             HasStdExtZbs, HasVInstructionsAnyF];
}

//===----------------------------------------------------------------------===//
// Define each kind of processor resource and number available.

// Modeling each pipeline as a ProcResource using the BufferSize = 0 since
// Snitch is in-order.

let BufferSize = 0 in {
def MempoolUnitSnitch  : ProcResource<1>; // Snitch's decoder and ALU
def MempoolUnitIMulDiv : ProcResource<1>; // Mul/Div accelerator
}

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedWrite types which both map the ProcResources and
// set the latency.

let SchedModel = MempoolModel in {

def : WriteRes<WriteJmp, [MempoolUnitSnitch]>;
def : WriteRes<WriteJal, [MempoolUnitSnitch]>;
def : WriteRes<WriteJalr, [MempoolUnitSnitch]>;

def : WriteRes<WriteIALU, [MempoolUnitSnitch]>;
def : WriteRes<WriteShiftImm, [MempoolUnitSnitch]>;
def : WriteRes<WriteShiftReg, [MempoolUnitSnitch]>;
def : WriteRes<WriteShiftReg32, [MempoolUnitSnitch]>;

// Multiplies on Snitch are pipelines and take four cycles
def : WriteRes<WriteIMul, [MempoolUnitSnitch, MempoolUnitIMulDiv]> {
let Latency = 3;
let ReleaseAtCycles = [1, 1];
}
def : WriteRes<WriteIDiv, [MempoolUnitSnitch, MempoolUnitIMulDiv]> {
let Latency = 16;
let ReleaseAtCycles = [1, 14];
}

// Memory
def : WriteRes<WriteSTB, [MempoolUnitSnitch]>;
def : WriteRes<WriteSTH, [MempoolUnitSnitch]>;
def : WriteRes<WriteSTW, [MempoolUnitSnitch]>;

let Latency = 8 in {
def : WriteRes<WriteLDB, [MempoolUnitSnitch]>;
def : WriteRes<WriteLDH, [MempoolUnitSnitch]>;
def : WriteRes<WriteLDW, [MempoolUnitSnitch]>;
def : WriteRes<WriteAtomicW, [MempoolUnitSnitch]>;
def : WriteRes<WriteAtomicLDW, [MempoolUnitSnitch]>;
}

def : WriteRes<WriteCSR, [MempoolUnitSnitch]>;
def : WriteRes<WriteAtomicSTW, [MempoolUnitSnitch]>;

def : WriteRes<WriteNop, []>;

def : InstRW<[WriteIALU], (instrs COPY)>;

let Unsupported = 1 in {
def : WriteRes<WriteBREV8, []>;  
def : WriteRes<WriteIALU32, []>;
def : WriteRes<WriteShiftImm32, []>;
def : WriteRes<WriteIMul32, []>;
def : WriteRes<WriteIDiv32, []>;
def : WriteRes<WriteSFB, []>;
def : WriteRes<WriteSTD, []>;
def : WriteRes<WriteLDD, []>;
def : WriteRes<WriteAtomicD, []>;
def : WriteRes<WriteAtomicLDD, []>;
def : WriteRes<WriteAtomicSTD, []>;
def : WriteRes<WriteFAdd32, []>;
def : WriteRes<WriteFMul32, []>;
def : WriteRes<WriteFMA32, []>;
def : WriteRes<WriteFMul64, []>;
def : WriteRes<WriteFMA64, []>;
def : WriteRes<WriteFDiv32, []>;
def : WriteRes<WriteFDiv64, []>;
def : WriteRes<WriteFSqrt32, []>;
def : WriteRes<WriteFSqrt64, []>;
def : WriteRes<WriteFAdd64, []>;
def : WriteRes<WriteFCvtI32ToF32, []>;
def : WriteRes<WriteFCvtI32ToF64, []>;
def : WriteRes<WriteFCvtI64ToF32, []>;
def : WriteRes<WriteFCvtI64ToF64, []>;
def : WriteRes<WriteFCvtF32ToI32, []>;
def : WriteRes<WriteFCvtF32ToI64, []>;
def : WriteRes<WriteFCvtF64ToI32, []>;
def : WriteRes<WriteFCvtF64ToI64, []>;
def : WriteRes<WriteFCvtF32ToF64, []>;
def : WriteRes<WriteFCvtF64ToF32, []>;
def : WriteRes<WriteFClass32, []>;
def : WriteRes<WriteFClass64, []>;
def : WriteRes<WriteFCmp32, []>;
def : WriteRes<WriteFCmp64, []>;
def : WriteRes<WriteFSGNJ32, []>;
def : WriteRes<WriteFSGNJ64, []>;
def : WriteRes<WriteFMinMax32, []>;
def : WriteRes<WriteFMinMax64, []>;
def : WriteRes<WriteFMovF32ToI32, []>;
def : WriteRes<WriteFMovI32ToF32, []>;
def : WriteRes<WriteFMovF64ToI64, []>;
def : WriteRes<WriteFMovI64ToF64, []>;
def : WriteRes<WriteFLI16, []>;
def : WriteRes<WriteFLI32, []>;
def : WriteRes<WriteFLD32, []>;
def : WriteRes<WriteFLD64, []>;
def : WriteRes<WriteFST32, []>;
def : WriteRes<WriteFST64, []>;
def : WriteRes<WriteFLI64, []>;
def : WriteRes<WriteFRoundF16, []>;
def : WriteRes<WriteFRoundF32, []>;
def : WriteRes<WriteFRoundF64, []>;
def : WriteRes<WritePACK, []>;
def : WriteRes<WritePACK32, []>;
def : WriteRes<WriteZIP, []>;
def : WriteRes<WriteXPERM, []>;
}

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedRead types with cycles.
// Dummy definitions for MempoolCore.
def : ReadAdvance<ReadBREV8, 0>;
def : ReadAdvance<ReadJmp, 0>;
def : ReadAdvance<ReadJalr, 0>;
def : ReadAdvance<ReadCSR, 0>;
def : ReadAdvance<ReadStoreData, 0>;
def : ReadAdvance<ReadMemBase, 0>;
def : ReadAdvance<ReadIALU, 0>;
def : ReadAdvance<ReadIALU32, 0>;
def : ReadAdvance<ReadShiftImm, 0>;
def : ReadAdvance<ReadShiftImm32, 0>;
def : ReadAdvance<ReadShiftReg, 0>;
def : ReadAdvance<ReadShiftReg32, 0>;
def : ReadAdvance<ReadSFBJmp, 0>;
def : ReadAdvance<ReadSFBALU, 0>;
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
def : ReadAdvance<ReadFMemBase, 0>;
def : ReadAdvance<ReadFAdd32, 0>;
def : ReadAdvance<ReadFMul32, 0>;
def : ReadAdvance<ReadFMA32, 0>;
def : ReadAdvance<ReadFMA32Addend, 0>;
def : ReadAdvance<ReadFMul64, 0>;
def : ReadAdvance<ReadFDiv32, 0>;
def : ReadAdvance<ReadFDiv64, 0>;
def : ReadAdvance<ReadFMA64, 0>;
def : ReadAdvance<ReadFMA64Addend, 0>;
def : ReadAdvance<ReadFSqrt32, 0>;
def : ReadAdvance<ReadFSqrt64, 0>;
def : ReadAdvance<ReadFAdd64, 0>;
def : ReadAdvance<ReadFCmp32, 0>;
def : ReadAdvance<ReadFCmp64, 0>;
def : ReadAdvance<ReadFSGNJ32, 0>;
def : ReadAdvance<ReadFSGNJ64, 0>;
def : ReadAdvance<ReadFMinMax32, 0>;
def : ReadAdvance<ReadFMinMax64, 0>;
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
def : ReadAdvance<ReadFRoundF16, 0>;
def : ReadAdvance<ReadFRoundF32, 0>;
def : ReadAdvance<ReadFRoundF64, 0>;
def : ReadAdvance<ReadFStoreData, 0>;
def : ReadAdvance<ReadPACK, 0>;
def : ReadAdvance<ReadPACK32, 0>;
def : ReadAdvance<ReadZIP, 0>;
def : ReadAdvance<ReadXPERM, 0>;

//===----------------------------------------------------------------------===//
// Unsupported extensions
defm : UnsupportedSchedV;
defm : UnsupportedSchedZba;
defm : UnsupportedSchedZbb;
defm : UnsupportedSchedZbc;
defm : UnsupportedSchedZbs;
defm : UnsupportedSchedZfh;
}
