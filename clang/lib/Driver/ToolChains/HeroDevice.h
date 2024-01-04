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

  std::string sysroot_str;
  std::string march_str;
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

  static std::string getHeroParam(const llvm::opt::ArgList &Args,
                           clang::driver::options::ID OPT, int HeroDeviceType, bool AddPrefix) {

    if (!Args.getLastArg(OPT))
      llvm::createStringError(llvm::inconvertibleErrorCode(),
                              "No argument found");

    // Get ready to remove -hero-X prefix from argument
    std::string prefix = "";
    std::string to_remove = "-hero" + std::to_string(HeroDeviceType);
    std::string result = "";

    // Isolate prefix
    prefix = Args.getLastArg(OPT)->getSpelling().str();
    prefix = ReplaceAll(prefix, to_remove, "");

    // Concat all argument value with or without prefix
    for (std::string s : Args.getAllArgValues(OPT)) {
      result += AddPrefix ? " " + prefix + s : " " + ReplaceAll(s, "=", ""); 
    }

    // Remove first space
    return result.substr(1);
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
  mutable std::string hero_ld_path;
  mutable std::string hero_ld_script_path;
  mutable std::string hero_l, hero_L;
};
} // end namespace HeroDevice
} // end namespace tools

} // end namespace driver
} // end namespace clang

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_DEVICE_H
