def RI5CYModel : SchedMachineModel {
  let MicroOpBufferSize = 0;
  let IssueWidth = 1;
  let LoadLatency = 1;
  let MispredictPenalty = 2;
  let PostRAScheduler = 1;
  let CompleteModel = 0;
}
