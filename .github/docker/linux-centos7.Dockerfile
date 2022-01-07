# Copyright 2022 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

# Dockerfile for CentOS 7 toolchain builds
FROM centos:centos7

LABEL maintainer huettern@ethz.ch

RUN yum -y upgrade && yum -y groupinstall 'Development tools' && \
    yum -y install dejagnu python3 texinfo wget which expat-devel

# Install newer toolchain components
RUN yum install -y centos-release-scl && yum install -y devtoolset-7

ENV PATH="/opt/rh/devtoolset-7/root/usr/bin:${PATH}" \
    LD_LIBRARY_PATH="/opt/rh/devtoolset-7/root/usr/lib64:/opt/rh/devtoolset-7/root/usr/lib:/opt/rh/devtoolset-7/root/usr/lib64/dyninst:/opt/rh/devtoolset-7/root/usr/lib/dyninst:/opt/rh/devtoolset-7/root/usr/lib64:/opt/rh/devtoolset-7/root/usr/lib"

# Install cmake 3.18.4
RUN mkdir -p /tmp/cmake && cd /tmp/cmake && \
    wget https://github.com/Kitware/CMake/releases/download/v3.18.4/cmake-3.18.4.tar.gz && \
    tar xf cmake-3.18.4.tar.gz && cd cmake-3.18.4 && \
    ./bootstrap --parallel=$(nproc) -- -DCMAKE_USE_OPENSSL=OFF && \
    make -j$(nproc) && make install && \
    cd /tmp && rm -rf cmake

# Install ninja 1.10.2
RUN mkdir -p /tmp/ninja && cd /tmp/ninja && \
    git clone https://github.com/ninja-build/ninja.git -b v1.10.2 . && \
    cmake -Bbuild-cmake -H. && cmake --build build-cmake && \
    cp build-cmake/ninja /usr/bin/ && \
    cd /tmp && rm -rf ninja

# Some tests require the user running testing to exist and have a home directory
# These values match what the Embecosm Buildbot builders are set up to use
RUN useradd -m -u 1002 builder
WORKDIR /home/builder
