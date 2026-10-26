#!/bin/bash







usage() {
    echo "Usage: $(basename $0) -i <dir> -m <path>"
    echo "                   --plugindir <plugin_dir> --libdir <lib_dir> --qmldir <qml_dir>"
    echo "                  [--qmake <qmake_path>] [--extra <extra_path>]..."
    echo "                  [--qml <qml_module>]... [--plugin <plugin>]... [--copy <src> <dest>]..."
    echo "                  [-f] [-s] [-V] [-h]"
    echo "  -i <input_dir>              Directory containing binaries and libraries"
    echo "  -m <corecmd_path>           Path to corecmd"
    echo "  --plugindir <plugin_dir>    Output directory for plugins"
    echo "  --libdir <lib_dir>          Output directory for libraries"
    echo "  --qmldir <qml_dir>          Output directory for QML files"
    echo "  --qmake <qmake_path>        Path to qmake (optional)"
    echo "  --extra <extra_path>        Extra plugin searching path (repeatable)"
    echo "  --qml <qml_module>          Relative path to QML directory (repeatable)"
    echo "  --plugin <plugin>           Specify a Qt plugin to deploy (repeatable)"
    echo "  --copy <src> <dest>         Specify additional binary file to copy and its destination directory (repeatable)"
    echo "  -@                          Add library searching paths from a list file"
    echo "  -L                          Add a library searching path"
    echo "  -f                          Force overwrite existing files"
    echo "  -s                          Ignore C/C++ runtime and system libraries"
    echo "  -V                          Show verbose output"
    echo "  -h                          Show this help message"
}


EXTRA_PLUGIN_PATHS=()
QML_REL_PATHS=()
ARGS=()
VERBOSE=""
PLUGINS=()
FILES=""


while (( "$#" )); do
    case "$1" in
        -i)            INPUT_DIR="$2"; shift 2;;
        -m)            CORECMD_PATH="$2"; shift 2;;
        -@)            ARGS+=("-@ \"$2\""); shift 2;;
        -L)            ARGS+=("-L \"$2\""); shift 2;;
        --plugindir)   PLUGIN_DIR="$2"; shift 2;;
        --libdir)      LIB_DIR="$2"; shift 2;;
        --qmldir)      QML_DIR="$2"; shift 2;;
        --qmake)       QMAKE_PATH="$2"; shift 2;;
        --extra)       EXTRA_PLUGIN_PATHS+=("$2"); shift 2;;
        --plugin)      PLUGINS+=("$2"); shift 2;;
        --qml)         QML_REL_PATHS+=("$2"); shift 2;;
        --copy)        ARGS+=("-c \"$2\" \"$3\""); shift 3;;
        -f|-s)         ARGS+=("$1"); shift;;
        -V)            VERBOSE="-V"; shift;;
        -h) usage; exit 0;;
        *) echo "Error: Unsupported argument $1"; usage; exit 1;;
    esac
done


for arg in INPUT_DIR PLUGIN_DIR LIB_DIR QML_DIR CORECMD_PATH; do
    if [[ -z ${!arg} ]]; then
        echo "Error: Missing required argument '$arg'"
        usage
        exit 1
    fi
done


PLUGIN_PATHS=()
QML_PATH=""
if [[ -n "$QMAKE_PATH" ]]; then
    QMAKE_PLUGIN_PATH=$($QMAKE_PATH -query QT_INSTALL_PLUGINS)
    PLUGIN_PATHS+=("$QMAKE_PLUGIN_PATH")

    QML_PATH=$($QMAKE_PATH -query QT_INSTALL_QML)
fi


PLUGIN_PATHS+=("${EXTRA_PLUGIN_PATHS[@]}")



