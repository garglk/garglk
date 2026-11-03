#!/bin/bash

set -eu

fatal() {
    echo "${@}" >&2
    exit 1
}

GARGOYLE_CLEAN=
GARGOYLE_FRANKENDRIFT="OFF"
GARGOYLE_INTERFACE="COCOA"
GARGOYLE_NO_DMG=
GARGOYLE_SOUND="SDL3"
GARGOYLE_SCARE="OFF"
GARGOYLE_CMAKE_EXTRAS=""

while getopts "2cfnsq" o
do
    case "${o}" in
        2)
            GARGOYLE_SOUND="SDL2"
            ;;
        c)
            GARGOYLE_CLEAN=1
            ;;
        f)
            GARGOYLE_FRANKENDRIFT="ON"
            ;;
        n)
            GARGOYLE_NO_DMG=1
            ;;
        s)
            GARGOYLE_SCARE="ON"
            ;;
        q)
            GARGOYLE_INTERFACE="QT"
            ;;
        *)
            fatal "Usage: $0 [-2cfnsq]"
            ;;
    esac
done

# Qt builds default to Qt sound (matching CMake); -2 still selects SDL2.
if [[ "${GARGOYLE_INTERFACE}" == "QT" && "${GARGOYLE_SOUND}" == "SDL3" ]]; then
    GARGOYLE_SOUND="QT"
fi

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

MACOS_MIN_VER="10.15"

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
cmake .. -DBUILD_SHARED_LIBS=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOS_MIN_VER} -DDIST_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_FIND_FRAMEWORK=LAST -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -DINTERFACE="${GARGOYLE_INTERFACE}" -DSOUND="${GARGOYLE_SOUND}" -DWITH_FRANKENDRIFT="${GARGOYLE_FRANKENDRIFT}" -DWITH_SCARE="${GARGOYLE_SCARE}" ${GARGOYLE_CMAKE_EXTRAS}
make "-j${NUMJOBS}"
make install
cd -

# Copy the main executable to the MacOS directory;
cp "$GARGDIST/gargoyle" "$BUNDLE/MacOS/Gargoyle"

# Copy terps to the PlugIns directory.
find "${GARGDIST}" -type f -not -name '*.dylib' -not -name 'gargoyle' -print0 | xargs -0 -J @ cp @ "$BUNDLE/PlugIns"

# Copy the dylibs built to the Frameworks directory.
find "${GARGDIST}" -type f -name '*.dylib' -exec cp {} "$BUNDLE/Frameworks" \;

# Qt apps need proper .framework bundles and platform plugins. The plain
# dylib copy below flattens framework binaries and leaves @rpath/Qt*.framework
# references unresolved, so deploy Qt with macdeployqt first.
if [[ "${GARGOYLE_INTERFACE}" == "QT" ]]; then
  command -v macdeployqt >/dev/null || fatal "macdeployqt is required for Qt builds (brew install qt)"

  # Point libgarglk at Frameworks before macdeployqt so it does not search
  # Homebrew for @rpath/libgarglk.dylib (which is our library, not Qt's).
  for qt_executable in "$BUNDLE/MacOS/Gargoyle" "$BUNDLE/Frameworks/libgarglk.dylib" "$BUNDLE/Frameworks/libgarglk-gpl2.dylib"
  do
    [[ -f "${qt_executable}" ]] || continue
    for lib in libgarglk.dylib libgarglk-gpl2.dylib
    do
      install_name_tool -change "@rpath/${lib}" "@executable_path/../Frameworks/${lib}" "${qt_executable}" 2>/dev/null || true
    done
  done
  [[ -f "$BUNDLE/Frameworks/libgarglk.dylib" ]] && \
    install_name_tool -id "@executable_path/../Frameworks/libgarglk.dylib" "$BUNDLE/Frameworks/libgarglk.dylib"
  [[ -f "$BUNDLE/Frameworks/libgarglk-gpl2.dylib" ]] && \
    install_name_tool -id "@executable_path/../Frameworks/libgarglk-gpl2.dylib" "$BUNDLE/Frameworks/libgarglk-gpl2.dylib"

  echo "Deploying Qt frameworks with macdeployqt..."
  # -no-plugins: macdeployqt otherwise copies every Qt plugin (PDF, SVG icons,
  # virtual keyboard, …). Those pull in optional frameworks Homebrew keeps
  # outside qtbase's rpath search path, producing noisy "Cannot resolve rpath"
  # errors we do not need. -no-codesign: we ad-hoc sign at the end.
  MACDEPLOYQT_ARGS=(Gargoyle.app -verbose=1 -no-plugins -no-codesign -libpath="${PWD}/${BUNDLE}/Frameworks")
  for qt_executable in "$BUNDLE/MacOS/Gargoyle" "$BUNDLE/Frameworks/libgarglk.dylib" "$BUNDLE/Frameworks/libgarglk-gpl2.dylib"
  do
    [[ -f "${qt_executable}" ]] && MACDEPLOYQT_ARGS+=(-executable="${qt_executable}")
  done
  macdeployqt "${MACDEPLOYQT_ARGS[@]}"

  # Install only the plugins Gargoyle actually needs.
  QT_PLUGINS="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
  if [[ -z "${QT_PLUGINS}" || ! -d "${QT_PLUGINS}" ]]; then
    QT_PLUGINS="$(brew --prefix qtbase 2>/dev/null)/share/qt/plugins"
  fi
  [[ -d "${QT_PLUGINS}" ]] || fatal "Qt plugins directory not found (tried qmake and brew --prefix qtbase)"
  mkdir -p "$BUNDLE/PlugIns/platforms" "$BUNDLE/PlugIns/multimedia" "$BUNDLE/PlugIns/styles"
  cp "${QT_PLUGINS}/platforms/libqcocoa.dylib" "$BUNDLE/PlugIns/platforms/"
  cp "${QT_PLUGINS}/multimedia/libdarwinmediaplugin.dylib" "$BUNDLE/PlugIns/multimedia/"
  if [[ -f "${QT_PLUGINS}/styles/libqmacstyle.dylib" ]]; then
    cp "${QT_PLUGINS}/styles/libqmacstyle.dylib" "$BUNDLE/PlugIns/styles/"
  fi
