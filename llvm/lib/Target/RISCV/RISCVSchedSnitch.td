//==- RISCVSchedSnitch.td - Snitch Scheduling Definitions --*- tablegen -*-=//
//
// Copyright 2021 ETH Zurich, University of Bologna.
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//

// Snitch machine model for scheduling and other instruction cost heuristics.
  // a value of -1 means this value is not changed by the target.
def SnitchModel : SchedMachineModel {
  // Snitch is (generally) in-order
  // TODO: can we somehow reflect pseudo-dual-issue?
  let IssueWidth = 1;

  // Max micro-ops that can be buffered.
  // TODO: can we somehow reflect pseudo-dual-issue?
  let MicroOpBufferSize = 0;

  // Max micro-ops that can be buffered for optimized loop dispatch/execution.
  // Choose as 5/8 of L0 cache: 5*8 insts (avoid thrashing, but do not evict important loop-adjacent blocks)
  let LoopMicroOpBufferSize = 40;

  // Ideally, TCDM accesses take 2 cycles. We assume 6 here to account for
  // multi-core and heavy SSR contention and not exceed our FP L/S queue (important)
  let LoadLatency = 6;

  // Approximation of cycles for "high latency" ops.
  // The default of 10 is not bad here, but we can observe worse in  practice
  // (i.e. highly-congested TCDM atomics or external load-stores)
  let HighLatency = 12;

  // "Mispredict" penalties depend in Snitch, but are notably lower than the default:
  // 1. L0 hit: 0
  // 2. L0 miss, L1 hit: ~NrHiveCores/2 = 4
  // 3. L1 miss: external memory latency; let's assume an LLC or RO$ and say 10.
  // In expectation, we mostly encounter 1 or 2, so 2 is a fair tradeoff
  let MispredictPenalty = 2;

  let CompleteModel = 0;

  let UnsupportedFeatures = [HasStdExtZbkb, HasStdExtZbkc, HasStdExtZbkx,
                             HasStdExtZknd, HasStdExtZkne, HasStdExtZknh,
                             HasStdExtZksed, HasStdExtZksh, HasStdExtZkr,
                             HasStdExtZbe, HasVInstructionsAnyF];
}

//===----------------------------------------------------------------------===//
// Define each kind of processor resource and number available.

// Modeling each pipeline as a ProcResource using the BufferSize = 0 since
// Snitch is in-order.

let BufferSize = 0 in {
def SnitchUnitICore  : ProcResource<1>; // Snitch's decoder and ALU
def SnitchUnitILSQ   : ProcResource<4>; // Snitch core's load-store queue
def SnitchUnitILQ    : ProcResource<1>; // Snitch core's load queue     // TODO: should really be 4 deep in HW..
def SnitchUnitFPSS   : ProcResource<1>; // FP subsystem
def SnitchUnitFPDS   : ProcResource<1>; // FP div/sqrt unit
def SnitchUnitFLSQ   : ProcResource<4>; // FP load-store queue
def SnitchUnitFLQ    : ProcResource<4>; // FP load queue
def SnitchUnitMulDiv : ProcResource<1>; // Mul/Div accelerator
// TODO: Add the instructions for, use these
def SnitchUnitSCfg   : ProcResource<1>; // SSR config interface
def SnitchUnitDMA    : ProcResource<1>; // iDMA
// TODO: Handle IPU here once fully implemented
}

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedWrite types which both map the ProcResources and
// set the latency.

