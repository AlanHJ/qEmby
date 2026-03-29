# src/qEmbyCore/CMakeLists.txt

file(GLOB_RECURSE CORE_HEADERS "*.h")
file(GLOB_RECURSE CORE_SOURCES "*.cpp")

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
