//===--- FrepHint.h - Types for FrepHint ------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_PARSE_FREPHINT_H
#define LLVM_CLANG_PARSE_FREPHINT_H

#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Sema/Ownership.h"
#include "clang/Sema/ParsedAttr.h"

namespace clang {
struct FrepHint {
  SourceRange Range;
  IdentifierLoc *PragmaNameLoc;
  IdentifierLoc *OptionLoc;

  FrepHint()
      : PragmaNameLoc(nullptr), OptionLoc(nullptr) {}
};

}

#endif // LLVM_CLANG_PARSE_FREPHINT_H
