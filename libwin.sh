# Helpers shared by windows.sh (MinGW) and msvc.sh (MSVC). Both
# scripts populate build/dist with a Gargoyle binary and DLLs for
# Windows; this file factors out the common DLL-copy logic.
#
# Sourced, not executed; assumes the cwd is the repo root (i.e. the
# parent of build/dist).

# Copy a Qt plugin to build/dist/plugins.
#
# Args:
#   $1: Qt's plugins directory (e.g. "${qt}/plugins")
#   $2: plugin path within that directory, no .dll extension
#       (e.g. "imageformats/qjpeg")
copy_plugin() {
    local plugins_dir="${1}"
    local plugin_path="${2}"
    local subdir="${plugin_path%/*}"
    mkdir -p "build/dist/plugins/${subdir}"
    cp "${plugins_dir}/${plugin_path}.dll" "build/dist/plugins/${subdir}"
}

# Recursively discover and copy DLL dependencies from all PE files in
# build/dist (binaries and plugins). For each DLL named in any import
# table, search the given paths in order; if found, copy it into
# build/dist and continue scanning. DLLs not found in any search path
# are assumed to be provided by Windows itself and silently skipped.
#
# Args:
#   $1..: directories to search for DLLs
copy_dll_deps() {
    local search_paths=("$@")
    while true
    do
        local changed=false
        local dll
        while IFS= read -r dll
        do
            [[ -e "build/dist/${dll}" ]] && continue
            local dir
            for dir in "${search_paths[@]}"
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
}
