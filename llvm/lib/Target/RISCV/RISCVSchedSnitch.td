//==- RISCVSchedSnitch.td - Snitch Scheduling Definitions --*- tablegen -*-=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//

// Snitch machine model for scheduling and other instruction cost heuristics.
def SnitchModel : SchedMachineModel {
  // a value of -1 means this value is not changed by the target
  let IssueWidth = 1; // Max micro-ops that may be scheduled per cycle.
  let MicroOpBufferSize = 0; // Max micro-ops that can be buffered.
  let LoopMicroOpBufferSize = 0; // Max micro-ops that can be buffered for
                                  // optimized loop dispatch/execution.
  let LoadLatency = -1; // Cycles for loads to access the cache.
  let HighLatency = -1; // Approximation of cycles for "high latency" ops.
  let MispredictPenalty = -1; // Extra cycles for a mispredicted branch.

  let CompleteModel = 0;

  let UnsupportedFeatures = [HasStdExtV, HasStdExtZvamo, HasStdExtZvlsseg];
}

// The Snitch microarchitecure has a single pipeline
//let SchedModel = SnitchModel, BufferSize = 0 in {
//  def SnitchPipeA       : ProcResource<1>;
//}
//def SnitchPipe : ProcResGroup<[SnitchPipeA]>;