fi

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
  # Skip .framework binaries: flattening them breaks @rpath/Qt*.framework loads.
  while IFS= read -r dylib
  do
    if [[ "${dylib}" == *".framework/"* ]]; then
      continue
    fi
    cp "${dylib}" "$BUNDLE/Frameworks"
    chmod 644 "$BUNDLE/Frameworks/$(basename "${dylib}")"
  done < "${UNIQUE_DYLIB_PATHS}"
  return 1
}
until copy_new_dylibs ; do true; done

echo "Changing dylib IDs and references..."

# Change the dylib IDs in Frameworks (not files inside .framework bundles).
find "${BUNDLE}/Frameworks" -type f -print0 | while IFS= read -r -d "" file
do
  [[ "${file}" == *".framework/"* ]] && continue
  install_name_tool -id "@executable_path/../Frameworks/$(basename "${file}")" "${file}"
done

# Use the dylibs in Frameworks.
find "${BUNDLE}" -type f -print0 | while IFS= read -r -d "" file_path
do
  file "${file_path}" | grep -Fq 'Mach-O' || continue

  # Replace absolute Homebrew/MacPorts dylib paths.
  for original_dylib_path in $(otool -L "${file_path}" | grep -F "${HOMEBREW_OR_MACPORTS_LOCATION}" | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/'); do
    if [[ "${original_dylib_path}" == *".framework/"* ]]; then
      # Point at the macdeployqt-bundled framework binary.
      framework_leaf="$(echo "${original_dylib_path}" | sed -E 's|.*/([^/]+\.framework/.*)|\1|')"
      install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/${framework_leaf}" "${file_path}"
    else
      install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/$(basename "${original_dylib_path}")" "${file_path}"
    fi
  done

  # Replace remaining @rpath/Qt*.framework references left by Homebrew Qt.
  for original_dylib_path in $(otool -L "${file_path}" | grep -E '@rpath/Qt[^ ]+\.framework/' | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/'); do
    framework_leaf="$(echo "${original_dylib_path}" | sed -E 's|^@rpath/||')"
    install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/${framework_leaf}" "${file_path}"
  done

  # macdeployqt can leave @rpath/libgarglk without an LC_RPATH; point at Frameworks.
  for original_dylib_path in $(otool -L "${file_path}" | grep -E '@rpath/libgarglk[^ ]*\.dylib' | sed -E -e 's/^[[:space:]]+(.*)[[:space:]]+\([^)]*\)$/\1/'); do
    install_name_tool -change "${original_dylib_path}" "@executable_path/../Frameworks/$(basename "${original_dylib_path}")" "${file_path}"
  done
done

# Use the built dylibs.
find "${BUNDLE}" -type f -print0 | while IFS= read -r -d "" file_path
do
  file "${file_path}" | grep -Fq 'Mach-O' || continue
  find "${GARGDIST}" -type f -name '*.dylib' -print0 | while IFS= read -r -d "" built_dylib
  do
    install_name_tool -change "@executable_path/$(basename "${built_dylib}")" "@executable_path/../Frameworks/$(basename "${built_dylib}")" "${file_path}"
  done
done

# Ensure interpreters can find libgarglk
find Gargoyle.app/Contents/PlugIns/ -type f -print0 | while IFS= read -r -d "" plugin
do
  file "${plugin}" | grep -Fq 'Mach-O' || continue
  install_name_tool -add_rpath '@executable_path/../Frameworks' "${plugin}" 2>/dev/null || true
done
install_name_tool -add_rpath '@executable_path/../Frameworks' "$BUNDLE/MacOS/Gargoyle" 2>/dev/null || true

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
