//===--- HeroSnitch.cpp - Hero Snitch ToolChain Implementations -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "HeroSnitch.h"
#include "CommonArgs.h"
#include "InputInfo.h"
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
HeroSnitchToolChain::HeroSnitchToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  this->Triple = Triple;
  llvm::dbgs() << "[HeroSnitchToolChain::HeroSnitchToolChain] Triple: " << Triple.str() << "\n";
  GCCInstallation.init(Triple, Args);


  llvm::dbgs() << "[HeroSnitchToolChain::HeroSnitchToolChain] getFilePaths() pre" << "\n";
  for (auto &P : getFilePaths()) {
    llvm::dbgs() << "-x- " << P << "\n";
  }

  auto SysRoot = computeSysRoot();
  llvm::dbgs() << "[HeroSnitchToolChain::HeroSnitchToolChain] SysRoot: " << SysRoot << "\n";
  llvm::dbgs() << "[HeroSnitchToolChain::HeroSnitchToolChain] GCCInstallation.isValid(): " << GCCInstallation.isValid() << "\n";
  getFilePaths().push_back(SysRoot + "/lib");
  if (GCCInstallation.isValid()) {
    getFilePaths().push_back(GCCInstallation.getInstallPath().str());
    getProgramPaths().push_back(
        (GCCInstallation.getParentLibPath() + "/../bin").str());
  }

  llvm::dbgs() << "[HeroSnitchToolChain::HeroSnitchToolChain] getFilePaths() post" << "\n";
  for (auto &P : getFilePaths()) {
    llvm::dbgs() << "-x- " << P << "\n";
  }
}

Tool *HeroSnitchToolChain::buildLinker() const {
  return new tools::HeroSnitch::Linker(*this);
}

void HeroSnitchToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind) const {
  // FIXME: extra argument for target to allow dynamic datalayout
  CC1Args.push_back("-D__host=__attribute((address_space(1)))");
  CC1Args.push_back("-D__device=__attribute((address_space(0)))");
  // TODO: Remove
  SmallString<128> SrDir("random-dir-addClangTargetOptions");
  addSystemInclude(DriverArgs, CC1Args, SrDir.str());

  // Fix cross compilation when not specifying --sysroot. Search in
  // <target-triple>/sysroot/usr/include for headers
  const Driver &D = getDriver();
  if (D.SysRoot.empty() && GCCInstallation.isValid()) {
    SmallString<128> SrDir(D.Dir); // = [...]/instal/bin
    llvm::sys::path::append(SrDir, "../" + GCCInstallation.getTriple().str() + "/sysroot/usr/include");
    llvm::dbgs() << "[HeroSnitchToolChain::AddClangSystemIncludeArgs::3] StringRef(D.Dir): "<<StringRef(D.Dir)<<"\n";
    llvm::dbgs() << "[HeroSnitchToolChain::AddClangSystemIncludeArgs::3] fixup add: "<<SrDir<<"\n";
    addSystemInclude(DriverArgs, CC1Args, SrDir.str());
  }
}

void HeroSnitchToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                               ArgStringList &CC1Args) const {
  SmallString<128> SysRootDir(computeSysRoot());
  llvm::sys::path::append(SysRootDir, "include");
  llvm::dbgs() << "[HeroSnitchToolChain::AddClangSystemIncludeArgs] Adding system include "<<SysRootDir.str()<<"\n";
  llvm::dbgs() << "[HeroSnitchToolChain::AddClangSystemIncludeArgs] call to addSystemInclude()\n";
  addSystemInclude(DriverArgs, CC1Args, SysRootDir.str());

  // TODO: Remove
  SmallString<128> SrDir("random-dir-AddClangSystemIncludeArgs");
  addSystemInclude(DriverArgs, CC1Args, SrDir.str());
}

llvm::opt::DerivedArgList *HeroSnitchToolChain::TranslateArgs(const llvm::opt::DerivedArgList &Args,
    StringRef BoundArch, Action::OffloadKind DeviceOffloadKind) const {
  DerivedArgList *DAL = new DerivedArgList(Args.getBaseArgs());

  // Set default march
  StringRef Value = "-march=";
  const OptTable &Opts = getDriver().getOpts();
  // TODO: Add Snitch custom extensions
  Arg *march = new Arg(Opts.getOption(options::OPT_march_EQ), Value,
                       Args.getBaseArgs().MakeIndex(Value), "rv32imafd");
  DAL->append(march);

  // Append all other args
  for (auto& arg : Args) {
    if(arg->getOption().getName() == "sysroot=") {
      llvm::dbgs() << "    skipp appending sysroot!\n";
    } else {
      llvm::dbgs() << "[HeroSnitchToolChain::TranslateArgs] appending: \n";
      arg->print(llvm::dbgs());
      DAL->append(arg);
    }
  }
  return DAL;
}

std::string HeroSnitchToolChain::computeSysRoot() const {
  llvm::dbgs()<<"[HeroSnitchToolChain::computeSysRoot()] getDriver().Dir: "<<getDriver().Dir<<"\n";
  llvm::dbgs()<<"[HeroSnitchToolChain::computeSysRoot()] getDriver().getTargetTriple(): "<<getDriver().getTargetTriple()<<"\n";
  llvm::dbgs()<<"[HeroSnitchToolChain::computeSysRoot()] SelectedMultilib.osSuffix(): "<<SelectedMultilib.osSuffix()<<"\n";

  SmallString<128> SysRootDir;
  llvm::sys::path::append(SysRootDir, getDriver().Dir, "../", Triple.str());
  llvm::dbgs()<<"[HeroSnitchToolChain::computeSysRoot()] returning sysroot: "<<SysRootDir<<"\n";
  return std::string(SysRootDir);
}

