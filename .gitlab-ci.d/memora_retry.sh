#!/usr/bin/env bash
# Copyright 2021 ETH Zurich and University of Bologna.
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
i=1
max_attempts=10
while ! memora "$@"; do
  echo "Attempt $i/$max_attempts of 'memora $@' failed."
  if test $i -ge $max_attempts; then
    echo "'memora $@' keeps failing; aborting!"
    exit 1
  fi
  i=$(($i+1))
done
