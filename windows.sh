#!/bin/bash

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS through Wine. This script makes
# assumptions about the locations of MinGW and Wine, so may need to be
# tweaked to get it to properly work.

set -e

[[ -e build/dist ]] && exit 1

makensis="${HOME}/.wine/drive_c/Program Files (x86)/NSIS/makensis"
nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(i686-w64-mingw32-gcc --version | head -1 | awk '{print $3}')

jam -sC++=i686-w64-mingw32-g++ -sCC=i686-w64-mingw32-gcc -sOS=MINGW -sMINGWARCH=i686-w64-mingw32 -sCROSS=1 -sUSETTS=yes -dx -j${nproc}
jam -sC++=i686-w64-mingw32-g++ -sCC=i686-w64-mingw32-gcc -sOS=MINGW -sMINGWARCH=i686-w64-mingw32 -sCROSS=1 -sUSETTS=yes -dx install

cp "/usr/lib/gcc/i686-w64-mingw32/${ver}/libstdc++-6.dll" "build/dist"
cp "/usr/lib/gcc/i686-w64-mingw32/${ver}/libgcc_s_sjlj-1.dll" "build/dist"
cp "/usr/i686-w64-mingw32/lib/libwinpthread-1.dll" "build/dist"

wine "${makensis}" installer.nsi
