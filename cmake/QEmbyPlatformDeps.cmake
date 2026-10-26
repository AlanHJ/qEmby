include_guard(GLOBAL)

function(qemby_append_default_qt_prefixes)
    set(_qt_candidates)

    if(QEMBY_QT_ROOT)
        list(APPEND _qt_candidates "${QEMBY_QT_ROOT}")
    endif()

    if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
        list(APPEND _qt_candidates "$ENV{QTDIR}")
    endif()

    if(WIN32)
        list(APPEND _qt_candidates "E:/Qt6/6.9.2/msvc2022_64")
    endif()

    foreach(_candidate IN LISTS _qt_candidates)
        if(EXISTS "${_candidate}")
            list(APPEND CMAKE_PREFIX_PATH "${_candidate}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()

function(qemby_link_libmpv target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "qemby_link_libmpv target does not exist: ${target_name}")
    endif()

    if(WIN32)
        if(NOT DEFINED QEMBY_LIBMPV_ROOT)
            set(QEMBY_LIBMPV_ROOT "${CMAKE_SOURCE_DIR}/libs/libmpv" CACHE PATH
                "Path to the bundled Windows libmpv package")
        endif()

        set(_mpv_include_dir "${QEMBY_LIBMPV_ROOT}/include")
        set(_mpv_import_library "${QEMBY_LIBMPV_ROOT}/lib/libmpv.dll.a")
        set(_mpv_runtime_dll "${QEMBY_LIBMPV_ROOT}/bin/libmpv-2.dll")

        if(NOT EXISTS "${_mpv_include_dir}/mpv/client.h")
            message(FATAL_ERROR
                "Bundled libmpv headers were not found in ${_mpv_include_dir}. "
                "Set QEMBY_LIBMPV_ROOT to your Windows libmpv bundle.")
        endif()

        if(NOT EXISTS "${_mpv_import_library}")
            message(FATAL_ERROR
                "Bundled libmpv import library was not found: ${_mpv_import_library}")
        endif()

        if(NOT TARGET qemby_libmpv)
            add_library(qemby_libmpv INTERFACE)
            target_include_directories(qemby_libmpv INTERFACE "${_mpv_include_dir}")
            target_link_libraries(qemby_libmpv INTERFACE "${_mpv_import_library}")
        endif()

        target_link_libraries(${target_name} PRIVATE qemby_libmpv)

        if(EXISTS "${_mpv_runtime_dll}")
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_mpv_runtime_dll}"
                    $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copying bundled libmpv runtime next to ${target_name}"
            )
        else()
            message(WARNING "Bundled libmpv runtime DLL was not found: ${_mpv_runtime_dll}")
        endif()

        return()
    endif()

    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBMPV QUIET IMPORTED_TARGET mpv)
    endif()

    if(NOT TARGET qemby_libmpv)
        add_library(qemby_libmpv INTERFACE)

        if(TARGET PkgConfig::LIBMPV)
            target_link_libraries(qemby_libmpv INTERFACE PkgConfig::LIBMPV)
        else()
            find_path(QEMBY_LIBMPV_INCLUDE_DIR
                NAMES mpv/client.h
                DOC "Directory containing mpv/client.h")
            find_library(QEMBY_LIBMPV_LIBRARY
                NAMES mpv libmpv
                DOC "libmpv shared library")

            if(NOT QEMBY_LIBMPV_INCLUDE_DIR OR NOT QEMBY_LIBMPV_LIBRARY)
                message(FATAL_ERROR
                    "Could not find libmpv on this platform. Install libmpv development files "
                    "(for example libmpv-dev on Ubuntu) or set QEMBY_LIBMPV_INCLUDE_DIR and "
                    "QEMBY_LIBMPV_LIBRARY manually.")
            endif()

            target_include_directories(qemby_libmpv INTERFACE
                "${QEMBY_LIBMPV_INCLUDE_DIR}")
            target_link_libraries(qemby_libmpv INTERFACE
                "${QEMBY_LIBMPV_LIBRARY}")
        endif()
    endif()

    target_link_libraries(${target_name} PRIVATE qemby_libmpv)
endfunction()