let SchedModel = SnitchModel in {

// These run on the Snitch core (generally) without latency
def : WriteRes<WriteJmp, [SnitchUnitICore]>;
def : WriteRes<WriteJal, [SnitchUnitICore]>;
def : WriteRes<WriteJalr, [SnitchUnitICore]>;
def : WriteRes<WriteJmpReg, [SnitchUnitICore]>;
def : WriteRes<WriteIALU, [SnitchUnitICore]>;
def : InstRW<[WriteIALU], (instrs COPY)>;
def : WriteRes<WriteShiftImm, [SnitchUnitICore]>;
def : WriteRes<WriteShiftReg, [SnitchUnitICore]>;
def : WriteRes<WriteShiftReg32, [SnitchUnitICore]>;
def : WriteRes<WriteCSR, [SnitchUnitICore]>;
def : WriteRes<WriteAtomicSTW, [SnitchUnitICore]>;
def : WriteRes<WriteNop, [SnitchUnitICore]>;

// Memory stores: fire-and-forget latency-wise, but occupy LSQ
let ResourceCycles = [1,4] in {
def : WriteRes<WriteSTB, [SnitchUnitICore, SnitchUnitILSQ]>;
def : WriteRes<WriteSTH, [SnitchUnitICore, SnitchUnitILSQ]>;
def : WriteRes<WriteSTW, [SnitchUnitICore, SnitchUnitILSQ]>;
}

// Memory loads and atomics: latency as for memory above
let Latency = 6, ResourceCycles = [1,4,4] in {
def : WriteRes<WriteLDB, [SnitchUnitICore, SnitchUnitILSQ, SnitchUnitILQ]>;
def : WriteRes<WriteLDH, [SnitchUnitICore, SnitchUnitILSQ, SnitchUnitILQ]>;
def : WriteRes<WriteLDW, [SnitchUnitICore, SnitchUnitILSQ, SnitchUnitILQ]>;
def : WriteRes<WriteAtomicW, [SnitchUnitICore, SnitchUnitILSQ, SnitchUnitILQ]>;
def : WriteRes<WriteAtomicLDW, [SnitchUnitICore, SnitchUnitILSQ, SnitchUnitILQ]>;
}

// Integer MULs are pipelined
let Latency = 3, ResourceCycles = [1,1] in {
def : WriteRes<WriteIMul, [SnitchUnitICore, SnitchUnitMulDiv]>;
}

// Integer DIVs are not pipelined
let Latency = 16, ResourceCycles = [1,14] in {
def : WriteRes<WriteIDiv, [SnitchUnitICore, SnitchUnitMulDiv]>;
}

// FPU: ADDMUL opgroup (FP32)
let Latency = 4 in {
def : WriteRes<WriteFALU32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMul32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMA32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCmp32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMinMax32, [SnitchUnitFPSS]>;
}

// FPU: ADDMUL opgroup (FP64)
let Latency = 5 in {
def : WriteRes<WriteFALU64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMul64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMA64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCmp64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMinMax64, [SnitchUnitFPSS]>;
}

// FPU: CONV opgroup (INT-FP transfer)
let Latency = 4 in {
def : WriteRes<WriteFCvtI32ToF32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtI32ToF64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtI64ToF32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtI64ToF64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtF32ToI32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtF32ToI64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtF64ToI32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtF64ToI64, [SnitchUnitFPSS]>;
}

// FPU: CONV opgroup (FP internal)
let Latency = 3 in {
def : WriteRes<WriteFCvtF32ToF64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFCvtF64ToF32, [SnitchUnitFPSS]>;
}

// FPU: NOCOMP opgroup (INT-FP transfer)
let Latency = 2 in {
def : WriteRes<WriteFMovF32ToI32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMovI32ToF32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMovF64ToI64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFMovI64ToF64, [SnitchUnitFPSS]>;
}

// FPU: NOCOMP opgroup (FP internal)
let Latency = 1 in {
def : WriteRes<WriteFClass32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFClass64, [SnitchUnitFPSS]>;
def : WriteRes<WriteFSGNJ32, [SnitchUnitFPSS]>;
def : WriteRes<WriteFSGNJ64, [SnitchUnitFPSS]>;
}

// FDIVSQRT group (FP32)
let Latency = 11,  ResourceCycles = [1, 11] in {
def : WriteRes<WriteFDiv32, [SnitchUnitFPSS, SnitchUnitFPDS]>;
def : WriteRes<WriteFSqrt32, [SnitchUnitFPSS, SnitchUnitFPDS]>;
}

// FDIVSQRT group (FP64)
let Latency = 21,  ResourceCycles = [1, 21] in {
def : WriteRes<WriteFDiv64, [SnitchUnitFPSS, SnitchUnitFPDS]>;
def : WriteRes<WriteFSqrt64, [SnitchUnitFPSS, SnitchUnitFPDS]>;
}

// Memory stores: fire-and-forget latency-wise, but occupy LSQ
let ResourceCycles = [1,4] in {
def : WriteRes<WriteFST32, [SnitchUnitFPSS, SnitchUnitFLSQ]>;
def : WriteRes<WriteFST64, [SnitchUnitFPSS, SnitchUnitFLSQ]>;
}

// Memory loads and atomics: latency as for memory above
let Latency = 6, ResourceCycles = [1,4,4] in {
def : WriteRes<WriteFLD32, [SnitchUnitFPSS, SnitchUnitFLSQ, SnitchUnitFLQ]>;
def : WriteRes<WriteFLD64, [SnitchUnitFPSS, SnitchUnitFLSQ, SnitchUnitFLQ]>;
}

// Snitch is an RV32 core -> no RV64 instructions
let Unsupported = 1 in {
def : WriteRes<WriteIALU32, []>;
def : WriteRes<WriteShiftImm32, []>;
def : WriteRes<WriteIMul32, []>;
def : WriteRes<WriteIDiv32, []>;
def : WriteRes<WriteSTD, []>;
def : WriteRes<WriteLDWU, []>;
def : WriteRes<WriteLDD, []>;
def : WriteRes<WriteAtomicD, []>;
def : WriteRes<WriteAtomicLDD, []>;
def : WriteRes<WriteAtomicSTD, []>;
}

// TODO: Define scheduling for extensions!

//===----------------------------------------------------------------------===//
// Subtarget-specific SchedRead types with cycles.
// Dummy definitions for Snitch.
// TODO: do we need forwarding anywhere?
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
def : ReadAdvance<ReadFALU32, 0>;
def : ReadAdvance<ReadFALU64, 0>;
def : ReadAdvance<ReadFMul32, 0>;
def : ReadAdvance<ReadFMA32, 0>;
def : ReadAdvance<ReadFMul64, 0>;
def : ReadAdvance<ReadFDiv32, 0>;
def : ReadAdvance<ReadFDiv64, 0>;
def : ReadAdvance<ReadFMA64, 0>;
def : ReadAdvance<ReadFSqrt32, 0>;
def : ReadAdvance<ReadFSqrt64, 0>;
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

//===----------------------------------------------------------------------===//
// Unsupported extensions
defm : UnsupportedSchedV;
defm : UnsupportedSchedZba;
defm : UnsupportedSchedZbb;
defm : UnsupportedSchedZbc;
defm : UnsupportedSchedZbs;
defm : UnsupportedSchedZbe;
defm : UnsupportedSchedZbf;
defm : UnsupportedSchedZfh;
defm : UnsupportedSchedZbm;

defm : UnsupportedSchedZbp;
defm : UnsupportedSchedZbr;
defm : UnsupportedSchedZbt;

}
