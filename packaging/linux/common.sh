#!/usr/bin/env bash
# Shared guards for Linux packaging work directories.

qemby_validate_work_dir() {
    local source_dir target_dir
    source_dir="$(realpath -m -- "$1")"
    target_dir="$(realpath -m -- "$2")"
    if [[ -L "$2" || "$target_dir" == / || "$target_dir" == "${HOME:-}" ]]; then
        printf 'Unsafe packaging directory: %s\n' "$2" >&2
        return 1
    fi
    case "$source_dir/" in
        "$target_dir/"*)
            printf 'Packaging directory cannot contain the source tree: %s\n' "$target_dir" >&2
            return 1
            ;;
    esac
}

qemby_check_build_cache() {
    qemby_validate_work_dir "$1" "$2" || return 1
    local source_dir build_dir cache_file cached_source cached_build
    source_dir="$(realpath -m -- "$1")"
    build_dir="$(realpath -m -- "$2")"
    cache_file="$build_dir/CMakeCache.txt"
    [[ -f "$cache_file" ]] || return 0
    cached_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_file" | head -n 1)"
    cached_build="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache_file" | head -n 1)"
    if [[ -n "$cached_source" && "$cached_source" != "$source_dir" ]] ||
       [[ -n "$cached_build" && "$cached_build" != "$build_dir" ]]; then
        printf 'Stale CMake cache in %s. Choose a new BUILD_DIR; existing files were preserved.\n' "$build_dir" >&2
        return 1
    fi
}

qemby_reset_owned_appdir() {
    qemby_validate_work_dir "$1" "$2" || return 1
    local source_dir app_dir marker expected
    source_dir="$(realpath -m -- "$1")"
    app_dir="$(realpath -m -- "$2")"
    marker="${app_dir}.qemby-owner"
    expected="$(printf '%s\n%s' "$source_dir" "$app_dir")"
    if [[ -e "$marker" || -L "$marker" ]]; then
        if [[ ! -f "$marker" || -L "$marker" || "$(cat -- "$marker")" != "$expected" ]]; then
            printf 'Refusing to overwrite an unrelated AppDir marker: %s\n' "$marker" >&2
            return 1
        fi
    fi
    if [[ -e "$app_dir" ]]; then
        if [[ ! -f "$marker" ]]; then
            printf 'Refusing to remove an unowned AppDir: %s. Choose an empty APPDIR.\n' "$app_dir" >&2
            return 1
        fi
        # The absolute target was checked above and carries this project's marker.
        rm -rf -- "$app_dir"
    fi
    mkdir -p -- "$app_dir"
    printf '%s\n' "$expected" > "$marker"
}
