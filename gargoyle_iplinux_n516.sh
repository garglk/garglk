#!/bin/sh

mv support/iplinux ./debian
dpkg-buildpackage -amipsel -nc
mv ./debian support/iplinux

