#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QString>


class LogManager : public QObject {
  Q_OBJECT

public:
  static LogManager *instance();

  
  void init();

  
  void enable();

  
  void disable();

  
  bool isEnabled() const { return m_enabled; }

  
  QString logFilePath() const;

  
  QString logFileSize() const;

  
  void clearLog();

private:
  explicit LogManager(QObject *parent = nullptr);
  void setupSpdlog();
  void teardownSpdlog();

  bool m_enabled = false;
  bool m_initialized = false;
};

#endif 
