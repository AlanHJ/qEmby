# src/qEmbyApp/CMakeLists.txt

set(TS_FILES
    resources/i18n/qEmby_zh_CN.ts
)
# set(QSS_QWK_FILES ../../libs/qwindowkit/examples/mainwindow/mainwindow.qrc)
# set(QRC_QWK_FILES ../../libs/qwindowkit/examples/shared/resources/shared.qrc)
set(APP_QRC_FILES resources/resources.qrc)

file(GLOB_RECURSE APP_HEADERS "*.h")
file(GLOB_RECURSE APP_SOURCES "*.cpp")
 
# ==========================================
# 平台原生资源配置 (如 Windows .exe 图标)
# ==========================================
if(WIN32)
    set(APP_NATIVE_RESOURCES qEmbyApp.rc)
endif()

qt_add_executable(qEmbyApp
    MANUAL_FINALIZATION
    ${APP_HEADERS} 
    ${APP_SOURCES}
    ${TS_FILES}
    ${APP_QRC_FILES}
    ${APP_NATIVE_RESOURCES} # <-- 将 .rc 资源文件加入编译列表
)

# ==========================================
# 集成 libmpv 播放器核心
# Windows 使用仓库内置 libmpv
# Linux / macOS 优先使用系统安装的 libmpv
# ==========================================
qemby_link_libmpv(qEmbyApp)

# ==========================================
# 集成 MDK 播放器核心
# ==========================================

# 1. 动态定位根目录下的 libs/mdk-sdk 文件夹
# 如果 CMAKE_SOURCE_DIR 指向了 src/qEmbyApp，这里的路径可能会错，
# 但既然你之前 libmpv 能用，说明 CMAKE_SOURCE_DIR 是 qEmby 根目录。
# set(MDK_DIR "${CMAKE_SOURCE_DIR}/libs/mdk-sdk")
# set(MDK_DLL_FILE "${MDK_DIR}/bin/x64/mdk.dll")

# 2. 引入头文件目录
# target_include_directories(qEmbyApp PRIVATE "${MDK_DIR}/include")

# 3. 终极解法：直接指定 mdk.lib 的绝对物理路径
# set(MDK_LIB_FILE "${MDK_DIR}/lib/x64/mdk.lib")

# 如果你想加个保险，可以取消下面三行的注释来让 CMake 在生成期帮你检查路径是否正确
# if(NOT EXISTS "${MDK_LIB_FILE}")
#     message(FATAL_ERROR "绝对路径错误！找不到文件: ${MDK_LIB_FILE}")
# endif()

# add_custom_command(TARGET qEmbyApp POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     "${MDK_DLL_FILE}"
#     $<TARGET_FILE_DIR:qEmbyApp>
#     COMMENT "正在自动拷贝 mdk.dll 到输出目录..."
# )

target_compile_definitions(qEmbyApp PRIVATE
    APP_NAME="${PROJECT_NAME}"
    APP_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(qEmbyApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::OpenGLWidgets
    qEmbyCore
    QWKWidgets
    WidgetFrame
    QCoro6::Core
    QCoro6::Network
    spdlog::spdlog
)
target_link_libraries(qEmbyApp PRIVATE Qt${QT_VERSION_MAJOR}::Core)

if(APPLE)
    target_link_libraries(qEmbyApp PRIVATE
        "-framework CoreFoundation"
        "-framework IOKit"
    )
endif()

if(TARGET Qt${QT_VERSION_MAJOR}::DBus)
    target_link_libraries(qEmbyApp PRIVATE Qt${QT_VERSION_MAJOR}::DBus)
    target_compile_definitions(qEmbyApp PRIVATE QEMBY_HAS_QT_DBUS=1)
endif()

# # 3. 处理翻译
# if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
#     # 注意路径从 ${CMAKE_SOURCE_DIR} 改为 ${CMAKE_CURRENT_SOURCE_DIR}
#     qt_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
# else()
#     qt5_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
# endif()


if(QT_VERSION_MAJOR EQUAL 6)
    # 自动编译 TS 为 QM，并将生成的 QM 虚拟嵌入到 :/i18n/ 资源路径下
    qt_add_translations(qEmbyApp
        TS_FILES ${TS_FILES}
        RESOURCE_PREFIX "/i18n"
        LUPDATE_OPTIONS -no-obsolete
    )
else()
    # 兼容 Qt 5.x 保留的后备方案
    qt5_create_translation(QM_FILES ${CMAKE_CURRENT_SOURCE_DIR} ${TS_FILES})
    target_sources(qEmbyApp PRIVATE ${QM_FILES})
endif()

# 4. 设置平台属性 (Windows 和 macOS)
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.hl.qEmby)
endif()

# 注意：目标从 qEmby 改为 qEmbyApp
set_target_properties(qEmbyApp PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    # MACOSX_BUNDLE_ICON_FILE app.icns # 如果未来需要适配 macOS 图标，取消此注释
)

# 5. 设置安装规则
include(GNUInstallDirs)
# 注意：目标从 qEmby 改为 qEmbyApp
install(TARGETS qEmbyApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 6. 最终确定 (必须在 install() 之后)
if(QT_VERSION_MAJOR EQUAL 6)
    # 注意：目标从 qEmby 改为 qEmbyApp
    qt_finalize_executable(qEmbyApp)
endif()

# ==========================================
# 终极防弹方案：调用 windeployqt 自动补全所有 Qt 依赖
# ==========================================
if(WIN32)
    # 找到 Qt 官方的部署工具
    set(_windeployqt_hints)
    if(QEMBY_QT_ROOT)
        list(APPEND _windeployqt_hints "${QEMBY_QT_ROOT}/bin")
    endif()
    if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
        list(APPEND _windeployqt_hints "$ENV{QTDIR}/bin")
    endif()
    list(APPEND _windeployqt_hints "E:/Qt6/6.9.2/msvc2022_64/bin")
    list(REMOVE_DUPLICATES _windeployqt_hints)

    find_program(WINDEPLOYQT_EXECUTABLE windeployqt PATHS ${_windeployqt_hints})
    
    if(WINDEPLOYQT_EXECUTABLE)
        # 在 qEmbyApp 编译完成后的那一刻，自动执行该工具
        add_custom_command(TARGET qEmbyApp POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE} $<TARGET_FILE:qEmbyApp>
            COMMENT "正在召唤 windeployqt 自动补全所有的 Qt DLL 和底层插件..."
        )
    else()
        message(WARNING "未找到 windeployqt，自动部署失败！")
    endif()
endif()
