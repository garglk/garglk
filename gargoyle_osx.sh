#!/bin/bash

set -e

# XXX Temporary hack for a broken sdl2_mixer or readline package in Homebrew.
# sdl2_mixer's pkg-config file requires readline, but the readline pkg-config
# file isn't installed to /usr/local/lib/pkgconfig, meaning it can't be found.
# I don't know the logic behind what gets symlinked into /usr/local/lib/pkgconfig
# so I don't know who's at fault here, but the hacky way around it is to set
# $PKG_CONFIG_PATH to point to readline's pkg-config directory. This only
# happens if $PKG_CONFIG_PATH isn't already set.
if ! pkg-config readline --exists; then
    if [[ -z "${PKG_CONFIG_PATH}" ]]; then
        export PKG_CONFIG_PATH="$(dirname $(find /usr/local/Cellar/readline -name readline.pc|tail -1))"
    fi
fi

# Use Homebrew if available. Alternately, you could just set the variable to
# either yes or no.
if [ "${MAC_USEHOMEBREW}" == "" ]; then
  MAC_USEHOMEBREW=no
  brew --prefix > /dev/null 2>&1 && MAC_USEHOMEBREW=yes
fi

if [ "${MAC_USEHOMEBREW}" == "yes" ]; then
  HOMEBREW_OR_MACPORTS_LOCATION="$(brew --prefix)"
else
  HOMEBREW_OR_MACPORTS_LOCATION="$(pushd "$(dirname "$(which port)")/.." > /dev/null ; pwd -P ; popd > /dev/null)"
fi

MACOS_MIN_VER="10.13"
echo "MACOS_MIN_VER $MACOS_MIN_VER"

# Use as many CPU cores as possible
NUMJOBS=$(sysctl -n hw.ncpu)

GARGDIST=build/dist
BUNDLE=Gargoyle.app/Contents

GARVERSION=$(cat VERSION)

rm -rf Gargoyle.app
mkdir -p "$BUNDLE/MacOS"
mkdir -p "$BUNDLE/Frameworks"
mkdir -p "$BUNDLE/Resources/Fonts"
mkdir -p "$BUNDLE/Resources/themes"
mkdir -p "$BUNDLE/PlugIns"

rm -rf $GARGDIST
mkdir -p build-osx
cd build-osx
cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_MIN_VER} -DDIST_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_FRAMEWORK=LAST -DCMAKE_EXPORT_COMPILE_COMMANDS=1
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
  diff "${PREVIOUS_UNIQUE_DYLIB_PATHS}" "${UNIQUE_DYLIB_PATHS}"

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

# exit

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

echo "Creating DMG..."
hdiutil create -fs "HFS+J" -ov -srcfolder Gargoyle.app/ "gargoyle-$GARVERSION-mac.dmg"

echo "Done."
