#ifndef MEDIAIDENTIFYUTILS_H
#define MEDIAIDENTIFYUTILS_H

#include <QJsonObject>
#include <QString>

struct MediaItem;

namespace MediaIdentifyUtils {

QString identifySearchType(const MediaItem& item);
bool canIdentify(const MediaItem& item);
QString defaultSearchText(const MediaItem& item);
QJsonObject buildProviderIds(QString imdbId, QString movieDbId,
                             QString tvdbId);
QJsonObject buildSearchInfo(const MediaItem& item, QString queryText,
                            int productionYear = 0,
                            QJsonObject providerIds = {});

} 

#endif 
