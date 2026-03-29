cmake_minimum_required(VERSION 3.16)

project(qEmby VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_PREFIX_PATH "/path/to/Qt6/lib/cmake")  # Set your Qt6 installation path here, or pass it via cmake -DCMAKE_PREFIX_PATH=...
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core Network Concurrent OpenGLWidgets LinguistTools WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core Network Concurrent OpenGLWidgets LinguistTools WebSockets)

# ==========================================
# 引入 QCoro (C++20 Coroutines for Qt)
# ==========================================
include(FetchContent)
FetchContent_Declare(
    QCoro
    GIT_REPOSITORY https://github.com/danvratil/qcoro.git
    GIT_TAG v0.10.0
)
# 强制 QCoro 使用 Qt6 进行编译，并关闭不必要的测试与示例构建以加快编译速度
set(QCORO_QT_MAJOR_VERSION 6 CACHE STRING "Use Qt6 for QCoro" FORCE)
set(QCORO_BUILD_EXAMPLES OFF CACHE BOOL "Disable QCoro examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable QCoro tests" FORCE)

FetchContent_MakeAvailable(QCoro)

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
FetchContent_MakeAvailable(spdlog)
# ==========================================

add_subdirectory(libs/qwindowkit)
add_subdirectory(libs/qwindowkit/examples/shared)
add_subdirectory(src/qEmbyCore)
add_subdirectory(src/qEmbyApp)
