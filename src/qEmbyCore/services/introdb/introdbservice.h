#ifndef INTRODBSERVICE_H
#define INTRODBSERVICE_H

#include "qEmbyCore_global.h"
#include "api/networkmanager.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <qcorotask.h>

class QEMBYCORE_EXPORT IntroDBService : public QObject
{
    Q_OBJECT
public:
    struct SegmentInfo {
        double startSec = -1;
        double endSec = -1;
    };

    struct EpisodeSegments {
        SegmentInfo intro;
        SegmentInfo outro;
        bool fetched = false;
        bool notFound = false;
    };

    explicit IntroDBService(NetworkManager *nm, QObject *parent = nullptr);

    QCoro::Task<EpisodeSegments> fetchSegments(QString imdbId,
                                                int season, int episode);
    void clearCache();

private:
    QPointer<NetworkManager> m_nm;
    QHash<QString, EpisodeSegments> m_cache;

    static QString cacheKey(const QString &imdbId, int season, int episode);
};

#endif 
