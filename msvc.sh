#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using clang-cl
# and the MSVC toolchain, populating build/dist with binaries and DLLs.
# Use package-windows.sh afterward to copy assets and create
# installers/ZIPs.
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

while getopts "a:c" o
do
    case "${o}" in
        a)
            GARGOYLE_ARCH="${OPTARG}"
            ;;
        c)
            GARGOYLE_CLEAN=1
            ;;
        *)
            fatal "Usage: $0 [-a x86_64|aarch64] [-c]"
            ;;
    esac
done

arch="${GARGOYLE_ARCH:-x86_64}"

case "$arch" in
    x86_64)
        qt="$HOME/Qt/current/msvc2022_64"
        toolchain="Toolchain-msvc-x86_64.cmake"
        sysroot="/usr/x86_64-pc-windows-msvc"
        ;;
    aarch64)
        qt="$HOME/Qt/current/msvc2022_arm64"
        toolchain="Toolchain-msvc-arm64.cmake"
        sysroot="/usr/aarch64-pc-windows-msvc"
        ;;
    *)
        fatal "Unsupported arch: $arch"
        ;;
esac

host_arch=$(uname -m)
case "$host_arch" in
    x86_64)  qt_host="$HOME/Qt/current/msvc2022_64" ;;
    aarch64) qt_host="$HOME/Qt/current/msvc2022_arm64" ;;
esac
[[ "$qt" != "$qt_host" ]] && qt_host_arg=(-DQT_HOST_PATH="${qt_host}") || qt_host_arg=()

nproc=$(getconf _NPROCESSORS_ONLN)

[[ -n "${GARGOYLE_CLEAN}" ]] && rm -rf build-msvc build/dist

[[ -e build/dist ]] && exit 1

(
mkdir build-msvc
cd build-msvc
cmake .. -DCMAKE_PREFIX_PATH="${qt}" "${qt_host_arg[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="${toolchain}" -DSOUND=QT -DWITH_BUNDLED_FMT=ON -DDIST_INSTALL=ON -DWITH_FRANKENDRIFT=ON
make -j${nproc}
make install
)

# Qt plugins are runtime-loaded (not in import tables), so copy them
# explicitly. Plugins are copied before copy_dll_deps so their own
# imports get picked up transitively.
copy_plugin "${qt}/plugins" "platforms/qwindows"
copy_plugin "${qt}/plugins" "imageformats/qjpeg"
copy_plugin "${qt}/plugins" "multimedia/windowsmediaplugin"

# ${sysroot}/lib is searched in addition to ${sysroot}/bin because
# locally-built DLLs land in lib/ alongside their import libs.
copy_dll_deps "${sysroot}/bin" "${sysroot}/lib" "${qt}/bin"
