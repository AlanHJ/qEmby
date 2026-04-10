#include "mediaidentifyutils.h"

#include <models/media/mediaitem.h>

namespace {

bool equalsInsensitive(const QString& left, const QString& right)
{
    return left.compare(right, Qt::CaseInsensitive) == 0;
}

void insertProviderId(QJsonObject& providerIds, const QString& key,
                      QString value)
{
    value = value.trimmed();
    if (!key.trimmed().isEmpty() && !value.isEmpty()) {
        providerIds.insert(key, value);
    }
}

} 

namespace MediaIdentifyUtils {

QString identifySearchType(const MediaItem& item)
{
    const QString itemType = item.type.trimmed();

    if (equalsInsensitive(itemType, QStringLiteral("Movie"))) {
        return QStringLiteral("Movie");
    }
    if (equalsInsensitive(itemType, QStringLiteral("Series"))) {
        return QStringLiteral("Series");
    }
    if (equalsInsensitive(itemType, QStringLiteral("BoxSet"))) {
        return QStringLiteral("BoxSet");
    }
    if (equalsInsensitive(itemType, QStringLiteral("MusicAlbum"))) {
        return QStringLiteral("MusicAlbum");
    }
    if (equalsInsensitive(itemType, QStringLiteral("MusicArtist"))) {
        return QStringLiteral("MusicArtist");
    }
    if (equalsInsensitive(itemType, QStringLiteral("MusicVideo"))) {
        return QStringLiteral("MusicVideo");
    }
    if (equalsInsensitive(itemType, QStringLiteral("Book"))) {
        return QStringLiteral("Book");
    }
    if (equalsInsensitive(itemType, QStringLiteral("Person"))) {
        return QStringLiteral("Person");
    }
    if (equalsInsensitive(itemType, QStringLiteral("Trailer"))) {
        return QStringLiteral("Trailer");
    }

    return {};
}

bool canIdentify(const MediaItem& item)
{
    return !item.id.trimmed().isEmpty() &&
           !identifySearchType(item).isEmpty();
}

QString defaultSearchText(const MediaItem& item)
{
    const QString originalTitle = item.originalTitle.trimmed();
    if (!originalTitle.isEmpty()) {
        return originalTitle;
    }

    return item.name.trimmed();
}

QJsonObject buildProviderIds(QString imdbId, QString movieDbId, QString tvdbId)
{
    QJsonObject providerIds;

    insertProviderId(providerIds, QStringLiteral("Imdb"), imdbId);

    movieDbId = movieDbId.trimmed();
    if (!movieDbId.isEmpty()) {
        
        
        insertProviderId(providerIds, QStringLiteral("Tmdb"), movieDbId);
        insertProviderId(providerIds, QStringLiteral("TheMovieDb"), movieDbId);
    }

    tvdbId = tvdbId.trimmed();
    if (!tvdbId.isEmpty()) {
        insertProviderId(providerIds, QStringLiteral("Tvdb"), tvdbId);
        insertProviderId(providerIds, QStringLiteral("TheTVDB"), tvdbId);
    }

    return providerIds;
}

QJsonObject buildSearchInfo(const MediaItem& item, QString queryText,
                            int productionYear, QJsonObject providerIds)
{
    QJsonObject searchInfo;

    queryText = queryText.trimmed();
    if (!queryText.isEmpty()) {
        searchInfo.insert(QStringLiteral("Name"), queryText);
    }

    const QString originalTitle = item.originalTitle.trimmed();
    if (!originalTitle.isEmpty()) {
        searchInfo.insert(QStringLiteral("OriginalTitle"), originalTitle);
    }

    if (!item.path.trimmed().isEmpty()) {
        searchInfo.insert(QStringLiteral("Path"), item.path.trimmed());
    }

    if (!item.premiereDate.trimmed().isEmpty()) {
        searchInfo.insert(QStringLiteral("PremiereDate"),
                          item.premiereDate.trimmed());
    }

    if (productionYear > 0) {
        searchInfo.insert(QStringLiteral("Year"), productionYear);
    }

    if (item.parentIndexNumber >= 0) {
        searchInfo.insert(QStringLiteral("ParentIndexNumber"),
                          item.parentIndexNumber);
    }

    if (item.indexNumber >= 0) {
        searchInfo.insert(QStringLiteral("IndexNumber"), item.indexNumber);
    }

    searchInfo.insert(QStringLiteral("ProviderIds"), providerIds);
    return searchInfo;
}

} 
