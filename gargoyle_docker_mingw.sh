#!/bin/bash

set -e

# Cross compile Gargoyle for Windows via Docker using the provided
# Dockerfile (LLVM MinGW + Qt 6 assets). Requires Docker.
#
# The llvm-mingw toolchain is x86_64-hosted, so the image is always built
# for linux/amd64 (including on Apple Silicon hosts).
#
# x86_64 is built by default. To select another architecture, use the -a
# option. Valid values:
#
# i686
# x86_64
# aarch64
# armv7

fatal() {
    echo "${@}" >&2
    exit 1
}

GARGOYLE_ARCH="x86_64"

usage="Usage: $0 [-a i686|x86_64|aarch64|armv7]"

while getopts "a:" o
do
    case "${o}" in
        a)
            GARGOYLE_ARCH="${OPTARG}"
            ;;
        *)
            fatal "${usage}"
            ;;
    esac
done

shift $((OPTIND - 1))
[[ $# -eq 0 ]] || fatal "${usage}"

case "${GARGOYLE_ARCH}" in
    i686|x86_64|aarch64|armv7)
        ;;
    *)
        fatal "Unsupported arch: ${GARGOYLE_ARCH}"
        ;;
esac

command -v docker >/dev/null || fatal "Docker is required but was not found in PATH"

root="$(cd "$(dirname "$0")" && pwd)"
cd "${root}"

image="garglk-mingw-${GARGOYLE_ARCH}"

# The llvm-mingw toolchain is x86_64-hosted, so the image is always built for
# `linux/amd64`, including on ARM/Apple Silicon hosts.

docker build --platform=linux/amd64 --build-arg ARCH="${GARGOYLE_ARCH}" -t "${image}" .
docker run --rm --platform=linux/amd64 -v "${root}:/src" -w /src "${image}" \
    bash -lc "./windows.sh -a ${GARGOYLE_ARCH} -c && ./package-windows.sh ${GARGOYLE_ARCH}"
