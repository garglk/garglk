#!/bin/bash

set -ex

# This script will cross compile Gargoyle for Windows using MinGW, and
# build an installer for it using NSIS. This script makes assumptions
# about the location of MinGW, so may need to be tweaked to get it to
# properly work.
#
# By default LLVM MinGW (assumed to be in /usr/llvm-mingw) is used. To
# use gcc MinGW (assumed to be in /usr), pass the -g flag.
#
# i686 is built by default. To select another architecture, use the -a
# option. Valid values:
#
# i686
# x86_64
# aarch64 (LLVM only)
# armv7 (LLVM only)
#
# An installer (if available) and standalone ZIP are created. To build
# without creating installers/ZIPs, pass the -b flag.

fatal() {
    echo "${@}" >&2
    exit 1
}

QT_VERSION="5"
GARGOYLE_SOUND="SDL"

while getopts "6a:bgq" o
do
    case "${o}" in
        6)
            QT_VERSION="6"
            ;;
        a)
            GARGOYLE_ARCH="${OPTARG}"
            ;;
        b)
            GARGOYLE_BUILD_ONLY=1
            ;;
        g)
            GARGOYLE_MINGW_GCC=1
            ;;
        q)
            GARGOYLE_SOUND="QT"
            ;;
        *)
            fatal "Usage: $0 [-a i686|x86_64|aarch64|armv7] [-b] [-g] [-q]"
            ;;
    esac
done

GARGOYLE_ARCH=${GARGOYLE_ARCH:-"i686"}

case "${GARGOYLE_ARCH}" in
    i686)
        libgcc=dw2
        ;;
    x86_64)
        libgcc=seh
        ;;
    aarch64|armv7)
        GARGOYLE_NO_INSTALLER=1
        [[ -n "${GARGOYLE_MINGW_GCC}" ]] && fatal "Unsupported arch on MinGW GCC: ${GARGOYLE_ARCH}"
        ;;
    *)
        fatal "Unsupported arch: ${GARGOYLE_ARCH}"
        ;;
esac

target="${GARGOYLE_ARCH}-w64-mingw32"

[[ -e build/dist ]] && exit 1

if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    mingw_location=/usr
else
    mingw_location=/usr/llvm-mingw
fi

if [[ "${QT_VERSION}" == "6" ]]
then
    if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
    then
        QT6HOME="${HOME}/Qt/6.7.1/mingw_64"
    else
        QT6HOME="${HOME}/Qt/6.7.1/llvm-mingw_64"
    fi

    CMAKE_QT6="-DCMAKE_PREFIX_PATH=${QT6HOME}"
fi

export PATH="${mingw_location}/bin:${PATH}"

nproc=$(getconf _NPROCESSORS_ONLN)
ver=$(${target}-gcc --version | head -1 | awk '{print $3}')

mkdir build-mingw

(
cd build-mingw
env MINGW_TRIPLE=${target} MINGW_LOCATION=${mingw_location} cmake .. ${CMAKE_QT6} -DCMAKE_TOOLCHAIN_FILE=../Toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DSOUND=${GARGOYLE_SOUND} -DQT_VERSION=${QT_VERSION}
make -j${nproc}
make install
)

if [[ -n "${GARGOYLE_MINGW_GCC}" ]]
then
    cp "/usr/lib/gcc/${target}/${ver}/libstdc++-6.dll" "build/dist"
    cp "${mingw_location}/${target}/lib/libwinpthread-1.dll" "build/dist"

    libgccpath="/usr/lib/gcc/${target}/${ver}/libgcc_s_${libgcc}-1.dll"
    [[ -e "${libgccpath}" ]] || libgccpath=/usr/lib/gcc/${target}/${ver}/libgcc_s_*-1.dll
    cp ${libgccpath} "build/dist"
else
    cp "${mingw_location}/${target}/bin/libc++.dll" "build/dist"
    cp "${mingw_location}/${target}/bin/libunwind.dll" "build/dist"
fi

case ${GARGOYLE_SOUND} in
    QT)
        SOUND_DLLS="Qt${QT_VERSION}Multimedia Qt${QT_VERSION}Network libfluidsynth-3 libglib-2.0-0 libintl-8 libmpg123-0 libogg-0 libopenmpt-0 libpcre2-8-0 libsndfile libvorbis-0 libvorbisenc-2 libvorbisfile-3"
        ;;
    SDL)
        SOUND_DLLS="SDL2 SDL2_mixer libmodplug-1 libmpg123-0 libogg-0 libopenmpt-0 libvorbis-0 libvorbisfile-3"
        ;;
esac

for dll in Qt${QT_VERSION}Core Qt${QT_VERSION}Gui Qt${QT_VERSION}Widgets libfmt libfreetype-6 libpng16-16 libturbojpeg zlib1 ${SOUND_DLLS}
do
    if [[ "${dll}" =~ ^Qt6 ]]
    then
        cp "${QT6HOME}/bin/${dll}.dll" "build/dist"
    else
        cp "${mingw_location}/${target}/bin/${dll}.dll" "build/dist"
    fi
done

find build/dist \( -name '*.exe' -o -name '*.dll' \) -exec ${target}-strip --strip-unneeded {} \;

mkdir -p "build/dist/plugins/platforms"
if [[ "${QT_VERSION}" == "5" ]]
then
    cp "${mingw_location}/${target}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"
else
    cp "${QT6HOME}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"
fi

if [[ "${GARGOYLE_SOUND}" == "QT" ]]
then
    if [[ "${QT_VERSION}" == "5" ]]
    then
        mkdir -p build/dist/plugins/audio
        cp "${mingw_location}/${target}/plugins/audio/qtaudio_windows.dll" "build/dist/plugins/audio"
    else
        mkdir -p build/dist/plugins/multimedia
        cp "${QT6HOME}/plugins/multimedia/windowsmediaplugin.dll" "build/dist/plugins/multimedia"
    fi
fi

[[ "${GARGOYLE_BUILD_ONLY}" ]] && exit

if [[ -z "${GARGOYLE_NO_INSTALLER}" ]]
then
    export GARGOYLE_ARCH
    makensis -V4 installer.nsi
fi

cp licenses/*.txt build/dist
cp fonts/Gargoyle*.ttf build/dist
cp fonts/unifont*.otf build/dist
mkdir build/dist/themes
cp themes/*.json build/dist/themes
unix2dos -n garglk/garglk.ini build/dist/garglk.ini

zip=gargoyle-$(<VERSION)-windows-${GARGOYLE_ARCH}.zip
rm -f ${zip}
(cd build/dist && zip -r ../../${zip} *)
