#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS. This script makes assumptions
# about the location of MinGW, so may need to be tweaked to get it to
# properly work.

if [[ -n "${GARGOYLE64}" ]]
then
    target=x86_64-w64-mingw32
    libgcc=seh
else
    target=i686-w64-mingw32
    libgcc=dw2
fi

[[ -e build/dist ]] && exit 1

mingw_location=/usr

nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(${target}-gcc --version | head -1 | awk '{print $3}')

mkdir build-mingw

(
cd build-mingw
cmake .. -DCMAKE_TOOLCHAIN_FILE="../Toolchain-${target}.cmake" -DMINGW_TRIPLE="${target}" -DCMAKE_BUILD_TYPE=Release
make -j${nproc}
make install
)

cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
cp "/usr/lib/gcc/${target}/${ver}/libgcc_s_${libgcc}-1.dll" "build/dist"
cp "/usr/${target}/lib/libwinpthread-1.dll" "build/dist"

for dll in Qt5Core Qt5Gui Qt5Widgets SDL2 SDL2_mixer libFLAC-8 libfreetype-6 libjpeg-8 libmodplug-1 libmpg123-0 libogg-0 libopenmpt-0 libpng16-16 libvorbis-0 libvorbisfile-3 zlib1
do
    cp "${mingw_location}/${target}/bin/${dll}.dll" "build/dist"
done

mkdir -p "build/dist/plugins/platforms"
cp "${mingw_location}/${target}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"

if [[ -z "${NO_INSTALLER}" ]]
then
    makensis -V4 installer.nsi
fi
