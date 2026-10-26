include_guard(GLOBAL)

include(GNUInstallDirs)

option(QEMBY_ENABLE_QT_DEPLOY
    "Deploy Qt runtime dependencies when running install() on Linux"
    OFF)
option(QEMBY_DEBIAN_PACKAGE_SHLIBDEPS
    "Let CPack query ELF dependencies for Debian packages"
    ON)
option(QEMBY_BUNDLE_MPV_RUNTIME
    "Bundle libmpv runtime dependencies into Linux package outputs"
    OFF)

set(QEMBY_RUNTIME_LIB_SUBDIR "qemby" CACHE STRING
    "Subdirectory under the install libdir for bundled private runtime libraries")
set(QEMBY_DEBIAN_PACKAGE_DEPENDS "" CACHE STRING
    "Manual Debian dependencies used when dpkg-shlibdeps is disabled")
set(_qemby_default_extra_depends
    "libqt6openglwidgets6, libqt6websockets6, libqt6svg6")
if(UNIX AND NOT APPLE)
    find_program(QEMBY_APT_CACHE_EXECUTABLE apt-cache)
    if(QEMBY_APT_CACHE_EXECUTABLE)
        execute_process(
            COMMAND "${QEMBY_APT_CACHE_EXECUTABLE}" show qt6-svg-plugins
            RESULT_VARIABLE _qemby_svg_plugin_available
            OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
        if(_qemby_svg_plugin_available EQUAL 0)
            string(APPEND _qemby_default_extra_depends ", qt6-svg-plugins")
        endif()
    endif()
endif()
set(QEMBY_DEBIAN_PACKAGE_EXTRA_DEPENDS
    "${_qemby_default_extra_depends}" CACHE STRING
    "Additional Debian runtime dependencies appended to generated package dependencies")
set(QEMBY_DEBIAN_PACKAGE_RELEASE "" CACHE STRING
    "Optional Debian package release suffix, e.g. jammy, noble, resolute, bookworm or trixie")

set(QEMBY_EXECUTABLE_NAME "qEmbyApp" CACHE STRING
    "Installed executable name used by Linux desktop files")
set(QEMBY_DESKTOP_NAME "qEmby" CACHE STRING
    "Display name used by Linux desktop files")
set(QEMBY_DESKTOP_FILE_BASENAME "qemby" CACHE STRING
    "Desktop file and icon basename for Linux packaging")
set(QEMBY_APP_ID "io.github.AlanHJ.qEmby" CACHE STRING
    "AppStream component id used by Linux packaging")
set(QEMBY_PACKAGE_VENDOR "AlanHJ" CACHE STRING
    "Package vendor used by Linux packaging")
set(QEMBY_PACKAGE_CONTACT "AlanHJ" CACHE STRING
    "Package contact used by Linux packaging")
set(QEMBY_PACKAGE_DESCRIPTION "基于 Qt 的 Emby/Jellyfin 客户端" CACHE STRING
    "Short package description used by Linux packaging")
set(QEMBY_HOMEPAGE_URL "https://github.com/AlanHJ/qEmby" CACHE STRING
    "Homepage URL used by Linux packaging")

if(UNIX AND NOT APPLE)
    # Bundled Qt must stay private. Installing libQt6*.so into /usr/lib can
    # override the distribution Qt used by KDE/Plasma after ldconfig refresh.
    # Keep qt.conf next to the private executable too; /usr/bin/qt.conf affects
    # unrelated Qt tools such as sddm-greeter.
    set(_qemby_private_lib_dir
        "${CMAKE_INSTALL_LIBDIR}/${QEMBY_RUNTIME_LIB_SUBDIR}")
    set(_qemby_private_bin_dir "${_qemby_private_lib_dir}")
    set(_qemby_private_plugins_dir
        "${_qemby_private_lib_dir}/plugins")
    set(_qemby_private_qml_dir
        "${_qemby_private_lib_dir}/qml")
    set(_qemby_private_translations_dir
        "${_qemby_private_lib_dir}/translations")

    if(TARGET qEmbyApp)
        set_target_properties(qEmbyApp PROPERTIES
            INSTALL_RPATH
                "$ORIGIN")
    endif()

    if(TARGET qEmbyCore)
        set_target_properties(qEmbyCore PROPERTIES
            INSTALL_RPATH
                "$ORIGIN")
    endif()

    if(TARGET QWKCore)
        set_target_properties(QWKCore PROPERTIES
            INSTALL_RPATH
                "$ORIGIN")
        install(TARGETS QWKCore
            LIBRARY DESTINATION ${_qemby_private_lib_dir}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${_qemby_private_lib_dir})
    endif()

    if(TARGET QWKWidgets)
        set_target_properties(QWKWidgets PROPERTIES
            INSTALL_RPATH
                "$ORIGIN")
        install(TARGETS QWKWidgets
            LIBRARY DESTINATION ${_qemby_private_lib_dir}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            ARCHIVE DESTINATION ${_qemby_private_lib_dir})
    endif()
endif()

set(_qemby_icon_source
    "${CMAKE_SOURCE_DIR}/src/qEmbyApp/resources/svg/qemby_logo.svg")
set(_qemby_packaging_source_dir "${CMAKE_SOURCE_DIR}/packaging/linux")
set(_qemby_packaging_build_dir "${CMAKE_BINARY_DIR}/packaging/linux")

file(MAKE_DIRECTORY "${_qemby_packaging_build_dir}")

set(_qemby_desktop_file
    "${_qemby_packaging_build_dir}/${QEMBY_DESKTOP_FILE_BASENAME}.desktop")
set(_qemby_metainfo_file
    "${_qemby_packaging_build_dir}/${QEMBY_DESKTOP_FILE_BASENAME}.metainfo.xml")
set(_qemby_launcher_file
    "${_qemby_packaging_build_dir}/${QEMBY_EXECUTABLE_NAME}")
set(_qemby_debian_postinst
    "${_qemby_packaging_build_dir}/postinst")
set(_qemby_debian_postrm
    "${_qemby_packaging_build_dir}/postrm")
set(_qemby_runtime_bundle_script
    "${_qemby_packaging_build_dir}/QEmbyBundleMpvRuntime.cmake")

set(_qemby_runtime_executable_name "${QEMBY_EXECUTABLE_NAME}")
if(TARGET qEmbyApp)
    get_target_property(_qemby_runtime_executable_name qEmbyApp OUTPUT_NAME)
    if(NOT _qemby_runtime_executable_name OR
       _qemby_runtime_executable_name STREQUAL "_qemby_runtime_executable_name-NOTFOUND")
        set(_qemby_runtime_executable_name "qEmbyApp")
    endif()
endif()

configure_file(
    "${_qemby_packaging_source_dir}/qemby.desktop.in"
    "${_qemby_desktop_file}"
    @ONLY
)

configure_file(
    "${_qemby_packaging_source_dir}/qemby.metainfo.xml.in"
    "${_qemby_metainfo_file}"
    @ONLY
)

if(UNIX AND NOT APPLE)
    set(QEMBY_PRIVATE_APP_RELATIVE_PATH
        "${_qemby_private_bin_dir}/${_qemby_runtime_executable_name}")

    configure_file(
        "${_qemby_packaging_source_dir}/qemby-wrapper.in"
        "${_qemby_launcher_file}"
        @ONLY
    )

    file(CHMOD "${_qemby_launcher_file}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)

    configure_file(
        "${_qemby_packaging_source_dir}/debian/postinst.in"
        "${_qemby_debian_postinst}"
        @ONLY
    )

    file(CHMOD "${_qemby_debian_postinst}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)

    configure_file(
        "${_qemby_packaging_source_dir}/debian/postrm.in"
        "${_qemby_debian_postrm}"
        @ONLY
    )
    file(CHMOD "${_qemby_debian_postrm}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/QEmbyBundleMpvRuntime.cmake.in"
    "${_qemby_runtime_bundle_script}"
    @ONLY
)

if(UNIX AND NOT APPLE)
    install(PROGRAMS "${_qemby_launcher_file}"
        DESTINATION "${CMAKE_INSTALL_BINDIR}")

    install(FILES "${_qemby_desktop_file}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

    install(FILES "${_qemby_metainfo_file}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")

    if(EXISTS "${_qemby_icon_source}")
        install(FILES "${_qemby_icon_source}"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
            RENAME "${QEMBY_DESKTOP_FILE_BASENAME}.svg")
    endif()

    if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
        install(FILES "${CMAKE_SOURCE_DIR}/README.md"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    endif()

    if(QEMBY_ENABLE_QT_DEPLOY AND QT_VERSION_MAJOR EQUAL 6 AND TARGET qEmbyApp)
        qt_generate_deploy_script(
            TARGET qEmbyApp
            OUTPUT_SCRIPT _qemby_qt_deploy_script
            CONTENT "
set(QT_DEPLOY_BIN_DIR \"${_qemby_private_bin_dir}\")
set(QT_DEPLOY_LIB_DIR \"${_qemby_private_lib_dir}\")
set(QT_DEPLOY_PLUGINS_DIR \"${_qemby_private_plugins_dir}\")
set(QT_DEPLOY_QML_DIR \"${_qemby_private_qml_dir}\")
set(QT_DEPLOY_TRANSLATIONS_DIR \"${_qemby_private_translations_dir}\")

qt6_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE:qEmbyApp>\"
    GENERATE_QT_CONF
    NO_TRANSLATIONS
)
"
        )
        install(SCRIPT "${_qemby_qt_deploy_script}")

        # Deploy Qt input context plugins from the same Qt tree as Qt6::Core.
        # Mixing system fcitx plugins with a bundled Qt from another minor
        # version can make Qt reject the plugin at runtime.
        get_target_property(_qemby_qt6_core_loc Qt6::Core LOCATION)
        get_filename_component(_qemby_qt6_lib_dir "${_qemby_qt6_core_loc}" DIRECTORY)
        get_filename_component(_qemby_qt6_prefix_dir "${_qemby_qt6_lib_dir}" DIRECTORY)
        set(_qemby_qt6_plugin_dirs)
        if(QEMBY_QT_ROOT)
            list(APPEND _qemby_qt6_plugin_dirs "${QEMBY_QT_ROOT}/plugins")
        endif()
        list(APPEND _qemby_qt6_plugin_dirs
            "${_qemby_qt6_prefix_dir}/plugins"
            "${_qemby_qt6_lib_dir}/qt6/plugins"
        )
        list(REMOVE_DUPLICATES _qemby_qt6_plugin_dirs)

        install(CODE "
            set(_dest_base \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
            set(_private_lib_dir \"${_qemby_private_lib_dir}\")
            set(_private_plugins_dir \"${_qemby_private_plugins_dir}\")
            set(_qt_input_plugin_dirs \"${_qemby_qt6_plugin_dirs}\")
            set(_qt_input_plugin_names
                \"libcomposeplatforminputcontextplugin.so\"
                \"libibusplatforminputcontextplugin.so\"
                \"libfcitx5platforminputcontextplugin.so\"
            )
            set(_copied_any_input_context FALSE)
            set(_copied_fcitx5_input_context FALSE)
            foreach(_plugin_dir \${_qt_input_plugin_dirs})
                foreach(_plugin_name \${_qt_input_plugin_names})
                    set(_plugin \"\${_plugin_dir}/platforminputcontexts/\${_plugin_name}\")
                    if(EXISTS \"\${_plugin}\")
                        message(STATUS \"Bundling Qt platform input context plugin: \${_plugin}\")
                        file(COPY \"\${_plugin}\"
                            DESTINATION \"\${_dest_base}/\${_private_plugins_dir}/platforminputcontexts\")
                        set(_copied_any_input_context TRUE)
                        if(_plugin_name STREQUAL \"libfcitx5platforminputcontextplugin.so\")
                            set(_copied_fcitx5_input_context TRUE)
                        endif()
                    endif()
                endforeach()
            endforeach()
            if(NOT _copied_any_input_context)
                message(WARNING \"No Qt platform input context plugins found in the deployed Qt tree; text input methods may be unavailable\")
            endif()
            if(_copied_fcitx5_input_context)
                file(GLOB _fcitx5_libs
                    LIST_DIRECTORIES false
                    \"/usr/lib/x86_64-linux-gnu/libFcitx5Qt6DBusAddons.so*\"
                    \"/usr/lib/libFcitx5Qt6DBusAddons.so*\")
                foreach(_lib \${_fcitx5_libs})
                    message(STATUS \"Bundling fcitx5 lib: \${_lib}\")
                    file(COPY \"\${_lib}\"
                        DESTINATION \"\${_dest_base}/\${_private_lib_dir}\")
                endforeach()
                set(_qt_lib_dir \"${_qemby_qt6_lib_dir}\")
                if(EXISTS \"\${_qt_lib_dir}/libQt6WaylandClient.so.6\")
                    file(GLOB _wl_libs
                        LIST_DIRECTORIES false
                        \"\${_qt_lib_dir}/libQt6WaylandClient.so*\")
                    foreach(_lib \${_wl_libs})
                        message(STATUS \"Bundling QtWaylandClient: \${_lib}\")
                        file(COPY \"\${_lib}\"
                            DESTINATION \"\${_dest_base}/\${_private_lib_dir}\")
                    endforeach()
                endif()
            endif()
        ")
    endif()

    if(QEMBY_BUNDLE_MPV_RUNTIME AND TARGET qEmbyApp)
        install(SCRIPT "${_qemby_runtime_bundle_script}")
    endif()

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "${QEMBY_DESKTOP_FILE_BASENAME}")
    set(CPACK_PACKAGE_VENDOR "${QEMBY_PACKAGE_VENDOR}")
    set(CPACK_PACKAGE_CONTACT "${QEMBY_PACKAGE_CONTACT}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${QEMBY_PACKAGE_DESCRIPTION}")
    set(CPACK_PACKAGE_HOMEPAGE_URL "${QEMBY_HOMEPAGE_URL}")
    set(CPACK_PACKAGE_FILE_NAME
        "${QEMBY_DESKTOP_FILE_BASENAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${QEMBY_DESKTOP_NAME}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_VERBATIM_VARIABLES YES)

    if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
        set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
    endif()

    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "video")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${QEMBY_PACKAGE_VENDOR}")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${QEMBY_PACKAGE_DESCRIPTION}")
    if(NOT QEMBY_ENABLE_QT_DEPLOY)
        set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fcitx5-frontend-qt6")
    endif()
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${_qemby_debian_postinst};${_qemby_debian_postrm}")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
    set(_qemby_debian_depends)
    foreach(_qemby_depends IN ITEMS
            "${QEMBY_DEBIAN_PACKAGE_DEPENDS}"
            "${QEMBY_DEBIAN_PACKAGE_EXTRA_DEPENDS}")
        if(_qemby_depends)
            list(APPEND _qemby_debian_depends "${_qemby_depends}")
        endif()
    endforeach()
    if(_qemby_debian_depends)
        list(JOIN _qemby_debian_depends ", " CPACK_DEBIAN_PACKAGE_DEPENDS)
    endif()
    if(QEMBY_DEBIAN_PACKAGE_RELEASE)
        set(CPACK_DEBIAN_PACKAGE_RELEASE "${QEMBY_DEBIAN_PACKAGE_RELEASE}")
    endif()

    if(QEMBY_DEBIAN_PACKAGE_SHLIBDEPS)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
        set(_qemby_private_dirs "${CMAKE_BINARY_DIR}/bin")
        if(QEMBY_QT_ROOT)
            list(APPEND _qemby_private_dirs "${QEMBY_QT_ROOT}/lib")
        endif()
        list(APPEND _qemby_private_dirs
            "${CMAKE_BINARY_DIR}/bin/${QEMBY_RUNTIME_LIB_SUBDIR}")
        list(REMOVE_DUPLICATES _qemby_private_dirs)
        set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS
            "${_qemby_private_dirs}")
    endif()
endif()

include(CPack)
