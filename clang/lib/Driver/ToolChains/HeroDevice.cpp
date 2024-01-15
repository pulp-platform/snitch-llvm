//===--- HeroDevice.cpp - Hero Device ToolChain Implementations -----*- C++
//-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "HeroDevice.h"
#include "CommonArgs.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"

#include <string>

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

/// Hero Toolchain
// methods are adapted from RISCV.cpp
HeroDeviceToolChain::HeroDeviceToolChain(const Driver &D,
                                         const llvm::Triple &Triple,
                                         const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
    GCCInstallation.init(Triple, Args);

    // Parse hero device id from triple
    parseHeroDevice(Triple.str());

    // Get hero params
    HeroDeviceToolChain::getHeroParam(sysroot_str, Args, options::OPT_hero0_sysroot_EQ, HeroDeviceType, false, true);
    HeroDeviceToolChain::getHeroParam(march_str, Args, options::OPT_hero0_march_EQ, HeroDeviceType, false, true);

    // Parse device's sysroot and add to the toolchain's path
    auto SysRoot = computeSysRoot();
    getFilePaths().push_back(SysRoot + "/lib");

    if (GCCInstallation.isValid()) {
        getFilePaths().push_back(GCCInstallation.getInstallPath().str());
        getProgramPaths().push_back(
            (GCCInstallation.getParentLibPath() + "/../bin").str());
    }
}

Tool *HeroDeviceToolChain::buildLinker() const {
    return new tools::HeroDevice::Linker(*this);
}

void HeroDeviceToolChain::addClangTargetOptions(
    const llvm::opt::ArgList &DriverArgs, llvm::opt::ArgStringList &CC1Args,
    Action::OffloadKind) const {
    // FIXME: extra argument for target to allow dynamic datalayout
    CC1Args.push_back("-D__host=__attribute((address_space(1)))");
    CC1Args.push_back("-D__device=__attribute((address_space(0)))");
}

void HeroDeviceToolChain::AddClangSystemIncludeArgs(
    const ArgList &DriverArgs, ArgStringList &CC1Args) const {
    SmallString<128> SysRootDir = SmallString<128>(computeSysRoot());
    llvm::sys::path::append(SysRootDir, "include");
    addSystemInclude(DriverArgs, CC1Args, SysRootDir.str());
}

llvm::opt::DerivedArgList *HeroDeviceToolChain::TranslateArgs(
    const llvm::opt::DerivedArgList &Args, StringRef BoundArch,
    Action::OffloadKind DeviceOffloadKind) const {
    DerivedArgList *DAL = new DerivedArgList(Args.getBaseArgs());

    // Append all other args
    for (auto &arg : Args) {
        DAL->append(arg);
    }

    // Append march
    const OptTable &Opts = getDriver().getOpts();
    StringRef Value = "-march=";
    Arg *march = new Arg(Opts.getOption(options::OPT_march_EQ), Value,
                       Args.getBaseArgs().MakeIndex(Value), march_str.back().c_str());
    DAL->append(march);

    return DAL;
}

std::string HeroDeviceToolChain::computeSysRoot() const {
    return this->sysroot_str.back();
}

HeroDevice::Linker::Linker(const ToolChain &TC)
    : Tool("HeroDevice::Linker", "ld.lld", TC) {
}

static void Add64BitLinkerMode(Compilation &C, const InputInfo &Output,
                               ArgStringList &CmdArgs) {
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
        C.getDriver().Diag(clang::diag::err_unable_to_make_temp)
            << EC.message();
        return;
    }

    Lksf << LksBuffer;
}

void HeroDevice::Linker::ConstructJob(Compilation &C, const JobAction &JA,
                                      const InputInfo &Output,
                                      const InputInfoList &Inputs,
                                      const ArgList &Args,
                                      const char *LinkingOutput) const {

    const ToolChain &TC = getToolChain();
    const HeroDeviceToolChain &HeroDeviceTC = static_cast<const HeroDeviceToolChain&>(TC);

    // Get hero params
    HeroDeviceToolChain::getHeroParam(hero_ld_path, Args, options::OPT_hero0_ld_path_EQ, HeroDeviceTC.HeroDeviceType, false, true);
    HeroDeviceToolChain::getHeroParam(hero_ld_script_path, Args, options::OPT_hero0_T, HeroDeviceTC.HeroDeviceType, true, true);
    HeroDeviceToolChain::getHeroParam(hero_l, Args, options::OPT_hero0_l, HeroDeviceTC.HeroDeviceType, true, true);
    HeroDeviceToolChain::getHeroParam(hero_L, Args, options::OPT_hero0_L, HeroDeviceTC.HeroDeviceType, true, true);
    HeroDeviceToolChain::getHeroParam(hero_nostdlib, Args, options::OPT_hero0_nostdlib, HeroDeviceTC.HeroDeviceType, true, false);

    ArgStringList CmdArgs;

    // Argument parsing buffer
    SmallString<128> ArgStr;
    // Linker path
    SmallString<128> Linker(hero_ld_path.back());

    CmdArgs.push_back("-melf32lriscv");
    CmdArgs.push_back("-plugin-opt=mcpu=snitch");

    TC.AddFilePathLibArgs(Args, CmdArgs);
    Args.AddAllArgs(CmdArgs,
                    {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                     options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

    // No relocation on ld.lld
    CmdArgs.push_back("--no-relax");

    // Add linker script given as argument
    CmdArgs.push_back(hero_ld_script_path.back().c_str());

    // Add argument flagged for linker input
    AddLinkerInputs(TC, Inputs, Args, CmdArgs, JA);

    Add64BitLinkerMode(C, Output, CmdArgs);

    // Currently no support for C++, otherwise add C++ includes and libs

    // Add -Lhero and -lhero args
    for (auto & element : hero_l)
        CmdArgs.push_back(element.c_str());
    for (auto & element : hero_L)
        CmdArgs.push_back(element.c_str());

    if(!hero_nostdlib.empty())
        CmdArgs.push_back(hero_nostdlib.back().c_str());

    CmdArgs.push_back("-lm");
    CmdArgs.push_back("--start-group");
    CmdArgs.push_back("-lc");
    CmdArgs.push_back("-lgloss");
    CmdArgs.push_back("--end-group");

    CmdArgs.push_back("-o");
    CmdArgs.push_back(Output.getFilename());

    CmdArgs.push_back("-v");

    C.addCommand(std::make_unique<Command>(
        JA, *this,
        ResponseFileSupport{ResponseFileSupport::RF_Full, llvm::sys::WEM_UTF8,
                            "--options-file"},
        Args.MakeArgString(Linker), CmdArgs, Inputs));
}
// Hero tools end.
