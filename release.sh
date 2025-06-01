#!/bin/bash

set -eux

# Simplify the building of releases.
#
# Options:
#
# -a: Build an AppImage (x86_64) on Linux.
# -m: Build both x86_64 and ARM DMG on Mac.
# -n: Notarize the resulting binary (Mac only).
# -w: Build i686 and x86_64 Windows installers and standalone ZIPs,
#     and aarch64 and armv7 Windows standalone ZIPs via MinGW.

fatal() {
    echo "${@}" >&2
    exit 1
}

build_appimage=
build_mac=
build_windows=
notarize=

while getopts "amnw" o
do
    case "${o}" in
        a)
            build_appimage=1
            ;;
        m)
            build_mac=1
            ;;
        n)
            notarize=1
            ;;
        w)
            build_windows=1
            ;;
        *)
            fatal "Usage: $0 [-amnw]"
            ;;
    esac
done

if [[ "${build_appimage}" ]]
then
    ./gargoyle-appimage.sh -c
fi

if [[ "${build_mac}" ]]
then
    if ! security show-keychain-info login.keychain > /dev/null 2>&1
    then
        echo "Keychain is locked; unlock before continuing"
        security unlock-keychain login.keychain
    fi

    rm -rf Gargoyle.app Gargoyle-x86_64.app
    PATH=/usr/local/bin:$PATH ./gargoyle_osx.sh -c -n
    mv Gargoyle.app Gargoyle-x86_64.app
    PATH=/opt/homebrew/bin:$PATH ./gargoyle_osx.sh -c -n

    binaries=(Gargoyle.app/Contents/Frameworks/*.dylib Gargoyle.app/Contents/PlugIns/* Gargoyle.app/Contents/MacOS/Gargoyle)

    for binary in "${binaries[@]}"
    do
        src="Gargoyle-x86_64.app/${binary#Gargoyle.app/}"
        lipo -create "${binary}" "${src}" -output "${binary}"
    done

    codesign -f -o runtime --sign "${APPLE_CERT_NAME}" "${binaries[@]}" Gargoyle.app
    ditto -c -k --keepParent Gargoyle.app Gargoyle.zip

    if [[ "${notarize}" ]]
    then
        xcrun notarytool submit Gargoyle.zip --apple-id "${APPLE_ID}" --team-id "${APPLE_TEAM_ID}" --wait
        xcrun stapler staple Gargoyle.app
    fi

    dmg_filename="Gargoyle-$(<VERSION).dmg"
    rm -f "${dmg_filename}"
    hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ "${dmg_filename}"
fi

if [[ "${build_windows}" ]]
then
    for arch in i686 x86_64 aarch64 armv7
    do
        ./windows.sh -cq -a "${arch}"
    done
fi
