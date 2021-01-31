#!/bin/bash

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS through Wine. This script makes
# assumptions about the locations of MinGW and Wine, so may need to be
# tweaked to get it to properly work.

set -e

[[ -e build/dist ]] && exit 1

sdl_location=/opt/local
target=i686-w64-mingw32

export PKG_CONFIG_PATH=${sdl_location}/${target}/lib/pkgconfig

makensis="${HOME}/.wine/drive_c/Program Files (x86)/NSIS/makensis"
nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(${target}-gcc --version | head -1 | awk '{print $3}')

jam -sC++=${target}-g++ -sCC=${target}-gcc -sOS=MINGW -sMINGWARCH=${target} -sCROSS=1 -sUSETTS=yes -dx -j${nproc}
jam -sC++=${target}-g++ -sCC=${target}-gcc -sOS=MINGW -sMINGWARCH=${target} -sCROSS=1 -sUSETTS=yes -dx install

cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
cp "/usr/lib/gcc/${target}/${ver}/libgcc_s_sjlj-1.dll" "build/dist"
cp "/usr/${target}/lib/libwinpthread-1.dll" "build/dist"

for dll in SDL2 SDL2_mixer
do
    cp "${sdl_location}/${target}/bin/${dll}.dll" "build/dist"
done

wine "${makensis}" installer.nsi
