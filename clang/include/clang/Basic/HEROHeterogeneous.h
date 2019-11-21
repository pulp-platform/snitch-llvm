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

#ifndef LLVM_CLANG_BASIC_HEROHETEROGENEOUS_H
#define LLVM_CLANG_BASIC_HEROHETEROGENEOUS_H

#include "clang/AST/ASTContext.h"
#include "clang/Basic/TargetInfo.h"

namespace clang {
  namespace hero {
    bool isHERODeviceHetero(const ASTContext &);
    bool isHERODeviceOnly(const ASTContext &);
    bool isHERODevice(const ASTContext &);
    LangAS getHERODeviceAS(const ASTContext &);
    LangAS getHEROHostAS(const ASTContext &);

    enum debug_level {
      EMERGENCY = 0,
      ALERT = 1,
      CRITICAL = 2,
      ERROR = 3,
      WARNING = 4,
      NOTICE = 5,
      INFO = 6,
      DEBUG = 7
    };

    enum debug_level getHERODbgLevel();
  }
}

#endif