HeroSnitch::Linker::Linker(const ToolChain &TC) : Tool("HeroSnitch::Linker", "ld.lld", TC) {
  // llvm::Optional<std::string> SnRuntimeInstallDir =
  //       llvm::sys::Process::GetEnv("SNRUNTIME_INSTALL");
  // if (SnRuntimeInstallDir.hasValue()) {
  //   this->SnRuntimeInstallDir = SnRuntimeInstallDir.getValue();
  // } else {
  //   TC.getDriver().Diag(diag::err_missing_snitch_sdk);
  // }
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
  llvm::raw_fd_ostream Lksf(LKS, EC, llvm::sys::fs::F_None);

  if (EC) {
    C.getDriver().Diag(clang::diag::err_unable_to_make_temp) << EC.message();
    return;
  }

  Lksf << LksBuffer;
}

void HeroSnitch::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const ToolChain &TC = getToolChain();
  const Driver &D = C.getDriver();
  ArgStringList CmdArgs;

  // Force using lld
  SmallString<128> Linker(D.Dir);
  llvm::sys::path::append(Linker, "ld.lld");
  llvm::dbgs() << "[HeroSnitch::Linker::ConstructJob] Linker: " << Linker << "\n";
  
  CmdArgs.push_back("-melf32lriscv");
  
  // from snRuntime
  CmdArgs.push_back("-plugin-opt=mcpu=snitch");
  // CmdArgs.push_back("-nostartfiles");
  // CmdArgs.push_back("-fuse-ld=lld");
  // CmdArgs.push_back("-z norelro");
  CmdArgs.push_back("--gc-sections");
  CmdArgs.push_back("--no-relax");

  // llvm::Optional<std::string> SnitchSdkInstallDir =
  //       llvm::sys::Process::GetEnv("PULP_CURRENT_CONFIG");
  // if(!PulpSdkInstallDir.hasValue()) {
  //   TC.getDriver().Diag(diag::err_missing_pulp_config);
  // }
  // llvm::Regex ConfigRegex("^(.+)@.+");
  // auto ConfigName = ConfigRegex.sub("\\1", PulpSdkInstallDir.getValue());

  // SmallString<128> InstallDir(this->PulpSdkInstallDir);
  // InstallDir.append("/hero/");
  // InstallDir.append(ConfigName);
  // SmallString<128> LibDir(this->PulpSdkInstallDir);
  // LibDir.append("/lib/");
  // LibDir.append(ConfigName);

  // SmallString<128> Crt0(LibDir);
  // llvm::sys::path::append(Crt0, "rt/crt0.o");
  // CmdArgs.push_back(Args.MakeArgString(Crt0));

  // SmallString<128> RtConf(InstallDir);
  // llvm::sys::path::append(RtConf, "rt_conf.o");
  // CmdArgs.push_back(Args.MakeArgString(RtConf));

  CmdArgs.push_back("-nostdlib");
  // CmdArgs.push_back("-nostartfiles");
  CmdArgs.push_back("--gc-sections");

  SmallString<128> ArgStr;
  // ArgStr.append("-L");
  // ArgStr.append(LibDir);
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  TC.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs,
                  {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                   options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

  // Clean inputs to linker
  InputInfoList FinalInputs;
  for(auto& Input : Inputs) {
    if(Input.isInputArg()) {
      const llvm::opt::Arg& Arg = Input.getInputArg();
      llvm::dbgs() << "+++ input " << Arg.getValue() << "\n";
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
  AddLinkerInputs(TC, FinalInputs, Args, CmdArgs, JA);

  ArgStr.clear();
  // ArgStr.append("-T");
  // ArgStr.append(InstallDir);
  // llvm::sys::path::append(ArgStr, "config.ld");
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  Add64BitLinkerMode(C, Output, CmdArgs);

  ArgStr.clear();
  // ArgStr.append("-T");
  // ArgStr.append(InstallDir);
  // llvm::sys::path::append(ArgStr, "../omptarget.ld");
  CmdArgs.push_back(Args.MakeArgString(ArgStr));

  // Currently no support for C++, otherwise add C++ includes and libs if compiling C++.

  // CmdArgs.push_back("-lomptarget-snitch");
  // CmdArgs.push_back("-lhero-target");
  // CmdArgs.push_back("-lomp");
  // CmdArgs.push_back("-lvmm");
  // CmdArgs.push_back("-larchi_host");
  // CmdArgs.push_back("-lrt"); // can call into `librtio`
  // CmdArgs.push_back("-lrtio"); // calls into `librt`
  // CmdArgs.push_back("-lm");
  // CmdArgs.push_back("-lgcc");
  // CmdArgs.push_back("-lc");

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());

  C.addCommand(std::make_unique<Command>(JA, *this,
               ResponseFileSupport{ResponseFileSupport::RF_Full,
                 llvm::sys::WEM_UTF8, "--options-file"},
               Args.MakeArgString(Linker), CmdArgs, Inputs));
}
// Hero tools end.
