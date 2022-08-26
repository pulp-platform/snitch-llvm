# Copyright 2021 ETH Zurich and University of Bologna.
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# Read host specific variables
include env-iis.mk

# Versions
NEWLIB_VERS ?= 3.3.0
LLVM_BASE   ?= llvmorg-12.0.1

# Main source and working directory
WORK ?= $(shell pwd)/work
SRC  ?= ../
WORK_LLVM=${WORK}/llvm
WORK_NEWLIB=${WORK}/newlib
WORK_CRT=${WORK}/crt

# Build settings
NPROC  ?= $(shell nproc)
CC     ?= gcc
CXX    ?= g++
CMAKE  ?= cmake

# Toolchain installation directory
INSTALLPREFIX?=${WORK}/install

########################################
## Build jobs
########################################

# Configure cmake with arguments:
#   1: Build directory
#   2: Install prefix
#   2: Source directory
define cmake_llvm
	mkdir -p $(1); \
	${CMAKE} \
		-DCMAKE_BUILD_TYPE="Release" -DLLVM_ENABLE_ASSERTIONS=ON \
		-DLLVM_OPTIMIZED_TABLEGEN=True \
		-DCMAKE_INSTALL_PREFIX=$(2) \
		-DLLVM_ENABLE_PROJECTS="clang;lld" \
		-DLLVM_TARGETS_TO_BUILD="RISCV" \
		-DLLVM_DEFAULT_TARGET_TRIPLE="riscv32-unknown-elf" \
		-DLLVM_ENABLE_LLD=False -DLLVM_APPEND_VC_REV=ON \
		-DCMAKE_C_COMPILER=${CC} \
		-DCMAKE_CXX_COMPILER=${CXX} \
		-G Ninja -B $(1) -S $(3)/llvm
endef

# Rule to build LLVM
tc-llvm: prep-dirs
	rm -rf ${WORK_LLVM}
	$(call cmake_llvm,${WORK_LLVM},${INSTALLPREFIX},${SRC})
	ninja -C ${WORK_LLVM} -j${NPROC}
	ninja -C ${WORK_LLVM} install
	cd ${INSTALLPREFIX}/bin; for TR in riscv32-unknown-elf; \
		do for T in clang clang++ cc c++; do \
			ln -sv clang $$TR-$$T ||:; \
		done; \
	done

# Rule to build newlib and install it to LLVM
tc-llvm-newlib: prep-dirs
	git clone --depth 1 -b newlib-${NEWLIB_VERS} https://sourceware.org/git/newlib-cygwin.git ${WORK_NEWLIB}/newlib
	mkdir ${WORK_NEWLIB}/build
	cd ${WORK_NEWLIB}/build && ${WORK_NEWLIB}/newlib/configure \
		--target=riscv32-unknown-elf                       \
		--prefix=${INSTALLPREFIX}           \
		AR_FOR_TARGET=${INSTALLPREFIX}/bin/llvm-ar                  \
		AS_FOR_TARGET=${INSTALLPREFIX}/bin/llvm-as                  \
		LD_FOR_TARGET=${INSTALLPREFIX}/bin/llvm-ld                  \
		RANLIB_FOR_TARGET=${INSTALLPREFIX}/bin/llvm-ranlib          \
		CC_FOR_TARGET="${INSTALLPREFIX}/bin/clang -march=rv32imafd"
	${MAKE} -C ${WORK_NEWLIB}/build -j${NPROC}
	${MAKE} -C ${WORK_NEWLIB}/build install

# Rule to build compiler-rt and install it to LLVM
tc-llvm-crt: prep-dirs
	${CMAKE} -G"Unix Makefiles"                                                        \
		-DCMAKE_SYSTEM_NAME=Linux                                                      \
		-DCMAKE_INSTALL_PREFIX=$(shell ${INSTALLPREFIX}/bin/clang -print-resource-dir) \
		-DCMAKE_C_COMPILER=${INSTALLPREFIX}/bin/clang                                  \
		-DCMAKE_CXX_COMPILER=${INSTALLPREFIX}/bin/clang                                \
		-DCMAKE_AR=${INSTALLPREFIX}/bin/llvm-ar                                        \
		-DCMAKE_NM=${INSTALLPREFIX}/bin/llvm-nm                                        \
		-DCMAKE_RANLIB=${INSTALLPREFIX}/bin/llvm-ranlib                                \
		-DCMAKE_C_COMPILER_TARGET="riscv32-unknown-elf"                                \
		-DCMAKE_CXX_COMPILER_TARGET="riscv32-unknown-elf"                              \
		-DCMAKE_ASM_COMPILER_TARGET="riscv32-unknown-elf"                              \
		-DCMAKE_C_FLAGS="-march=rv32imafd -mabi=ilp32d"                                \
		-DCMAKE_CXX_FLAGS="-march=rv32imafd -mabi=ilp32d"                              \
		-DCMAKE_ASM_FLAGS="-march=rv32imafd -mabi=ilp32d"                              \
		-DCMAKE_EXE_LINKER_FLAGS="-nostartfiles -nostdlib -fuse-ld=lld"                \
		-DCOMPILER_RT_BAREMETAL_BUILD=ON                                               \
		-DCOMPILER_RT_BUILD_BUILTINS=ON                                                \
		-DCOMPILER_RT_BUILD_MEMPROF=OFF                                                \
		-DCOMPILER_RT_BUILD_LIBFUZZER=OFF                                              \
		-DCOMPILER_RT_BUILD_PROFILE=OFF                                                \
		-DCOMPILER_RT_BUILD_SANITIZERS=OFF                                             \
		-DCOMPILER_RT_BUILD_XRAY=OFF                                                   \
		-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON                                           \
		-DCOMPILER_RT_OS_DIR=""                                                        \
		-DLLVM_CONFIG_PATH=${INSTALLPREFIX}/bin/llvm-config                            \
		-S ${SRC}/compiler-rt -B ${WORK_CRT}
	${MAKE} -C ${WORK_CRT} -j${NPROC}
	${MAKE} -C ${WORK_CRT} install

########################################
## Test jobs
########################################

# Check newly added regression tests
test-llvm-new:
	WORK=${WORK} mergebase=${LLVM_BASE} bash test-new.sh
# Run entire LLVM test-suite
test-llvm-all:
	ninja -C ${WORK_LLVM} -j${NPROC} check-all
# Runn LLVM regression tests
LLVM_TESTS=../clang/test ../llvm/test
test-llvm-regression:
	${WORK}/llvm/bin/llvm-lit ${LLVM_TESTS}

# Helper
prep-dirs:
	mkdir -p ${WORK} ${INSTALLPREFIX}
