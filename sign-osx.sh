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
        s)
            operation="status"
            req_uuid="${OPTARG}"
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
        xcrun altool --notarize-app --primary-bundle-id 'com.googlecode.garglk.Launcher' --username "${APPLE_ID}" --asc-provider "${PROV_SHORT_NAME}" --file Gargoyle.zip
        ;;
    "bundle")
        xcrun stapler staple Gargoyle.app
        rm -f gargoyle-2023.1-mac.dmg
        hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ gargoyle-2023.1-mac.dmg
        ;;
    "status")
        xcrun altool --notarization-info "${req_uuid}" -u "${APPLE_ID}"
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
