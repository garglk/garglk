#!/bin/bash

set -ex

qt="$HOME/Qt/6.6.0/msvc2019_64"

nproc=$(getconf _NPROCESSORS_ONLN)

[[ -e build/dist ]] && exit 1

(
mkdir build-msvc
cd build-msvc
cmake .. -DWITH_QT6=ON -DCMAKE_PREFIX_PATH="${qt}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=Toolchain-msvc.cmake -DSOUND=QT -DWITH_BUNDLED_FMT=ON -DDIST_INSTALL=ON
make -j${nproc}
make install
)

for dll in libpng16.dll sndfile.dll turbojpeg.dll zlib1.dll
do
    cp "/usr/x86_64-pc-windows-msvc/bin/${dll}" build/dist
done

for dll in Qt6Core.dll Qt6Gui.dll Qt6Multimedia.dll Qt6Network.dll Qt6Widgets.dll
do
    cp "${qt}/bin/${dll}" build/dist
done

mkdir -p "build/dist/plugins/platforms"
cp "${qt}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"
mkdir -p "build/dist/plugins/multimedia"
cp "${qt}/plugins/multimedia/windowsmediaplugin.dll" "build/dist/plugins/multimedia"

cp licenses/*.txt build/dist
cp fonts/Gargoyle*.ttf build/dist
cp fonts/unifont*.otf build/dist
mkdir build/dist/themes
cp themes/*.json build/dist/themes
unix2dos -n garglk/garglk.ini build/dist/garglk.ini
