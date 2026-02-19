#!/bin/bash

set -ex

qt="$HOME/Qt/current/msvc2022_64"

nproc=$(getconf _NPROCESSORS_ONLN)

[[ -e build/dist ]] && exit 1

(
mkdir build-msvc
cd build-msvc
cmake .. -DCMAKE_PREFIX_PATH="${qt}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=Toolchain-msvc.cmake -DSOUND=QT -DWITH_BUNDLED_FMT=ON -DDIST_INSTALL=ON -DWITH_FRANKENDRIFT=ON
make -j${nproc}
make install
)

# Qt plugins are runtime-loaded (not in import tables), so copy explicitly.
mkdir -p "build/dist/plugins/platforms"
cp "${qt}/plugins/platforms/qwindows.dll" "build/dist/plugins/platforms"
mkdir -p "build/dist/plugins/multimedia"
cp "${qt}/plugins/multimedia/windowsmediaplugin.dll" "build/dist/plugins/multimedia"

# Recursively discover and copy DLL dependencies from all PE files in
# build/dist (including plugins). Search paths are the MSVC sysroot and
# the Qt bin directory. System DLLs (provided by Windows) are skipped.
dll_search_paths=("/usr/x86_64-pc-windows-msvc/bin" "${qt}/bin")
while true
do
    changed=false
    while IFS= read -r dll
    do
        [[ -e "build/dist/${dll}" ]] && continue
        for dir in "${dll_search_paths[@]}"
        do
            if [[ -e "${dir}/${dll}" ]]
            then
                cp "${dir}/${dll}" build/dist
                changed=true
                break
            fi
        done
    done < <(find build/dist \( -iname "*.exe" -o -iname "*.dll" \) -exec objdump -p {} + 2>/dev/null | sed -n "s/.*DLL Name: //p" | sort -u)
    $changed || break
done

cp licenses/*.txt build/dist
cp fonts/Gargoyle*.ttf build/dist
cp fonts/unifont*.otf build/dist
mkdir build/dist/themes
cp themes/*.json build/dist/themes
unix2dos -n garglk/garglk.ini build/dist/garglk.ini
