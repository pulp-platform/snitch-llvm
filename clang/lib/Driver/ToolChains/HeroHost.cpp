//===--- HeroHost.cpp - HERO RISCV ToolChain Implementations ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "HeroHost.h"
#include "CommonArgs.h"
#include "Arch/RISCV.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormatVariadic.h"

#include <iostream>

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// RISCV Toolchain
HeroHostToolChain::HeroHostToolChain(const Driver &D, const llvm::Triple &Triple,
                               const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
  GCCInstallation.init(Triple, Args);
  std::string SysRoot = computeSysRoot();
  getFilePaths().push_back(SysRoot + "/lib");
  getFilePaths().push_back(SysRoot + "/usr/lib");

  std::cout << "!!! CALLING HEROHOST: " << std::endl;

  if (GCCInstallation.isValid()) {
    getFilePaths().push_back(GCCInstallation.getInstallPath().str());
    getProgramPaths().push_back(
        (GCCInstallation.getParentLibPath() + "/../bin").str());
  }

  // Fix cross compilation when not specifying --sysroot. Search in
  // <target-triple>/sysroot/usr/lib for libraries
  if (getDriver().SysRoot.empty() && GCCInstallation.isValid()) {
    SmallString<128> SrDir(D.Dir); // = [...]/instal/bin
    llvm::sys::path::append(SrDir, "../" + GCCInstallation.getTriple().str() + "/sysroot/usr/lib");
    getFilePaths().push_back(SrDir.str().str());
  }
}

Tool *HeroHostToolChain::buildLinker() const {
  return new tools::HeroHost::Linker(*this);
}

void HeroHostToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind) const {
  CC1Args.push_back("-nostdsysteminc");
  //CC1Args.push_back("-fuse-init-array");
  CC1Args.push_back("-D__host=__attribute((address_space(1)))");
  CC1Args.push_back("-D__device=__attribute((address_space(0)))");

  // Fix cross compilation when not specifying --sysroot. Search in
  // <target-triple>/sysroot/usr/include for headers
  const Driver &D = getDriver();
  if (D.SysRoot.empty() && GCCInstallation.isValid()) {
    SmallString<128> SrDir(D.Dir); // = [...]/instal/bin
    llvm::sys::path::append(SrDir, "../" + GCCInstallation.getTriple().str() + "/sysroot/usr/include");
    addSystemInclude(DriverArgs, CC1Args, SrDir.str());
  }
}

void HeroHostToolChain::AddClangSystemIncludeArgs(const ArgList &DriverArgs,
                                                   ArgStringList &CC1Args) const {
  if (DriverArgs.hasArg(options::OPT_nostdinc))
    return;

  std::string SysRoot = computeSysRoot();

  if (!DriverArgs.hasArg(options::OPT_nobuiltininc)) {
    SmallString<128> P(getDriver().ResourceDir);
    llvm::sys::path::append(P, "include");
    addSystemInclude(DriverArgs, CC1Args, P);
  }

  SmallString<128> UsrDir(SysRoot);
  llvm::sys::path::append(UsrDir, "usr/include");
  addSystemInclude(DriverArgs, CC1Args, UsrDir.str());
}

void HeroHostToolChain::addLibStdCxxIncludePaths(
    const llvm::opt::ArgList &DriverArgs,
    llvm::opt::ArgStringList &CC1Args) const {
  const GCCVersion &Version = GCCInstallation.getVersion();
  StringRef TripleStr = GCCInstallation.getTriple().str();
  const Multilib &Multilib = GCCInstallation.getMultilib();
  addLibStdCXXIncludePaths(computeSysRoot() + "/usr/include/c++/" + Version.Text,
      TripleStr, Multilib.includeSuffix(), DriverArgs, CC1Args);
}

std::string HeroHostToolChain::computeSysRoot() const {
  if (!getDriver().SysRoot.empty())
    return getDriver().SysRoot;

  if (!GCCInstallation.isValid())
    return std::string();

  StringRef LibDir = GCCInstallation.getParentLibPath();
  StringRef TripleStr = GCCInstallation.getTriple().str();
  std::string SysRootDir = LibDir.str() + "/../" + TripleStr.str();

  if (!llvm::sys::fs::exists(SysRootDir))
    return std::string();

  return SysRootDir;
}

