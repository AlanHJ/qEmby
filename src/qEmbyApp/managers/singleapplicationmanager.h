#ifndef SINGLEAPPLICATIONMANAGER_H
#define SINGLEAPPLICATIONMANAGER_H

#include <QObject>
#include <memory>

class QLocalServer;
class QLockFile;

class SingleApplicationManager : public QObject {
  Q_OBJECT

public:
  enum class StartResult { Disabled, PrimaryInstance, SecondaryInstance };

  explicit SingleApplicationManager(QObject *parent = nullptr);
  ~SingleApplicationManager() override;
  StartResult start(bool enabled);

signals:
  void activationRequested();

private:
  bool notifyRunningInstance() const;
  QString serverName() const;

  QLocalServer *m_server = nullptr;
  std::unique_ptr<QLockFile> m_lockFile;
};

#endif 
