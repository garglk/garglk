#!/bin/bash
export PATH=~/dev/buildroot/buildroot-2015.11.1/output/host/usr/bin:$PATH
#export PKG_CONFIG_DIR=~/dev/buildroot/buildroot-2015.08/output/staging/usr/lib/pkgconfig

SYSROOT=~/dev/buildroot/buildroot-2015.11.1/output/staging
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

#jam -sBUILD=DEBUG -j8 -sCROSS=true -sGARGLKINI=/mnt/us/extensions/gargoyle/garglk.ini
#jam -sBUILD=DEBUG install
#jam  -sCROSS=true -j8 -sGARGLKINI=/mnt/us/extensions/gdev/garglk.ini
jam  -sCROSS=true -j8 -sGARGLKINI=/mnt/us/extensions/gargoyle/garglk.ini
jam  install
