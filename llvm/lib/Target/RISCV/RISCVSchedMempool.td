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
