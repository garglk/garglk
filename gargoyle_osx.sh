#!/bin/sh

set -e

# Use Homebrew if available. Alternately, you could just set the variable to
# either yes or no.
if [ "${MAC_USEHOMEBREW}" == "" ]; then
  MAC_USEHOMEBREW=no
  brew --prefix > /dev/null 2>&1 && MAC_USEHOMEBREW=yes
fi

if [ "${MAC_USEHOMEBREW}" == "yes" ]; then
  HOMEBREW_OR_MACPORTS_LOCATION="$(brew --prefix)"
else
  HOMEBREW_OR_MACPORTS_LOCATION="$(pushd "$(dirname $(which port))/.." > /dev/null ; pwd -P ; popd > /dev/null)"
fi

# If building with XCode 10+ (SDK 10.14+ Mojave), the minimum target SDK is
# 10.9 (Mavericks), due to removal of libstdc++.
SDK_VERSION=$(xcrun --show-sdk-version)
echo "SDK_VERSION $SDK_VERSION"

case $SDK_VERSION in
  *10.[7-9]* )
    MACOS_MIN_VER=10.7
    ;;
  *10.1[0-3]* )
    MACOS_MIN_VER=10.7
    ;;
  * )
    MACOS_MIN_VER=10.9
    ;;
esac
echo "MACOS_MIN_VER $MACOS_MIN_VER"

# Use as many CPU cores as possible
NUMJOBS=$(sysctl -n hw.ncpu)

GARGDIST=build/dist
DYLIBS=support/dylibs
BUNDLE=Gargoyle.app/Contents

GARVERSION=$(cat VERSION)

rm -rf Gargoyle.app
mkdir -p "$BUNDLE/MacOS"
mkdir -p "$BUNDLE/Frameworks"
mkdir -p "$BUNDLE/Resources/Fonts"
mkdir -p "$BUNDLE/PlugIns"

rm -rf $GARGDIST
mkdir -p build-osx
cd build-osx
cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_MIN_VER} -DDIST_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_FRAMEWORK=LAST
make -j${NUMJOBS}
make install
cd -

# Copy the main executable to the MacOS directory;
cp "$GARGDIST/gargoyle" "$BUNDLE/MacOS/Gargoyle"

# Copy terps to the PlugIns directory.
find "${GARGDIST}" -type f -not -name '*.dylib' -not -name 'gargoyle' -print0 | xargs -0 -J @ cp @ "$BUNDLE/PlugIns"

# Copy the dylibs built to the Frameworks directory.
for dylib in $(find "${GARGDIST}" -type f -name '*.dylib'); do
  cp "${dylib}" "$BUNDLE/Frameworks"
done

echo "Copying all required dylibs..."
PREVIOUS_UNIQUE_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
copy_new_dylibs() {
  # Get the dylibs needed.
  ALL_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
  for file in $(find "${BUNDLE}" -type f); do
    otool -L "${file}" | fgrep "${HOMEBREW_OR_MACPORTS_LOCATION}" | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/' >> "${ALL_DYLIB_PATHS}"
  done
  UNIQUE_DYLIB_PATHS="$(mktemp -t gargoylebuild)"
  cat "${ALL_DYLIB_PATHS}" | sort | uniq > "${UNIQUE_DYLIB_PATHS}"
  rm "${ALL_DYLIB_PATHS}"

  # Compare the list to the previous one.
  if diff -q "${PREVIOUS_UNIQUE_DYLIB_PATHS}" "${UNIQUE_DYLIB_PATHS}" > /dev/null ; then
    rm "${PREVIOUS_UNIQUE_DYLIB_PATHS}"
    rm "${UNIQUE_DYLIB_PATHS}"
    return 0
  fi

  cp "${UNIQUE_DYLIB_PATHS}" "${PREVIOUS_UNIQUE_DYLIB_PATHS}"
  diff "${PREVIOUS_UNIQUE_DYLIB_PATHS}" "${UNIQUE_DYLIB_PATHS}"

  # Copy dylibs to the Frameworks directory.
  for dylib in $(cat ${UNIQUE_DYLIB_PATHS}); do
    cp "${dylib}" "$BUNDLE/Frameworks"
    chmod 644 "$BUNDLE/Frameworks/$(basename ${dylib})"
  done
  return 1
}
until copy_new_dylibs ; do true; done

echo "Changing dylib IDs and references..."

# Change the dylib IDs in Frameworks.
for dylib_path in $(find "${BUNDLE}/Frameworks" -type f); do
  install_name_tool -id "@executable_path/../Frameworks/$(basename "${dylib_path}")" "${dylib_path}"
done

# Use the dylibs in Frameworks.
for file_path in $(find "${BUNDLE}" -type f); do
  # Replace dylib paths.
  for original_dylib_path in $(otool -L "${file_path}" | fgrep "${HOMEBREW_OR_MACPORTS_LOCATION}" | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/'); do
    install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/$(basename "${original_dylib_path}")" "${file_path}"
  done
done

# Use the built dylibs.
for file_path in $(find "${BUNDLE}" -type f); do
  for dylib_built_path in $(find "${GARGDIST}" -type f -name '*.dylib'); do
    install_name_tool -change "@executable_path/$(basename "${dylib_built_path}")" "@executable_path/../Frameworks/$(basename "${dylib_built_path}")" "${file_path}"
  done
done

echo "Copying additional support files..."
cat garglk/launcher.plist | /usr/bin/sed -E -e "s/INSERT_VERSION_HERE/$GARVERSION/" > $BUNDLE/Info.plist

cp garglk/launchmac.nib "$BUNDLE/Resources/MainMenu.nib"
cp garglk/garglk.ini "$BUNDLE/Resources"
cp garglk/*.icns "$BUNDLE/Resources"
cp licenses/* "$BUNDLE/Resources"

cp fonts/Gargoyle*.ttf $BUNDLE/Resources/Fonts

codesign -s - -f --deep Gargoyle.app

echo "Creating DMG..."
hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ "gargoyle-$GARVERSION-mac.dmg"

echo "Done."
