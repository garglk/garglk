#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using MinGW,
# populating build/dist with binaries and DLLs. Use package-windows.sh
# afterward to copy assets and create installers/ZIPs.
#
# This script makes assumptions about the location of MinGW, so may
# need to be tweaked to get it to properly work.
#
# By default LLVM MinGW (assumed to be in /usr/llvm-mingw) is used. To
# use gcc MinGW (assumed to be in /usr), pass the -g flag.
#
# Images are loaded via Qt by default. To use system image libraries
# (libpng/libturbojpeg) instead, pass the -s flag.
#
# x86_64 is built by default. To select another architecture, use the -a
# option. Valid values:
#
# i686
# x86_64
# aarch64 (LLVM only)
# armv7 (LLVM only)

. "$(dirname "$0")/libwin.sh"

fatal() {
    echo "${@}" >&2
    exit 1
}

QT_VERSION="5"
GARGOYLE_SOUND="SDL"
GARGOYLE_IMAGES="QT"

while getopts "6a:cgqs" o
do
    case "${o}" in
        6)
            QT_VERSION="6"
            ;;
        a)
            GARGOYLE_ARCH="${OPTARG}"
            ;;
        c)
            GARGOYLE_CLEAN=1
            ;;
        g)
            GARGOYLE_MINGW_GCC=1
            ;;
        q)
            GARGOYLE_SOUND="QT"
            ;;
        s)
            GARGOYLE_IMAGES="SYSTEM"
            ;;
        *)
            fatal "Usage: $0 [-a i686|x86_64|aarch64|armv7] [-6cgqs]"
            ;;
    esac
done

GARGOYLE_ARCH=${GARGOYLE_ARCH:-"x86_64"}

case "${GARGOYLE_ARCH}" in
    i686|x86_64)
        ;;
    aarch64|armv7)
        [[ -n "${GARGOYLE_MINGW_GCC}" ]] && fatal "Unsupported arch on MinGW GCC: ${GARGOYLE_ARCH}"
        ;;
    *)
        fatal "Unsupported arch: ${GARGOYLE_ARCH}"
        ;;
esac

target="${GARGOYLE_ARCH}-w64-mingw32"

[[ -n "${GARGOYLE_CLEAN}" ]] && rm -rf build-mingw build/dist

[[ -e build/dist ]] && exit 1

if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    mingw_location=/usr
else
    mingw_location=/usr/llvm-mingw
fi

if [[ "${QT_VERSION}" == "6" ]]
then
    if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
    then
        QT6HOME="${HOME}/Qt/current/mingw_64"
    else
        QT6HOME="${HOME}/Qt/current/llvm-mingw_64"
    fi

    CMAKE_QT6="-DCMAKE_PREFIX_PATH=${QT6HOME}"
fi

export PATH="${mingw_location}/bin:${PATH}"

nproc=$(getconf _NPROCESSORS_ONLN)

mkdir build-mingw

(
cd build-mingw
env MINGW_TRIPLE=${target} MINGW_LOCATION=${mingw_location} cmake .. ${CMAKE_QT6} -DCMAKE_TOOLCHAIN_FILE=../Toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DSOUND=${GARGOYLE_SOUND} -DIMAGES=${GARGOYLE_IMAGES} -DQT_VERSION=${QT_VERSION} -DDIST_INSTALL=ON
make -j${nproc}
make install
)

if [[ "${QT_VERSION}" == "5" ]]
then
    qt_plugins="${mingw_location}/${target}/plugins"
else
    qt_plugins="${QT6HOME}/plugins"
fi

# Qt plugins are runtime-loaded (not in import tables), so copy them
# explicitly. Plugins are copied before copy_dll_deps so their own
# imports get picked up transitively.
copy_plugin "${qt_plugins}" "platforms/qwindows"

if [[ "${GARGOYLE_IMAGES}" == "QT" ]]
then
    copy_plugin "${qt_plugins}" "imageformats/qjpeg"
fi

if [[ "${GARGOYLE_SOUND}" == "QT" ]]
then
    if [[ "${QT_VERSION}" == "5" ]]
    then
        copy_plugin "${qt_plugins}" "audio/qtaudio_windows"
    else
        copy_plugin "${qt_plugins}" "multimedia/windowsmediaplugin"
    fi
fi

dll_search_paths=("${mingw_location}/${target}/bin")
[[ "${QT_VERSION}" == "6" ]] && dll_search_paths+=("${QT6HOME}/bin")
if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    ver=$(${target}-gcc --version | head -1 | awk '{print $3}')
    dll_search_paths+=("/usr/lib/gcc/${target}/${ver}" "${mingw_location}/${target}/lib")
fi

copy_dll_deps "${dll_search_paths[@]}"

find build/dist \( -name '*.exe' -o -name '*.dll' \) -exec ${target}-strip --strip-unneeded {} \;
