//===--- HeroPULP.cpp - Hero PULP ToolChain Implementations -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "HeroPULP.h"
#include "CommonArgs.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// Hero Toolchain
// methods are adapted from RISCV.cpp
HeroPULPToolChain::HeroPULPToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  GCCInstallation.init(Triple, Args);

  auto SysRoot = computeSysRoot();
  getFilePaths().push_back(SysRoot + "/lib");
  if (GCCInstallation.isValid()) {
    getFilePaths().push_back(GCCInstallation.getInstallPath().str());
    getProgramPaths().push_back(
        (GCCInstallation.getParentLibPath() + "/../bin").str());
  }
}

Tool *HeroPULPToolChain::buildLinker() const {
  return new tools::HeroPULP::Linker(*this);
}

void HeroPULPToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind) const {
  // FIXME: extra argument for target to allow dynamic datalayout
  CC1Args.push_back("-D__host=__attribute((address_space(1)))");
  CC1Args.push_back("-D__device=__attribute((address_space(0)))");
}

void HeroPULPToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  SmallString<128> SysRootDir(computeSysRoot());
  llvm::sys::path::append(SysRootDir, "include");
  addSystemInclude(DriverArgs, CC1Args, SysRootDir.str());
}

llvm::opt::DerivedArgList *HeroPULPToolChain::TranslateArgs(const llvm::opt::DerivedArgList &Args,
    StringRef BoundArch, Action::OffloadKind DeviceOffloadKind) const {
  DerivedArgList *DAL = new DerivedArgList(Args.getBaseArgs());

  // Set default march
  StringRef Value = "-march=";
  const OptTable &Opts = getDriver().getOpts();
  Arg *march = new Arg(Opts.getOption(options::OPT_march_EQ), Value,
                       Args.getBaseArgs().MakeIndex(Value), "rv32imafcxpulpv2");
  DAL->append(march);

  // Append all other args
  for (auto& arg : Args) {
    DAL->append(arg);
  }
  return DAL;
}

std::string HeroPULPToolChain::computeSysRoot() const {
  // FIXME: ignore standard sysroot as it is not updated during offloading,
  // and determine the sysroot from the computed GCC toolchain instead

  if (!GCCInstallation.isValid())
    return std::string();

  StringRef LibDir = GCCInstallation.getParentLibPath();
  StringRef TripleStr = GCCInstallation.getTriple().str();
  std::string SysRootDir = LibDir.str() + "/../" + TripleStr.str();

  if (!llvm::sys::fs::exists(SysRootDir))
    return std::string();

  return SysRootDir;
}

HeroPULP::Linker::Linker(const ToolChain &TC) : Tool("HeroPULP::Linker", "ld", TC) {
  llvm::Optional<std::string> PulpSdkInstallDir =
        llvm::sys::Process::GetEnv("PULP_SDK_INSTALL");
  if (PulpSdkInstallDir.hasValue()) {
    this->PulpSdkInstallDir = PulpSdkInstallDir.getValue();
  } else {
    TC.getDriver().Diag(diag::err_missing_pulp_sdk);
  }
}

static void Add64BitLinkerMode(Compilation &C, const InputInfo &Output,
                               ArgStringList& CmdArgs) {
  // Create temporary linker script. Keep it if save-temps is enabled.
  const char *LKS;
  SmallString<256> Name = llvm::sys::path::filename(Output.getFilename());
  if (C.getDriver().isSaveTempsEnabled()) {
    llvm::sys::path::replace_extension(Name, "lh");
    LKS = C.getArgs().MakeArgString(Name.c_str());
  } else {
    llvm::sys::path::replace_extension(Name, "");
    Name = C.getDriver().GetTemporaryPath(Name, "lh");
    LKS = C.addTempFile(C.getArgs().MakeArgString(Name.c_str()));
  }

  // Add linker script option to the command.
  CmdArgs.push_back("-T");
  CmdArgs.push_back(LKS);

  // Create a buffer to write the contents of the linker script.
  std::string LksBuffer;
  llvm::raw_string_ostream LksStream(LksBuffer);
  // XXX Write to the LksStream to add stuff to the linker script.

  // Open script file and write the contents.
  std::error_code EC;
  llvm::raw_fd_ostream Lksf(LKS, EC, llvm::sys::fs::OF_None);

  if (EC) {
    C.getDriver().Diag(clang::diag::err_unable_to_make_temp) << EC.message();
    return;
  }

  Lksf << LksBuffer;
}

