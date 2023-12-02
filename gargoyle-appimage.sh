#!/bin/bash

set -ex

# https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
# https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage
# https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

mkdir build-appimage
cd build-appimage
xargs -n 1 -P 0 wget -q <<EOF
https://github.com/garglk/assets/raw/appimage/linuxdeploy-x86_64.AppImage
https://github.com/garglk/assets/raw/appimage/linuxdeploy-plugin-appimage-x86_64.AppImage
https://github.com/garglk/assets/raw/appimage/linuxdeploy-plugin-qt-x86_64.AppImage
EOF
chmod +x linuxdeploy*
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DAPPIMAGE=TRUE -DBUILD_SHARED_LIBS=OFF
make "-j$(nproc)"
make install DESTDIR=AppDir

OUTPUT=../Gargoyle-x86_64.AppImage ./linuxdeploy-x86_64.AppImage \
    --appdir=AppDir \
    --plugin qt \
    --icon-file=../garglk/gargoyle-house.png \
    --icon-filename=io.github.garglk.Gargoyle \
    -d ../garglk/gargoyle.desktop \
    --output=appimage
