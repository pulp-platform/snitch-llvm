//===--- HEROHeterogeneous.h - Check if device compilation ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines some functions for figuring out if we are compiling for the device
/// on the HERO platform from ETHZ.
///
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTContext.h"
#include "clang/Basic/HEROHeterogeneous.h"
#include "clang/Basic/TargetInfo.h"

namespace clang {

  namespace hero {

    bool isHERODeviceHetero(const ASTContext &C) {
      const TargetInfo &TI = C.getTargetInfo();
      return TI.getTriple().getVendor() == llvm::Triple::HERO &&
             TI.getTriple().getArch() == llvm::Triple::riscv32 &&
             C.getAuxTargetInfo() != nullptr && C.getLangOpts().OpenMP &&
             C.getLangOpts().OpenMPIsDevice;
    }

    bool isHERODeviceOnly(const ASTContext &C) {
      const TargetInfo &TI = C.getTargetInfo();
      return TI.getTriple().getVendor() == llvm::Triple::HERO &&
             TI.getTriple().getArch() == llvm::Triple::riscv32 &&
             C.getAuxTargetInfo() == nullptr;
    }

    bool isHERODevice(const ASTContext &C) {
      return isHERODeviceHetero(C) || isHERODeviceOnly(C);
    }

    LangAS getHERODeviceAS(const ASTContext &C) {
      return LangAS::FirstTargetAddressSpace;
    }

    LangAS getHEROHostAS(const ASTContext &C) {
      return (LangAS) ((unsigned) LangAS::FirstTargetAddressSpace + 1);
    }

    enum debug_level getHERODbgLevel() {
      if (char *env = std::getenv("HERO_VERBOSITY")) {
        int level = std::atoi(env);
        if (level >= EMERGENCY && level <= DEBUG) {
          return (enum debug_level) level;
        }
      }
      return EMERGENCY;
    }
  }

}
