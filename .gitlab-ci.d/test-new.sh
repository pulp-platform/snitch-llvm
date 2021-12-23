#!/bin/env bash
# Copyright 2021 ETH Zurich and University of Bologna.
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Author: Noah Huetter <huettern@iis.ee.ethz.ch>, ETH Zurich

# Arguments as environment variables:
#  mergebase    to which revision to compare for new tests
#  WORK         working directory with build LLVM

set -ex

old_pwd=${PWD}; cd ${WORK}

if [ -z ${mergebase+x} ]; then
    mergebase=llvmorg-12.0.1
fi

# Test discovery
prefix=$(git rev-parse --show-toplevel)
clang_tests=$(git diff --name-only $mergebase HEAD -- ${prefix}/clang/test | xargs -I{} -n1 echo ${prefix}/{})
llvm_tests=$(git diff --name-only $mergebase HEAD -- ${prefix}/llvm/test | xargs -I{} -n1 echo ${prefix}/{})

# Collect all tests and remove configs
tests="${clang_tests} ${llvm_tests}"
tests=$(echo "${tests}" | grep -vE 'lit\.local\.cfg|lit\.cfg\.py|Inputs')
echo "-- Discovered tests:"
for f in ${tests}; do
    echo $f
done
echo "--------------------"

# Run tests
llvm/bin/llvm-lit -v --debug --no-indirectly-run-check ${tests}
# lit -v --debug --no-indirectly-run-check ${tests}
ret=$?

cd ${old_pwd}
exit ${ret}
