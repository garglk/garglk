#!/bin/bash

CMAKE_BUILD_TYPE=Release

if [ "$1" = "-d" ]
then
  CMAKE_BUILD_TYPE=Debug
  echo "Enabling debug build."
fi

CMAKE_INSTALL_PREFIX=dist

# Debug compile options
CMAKE_C_FLAGS_DEBUG="-ggdb -rdynamic -Werror=implicit-function-declaration"
CMAKE_CXX_FLAGS_DEBUG="-ggdb -rdynamic -Werror=implicit-function-declaration"
#CMAKE_C_FLAGS_DEBUG="-ggdb"
#CMAKE_CXX_FLAGS_DEBUG="-ggdb"

# Kindle release build options
CMAKE_C_FLAGS_RELEASE="-O3 -fomit-frame-pointer -ffast-math -fweb -frename-registers -fuse-linker-plugin -ftree-vectorize -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block"
CMAKE_CXX_FLAGS_RELEASE="-O3 -fomit-frame-pointer -ffast-math -fweb -frename-registers -fuse-linker-plugin -ftree-vectorize -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block"

mkdir -p cbuildLinux
cd cbuildLinux

#-DCOMPILE_FOR_KINDLE=ON
#-DCMAKE_C_FLAGS_DEBUG="${CMAKE_C_FLAGS_DEBUG}" 
#-DCMAKE_CXX_FLAGS_DEBUG="${CMAKE_CXX_FLAGS_DEBUG}" 
cmake -G "Ninja"  -DWITH_SDL=off  \
  -DCOMPILE_FOR_KINDLE=ON \
  -DENABLE_ALT_MOUSE_HANDLING=ON \
  -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}" \
  -DCMAKE_C_FLAGS_DEBUG="${CMAKE_C_FLAGS_DEBUG}" \
  -DCMAKE_CXX_FLAGS_DEBUG="${CMAKE_CXX_FLAGS_DEBUG}" \
  -DCMAKE_C_FLAGS_RELEASE="${CMAKE_C_FLAGS_RELEASE}" \
  -DCMAKE_CXX_FLAGS_RELEASE="${CMAKE_CXX_FLAGS_RELEASE}" \
  ..
cd ..
