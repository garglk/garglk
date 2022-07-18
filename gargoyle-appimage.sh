#!/bin/bash

set -ex

mkdir build-appimage
cd build-appimage
xargs -n 1 -P 0 wget -q <<EOF
https://github.com/garglk/assets/raw/appimage/linuxdeploy-x86_64.AppImage
https://github.com/garglk/assets/raw/appimage/linuxdeploy-plugin-appimage-x86_64.AppImage
https://github.com/garglk/assets/raw/appimage/linuxdeploy-plugin-qt-x86_64.AppImage
EOF
chmod +x linuxdeploy*
# g++-9, on Ubuntu 20.04 (used by Github Actions) hits an ICE when building with static libraries:
# lto1: internal compiler error: in add_symbol_to_partition_1, at lto/lto-partition.c:153
# Use clang for AppImage building for now.
CC=clang CXX=clang++ cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DAPPIMAGE=TRUE -DBUILD_SHARED_LIBS=OFF
make -j$(nproc)
make install DESTDIR=AppDir

OUTPUT=../Gargoyle-x86_64.AppImage ./linuxdeploy-x86_64.AppImage \
    --appdir=AppDir \
    --plugin qt \
    -i ../garglk/gargoyle-house.png \
    -i ../garglk/gargoyle-docu2.png \
    -d ../garglk/gargoyle.desktop \
    --output=appimage
