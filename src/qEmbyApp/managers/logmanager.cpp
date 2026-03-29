#include "logmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/null_sink.h>

static const char *kLoggerName = "qemby";
static const int kMaxFileSize = 5 * 1024 * 1024; 
static const int kMaxFiles = 3;


static void spdlogMessageHandler(QtMsgType type, const QMessageLogContext &ctx,
                                  const QString &msg) {
  auto logger = spdlog::get(kLoggerName);
  if (!logger)
    return;

  std::string m = msg.toStdString();
  std::string category = ctx.category ? ctx.category : "";
  std::string formatted =
      category.empty() ? m : fmt::format("[{}] {}", category, m);

  switch (type) {
  case QtDebugMsg:
    logger->debug("{}", formatted);
    break;
  case QtInfoMsg:
    logger->info("{}", formatted);
    break;
  case QtWarningMsg:
    logger->warn("{}", formatted);
    break;
  case QtCriticalMsg:
    logger->error("{}", formatted);
    break;
  case QtFatalMsg:
    logger->critical("{}", formatted);
    logger->flush();
    break;
  }
}

LogManager::LogManager(QObject *parent) : QObject(parent) {}

LogManager *LogManager::instance() {
  static LogManager inst;
  return &inst;
}

QString LogManager::logFilePath() const {
  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return configDir + "/qemby.log";
}

void LogManager::init() {
  if (m_initialized)
    return;
  m_initialized = true;

  bool enabled =
      ConfigStore::instance()->get<bool>(ConfigKeys::LogEnable, false);
  if (enabled) {
    enable();
  }
}

void LogManager::enable() {
  if (m_enabled)
    return;

  setupSpdlog();
  m_enabled = true;
  ConfigStore::instance()->set(ConfigKeys::LogEnable, true);
}

void LogManager::disable() {
  if (!m_enabled)
    return;

  teardownSpdlog();
  m_enabled = false;
  ConfigStore::instance()->set(ConfigKeys::LogEnable, false);
}

void LogManager::setupSpdlog() {
  
  QString logPath = logFilePath();
  QDir().mkpath(QFileInfo(logPath).absolutePath());

  try {
    
    auto fileSink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            logPath.toStdString(), kMaxFileSize, kMaxFiles);

    auto logger =
        std::make_shared<spdlog::logger>(kLoggerName, fileSink);
    logger->set_level(spdlog::level::debug);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    logger->flush_on(spdlog::level::warn); 

    spdlog::register_logger(logger);

    
    qInstallMessageHandler(spdlogMessageHandler);

    logger->info("=== qEmby Logging Started ===");
  } catch (const spdlog::spdlog_ex &ex) {
    
    Q_UNUSED(ex);
  }
}

void LogManager::teardownSpdlog() {
  auto logger = spdlog::get(kLoggerName);
  if (logger) {
    logger->info("=== qEmby Logging Stopped ===");
    logger->flush();
  }

  
  qInstallMessageHandler(nullptr);

  spdlog::drop(kLoggerName);
}

QString LogManager::logFileSize() const {
  QFileInfo info(logFilePath());
  if (!info.exists())
    return QStringLiteral("0 B");

  qint64 bytes = info.size();
  if (bytes < 1024)
    return QStringLiteral("%1 B").arg(bytes);
  if (bytes < 1024 * 1024)
    return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
  return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
}

void LogManager::clearLog() {
  
  bool wasEnabled = m_enabled;
  if (wasEnabled) {
    teardownSpdlog();
  }

  
  
  QString logPath = logFilePath();
  QFileInfo fi(logPath);
  QString baseName = fi.completeBaseName(); 
  QString suffix = fi.suffix();             
  QString dir = fi.absolutePath();

  QFile::remove(logPath);
  for (int i = 1; i <= kMaxFiles; ++i) {
    QString rotated = dir + "/" + baseName + "." + QString::number(i) + "." + suffix;
    QFile::remove(rotated);
  }

  
  if (wasEnabled) {
    m_enabled = false; 
    enable();
  }
}
