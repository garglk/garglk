FROM amd64/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update -qq && apt-get install -y -qq \
    build-essential cmake dos2unix git libarchive-tools \
    nsis p7zip-full pkg-config unzip wget zip ca-certificates \
 && rm -rf /var/lib/apt/lists/*

ARG ARCH=x86_64
ARG UBUNTU=24.04
ARG LLVM_MINGW=20260616
ARG LLVM_MINGW_HOST=msvcrt-ubuntu-22.04-x86_64
ARG ASSET_RELEASE=mingw-llvm-24.04

RUN mkdir -p /usr/llvm-mingw \
 && wget -q https://github.com/mstorsjo/llvm-mingw/releases/download/${LLVM_MINGW}/llvm-mingw-${LLVM_MINGW}-${LLVM_MINGW_HOST}.tar.xz \
 && bsdtar --strip-components 1 -x -f llvm-mingw-${LLVM_MINGW}-${LLVM_MINGW_HOST}.tar.xz -C /usr/llvm-mingw \
 && rm llvm-mingw-${LLVM_MINGW}-${LLVM_MINGW_HOST}.tar.xz \
 && rm -rf /usr/llvm-mingw/${ARCH}-w64-mingw32/include \
 && cp -a /usr/llvm-mingw/generic-w64-mingw32/include /usr/llvm-mingw/${ARCH}-w64-mingw32/ \
 && assets=https://github.com/garglk/assets/releases/download/${ASSET_RELEASE} \
 && wget -q ${assets}/mingw-llvm-${ARCH}-${UBUNTU}.tar.xz \
 && bsdtar xf mingw-llvm-${ARCH}-${UBUNTU}.tar.xz -C /usr/llvm-mingw \
 && rm mingw-llvm-${ARCH}-${UBUNTU}.tar.xz \
 && wget -q ${assets}/mingw-llvm-host-qt6-${UBUNTU}.tar.xz \
 && bsdtar xf mingw-llvm-host-qt6-${UBUNTU}.tar.xz -C /usr/llvm-mingw \
 && rm mingw-llvm-host-qt6-${UBUNTU}.tar.xz \
 && mkdir NSIS && cd NSIS \
 && wget -q https://nsis.sourceforge.io/mediawiki/images/7/78/FontName-0.7.zip \
 && unzip -q FontName-0.7.zip \
 && 7z x -bb3 FontName-0.7.exe \
 && mkdir -p /usr/share/nsis/Include /usr/share/nsis/Plugins/x86-ansi \
 && for inc in Include/*; do iconv -f CP1251 -t UTF8 "${inc}" -o "/usr/share/nsis/Include/$(basename "${inc}")"; done \
 && cp Plugins/FontName.dll /usr/share/nsis/Plugins/x86-ansi \
 && cd .. && rm -rf NSIS

WORKDIR /src
