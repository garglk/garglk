#!/bin/bash

# Package a Windows build of Gargoyle into a ZIP (and optionally an NSIS
# installer). This is meant to be run after windows.sh or msvc.sh has
# populated build/dist with binaries and DLLs.
#
# Usage: package-windows.sh <architecture>
#
# Supported architectures: i686, x86_64, aarch64, armv7
#
# An NSIS installer is only created for i686 and x86_64.

set -ex

fatal() {
    echo "${@}" >&2
    exit 1
}

arch="${1:?Usage: $0 <architecture>}"

[[ -d build/dist ]] || fatal "build/dist does not exist; run a build script first"

case "${arch}" in
    i686|x86_64)
        export GARGOYLE_ARCH="${arch}"
        makensis -V4 installer.nsi
        ;;
    aarch64|armv7)
        ;;
    *)
        fatal "Unsupported arch: ${arch}"
        ;;
esac

cp licenses/*.txt build/dist
unix2dos -n garglk/garglk.ini build/dist/garglk.ini

zip="gargoyle-$(<VERSION)-windows-${arch}.zip"
rm -f "${zip}"
(cd build/dist && zip -r "../../${zip}" *)
