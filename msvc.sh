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

# Qt plugins are runtime-loaded (not in import tables), so copy explicitly.
mkdir -p "build/dist/plugins/platforms"
cp "${qt}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"
mkdir -p "build/dist/plugins/multimedia"
cp "${qt}/plugins/multimedia/windowsmediaplugin.dll" "build/dist/plugins/multimedia"

# Recursively discover and copy DLL dependencies from all PE files in
# build/dist (including plugins). Search paths are the MSVC sysroot and
# the Qt bin directory. System DLLs (provided by Windows) are skipped.
dll_search_paths=("${sysroot}/bin" "${qt}/bin")
while true
do
    changed=false
    while IFS= read -r dll
    do
        [[ -e "build/dist/${dll}" ]] && continue
        for dir in "${dll_search_paths[@]}"
        do
            if [[ -e "${dir}/${dll}" ]]
            then
                cp "${dir}/${dll}" build/dist
                changed=true
                break
            fi
        done
    done < <(find build/dist \( -iname "*.exe" -o -iname "*.dll" \) -exec objdump -p {} + 2>/dev/null | sed -n "s/.*DLL Name: //p" | sort -u)
    $changed || break
done
