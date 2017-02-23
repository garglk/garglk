#!/bin/bash
export PATH=~/dev/buildroot/buildroot-2016.11.1/output/host/usr/bin:$PATH

SYSROOT=~/dev/buildroot/buildroot-2016.11.1/output/staging
export PKG_CONFIG_DIR=
export PKG_CONFIG_LIBDIR=${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=${SYSROOT}

#jam -sBUILD=DEBUG 
#jam -sBUILD=DEBUG install
jam  -dx -q -sCROSS=true -j3 -sGARGLKINI=/mnt/us/extensions/gargoyle/garglk.ini
#jam  install