void HeroPULP::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const ToolChain &ToolChain = getToolChain();
  ArgStringList CmdArgs;

  std::string Linker = getToolChain().GetProgramPath((ToolChain.getTriple().getTriple() + "-" + getShortName()).c_str());
  CmdArgs.push_back("-melf32lriscv");

  llvm::Optional<std::string> PulpSdkInstallDir =
        llvm::sys::Process::GetEnv("PULP_CURRENT_CONFIG");
  if(!PulpSdkInstallDir.hasValue()) {
    ToolChain.getDriver().Diag(diag::err_missing_pulp_config);
  }
  llvm::Regex ConfigRegex("^(.+)@.+");
  auto ConfigName = ConfigRegex.sub("\\1", PulpSdkInstallDir.getValue());

  SmallString<128> InstallDir(this->PulpSdkInstallDir);
  InstallDir.append("/hero/");
  InstallDir.append(ConfigName);
  SmallString<128> LibDir(this->PulpSdkInstallDir);
  LibDir.append("/lib/");
  LibDir.append(ConfigName);

  SmallString<128> Crt0(LibDir);
  llvm::sys::path::append(Crt0, "rt/crt0.o");
  CmdArgs.push_back(Args.MakeArgString(Crt0));

  SmallString<128> RtConf(InstallDir);
  llvm::sys::path::append(RtConf, "rt_conf.o");
  CmdArgs.push_back(Args.MakeArgString(RtConf));

  CmdArgs.push_back("-nostdlib");
  CmdArgs.push_back("-nostartfiles");
  CmdArgs.push_back("--gc-sections");

  SmallString<128> ArgStr;
  ArgStr.append("-L");
  ArgStr.append(LibDir);
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs,
                  {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                   options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

  // Clean inputs to linker
  InputInfoList FinalInputs;
  for(auto& Input : Inputs) {
    if(Input.isInputArg()) {
      const llvm::opt::Arg& Arg = Input.getInputArg();
      if(Arg.getSpelling() == "-l" && Arg.getNumValues() > 0) {
        // Libraries might be host-only
        // FIXME: use host suffix for now to detect host-only libraries
        StringRef ArgVal = Arg.getValue();
        if(!ArgVal.endswith("-host")) {
          FinalInputs.push_back(Input);
        }
        continue;
      }
    }
    FinalInputs.push_back(Input);
  }
  AddLinkerInputs(ToolChain, FinalInputs, Args, CmdArgs, JA);

  ArgStr.clear();
  ArgStr.append("-T");
  ArgStr.append(InstallDir);
  llvm::sys::path::append(ArgStr, "config.ld");
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  Add64BitLinkerMode(C, Output, CmdArgs);

  ArgStr.clear();
  ArgStr.append("-T");
  ArgStr.append(InstallDir);
  llvm::sys::path::append(ArgStr, "../omptarget.ld");
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  // Currently no support for C++, otherwise add C++ includes and libs if compiling C++.

  CmdArgs.push_back("-lomptarget-pulp");
  CmdArgs.push_back("-lhero-target");
  CmdArgs.push_back("-lomp");
  CmdArgs.push_back("-lvmm");
  CmdArgs.push_back("-larchi_host");
  CmdArgs.push_back("-lrt"); // can call into `librtio`
  CmdArgs.push_back("-lrtio"); // calls into `librt`
  CmdArgs.push_back("-lrt");
  CmdArgs.push_back("-lm");
  CmdArgs.push_back("-lgcc");
  CmdArgs.push_back("-lc");

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(std::make_unique<Command>(JA, *this,
               ResponseFileSupport{ResponseFileSupport::RF_Full,
                 llvm::sys::WEM_UTF8, "--options-file"},
               Args.MakeArgString(Linker), CmdArgs, Inputs));
}
// Hero tools end.
