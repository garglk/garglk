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

jamargs="-sC++=${target}-g++ -sCC=${target}-gcc -sOS=MINGW -sMINGWARCH=${target} -sCROSS=1 -sUSETTS=yes -sMINGW_USE_SYSTEM_LIBRARIES=yes -dx"

jam ${jamargs} -j${nproc}
jam ${jamargs} install

cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
cp "/usr/lib/gcc/${target}/${ver}/libgcc_s_dw2-1.dll" "build/dist"
cp "/usr/${target}/lib/libwinpthread-1.dll" "build/dist"

for dll in SDL2 SDL2_mixer libFLAC-8 libmodplug-1 libmpg123-0 libogg-0 libopenmpt-0 libvorbis-0 libvorbisfile-3
do
    cp "${mingw_location}/${target}/bin/${dll}.dll" "build/dist"
done

wine "${makensis}" installer.nsi