void HeroHost::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                 const InputInfo &Output,
                                 const InputInfoList &Inputs,
                                 const ArgList &Args,
                                 const char *LinkingOutput) const {
  const ToolChain &ToolChain = getToolChain();
  const Driver &D = ToolChain.getDriver();
  const llvm::Triple &Triple = ToolChain.getTriple();
  ArgStringList CmdArgs;

  // Attention, this should be given by --ld-path
  !Args.hasArg(options::OPT_ld_path_EQ);
  std::string Linker = ToolChain.GetLinkerPath();

  if (!D.SysRoot.empty())
    CmdArgs.push_back(Args.MakeArgString("--sysroot=" + D.SysRoot));

  bool WantCRTs =
      !Args.hasArg(options::OPT_nostdlib, options::OPT_nostartfiles);

  if(Args.hasArg(options::OPT_shared)) {
    CmdArgs.push_back(Args.MakeArgString("-shared"));
  }

  if (WantCRTs) {
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crt1.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crti.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtbegin.o")));
  }

  Args.AddAllArgs(CmdArgs, options::OPT_L);
  ToolChain.AddFilePathLibArgs(Args, CmdArgs);
  Args.AddAllArgs(CmdArgs,
                  {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                   options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

  AddLinkerInputs(ToolChain, Inputs, Args, CmdArgs, JA);

  addOpenMPRuntime(CmdArgs, ToolChain, Args,
                   /* ForceStaticHostRuntime = */ false,
                   /* OffloadHost = */ JA.isHostOffloading(Action::OFK_OpenMP),
                   /* GompNeedsRT = */ false);

  if (!Args.hasArg(options::OPT_nostdlib) &&
      !Args.hasArg(options::OPT_nodefaultlibs)) {
    if (ToolChain.ShouldLinkCXXStdlib(Args))
      ToolChain.AddCXXStdlibLibArgs(Args, CmdArgs);
    CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lpthread");
    CmdArgs.push_back("-lc");
    CmdArgs.push_back("--end-group");

    AddRunTimeLibs(ToolChain, D, CmdArgs, Args);
  }

  if (WantCRTs) {
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtend.o")));
    CmdArgs.push_back(Args.MakeArgString(ToolChain.GetFilePath("crtn.o")));
  }

  // TODO: Check if enough to comment this out, or if there were additional
  //       important changes in https://github.com/llvm/llvm-project/commit/a0d83768f10849e5cf230391fac949dc5118c0a6
  //AddOpenMPLinkerScript(ToolChain, C, Output, Inputs, Args, CmdArgs, JA);

  // dynamic linker, snipped from ToolChains/Linux.cpp
  std::string LibDir;
  std::string Loader;
  switch (Triple.getArch()) {
  default:
    llvm_unreachable("unsupported architecture");
  case llvm::Triple::aarch64:
    LibDir = "lib";
    Loader = "ld-linux-aarch64.so.1";
    break;
  case llvm::Triple::riscv64:  {
    StringRef ABIName = tools::riscv::getRISCVABI(Args, Triple);
    LibDir = "lib";
    Loader = ("ld-linux-riscv64-" + ABIName + ".so.1").str();
    break;
  }
  }
  std::string DynamicLinker = "/" + LibDir + "/" + Loader;
  std::cout << "!!! Linkers : " << Linker << " + " << DynamicLinker << std::endl;
  CmdArgs.push_back("-dynamic-linker");
  CmdArgs.push_back(Args.MakeArgString(Twine(D.DyldPrefix) +
                                        DynamicLinker));

  CmdArgs.push_back("-o");
  CmdArgs.push_back(Output.getFilename());
  C.addCommand(std::make_unique<Command>(JA, *this,
               ResponseFileSupport{ResponseFileSupport::RF_Full,
                 llvm::sys::WEM_UTF8, "--options-file"},
               Args.MakeArgString(Linker), CmdArgs, Inputs));
}
// RISCV tools end.
