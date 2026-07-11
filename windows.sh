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
# Sound uses Qt by default. To use SDL2 instead, pass the -2 flag; to
# use SDL3, pass the -3 flag.
#
# Qt 6 is used by default. To build against Qt 5 instead, pass the -5
# flag.
#
# Qt comes from the system (the native cross-compiled Qt in the MinGW
# sysroot) by default. To build against an official Qt install instead,
# pass -Q with the path to the Qt version directory (e.g. -Q
# "$HOME/Qt/6.11.1"); the compiler-specific subdir (llvm-mingw_64, or
# mingw_64 with -g) is appended automatically.
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

QT_VERSION="6"
GARGOYLE_SOUND="QT"
GARGOYLE_IMAGES="QT"
GARGOYLE_QT_HOME=""

while getopts "235a:cgQ:s" o
do
    case "${o}" in
        2)
            GARGOYLE_SOUND="SDL2"
            ;;
        3)
            GARGOYLE_SOUND="SDL3"
            ;;
        5)
            QT_VERSION="5"
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
        Q)
            GARGOYLE_QT_HOME="${OPTARG}"
            ;;
        s)
            GARGOYLE_IMAGES="SYSTEM"
            ;;
        *)
            fatal "Usage: $0 [-a i686|x86_64|aarch64|armv7] [-235cgs] [-Q /path/to/Qt]"
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

if [[ -n "${GARGOYLE_QT_HOME}" ]]
then
    if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
    then
        GARGOYLE_QT_HOME="${GARGOYLE_QT_HOME}/mingw_64"
    else
        GARGOYLE_QT_HOME="${GARGOYLE_QT_HOME}/llvm-mingw_64"
    fi

    CMAKE_QT6="-DCMAKE_PREFIX_PATH=${GARGOYLE_QT_HOME}"
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

if [[ -n "${GARGOYLE_QT_HOME}" ]]
then
    qt_plugins="${GARGOYLE_QT_HOME}/plugins"
else
    qt_plugins="${mingw_location}/${target}/plugins"
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

# Qt* DLLs are taken from the explicit -Q Qt (if any), everything else from
# the sysroot; see copy_dll_deps for why.
qt_bin=""
[[ -n "${GARGOYLE_QT_HOME}" ]] && qt_bin="${GARGOYLE_QT_HOME}/bin"
dll_search_paths=("${mingw_location}/${target}/bin")
if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    ver=$(${target}-gcc --version | head -1 | awk '{print $3}')
    dll_search_paths+=("/usr/lib/gcc/${target}/${ver}" "${mingw_location}/${target}/lib")
fi

copy_dll_deps "${qt_bin}" "${dll_search_paths[@]}"

find build/dist \( -name '*.exe' -o -name '*.dll' \) -exec ${target}-strip --strip-unneeded {} \;
