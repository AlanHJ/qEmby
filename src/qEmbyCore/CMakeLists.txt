# src/qEmbyCore/CMakeLists.txt

file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(qEmbyCore SHARED
    ${CORE_HEADERS}
    ${CORE_SOURCES}
)

target_include_directories(qEmbyCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(qEmbyCore PUBLIC Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::WebSockets
    QCoro6::Core
    QCoro6::Network
)

target_compile_definitions(qEmbyCore PRIVATE QEMBYCORE_LIBRARY)

include(GNUInstallDirs)

set(_qemby_core_library_install_dir "${CMAKE_INSTALL_LIBDIR}")
if(UNIX AND NOT APPLE AND QEMBY_RUNTIME_LIB_SUBDIR)
    set(_qemby_core_library_install_dir
        "${CMAKE_INSTALL_LIBDIR}/${QEMBY_RUNTIME_LIB_SUBDIR}")
endif()

install(TARGETS qEmbyCore
    LIBRARY DESTINATION ${_qemby_core_library_install_dir}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${_qemby_core_library_install_dir}
)
