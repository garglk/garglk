#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS. This script makes assumptions
# about the location of MinGW, so may need to be tweaked to get it to
# properly work.
#
# By default LLVM MinGW (assumed to be in /usr/llvm-mingw) is used. To
# use gcc MinGW (assumed to be in /usr), set the environment variable
# ${GARGOYLE_MINGW_GCC} to any value.
#
# i686 is built by default. To select another architecture, set the
# environment ${GARGOYLE_ARCH} to that architecture. Valid values:
#
# i686
# x86_64
# aarch64 (LLVM only)
# armv7 (LLVM only)

fatal() {
    echo "${@}" >&2
    exit 1
}

GARGOYLE_ARCH=${GARGOYLE_ARCH:-"i686"}

case "${GARGOYLE_ARCH}" in
    i686)
        libgcc=dw2
        ;;
    x86_64)
        libgcc=seh
        ;;
    aarch64|armv7)
        # Currently no installer support for ARM.
        NO_INSTALLER=1
        [[ -n "${GARGOYLE_MINGW_GCC}" ]] && fatal "Unsupported arch on MinGW GCC: ${GARGOYLE_ARCH}"
        ;;
    *)
        fatal "Unsupported arch: ${GARGOYLE_ARCH}"
        ;;
esac

target="${GARGOYLE_ARCH}-w64-mingw32"

[[ -e build/dist ]] && exit 1

if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    mingw_location=/usr
else
    mingw_location=/usr/llvm-mingw
fi

export PATH="${mingw_location}/bin:${PATH}"

nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(${target}-gcc --version | head -1 | awk '{print $3}')

mkdir build-mingw

(
cd build-mingw
env MINGW_TRIPLE=${target} MINGW_LOCATION=${mingw_location} cmake .. -DCMAKE_TOOLCHAIN_FILE=../Toolchain.cmake -DCMAKE_BUILD_TYPE=Release
make -j${nproc}
make install
)

if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
    cp "${mingw_location}/${target}/lib/libwinpthread-1.dll" "build/dist"

    libgccpath="/usr/lib/gcc/${target}/${ver}/libgcc_s_${libgcc}-1.dll"
    [[ -e "${libgccpath}" ]] || libgccpath=/usr/lib/gcc/${target}/${ver}/libgcc_s_*-1.dll
    cp ${libgccpath} "build/dist"
else
    cp "${mingw_location}/${target}/bin/libc++.dll" "build/dist"
    cp "${mingw_location}/${target}/bin/libunwind.dll" "build/dist"
fi

for dll in Qt5Core Qt5Gui Qt5Widgets SDL2 SDL2_mixer libfreetype-6 libjpeg-8 libmodplug-1 libmpg123-0 libogg-0 libopenmpt-0 libpng16-16 libvorbis-0 libvorbisfile-3 zlib1
do
    cp "${mingw_location}/${target}/bin/${dll}.dll" "build/dist"
done

find build/dist -name '*.exe' -o -name '*.dll' -exec ${target}-strip --strip-unneeded {} \;

mkdir -p "build/dist/plugins/platforms"
cp "${mingw_location}/${target}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"

if [[ -z "${NO_INSTALLER}" ]]
then
    makensis -V4 installer.nsi
fi
