#!/bin/bash

set -eu

fatal() {
    echo "${@}" >&2
    exit 1
}

GARGOYLE_CLEAN=
GARGOYLE_FRANKENDRIFT="OFF"
GARGOYLE_NO_DMG=
GARGOYLE_CMAKE_EXTRAS=""

while getopts "cfn" o
do
    case "${o}" in
        c)
            GARGOYLE_CLEAN=1
            ;;
        f)
            GARGOYLE_FRANKENDRIFT="ON"
            ;;
        n)
            GARGOYLE_NO_DMG=1
            ;;
        *)
            fatal "Usage: $0 [-cfn]"
            ;;
    esac
done

# Use Homebrew if available. Alternately, you could just set the variable to
# either yes or no.
MAC_USEHOMEBREW=${MAC_USEHOMEBREW:-}
if [ "${MAC_USEHOMEBREW}" == "" ]; then
  MAC_USEHOMEBREW=no
  brew --prefix > /dev/null 2>&1 && MAC_USEHOMEBREW=yes
fi

if [ "${MAC_USEHOMEBREW}" == "yes" ]; then
  command -v brew &> /dev/null || fatal "Homebrew requested but not found"
  HOMEBREW_OR_MACPORTS_LOCATION="$(brew --prefix)"
else
  command -v port &> /dev/null || fatal "Neither Homebrew nor MacPorts is available"
  HOMEBREW_OR_MACPORTS_LOCATION="$(cd "$(dirname "$(which port)")/.." && pwd)"
fi

HOST_ARCH="$(uname -m)"

if [[ "${MAC_USEHOMEBREW}" == "yes" ]]
then
    echo "Probing Homebrew architecture..."
    HOMEBREW_ARCH=$(brew config | grep "^macOS:" | cut -d "-" -f2)
else
    HOMEBREW_ARCH=""
fi

# If the Homebrew architecture in $PATH is not the same as the current
# architecture, assume a cross compile. Cross compiling is currently only
# supported on Homebrew, and only from arm64 to x86_64.
case "${HOMEBREW_ARCH}" in
    # Building for x86_64
    "x86_64")
        case "${HOST_ARCH}" in
            x86_64)
                ;;
            arm64)
                echo "Cross compiling to ${HOMEBREW_ARCH} from ${HOST_ARCH}"
                GARGOYLE_CMAKE_EXTRAS="-DCMAKE_OSX_ARCHITECTURES=${HOMEBREW_ARCH} -DCMAKE_PREFIX_PATH=${HOMEBREW_OR_MACPORTS_LOCATION}"
                ;;
            *)
                fatal "Don't know how to cross compile for ${HOMEBREW_ARCH} from ${HOST_ARCH}"
                ;;
        esac

        TARGET_ARCH="${HOMEBREW_ARCH}"
        ;;

    # Building for arm64
    "arm64")
        [[ "${HOST_ARCH}" != "arm64" ]] && fatal "Don't know how to cross compile to ${HOMEBREW_ARCH} from ${HOST_ARCH}"

        TARGET_ARCH="${HOMEBREW_ARCH}"
        ;;

    # No support for cross compiling on MacPorts at the moment. Assume the
    # current architecture.
    "")
        TARGET_ARCH="${HOST_ARCH}"
        ;;

    *)
        fatal "Unknown target architecture: ${HOMEBREW_ARCH}"
        ;;
esac

# Ensure a sane environment (mainly be certain GNU programs aren't visible).
export PATH="${HOMEBREW_OR_MACPORTS_LOCATION}/bin:/usr/bin:/bin:/usr/sbin"

if [[ "$(sw_vers -productVersion)" =~ ^10\.([0-9]+) && ${BASH_REMATCH[1]} -lt 15 ]]; then
  MACOS_MIN_VER="10.13"
else
  MACOS_MIN_VER="10.15"
fi

echo "MACOS_MIN_VER $MACOS_MIN_VER"

# Use as many CPU cores as possible
NUMJOBS=$(sysctl -n hw.ncpu)

GARGDIST=build/dist
BUNDLE=Gargoyle.app/Contents

GARVERSION=$(<VERSION)

rm -rf Gargoyle.app
mkdir -p "$BUNDLE/MacOS"
mkdir -p "$BUNDLE/Frameworks"
mkdir -p "$BUNDLE/Resources/Fonts"
mkdir -p "$BUNDLE/Resources/themes"
mkdir -p "$BUNDLE/PlugIns"

[[ -n "${GARGOYLE_CLEAN}" ]] && rm -rf build-osx build/dist

