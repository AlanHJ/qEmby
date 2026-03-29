#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QSurfaceFormat>
#include <QThread>
#include <QTranslator>
#include "managers/languagemanager.h"
#include "managers/logmanager.h"

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
  a.setWindowIcon(QIcon(":/svg/qemby_logo.svg"));

  LanguageManager::instance()->init();
  LogManager::instance()->init();

  MainWindow w;
  w.show();
  int ret = a.exec();
  
  return ret;
}
