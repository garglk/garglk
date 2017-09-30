#!/bin/bash

CMAKE_BUILD_TYPE=Release

if [ "$1" = "-d" ]
then
  CMAKE_BUILD_TYPE=Debug
  echo "Enabling debug build."
fi

CMAKE_TOOLCHAIN_FILE=/home/vagrant/dev/buildroot/buildroot-2017.08/output/host/usr/share/buildroot/toolchainfile.cmake
CMAKE_INSTALL_PREFIX=dist

# Debug compile options
#CMAKE_C_FLAGS_DEBUG="-g3 -gdwarf-2"
CMAKE_C_FLAGS_DEBUG="-ggdb"
CMAKE_CXX_FLAGS_DEBUG="-ggdb"

# Kindle release build options
CMAKE_C_FLAGS_RELEASE="-O3 -fomit-frame-pointer -ffast-math -funsafe-math-optimizations -fweb -frename-registers -fuse-linker-plugin -ftree-vectorize -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block"

#-fpeephole2 leads to crash on armv7-a
CMAKE_CXX_FLAGS_RELEASE="-O3 -fno-peephole2 -fomit-frame-pointer -ffast-math -funsafe-math-optimizations -fweb -frename-registers -fuse-linker-plugin -ftree-vectorize -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block"

mkdir -p cbuildKindle
cd cbuildKindle

cmake -G "Ninja"  -DWITH_SDL=off  \
    -DCOMPILE_FOR_KINDLE=ON \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}" \
    -DCMAKE_C_FLAGS="" \
    -DCMAKE_CXX_FLAGS="" \
    -DCMAKE_C_FLAGS_DEBUG="${CMAKE_C_FLAGS_DEBUG}" \
    -DCMAKE_CXX_FLAGS_DEBUG="${CMAKE_CXX_FLAGS_DEBUG}" \
    -DCMAKE_C_FLAGS_RELEASE="${CMAKE_C_FLAGS_RELEASE}" \
    -DCMAKE_CXX_FLAGS_RELEASE="${CMAKE_CXX_FLAGS_RELEASE}" \
    -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}" \
    ..

ninja install

