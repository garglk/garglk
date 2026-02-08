#!/bin/bash

set -e

operation=""

while getopts "bhns:v" o
do
    case "${o}" in
        b)
            operation="bundle"
            ;;
        n)
            operation="notarize"
            ;;
        v)
            operation="verify"
            ;;
        *)
            exit 1
            ;;
    esac
done

function die() {
    echo "${1}"
    exit 1
}

[[ -d "Gargoyle.app" ]] || die "Gargoyle.app does not exist"
[[ -n "${CERT_NAME}" ]] || die "\${CERT_NAME} not provided"
[[ -n "${APPLE_ID}" ]] || die "\${APPLE_ID} not provided"
[[ -n "${PROV_SHORT_NAME}" ]] || die "\${PROV_SHORT_NAME} not provided"

case "${operation}" in
    "notarize")
        codesign -f -o runtime --deep --sign "${CERT_NAME}" Gargoyle.app
        ditto -c -k --keepParent Gargoyle.app Gargoyle.zip
        xcrun notarytool submit Gargoyle.zip --apple-id "${APPLE_ID}" --team-id "${APPLE_TEAM_ID}" --wait
        ;;
    "bundle")
        xcrun stapler staple Gargoyle.app
        rm -f gargoyle-2026.1.1-mac.dmg
        hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ gargoyle-2026.1.1-mac.dmg
        ;;
    "verify")
        codesign -d --verbose=4 Gargoyle.app
        ;;
    "")
        die "No operation requested"
        ;;
    *)
        die "Unknown operation: ${operation}"
        ;;
esac
