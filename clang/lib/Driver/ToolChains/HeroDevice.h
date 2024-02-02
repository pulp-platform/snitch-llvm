//===--- HeroDevice.h - Hero Device ToolChain Implementations -------*- C++
//-*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HERO_DEVICE_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HERO_DEVICE_H

#include <string>
#include <iostream>

#include "Gnu.h"
#include "clang/Driver/ToolChain.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Error.h"

namespace clang {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY HeroDeviceToolChain : public Generic_ELF {
public:
  HeroDeviceToolChain(const Driver &D, const llvm::Triple &Triple,
                      const llvm::opt::ArgList &Args);

  bool IsIntegratedAssemblerDefault() const override { return true; }
  void addClangTargetOptions(const llvm::opt::ArgList &DriverArgs,
                             llvm::opt::ArgStringList &CC1Args,
                             Action::OffloadKind) const override;
  void
  AddClangSystemIncludeArgs(const llvm::opt::ArgList &DriverArgs,
                            llvm::opt::ArgStringList &CC1Args) const override;
  void
  addLibStdCxxIncludePaths(const llvm::opt::ArgList &DriverArgs,
                           llvm::opt::ArgStringList &CC1Args) const override{};

  virtual llvm::opt::DerivedArgList *
  TranslateArgs(const llvm::opt::DerivedArgList &Args, StringRef BoundArch,
                Action::OffloadKind DeviceOffloadKind) const override;

  llvm::SmallVector<std::string, 1> hero_sysroot;
  llvm::SmallVector<std::string, 1> hero_march;
  llvm::SmallVector<std::string, 2> hero_D;
  int HeroDeviceType;

  // Helper function, Python like string replace function
  static std::string ReplaceAll(std::string str, const std::string& from, const std::string& to) {
      size_t start_pos = 0;
      while((start_pos = str.find(from, start_pos)) != std::string::npos) {
          str.replace(start_pos, from.length(), to);
          start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
      }
      return str;
  }

  static void getHeroParam(llvm::SmallVectorImpl<std::string> &Result, const llvm::opt::ArgList &Args,
                           clang::driver::options::ID OPT, int HeroDeviceType, bool AddPrefix, bool Mandatory) {
    // We need to offset the OPT by the HeroDeviceType (device idx)
    // Based on the ordering of options IDs in Options.inc we need to know
    // the stride between hero0-X and hero1-X
    int opt_stride = options::OPT_hero1_sysroot_EQ - options::OPT_hero0_sysroot_EQ;
    assert(opt_stride == options::OPT_hero1_l - options::OPT_hero0_l && "Something went wrong in hero options ordering");
    assert(opt_stride == options::OPT_hero1_L - options::OPT_hero0_L && "Something went wrong in hero options ordering");

    int OPT_strided = OPT + opt_stride * HeroDeviceType;

    // Verify the argument has been given by the user
    if (!Args.getLastArg(OPT_strided)) {
      if (Mandatory) {
        std::cerr << "Cannot find option " << getDriverOptTable().getOption(OPT_strided).getPrefixedName() <<
                    " ( " << getDriverOptTable().getOption(OPT).getPrefixedName() <<
                    " + device " <<  HeroDeviceType << " )" << std::endl;
        assert(false);
      } else
        return;
    }

    // Get ready to remove -hero-X prefix from argument
    std::string prefix = "";
    std::string to_remove = "-hero" + std::to_string(HeroDeviceType);
    std::string result = "";

    // Isolate argument prefix without -hero-X
    prefix = Args.getLastArg(OPT_strided)->getSpelling().str();
    prefix = ReplaceAll(prefix, to_remove, "");

    // Concat all argument value with or without prefix
    for (std::string s : Args.getAllArgValues(OPT_strided)) {
      Result.push_back(AddPrefix ? prefix + s : ReplaceAll(s, "=", ""));
    }

    // Check if this parameter is just a flag without value (ex: -heroX-nostdlib)
    if (Result.empty()) {
      Result.push_back(AddPrefix ? prefix : "");
    }
  }

protected:
  Tool *buildLinker() const override;

private:
  // Todo keep signature without arg
  std::string computeSysRoot() const override;

  void parseHeroDevice(std::string TripleStr) {
    // Get device index by parsing OS part of the Triple
    SmallVector<StringRef, 4> Components;
    StringRef(TripleStr).split(Components, '-', 3);
    std::string OsStr = Components[2].str();
    size_t Pos = OsStr.find("hero");
    if (Pos != std::string::npos)
      OsStr.erase(Pos, 4);
    HeroDeviceType = stoi(OsStr);
  }
};

} // end namespace toolchains

namespace tools {
namespace HeroDevice {
class LLVM_LIBRARY_VISIBILITY Linker : public Tool {
public:
  Linker(const ToolChain &TC);
  bool hasIntegratedCPP() const override { return false; }
  bool isLinkJob() const override { return true; }
  void ConstructJob(Compilation &C, const JobAction &JA,
                    const InputInfo &Output, const InputInfoList &Inputs,
                    const llvm::opt::ArgList &TCArgs,
                    const char *LinkingOutput) const override;

private:
  mutable llvm::SmallVector<std::string, 1> hero_ld_path;
  mutable llvm::SmallVector<std::string, 1> hero_ld_script_path;
  mutable llvm::SmallVector<std::string, 4> hero_l, hero_L;
  mutable llvm::SmallVector<std::string, 1> hero_nostdlib;
};
} // end namespace HeroDevice
} // end namespace tools

} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_DEVICE_H
