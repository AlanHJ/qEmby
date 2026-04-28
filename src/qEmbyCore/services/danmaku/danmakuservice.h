#ifndef DANMAKUSERVICE_H
#define DANMAKUSERVICE_H

#include "../../models/danmaku/danmakumodels.h"

#include <QObject>
#include <qcorotask.h>

class NetworkManager;
class ServerManager;
class DandanplayProvider;
class DanmakuCacheStore;

class QEMBYCORE_EXPORT DanmakuService : public QObject
{
    Q_OBJECT
public:
    explicit DanmakuService(NetworkManager *networkManager,
                            ServerManager *serverManager,
                            QObject *parent = nullptr);
    ~DanmakuService() override;

    static DanmakuMatchCandidate createLocalFileCandidate(
        QString filePath);
    static QString localDanmakuDirectoryPath(
        QString serverId = QString());
    static bool ensureLocalDanmakuDirectory(
        QString serverId = QString());

    DanmakuRenderOptions renderOptions() const;
    DanmakuProviderConfig providerConfig(
        QString serverId = QString()) const;

    QCoro::Task<DanmakuLoadResult> prepareDanmaku(
        DanmakuMediaContext context,
        QString manualKeyword = QString());
    QCoro::Task<DanmakuLoadResult> prepareDanmakuForCandidate(
        DanmakuMediaContext context,
        DanmakuMatchCandidate candidate);
    QCoro::Task<QList<DanmakuMatchCandidate>> searchCandidates(
        DanmakuMediaContext context,
        QString manualKeyword = QString());
    QCoro::Task<QList<DanmakuMatchCandidate>> searchAllCandidates(
        DanmakuMediaContext context,
        QString manualKeyword = QString());
    QCoro::Task<DanmakuMatchResult> resolveMatch(
        DanmakuMediaContext context,
        QString manualKeyword = QString());

    void saveManualMatch(const DanmakuMediaContext &context,
                         const DanmakuMatchCandidate &candidate);
    void clearCache();

private:
    bool autoMatchEnabled(const QString &serverId) const;
    QList<DanmakuProviderConfig> enabledProviderConfigs(
        QString serverId = QString()) const;
    DanmakuProviderConfig providerConfigForCandidate(
        QString serverId,
        const DanmakuMatchCandidate &candidate) const;
    QCoro::Task<QList<DanmakuMatchCandidate>> searchCandidatesForConfig(
        DanmakuMediaContext context,
        DanmakuProviderConfig config,
        QString manualKeyword = QString());
    QString assCacheKey(const DanmakuMatchCandidate &candidate,
                        const DanmakuRenderOptions &options) const;

    NetworkManager *m_networkManager;
    ServerManager *m_serverManager;
    DandanplayProvider *m_dandanplayProvider;
    DanmakuCacheStore *m_cacheStore;
};

#endif 
