#ifndef DANMAKUSETTINGS_H
#define DANMAKUSETTINGS_H

#include "../../models/danmaku/danmakumodels.h"

#include <QList>
#include <QString>

class QEMBYCORE_EXPORT DanmakuSettings final
{
public:
    static QList<DanmakuServerDefinition> loadServers(QString serverId);
    static DanmakuServerDefinition selectedServer(QString serverId);
    static QString selectedServerId(QString serverId);
    static void saveServers(QString serverId,
                            QList<DanmakuServerDefinition> servers,
                            QString selectedServerId);
};

#endif 
