//===--- HeroDevice.cpp - Hero Device ToolChain Implementations -----*- C++
//-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "HeroCommon.h"
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

using namespace clang::driver;
using namespace clang::driver::toolchains;
using namespace clang::driver::tools;
using namespace clang;
using namespace llvm::opt;

#include <iostream>

/// Hero Toolchain
// methods are adapted from RISCV.cpp
HeroDeviceToolChain::HeroDeviceToolChain(const Driver &D,
                                         const llvm::Triple &Triple,
                                         const ArgList &Args)
    : Generic_ELF(D, Triple, Args) {
    GCCInstallation.init(Triple, Args);

    // Parse device's sysroot and add to the toolchain's path
    getHeroParam(Args, &sysroot, options::OPT_hero_sysroot_EQ);
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
    return DAL;
}

std::string HeroDeviceToolChain::computeSysRoot() const {
    return this->sysroot;
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
    const ToolChain &ToolChain = getToolChain();
    ArgStringList CmdArgs;

    // Get hero params
    getHeroParam(Args, (std::string*) &this->sysroot, options::OPT_hero_sysroot_EQ);
    getHeroParam(Args, (std::string*) &this->hero_ld_path, options::OPT_hero_ld_path_EQ);
    getHeroParam(Args, (std::string*) &this->hero_ld_script_path, options::OPT_hero_T);

    // Argument parsing buffer
    SmallString<128> ArgStr;
    // Linker path
    SmallString<128> Linker(hero_ld_path);

    CmdArgs.push_back("-melf32lriscv");

    // Note: Do not add the -L (they are for the host)
    // Args.AddAllArgs(CmdArgs, options::OPT_L);

    ToolChain.AddFilePathLibArgs(Args, CmdArgs);
    Args.AddAllArgs(CmdArgs,
                    {options::OPT_T_Group, options::OPT_e, options::OPT_s,
                     options::OPT_t, options::OPT_Z_Flag, options::OPT_r});

    // No relocation on ld.lld
    CmdArgs.push_back("--no-relax");

    // Add linker script given as argument
    std::cout << "!!! Debug1" << this->hero_ld_script_path << std::endl;
    CmdArgs.push_back("-T");
    CmdArgs.push_back(this->hero_ld_script_path.c_str());

    // Clean inputs to linker
    InputInfoList FinalInputs;
    for (auto &Input : Inputs) {
        if (Input.isInputArg()) {
            const llvm::opt::Arg &Arg = Input.getInputArg();
            if (Arg.getSpelling() == "-l" && Arg.getNumValues() > 0) {
                // Libraries might be host-only
                // FIXME: use host suffix for now to detect host-only libraries
                StringRef ArgVal = Arg.getValue();
                if (ArgVal.endswith("-host")) {
                    continue;
                }
            }
        }
        FinalInputs.push_back(Input);
    }
    AddLinkerInputs(ToolChain, FinalInputs, Args, CmdArgs, JA);

    Add64BitLinkerMode(C, Output, CmdArgs);

    // Currently no support for C++, otherwise add C++ includes and libs if
    // compiling C++.

    // Add -Lhero and -lhero
    for (std::string s : Args.getAllArgValues(options::OPT_hero_L)) {
        CmdArgs.push_back(Args.MakeArgString("-L" + s));
    }
    for (std::string s : Args.getAllArgValues(options::OPT_hero_l)) {
        CmdArgs.push_back(Args.MakeArgString("-l" + s));
    }


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
