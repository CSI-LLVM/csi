#!/usr/bin/env bash
set -xe

GOLD=/usr/bin/ld.gold
PLUGIN_API=/usr/include/plugin-api.h

CLONEDIR=..
BUILD="Release"
USE_CMAKE=1
MAKE_JOBS=$(nproc)

function configure {
    pushd $CLONEDIR
    if [ ! -f $GOLD ]; then
        echo "Error: gold linker not found at $GOLD"
        exit 1
    fi
    if [ ! -f $PLUGIN_API ]; then
        echo "Error: plugin-api.h not found at $PLUGIN_API"
        exit 1
    fi
    mkdir -p build
    cd build
    if [ $BUILD == "Release" ]; then
        if [ $USE_CMAKE -eq 1 ]; then
            cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=OFF -DLLVM_TARGETS_TO_BUILD=host -DCMAKE_C_FLAGS="-O3" -DCMAKE_CXX_FLAGS="-O3" -DLLVM_BINUTILS_INCDIR=/usr/include ../
        else
            ../configure --enable-targets=host --enable-optimized --disable-assertions --with-binutils-include=/usr/include
        fi
    else
        if [ $USE_CMAKE -eq 1 ]; then
            cmake -DCMAKE_BUILD_TYPE=Debug -DLLVM_ENABLE_ASSERTIONS=ON -DLLVM_TARGETS_TO_BUILD=host -DLLVM_BINUTILS_INCDIR=/usr/include ../
        else
            ../configure --enable-targets=host --disable-optimized --enable-assertions --with-binutils-include=/usr/include
        fi
    fi
    popd
}

function build {
    pushd $CLONEDIR/build
    make -j${MAKE_JOBS}
    popd
}

function checkcsi {
    pushd $CLONEDIR/build
    make check-csi
    popd
}

configure
build
checkcsi
