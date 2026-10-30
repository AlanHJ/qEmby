#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QSurfaceFormat>
#include <QThread>
#include <QTranslator>
#include "api/proxymanager.h"
#include "components/mpvcontroller.h"
#include "managers/languagemanager.h"
#include "managers/logmanager.h"
#include "managers/singleapplicationmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "utils/gpudevicemanager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif
int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
  bool isRDP = GetSystemMetrics(SM_REMOTESESSION) != 0;
  if (isRDP) {
    
    
    qputenv("QT_OPENGL", "software");
  }
#endif

  
  QSurfaceFormat format;
  format.setVersion(3, 3); 
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);  
  format.setStencilBufferSize(8); 
  format.setSwapInterval(1);      
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer); 
  QSurfaceFormat::setDefaultFormat(format);

  // GPU 设备选择必须在 Qt 图形栈初始化（首个 GL 上下文）之前应用：Linux 的
  // DRI_PRIME 由 Mesa 在 GL 栈初始化时读取，Windows 的 GPU 性能偏好注册表同样
  // 需在进程图形栈初始化前写入，晚了均不生效。此处位于 QApplication 构造前，
  // ConfigStore 依赖 QApplication，故用 QSettings 直接读同一 config.ini
  // （组织名/应用名与下方保持一致，键 player/gpu_device）。
  QCoreApplication::setApplicationName(APP_NAME);
  QCoreApplication::setOrganizationName("AlanHJ");
  QCoreApplication::setOrganizationDomain("github.com/AlanHJ/qEmby");
  {
    QSettings gpuSettings(
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
            QStringLiteral("/config.ini"),
        QSettings::IniFormat);
    GpuDeviceManager::applyDevice(
        gpuSettings.value(ConfigKeys::PlayerGpuDevice, QStringLiteral("auto"))
            .toString());
  }

  QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
  QApplication a(argc, argv);
  a.setApplicationName(APP_NAME);
  a.setApplicationVersion(APP_VERSION);
  a.setOrganizationName("AlanHJ");
  a.setOrganizationDomain("github.com/AlanHJ/qEmby");

  LogManager::instance()->init();

  SingleApplicationManager singleApplication;
  const auto singleApplicationResult = singleApplication.start(
      ConfigStore::instance()->get<bool>(ConfigKeys::SingleApplication, false));
  if (singleApplicationResult ==
      SingleApplicationManager::StartResult::SecondaryInstance) {
    return 0;
  }
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
  QGuiApplication::setDesktopFileName(QStringLiteral("qemby"));
  const QIcon appIcon(QStringLiteral(":/svg/qemby_logo.svg"));
  a.setWindowIcon(appIcon);
#endif

  LanguageManager::instance()->init();

  
  
  ProxyManager::installApplicationFactory();

  MainWindow w;
  QObject::connect(&singleApplication,
                   &SingleApplicationManager::activationRequested,
                   &w, &MainWindow::showCurrentWindow);
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
  w.setWindowIcon(appIcon);
#endif
  w.showCurrentWindow();

  
  
  
  
  
  
  
  
  
  
  
  
  
  MpvController::warmupOnce();

  int ret = a.exec();
  
  return ret;
}