if [[ ${
    echo "Error: qmake path must be specified when QML paths are provided"
    usage
    exit 1
fi


search_input_dir() {
    local path="$1"
    for item in "$path"/*; do
        if [ -d "$item" ]; then

            if [[ "OSTYPE" == "darwin"* ]] && [[ "$item" == *.framework ]]; then
                FILES="$FILES \"$item\""
            else
                search_input_dir "$item"
            fi
        elif [ -f "$item" ]; then
            if [[ "$OSTYPE" == "msys"* ]] || [[ "$OSTYPE" == "win32" ]] || [[ "$OSTYPE" == "cygwin"* ]]; then

                FILES="$FILES \"$item\""
            else


                file_type=$(file -b "$item")
                if [[ ($file_type == "ELF"* || $file_type == "Mach-O"*) && -x "$item"  ]]; then
                    FILES="$FILES \"$item\""
                fi
            fi
        fi
    done
}

search_input_dir "$INPUT_DIR"


for plugin_path in "${PLUGINS[@]}"; do

    if [[ $plugin_path == */* ]]; then
        IFS='/' read -r -a plugin_parts <<< "$plugin_path"
        

        category=${plugin_parts[0]}
        name=${plugin_parts[1]}


        dest_dir="${PLUGIN_DIR}/${category}"


        found_plugins=""


        for search_path in "${PLUGIN_PATHS[@]}"; do
            while IFS= read -r plugin; do

                plugin_name=$(basename "$plugin")


                if [[ ! $found_plugins =~ $plugin_name ]]; then
                    found_plugins+="$plugin_name "
                    ARGS+=("-c \"$plugin\" \"$dest_dir\"")
                fi
            done < <(find "${search_path}/${category}" -name "lib${name}.*" ! -name "*debug*" ! -d -print)
        done

        if [ ${
            echo "Error: Plugin '${plugin_path}' not found in any search paths."
            exit 1
        fi
    else
        echo "Error: Invalid plugin format '${plugin_path}'. Expected format: <category>/<name>"
        exit 1
    fi
done


handle_qml_file() {
    local file="$1"
    local rel_path="${file#$QML_PATH/}"

    local target="$QML_DIR/$rel_path"
    local target_dir="$(dirname "$target")"


    if [ -d "$file" ]; then
        ARGS+=("-c \"$file\" \"$target_dir\"")
        return
    fi


    if [[ "$OSTYPE" == "msys"* ]] || [[ "$OSTYPE" == "win32" ]] || [[ "$OSTYPE" == "cygwin"* ]]; then
        if [[ "$file" == *.pdb ]] || [[ "$file" == *d.dll ]]; then
            return
        fi
    else
        if [[ "$file" == *_debug.dylib ]] || [[ "$file" == *.so.debug ]]; then
            return
        fi
    fi


    if [[ "$OSTYPE" == "msys"* ]] || [[ "$OSTYPE" == "win32" ]] || [[ "$OSTYPE" == "cygwin"* ]]; then
        if [[ "$file" == *.dll || "$file" == *.exe ]]; then
            ARGS+=("-c \"$file\" \"$target_dir\"")
        else
            mkdir -p "$target_dir"
            cp "$file" "$target"
        fi
    else
        file_type=$(file -b "$file")
        if [[ ($file_type == "ELF"* || $file_type == "Mach-O"*) && -x "$file" ]]; then
            ARGS+=("-c \"$file\" \"$target_dir\"")
        else
            mkdir -p "$target_dir"
            cp "$file" "$target"
        fi
    fi
}


search_qml_dir() {
    local path="$1"
    for item in "$path"/*; do
        if [ -d "$item" ]; then

            if [[ "OSTYPE" == "darwin"* ]] && [[ "$item" == *.framework ]]; then
                handle_qml_file "$item"
            else
                search_qml_dir "$item"
            fi
        elif [ -f "$item" ]; then
            handle_qml_file "$item"
        fi
    done
}


for qml_rel_path in "${QML_REL_PATHS[@]}"; do
    full_path="$QML_PATH/$qml_rel_path"
    if [[ -d "$full_path" ]]; then

        search_qml_dir "$full_path"
    elif [[ -f "$full_path" ]]; then

        handle_qml_file "$full_path" "$QML_DIR"
    fi
done


DEPLOY_CMD="$CORECMD_PATH deploy $FILES ${ARGS[@]} -o \"$LIB_DIR\" $VERBOSE"
if [[ "$VERBOSE" == "-V" ]]; then
    echo "Executing: $DEPLOY_CMD"
fi
eval $DEPLOY_CMD


if [ $? -ne 0 ]; then
    exit 1
fi