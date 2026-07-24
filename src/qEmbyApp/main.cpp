#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
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
                   &SingleApplicationManager::activationRequested, &w, [&w]() {
                     if (w.isMinimized()) {
                       w.setWindowState(w.windowState() & ~Qt::WindowMinimized);
                     }
                     w.show();
                     w.raise();
                     w.activateWindow();
                   });
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
  w.setWindowIcon(appIcon);
#endif
  w.show();

  
  
  
  
  
  
  
  
  
  
  
  
  
  MpvController::warmupOnce();

  int ret = a.exec();
  
  return ret;
}
