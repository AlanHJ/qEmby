#include "singleapplicationmanager.h"

#include "config/configstore.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>

namespace {
constexpr int kConnectTimeoutMs = 300;
}

SingleApplicationManager::SingleApplicationManager(QObject *parent)
    : QObject(parent) {}

SingleApplicationManager::~SingleApplicationManager() = default;

SingleApplicationManager::StartResult
SingleApplicationManager::start(bool enabled) {
  if (!enabled) {
    qInfo() << "SingleApplication: disabled by configuration";
    return StartResult::Disabled;
  }

  const QString name = serverName();
  m_lockFile = std::make_unique<QLockFile>(ConfigStore::instance()->filePath() +
                                           QStringLiteral(".instance.lock"));
  m_lockFile->setStaleLockTime(0);
  if (!m_lockFile->tryLock()) {
    const bool notified = notifyRunningInstance();
    qInfo() << "SingleApplication: another instance owns the lock"
            << "| activation notified:" << notified;
    return StartResult::SecondaryInstance;
  }

  
  QLocalServer::removeServer(name);
  m_server = new QLocalServer(this);
  if (!m_server->listen(name)) {
    qWarning() << "SingleApplication: failed to listen" << name
               << "| error:" << m_server->errorString();
    m_lockFile->unlock();
    m_lockFile.reset();
    return StartResult::Disabled;
  }

  connect(m_server, &QLocalServer::newConnection, this, [this]() {
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
      socket->readAll();
      socket->disconnectFromServer();
      socket->deleteLater();
    }
    qInfo() << "SingleApplication: activation requested by another launch";
    emit activationRequested();
  });

  qInfo() << "SingleApplication: primary instance is listening";
  return StartResult::PrimaryInstance;
}

bool SingleApplicationManager::notifyRunningInstance() const {
  QLocalSocket socket;
  socket.connectToServer(serverName(), QIODevice::WriteOnly);
  if (!socket.waitForConnected(kConnectTimeoutMs)) {
    return false;
  }

  socket.write("activate");
  socket.flush();
  socket.waitForBytesWritten(kConnectTimeoutMs);
  socket.disconnectFromServer();
  return true;
}

QString SingleApplicationManager::serverName() const {
  const QByteArray identity =
      QCoreApplication::organizationName().toUtf8() + '/' +
      QCoreApplication::applicationName().toUtf8() + '/' +
      ConfigStore::instance()->filePath().toUtf8();
  const QByteArray suffix =
      QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(16);
  return QStringLiteral("qemby-%1").arg(QString::fromLatin1(suffix));
}
