#!/bin/bash

set -e

clean() {
    rm -rf build/dist build-mingw
}

args=("-a i686"
      "-a x86_64"
      "-a aarch64"
      "-a armv7")

for a in "${args[@]}"
do
    clean
    ./windows.sh ${a}
done
