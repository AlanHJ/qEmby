#ifndef DANDANPLAYPROVIDER_H
#define DANDANPLAYPROVIDER_H

#include "../../api/networkmanager.h"
#include "../../models/danmaku/danmakumodels.h"

class DandanplayProvider
{
public:
    explicit DandanplayProvider(NetworkManager *networkManager);

    QCoro::Task<QList<DanmakuMatchCandidate>> searchCandidates(
        DanmakuMediaContext context,
        DanmakuProviderConfig config,
        QString manualKeyword = QString()) const;

    QCoro::Task<QList<DanmakuComment>> fetchComments(
        DanmakuMatchCandidate candidate,
        DanmakuProviderConfig config) const;

private:
    NetworkManager *m_networkManager;
};

#endif 
