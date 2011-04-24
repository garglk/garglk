#!/bin/sh

mv support/iplinux ./debian
dpkg-buildpackage -amipsel -nc
#dpkg-buildpackage -ai386 -nc
#dpkg-checkbuilddeps --arch=i386
mv ./debian support/iplinux

