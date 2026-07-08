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
# table, search for it and copy it into build/dist, then continue
# scanning. DLLs not found in any search path are assumed to be provided
# by Windows itself and silently skipped.
#
# The first argument is an explicit-Qt bin directory (from -Q), or empty
# for a system build. Qt* DLLs are taken from there first so a plugin's
# Qt libraries match the exact Qt build the plugin came from (Qt plugins
# rely on private symbols). Every other DLL (libc++, zlib, …) prefers
# the sysroot; otherwise a Qt-bundled stale libc++.dll would shadow the
# sysroot's and break code using newer symbols. The Qt dir is still a
# last-resort fallback for non-Qt deps that a Qt build ships
# but the sysroot lacks.
#
# Args:
#   $1:   explicit-Qt bin dir, or "" for a system build
#   $2..: sysroot directories to search for DLLs
copy_dll_deps() {
    local qt_dir="$1"
    shift
    local search_paths=("$@")
    while true
    do
        local changed=false
        local dll
        while IFS= read -r dll
        do
            [[ -e "build/dist/${dll}" ]] && continue
            local dirs
            if [[ -n "${qt_dir}" && "${dll}" == Qt*.dll ]]
            then
                dirs=("${qt_dir}" "${search_paths[@]}")
            elif [[ -n "${qt_dir}" ]]
            then
                dirs=("${search_paths[@]}" "${qt_dir}")
            else
                dirs=("${search_paths[@]}")
            fi
            local dir
            for dir in "${dirs[@]}"
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
