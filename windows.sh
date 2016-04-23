#!/bin/bash

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS through Wine. This script makes
# assumptions about the locations of MinGW and Wine, so may need to be
# tweaked to get it to properly work.

set -e

[[ -e build/dist ]] && exit 1

mingw_location=/usr
target=i686-w64-mingw32

makensis="${HOME}/.wine/drive_c/Program Files (x86)/NSIS/makensis.exe"
nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(${target}-gcc --version | head -1 | awk '{print $3}')

mkdir build-mingw

(
cd build-mingw
PKG_CONFIG=${target}-pkg-config cmake .. -DCMAKE_TOOLCHAIN_FILE=../Toolchain-mingw32.cmake
make -j${nproc}
make install
)

cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
cp "/usr/lib/gcc/${target}/${ver}/libgcc_s_dw2-1.dll" "build/dist"
cp "/usr/${target}/lib/libwinpthread-1.dll" "build/dist"

for dll in SDL2 SDL2_mixer libFLAC-8 libfreetype-6 libjpeg-8 libmodplug-1 libmpg123-0 libogg-0 libopenmpt-0 libpng16-16 libvorbis-0 libvorbisfile-3 zlib1
do
    cp "${mingw_location}/${target}/bin/${dll}.dll" "build/dist"
done

wine "${makensis}" installer.nsi
