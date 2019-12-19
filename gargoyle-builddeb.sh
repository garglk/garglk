#!/bin/bash
# This scripts creates and builds a simple Gargoyle DEB package
#
# Prerequisites:
#  - dpkg-deb tools must be installed.
#

# Setup working vars for deb build.  These can  be changed as needed for different revisions or build environments.
PROJECT=gargoyle
MAJORVER=2019
MINORVER=1
PACKAGEREV=1
ARCH=amd64
DEB_ROOT_DIR=~/deb_factory
DATE=$(date "+%a, %e %h %Y %H:%M:%S %:::z")
PKG_NAME=${PROJECT}_${MAJORVER}.${MINORVER}-${PACKAGEREV}
PKG_TAR=/tmp/${PKG_NAME}.tar.gz
PKG_DIR=${DEB_ROOT_DIR}/${PKG_NAME}/debian/tmp
PKG_DEB_ROOT=${DEB_ROOT_DIR}/${PKG_NAME}/debian

#Clean up any pre-existing build directories so we have a fresh build.
if [ -d "./build/dist" ]
then
    rm -rfv ./build/dist
fi
if [ -d "./build/linux.release" ]
then
    rm -rfv ./build/linux.release
fi

#Build Gargoyle without SDL (currently causing issue building on Ubuntu)
jam -sUSESDL=no
jam -sUSESDL=no install

#Create temp directories to contain all elements of the distribution and copy them into it.
mkdir -p ${PKG_DIR}
mkdir -p ${PKG_DIR}/etc
mkdir -p ${PKG_DIR}/usr/{bin,lib,share}
mkdir -p ${PKG_DIR}/usr/lib/${PROJECT}
mkdir -p ${PKG_DIR}/usr/share/{applications,doc,lintian,pixmaps}
mkdir -p ${PKG_DIR}/usr/share/doc/${PROJECT}
mkdir -p ${PKG_DIR}/DEBIAN

#Copy over the gargoyle package elements into the temp directory structure.
cp -v ./build/dist/* ${PKG_DIR}/usr/lib/${PROJECT}
mv ${PKG_DIR}/usr/lib/${PROJECT}/libgarglk.so ${PKG_DIR}/usr/lib/libgarglk.so
cp -v ./garglk/garglk.ini ${PKG_DIR}/etc
cp -v ./garglk/gargoyle-house.png ${PKG_DIR}/usr/share/pixmaps

#Switch into the deb root directory and create the various control files dpkg needs to build a deb package.
pushd ${PKG_DEB_ROOT}

#Create a small script to envoke gargoyle with to avoid having the git interpreter confused with the git SCM binary.
echo -e '#!/bin/sh\n/usr/lib/gargoyle/gargoyle &\nexit' > gargoyle-free
chmod +x gargoyle-free

#Create a desktop entry so gargoyle can be started without having to open the cmd line.
cat > ${PROJECT}.desktop <<'EOF'
[Desktop Entry]
Name=gargoyle-free
Comment=Interactive fiction interpreter / player supporting many formats.
Exec=gargoyle-free
Icon=/usr/share/pixmaps/gargoyle-house.png
Categories=Game;Emulator;
Type=Application
Terminal=0
X-Desktop-File-Install-Version=0.3
EOF

#Move the launcher script and the desktop entry into the temp directory structure.
mv ./gargoyle-free ./tmp/usr/bin
mv ./gargoyle.desktop ./tmp/usr/share/applications

#Write out a simple control file template.  This file will be used by dkpg-gencontrol in order to generate the final debian control file that includes dynamically determined library dependencies.  The library dependencies are generated using dpkg-shlibdeps.
cat > control << EOF
Source: gargoyle-free
Maintainer: Garglk Developer <https://github.com/garglk/garglk>

Package: gargoyle-free
Priority: extra
Architecture: $ARCH
EOF
cat >> control << 'EOF'
Depends: ${shlibs:Depends}
Description: graphical player for Interactive Fiction games
 Gargoyle is an Interactive Fiction (text adventure) player that
 supports all the major interactive fiction formats.
 .
 Most interactive fiction is distributed as portable game files. These
 portable game files come in many formats. In the past, you used to
 have to download a separate player (interpreter) for each format of
 IF you wanted to play. Instead, Gargoyle provides unified player.
 .
 Gargoyle is based on the standard interpreters for the formats it
 supports: .taf (Adrift games, played with Scare), .dat (AdvSys),
 *.agx/.d$$ (AGiliTy), .a3c (Alan3), .asl/.cas (Quest games, played
 with Geas), .jacl/.j2 (JACL), .l9/.sna (Level 9), .mag (Magnetic),
 *.saga (Scott Adams Grand Adventures), .gam/.t3 (TADS),
 *.z1/.z2/.z3/.z4/.z5/.z6/.z7/.z8 (Inform Z-Machine games, played with
 Frotz, Nitfol or Bocfel), .ulx/.blb/.blorb/.glb/.gblorb (Inform or
 Superglús games compiled to the Glulxe VM in Blorb archives, played
 with Git or Glulxe), .zlb, .zblorb (Inform Z-Machine games in Blorb
 archives, played with Frotz).
 .
 (note: do not confuse the Git Glux interpreter with the Git DVCS or
 the GNU Interactive Tools)
 .
 Gargoyle also features graphics, sounds and Unicode support.
 .
 Technically all the bundled interpreters support the Glk API to
 manage I/O (keyboard, graphics, sounds, file) in IF games. Gargoyle
 provides a Glk implementation called garglk that displays texts and
 images in a graphical Gtk window, with care on typography.
 .
 Limitations:
 .
 * This free version of gargoyle does not include the non-free Hugo
 interpreter (and uses a different, free monospace font).
 .
 * While Gargoyle can display in-game pictures, it does not provide a
 way to display the cover art present in some Blorb archives.
 .
 * The TADS interpreter doesn't support HTML TADS; you can play
 the games, but will miss the hyperlinks.
EOF

#Write out a simple change log, dpkg-gencontrol refuses to run without one.
cat > changelog << EOF
gargoyle-free ($MAJORVER.$MINORVER.$PACKAGEREV) sid; urgency=low, binary-only=yes

  * Binary-only non-maintainer package for amd64; no source changes.
  * Building for most recent version of gargoyle.

 -- $ARCH Garglk Developer <https://github.com/garglk/garglk>  $DATE
EOF

popd

#Move into the package directory to generate library dependency list and the final control file.
pushd ${DEB_ROOT_DIR}/${PKG_NAME}

#Make the libgarglk.so visible in LD_LIBRARY_PATH so dpkg-shlibdeps does not complain it cannot find it.
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PKG_DIR}/usr/lib/${PROJECT}

#Determine dependencies and write out final control file.
dpkg-shlibdeps ./debian/tmp/usr/lib/${PROJECT}/*
dpkg-gencontrol -v$MAJORVER.$MINORVER

#Remove the control template, copy the package contents to final packaging location, remove temp directories as needed.
rm -rfv ./debian/control
mkdir ./DEBIAN
cp -v ./debian/tmp/DEBIAN/control ./DEBIAN/control
mv ${PKG_DIR}/etc ./
mv ${PKG_DIR}/usr ./
cp -v ./debian/changelog ./usr/share/doc/${PROJECT}/changelog
rm -rfv ./debian
popd

#Build the actual debian package and clean up.
pushd ${DEB_ROOT_DIR}
dpkg-deb --build ${PKG_NAME}
rm -rfv ${PKG_NAME}
popd


