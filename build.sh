#!/bin/bash
export PATH=/home/schoen/dev/buildroot/buildroot-2015.11.1/output/host/usr/bin:$PATH
#export PKG_CONFIG_DIR=/home/schoen/dev/buildroot/buildroot-2015.08/output/staging/usr/lib/pkgconfig

SYSROOT=/home/schoen/dev/buildroot/buildroot-2015.11.1/output/staging
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

#jam -sBUILD=DEBUG -j6 -sCROSS=true
#jam -sBUILD=DEBUG install
jam -sCROSS=true -j6 -sGARGLKINI=/mnt/us/extensions/gdev/garglk.ini
jam install
