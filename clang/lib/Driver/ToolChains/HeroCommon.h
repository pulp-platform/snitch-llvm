//===--- HeroCommon.h - Hero Device ToolChain Implementations -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HERO_COMMON_H
#define LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HERO_COMMON_H

#include "clang/Driver/Compilation.h"
#include "clang/Driver/DriverDiagnostic.h"
#include "clang/Driver/InputInfo.h"
#include "clang/Driver/Options.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <string>

using namespace clang::driver;
using namespace clang;
using namespace llvm::opt;

void getHeroParam(const ArgList &Args, std::string *StrArg, options::ID OPT) {
    // Set StrArg based on Args and the OPT to fetch
    if (Arg *A = Args.getLastArg(OPT)) {
        StringRef Value = A->getValue();
        std::string StrArgVal = Value.str();
        std::cout << "!!! StrArgVal " << StrArgVal << std::endl;
        if (llvm::sys::fs::exists(StrArgVal))
            *StrArg = StrArgVal;
        else
            llvm::createStringError(llvm::inconvertibleErrorCode(), "Incorrect argument " + A->getSpelling() + StrArgVal);
    }
}

#endif // LLVM_CLANG_LIB_DRIVER_TOOLCHAINS_HERO_COMMON_H
