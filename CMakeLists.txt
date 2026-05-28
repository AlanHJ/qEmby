cmake_minimum_required(VERSION 3.19)

project(qEmby VERSION 0.0.4 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(QEmbyPlatformDeps)

set(QEMBY_RUNTIME_LIB_SUBDIR "qemby" CACHE STRING
    "Subdirectory under the install libdir for bundled private runtime libraries")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")


set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/MP)
endif()

# ==========================================
# 允许通过缓存变量或环境变量提供 Qt 安装路径
# Windows 未显式指定时仍会尝试兼容现有本地默认路径
# ==========================================
set(QEMBY_QT_ROOT "" CACHE PATH
    "Optional Qt installation root or CMake prefix path")
qemby_append_default_qt_prefixes()

# 寻找 Qt6 (不再允许降级)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core Network Concurrent OpenGLWidgets LinguistTools WebSockets)
if(UNIX AND NOT APPLE)
    find_package(Qt6 QUIET COMPONENTS DBus)
endif()

# ==========================================
# 【关键修复】: 为第三方子模块 (qwindowkit/qmsetup) 注入所需的 Qt 版本环境变量
# ==========================================
set(QT_VERSION_MAJOR ${Qt6_VERSION_MAJOR})
set(QT_VERSION_MINOR ${Qt6_VERSION_MINOR})
set(QT_VERSION ${Qt6_VERSION})

# ==========================================
# 引入 QCoro (C++20 Coroutines for Qt)
# ==========================================
include(FetchContent)

function(qemby_ensure_noop_install_script build_dir)
    if(NOT build_dir)
        return()
    endif()

    file(MAKE_DIRECTORY "${build_dir}")
    file(WRITE "${build_dir}/cmake_install.cmake"
        "# qEmby placeholder install script for third-party dependency\n")
endfunction()

set(_qemby_saved_skip_install_rules ${CMAKE_SKIP_INSTALL_RULES})
set(CMAKE_SKIP_INSTALL_RULES ON)
FetchContent_Declare(
    QCoro
    GIT_REPOSITORY https://github.com/danvratil/qcoro.git
    GIT_TAG v0.10.0
)

# 强制 QCoro 使用 Qt6 进行编译，并关闭不必要的测试与示例构建以加快编译速度
set(QCORO_QT_MAJOR_VERSION 6 CACHE STRING "Use Qt6 for QCoro" FORCE)
set(QCORO_BUILD_EXAMPLES OFF CACHE BOOL "Disable QCoro examples" FORCE)
set(QCORO_WITH_QML OFF CACHE BOOL "Disable QCoro QML module" FORCE)
set(QCORO_WITH_QTQUICK OFF CACHE BOOL "Disable QCoro QtQuick module" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable QCoro tests" FORCE)

FetchContent_MakeAvailable(QCoro)
FetchContent_GetProperties(QCoro)
qemby_ensure_noop_install_script("${qcoro_BINARY_DIR}")

# ==========================================
# 引入 spdlog (高性能线程安全日志库, MIT License)
# ==========================================
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Disable spdlog examples" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Disable spdlog tests" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "Disable spdlog install rules" FORCE)
FetchContent_MakeAvailable(spdlog)
FetchContent_GetProperties(spdlog)
qemby_ensure_noop_install_script("${spdlog_BINARY_DIR}")
set(CMAKE_SKIP_INSTALL_RULES ${_qemby_saved_skip_install_rules})
unset(_qemby_saved_skip_install_rules)
# ==========================================

set(QWINDOWKIT_INSTALL OFF CACHE BOOL
    "Disable QWindowKit development install rules inside qEmby" FORCE)

add_subdirectory(libs/qwindowkit)
add_subdirectory(libs/qwindowkit/examples/shared)
add_subdirectory(src/qEmbyCore)
add_subdirectory(src/qEmbyApp)

include(QEmbyPackaging)
