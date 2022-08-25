# LLVM for PULP Platform Projects

LLVM 12 with extensions for processors and computer systems of the [PULP platform](https://pulp-platform.org).  These include:
- MemPool: Instruction scheduling model for the MemPool architecture; `Xmempool` extension to allow dynamic instruction tracing;
- [PULPv2 RISC-V ISA extension (`Xpulpv2`)][hero]: automatic insertion of hardware loops, post-increment memory accesses, and multiply-accumulates; intrinsics, `clang` builtins , and assembly support for all instructions of the extension;
- [Snitch RISC-V ISA extensions (`Xssr`, `Xfrep`, and `Xdma`)][snitch]: automatic insertion of `frep` hardware loops; intrinsics and `clang` builtins for `Xssr` and `Xdma` extensions; assembly support for all instructions of the extension.

# Snitch RISC-V ISA Extension Support

## Build instructions
Refer to [snitch-toolchain-cd](https://github.com/pulp-platform/snitch-toolchain-cd) for build scripts and continuous deployment of pre-built toolchains.

## Command-line options

| Flag | Description |
|---|---|
| `--mcpu=snitch` | Enables all extensions for Snitch `rv32imafd,xfrep,xssr,xdma` and the Snitch machine model, which is not adapted for Snitch yet |
| `--debug-only=riscv-sdma` | Enable the debug output of the DMA pseudo instruction expansion pass |
| `--debug-only=riscv-ssr` | Enable the debug output of the SSR pseudo instruction expansion pass |
| `--debug-only=snitch-freploops` | Enable the debug output of the FREP loop inference pass |
| `--ssr-noregmerge` | Disable the SSR register merging in the SSR pseudo instruction expansion pass. Register merging is enabled by default and can be disabled with this flag. |
| `--snitch-frep-inference` | Globally enable the FREP inference on all loops in the compiled module. |
| `--enable-misched=false` | Disable the machine instruction scheduler. Instructions in a complex loop with multiple SSR push or pop instructions on the same data mover may not be rescheduled because the order in which the SSR are accessed is important. |

## `clang` builtins
The following `clang` builtins can be used to directly make use of the SSR and DMA extensions.

### SSR

```c
/**
 * @brief Setup 1D SSR read transfer
 * @details rep, b, s are raw values written directly to the SSR registers
 * 
 * @param DM data mover ID
 * @param rep repetition count minus one
 * @param b bound minus one
 * @param s relative stride
 * @param data pointer to data
 */
void __builtin_ssr_setup_1d_r(uint32_t DM, uint32_t rep, uint32_t b, uint32_t s, void* data);

/**
 * @brief Setup 1D SSR write transfer
 * @details rep, b, s are raw values written directly to the SSR registers
 * 
 * @param DM data mover ID
 * @param rep repetition count minus one
 * @param b bound minus one
 * @param s relative stride
 * @param data pointer to data
 */
void __builtin_ssr_setup_1d_w(uint32_t DM, uint32_t rep, uint32_t b, uint32_t s, void* data);

/**
 * @brief Write a datum to an SSR streamer
 * @details Must be within an SSR region
 * 
 * @param DM data mover ID
 * @param val value to write
 */
void __builtin_ssr_push(uint32_t DM, double val);

/**
 * @brief Read a datum from an SSR streamer
 * @details Must be within an SSR region
 * 
 * @param DM data mover ID
 * @return datum fetched from DM
 */
double __builtin_ssr_pop(uint32_t DM);

/**
 * @brief Enable an SSR region
 * @details FT registers are reserved and the push/pop methods can be used to access the SSR
 */
void __builtin_ssr_enable();

/**
 * @brief Disable an SSR region
 * @details FT registers are restored and push/pop is not possible
 */
void __builtin_ssr_disable();

/**
 * @brief Start an SSR read transfer
 * @details Bound and stride can be configured using the respective methods
 * 
 * @param DM data mover ID
 * @param dim Number of dimensions minus one
 * @param data pointer to data
 */
void __builtin_ssr_read(uint32_t DM, uint32_t dim, void* data);

/**
 * @brief Start an SSR write transfer
 * @details Bound and stride can be configured using the respective methods
 * 
 * @param DM data mover ID
 * @param dim Number of dimensions minus one
 * @param data pointer to data
 */
void __builtin_ssr_write(uint32_t DM, uint32_t dim, void* data);

/**
 * @brief Start an SSR read transfer. `DM` and `dim` must be constant. This method 
 * lowers to a single `scfgwi` instruction as opposed to the non-immediate version which
 * does address calculation first.
 * @details Bound and stride can be configured using the respective methods
 * 
 * @param DM data mover ID
 * @param dim Number of dimensions minus one
 * @param data pointer to data
 */
void __builtin_ssr_read_imm(uint32_t DM, uint32_t dim, void* data);

/**
 * @brief Start an SSR write transfer. `DM` and `dim` must be constant. This method 
 * lowers to a single `scfgwi` instruction as opposed to the non-immediate version which
 * does address calculation first.
 * @details Bound and stride can be configured using the respective methods
 * 
 * @param DM data mover ID
 * @param dim Number of dimensions minus one
 * @param data pointer to data
 */
void __builtin_ssr_write_imm(uint32_t DM, uint32_t dim, void* data);

/**
 * @brief Configure repetition value
 * @details A value of 0 loads each datum once
 * 
 * @param DM data mover ID
 * @param rep repetition count minus one
 */
void __builtin_ssr_setup_repetition(uint32_t DM, uint32_t rep);

/**
 * @brief Configure bound and stride for dimension 1
 * @details 
 * 
 * @param DM data mover ID
 * @param b bound minus one
 * @param s relative stride
 */
void __builtin_ssr_setup_bound_stride_1d(uint32_t DM, uint32_t b, uint32_t s);

/**
 * @brief Configure bound and stride for dimension 2
 * @details 
 * 
 * @param DM data mover ID
 * @param b bound minus one
 * @param s relative stride
 */
void __builtin_ssr_setup_bound_stride_2d(uint32_t DM, uint32_t b, uint32_t s);

/**
 * @brief Configure bound and stride for dimension 3
 * @details 
 * 
 * @param DM data mover ID
 * @param b bound minus one
 * @param s relative stride
 */
void __builtin_ssr_setup_bound_stride_3d(uint32_t DM, uint32_t b, uint32_t s);

/**
 * @brief Configure bound and stride for dimension 4
 * @details 
 * 
 * @param DM data mover ID
 * @param b bound minus one
 * @param s relative stride
 */
void __builtin_ssr_setup_bound_stride_4d(uint32_t DM, uint32_t b, uint32_t s);

/**
 * @brief Wait for the done bit to be set on data mover `DM`
 * @details Creates a polling loop and might not exit if SSR not configured correctly
 * 
 * @param DM data mover ID
 */
void __builtin_ssr_barrier(uint32_t DM);
```

### SDMA

```c
/**
 * @brief Start 1D DMA transfer
 * @details non-blocking call, doesn't check if DMA is ready to accept a new transfer
 * 
 * @param src Pointer to source
 * @param dst Pointer to destination
 * @param size Number of bytes to copy
 * @param cfg DMA configuration word
 * @return transfer ID
 */
uint32_t __builtin_sdma_start_oned(uint64_t src, uint64_t dst, uint32_t size, uint32_t cfg);

/**
 * @brief Start 2D DMA transfer
 * @details non-blocking call, doesn't check if DMA is ready to accept a new transfer
 * 
 * @param src Pointer to source
 * @param dst Pointer to destination
 * @param size Number of bytes in the inner transfer
 * @param sstrd Source stride
 * @param dstrd Destination stride
 * @param nreps Number of repetitions in the outer transfer
 * @param cfg DMA configuration word
 * @return transfer ID
 */
uint32_t __builtin_sdma_start_twod(uint64_t src, uint64_t dst, uint32_t size, 
  uint32_t sstrd, uint32_t dstrd, uint32_t nreps, uint32_t cfg);

/**
 * @brief Read DMA status register
 * @details 
 * 
 * @param tid Transfer ID to check
 * @return status register
 */
uint32_t __builtin_sdma_stat(uint32_t tid);

/**
 * @brief Polling wiat for idle
 * @details Block until all transactions have completed
 */
void __builtin_sdma_wait_for_idle(void);
```

## FREP hardware loops

Inference can be enabled globally with `--snitch-frep-inference` or locally with `#pragma frep infer`.

__For `frep` inference to work, `clang` must be invoked with at least `-O1`__

```c
#pragma frep infer
for(unsigned i = 0; i < 128; ++i)
  acc += __builtin_ssr_pop(0)*__builtin_ssr_pop(1);
```

# The LLVM Compiler Infrastructure

[![OpenSSF Scorecard](https://api.securityscorecards.dev/projects/github.com/llvm/llvm-project/badge)](https://securityscorecards.dev/viewer/?uri=github.com/llvm/llvm-project)
[![OpenSSF Best Practices](https://www.bestpractices.dev/projects/8273/badge)](https://www.bestpractices.dev/projects/8273)
[![libc++](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml/badge.svg?branch=main&event=schedule)](https://github.com/llvm/llvm-project/actions/workflows/libcxx-build-and-test.yaml?query=event%3Aschedule)

Welcome to the LLVM project!

This repository contains the source code for LLVM, a toolkit for the
construction of highly optimized compilers, optimizers, and run-time
environments.

The LLVM project has multiple components. The core of the project is
itself called "LLVM". This contains all of the tools, libraries, and header
files needed to process intermediate representations and convert them into
object files. Tools include an assembler, disassembler, bitcode analyzer, and
bitcode optimizer.

C-like languages use the [Clang](http://clang.llvm.org/) frontend. This
component compiles C, C++, Objective-C, and Objective-C++ code into LLVM bitcode
-- and from there into object files, using LLVM.

Other components include:
the [libc++ C++ standard library](https://libcxx.llvm.org),
the [LLD linker](https://lld.llvm.org), and more.

## Getting the Source Code and Building LLVM

Consult the
[Getting Started with LLVM](https://llvm.org/docs/GettingStarted.html#getting-the-source-code-and-building-llvm)
page for information on building and running LLVM.

For information on how to contribute to the LLVM project, please take a look at
the [Contributing to LLVM](https://llvm.org/docs/Contributing.html) guide.

## Getting in touch

Join the [LLVM Discourse forums](https://discourse.llvm.org/), [Discord
chat](https://discord.gg/xS7Z362),
[LLVM Office Hours](https://llvm.org/docs/GettingInvolved.html#office-hours) or
[Regular sync-ups](https://llvm.org/docs/GettingInvolved.html#online-sync-ups).

The LLVM project has adopted a [code of conduct](https://llvm.org/docs/CodeOfConduct.html) for
participants to all modes of communication within the project.