rm -rf $GARGDIST
mkdir -p build-osx
cd build-osx
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_MIN_VER} -DDIST_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_FRAMEWORK=LAST -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DWITH_FRANKENDRIFT="${GARGOYLE_FRANKENDRIFT}" ${GARGOYLE_CMAKE_EXTRAS}
make "-j${NUMJOBS}"
make install
cd -

# Copy the main executable to the MacOS directory;
cp "$GARGDIST/gargoyle" "$BUNDLE/MacOS/Gargoyle"

# Copy terps to the PlugIns directory.
find "${GARGDIST}" -type f -not -name '*.dylib' -not -name 'gargoyle' -print0 | xargs -0 -J @ cp @ "$BUNDLE/PlugIns"

# Copy the dylibs built to the Frameworks directory.
find "${GARGDIST}" -type f -name '*.dylib' -exec cp {} "$BUNDLE/Frameworks" \;

echo "Copying all required dylibs..."
PREVIOUS_UNIQUE_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
copy_new_dylibs() {
  # Get the dylibs needed.
  ALL_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
  find "${BUNDLE}" -type f -print0 | while IFS= read -r -d "" file
  do
    otool -L "${file}" | grep -F "${HOMEBREW_OR_MACPORTS_LOCATION}" | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/' >> "${ALL_DYLIB_PATHS}"
  done
  UNIQUE_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
  sort "${ALL_DYLIB_PATHS}" | uniq > "${UNIQUE_DYLIB_PATHS}"
  rm "${ALL_DYLIB_PATHS}"

  # Compare the list to the previous one.
  if diff -q "${PREVIOUS_UNIQUE_DYLIB_PATHS}" "${UNIQUE_DYLIB_PATHS}" > /dev/null ; then
    rm "${PREVIOUS_UNIQUE_DYLIB_PATHS}"
    rm "${UNIQUE_DYLIB_PATHS}"
    return 0
  fi

  cp "${UNIQUE_DYLIB_PATHS}" "${PREVIOUS_UNIQUE_DYLIB_PATHS}"

  # Copy dylibs to the Frameworks directory.
  while IFS= read -r dylib
  do
    cp "${dylib}" "$BUNDLE/Frameworks"
    chmod 644 "$BUNDLE/Frameworks/$(basename "${dylib}")"
  done < "${UNIQUE_DYLIB_PATHS}"
  return 1
}
until copy_new_dylibs ; do true; done

echo "Changing dylib IDs and references..."

# Change the dylib IDs in Frameworks.
find "${BUNDLE}/Frameworks" -type f -exec install_name_tool -id "@executable_path/../Frameworks/$(basename "{}")" {} \;

# Use the dylibs in Frameworks.
find "${BUNDLE}" -type f -print0 | while IFS= read -r -d "" file_path
do
  # Replace dylib paths.
  for original_dylib_path in $(otool -L "${file_path}" | grep -F "${HOMEBREW_OR_MACPORTS_LOCATION}" | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/'); do
    install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/$(basename "${original_dylib_path}")" "${file_path}"
  done
done

# Use the built dylibs.
find "${BUNDLE}" -type f -print0 | while IFS= read -r -d "" file_path
do
  find "${GARGDIST}" -type f -name '*.dylib' -exec install_name_tool -change "@executable_path/$(basename "{}")" "@executable_path/../Frameworks/$(basename "{}")" "${file_path}" \;
done

# Ensure interpreters can find libgarglk
find Gargoyle.app/Contents/PlugIns/ -type f -exec install_name_tool -add_rpath '@executable_path/../Frameworks' {} \;
install_name_tool -add_rpath '@executable_path/../Frameworks' "$BUNDLE/MacOS/Gargoyle"

echo "Copying additional support files..."
/usr/bin/sed -E -e "s/INSERT_VERSION_HERE/$GARVERSION/" garglk/launcher.plist > $BUNDLE/Info.plist

cp garglk/launchmac.nib "$BUNDLE/Resources/MainMenu.nib"
cp garglk/garglk.ini "$BUNDLE/Resources"
cp garglk/*.icns "$BUNDLE/Resources"
cp licenses/* "$BUNDLE/Resources"

cp fonts/Gargoyle*.ttf $BUNDLE/Resources/Fonts
cp fonts/unifont*.otf $BUNDLE/Resources
cp themes/*.json $BUNDLE/Resources/themes

codesign -s - -f --deep Gargoyle.app

if [[ -z "${GARGOYLE_NO_DMG}" ]]
then
    echo "Creating DMG..."
    hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ "gargoyle-$GARVERSION-$TARGET_ARCH.dmg"
fi

echo "Done."
