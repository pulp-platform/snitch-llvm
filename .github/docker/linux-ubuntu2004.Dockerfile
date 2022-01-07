# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Dockerfile for Ubuntu 20.04 builds
FROM ubuntu:20.04

LABEL maintainer huettern@ethz.ch

RUN apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -y git flex bison build-essential dejagnu git python python3 python3-distutils texinfo wget libexpat-dev \
                       ninja-build ccache

# Install cmake 3.18.4
RUN mkdir -p /tmp/cmake && cd /tmp/cmake && \
    wget https://github.com/Kitware/CMake/releases/download/v3.18.4/cmake-3.18.4.tar.gz && \
    tar xf cmake-3.18.4.tar.gz && cd cmake-3.18.4 && \
    ./bootstrap --parallel=$(nproc) -- -DCMAKE_USE_OPENSSL=OFF && \
    make -j$(nproc) && make install && \
    cd /tmp && rm -rf cmake

# Some tests require the user running testing to exist and have a home directory
# These values match what the Embecosm Buildbot builders are set up to use
RUN useradd -m -u 1002 builder

WORKDIR /home/builder
