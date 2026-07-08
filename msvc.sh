#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using clang-cl
# and the MSVC toolchain, populating build/dist with binaries and DLLs.
# Use package-windows.sh afterward to copy assets and create
# installers/ZIPs.
#
# Qt comes from the system (the native cross-compiled Qt in the MSVC
# sysroot) by default. To build against an official Qt install instead,
# pass -Q with the path to the Qt version directory (e.g. -Q
# "$HOME/Qt/6.11.1"); the arch-specific subdir (msvc2022_64, or
# msvc2022_arm64 with -a aarch64) is appended automatically.
#
# x86_64 is built by default. To select another architecture, use the
# -a option. Valid values:
#
# x86_64
# aarch64

. "$(dirname "$0")/libwin.sh"

fatal() {
    echo "${@}" >&2
    exit 1
}

GARGOYLE_QT_HOME=""

while getopts "a:cQ:" o
do
    case "${o}" in
        a)
            GARGOYLE_ARCH="${OPTARG}"
            ;;
        c)
            GARGOYLE_CLEAN=1
            ;;
        Q)
            GARGOYLE_QT_HOME="${OPTARG}"
            ;;
        *)
            fatal "Usage: $0 [-a x86_64|aarch64] [-c] [-Q /path/to/Qt]"
            ;;
    esac
done

arch="${GARGOYLE_ARCH:-x86_64}"

case "$arch" in
    x86_64)
        toolchain="Toolchain-msvc-x86_64.cmake"
        sysroot="/usr/x86_64-pc-windows-msvc"
        qt_subdir="msvc2022_64"
        ;;
    aarch64)
        toolchain="Toolchain-msvc-arm64.cmake"
        sysroot="/usr/aarch64-pc-windows-msvc"
        qt_subdir="msvc2022_arm64"
        ;;
    *)
        fatal "Unsupported arch: $arch"
        ;;
esac

cmake_qt_args=()
if [[ -n "${GARGOYLE_QT_HOME}" ]]
then
    # Official Qt install: point at the arch-specific subdir, and when the
    # target arch differs from the host, supply the host Qt for moc/rcc/etc.
    qt="${GARGOYLE_QT_HOME}/${qt_subdir}"
    cmake_qt_args+=(-DCMAKE_PREFIX_PATH="${qt}")

    case "$(uname -m)" in
        x86_64)  qt_host="${GARGOYLE_QT_HOME}/msvc2022_64" ;;
        aarch64) qt_host="${GARGOYLE_QT_HOME}/msvc2022_arm64" ;;
    esac
    [[ "${qt}" != "${qt_host}" ]] && cmake_qt_args+=(-DQT_HOST_PATH="${qt_host}")
fi
# System Qt (default): found in ${sysroot} via the toolchain's find-root, and
# its baked-in QT_HOST_PATH (the host Qt at /usr) supplies the host tools, so
# no CMAKE_PREFIX_PATH/QT_HOST_PATH are needed.

nproc=$(getconf _NPROCESSORS_ONLN)

[[ -n "${GARGOYLE_CLEAN}" ]] && rm -rf build-msvc build/dist

[[ -e build/dist ]] && exit 1

(
mkdir build-msvc
cd build-msvc
cmake .. "${cmake_qt_args[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="${toolchain}" -DSOUND=QT -DWITH_BUNDLED_FMT=ON -DDIST_INSTALL=ON -DWITH_FRANKENDRIFT=ON
make -j${nproc}
make install
)

if [[ -n "${GARGOYLE_QT_HOME}" ]]
then
    qt_plugins="${qt}/plugins"
else
    qt_plugins="${sysroot}/plugins"
fi

# Qt plugins are runtime-loaded (not in import tables), so copy them
# explicitly. Plugins are copied before copy_dll_deps so their own
# imports get picked up transitively.
copy_plugin "${qt_plugins}" "platforms/qwindows"
copy_plugin "${qt_plugins}" "imageformats/qjpeg"
copy_plugin "${qt_plugins}" "multimedia/windowsmediaplugin"

# Qt* DLLs are taken from the explicit -Q Qt (if any), everything else from
# the sysroot; see copy_dll_deps for why.
qt_bin=""
[[ -n "${GARGOYLE_QT_HOME}" ]] && qt_bin="${qt}/bin"

copy_dll_deps "${qt_bin}" "${sysroot}/bin"
