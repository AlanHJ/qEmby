#include "dashboardsectionorderutils.h"

namespace {

bool isKnownSectionId(const QString& sectionId)
{
    return sectionId == QLatin1String(DashboardSectionOrderUtils::ContinueWatchingSectionId) ||
           sectionId == QLatin1String(DashboardSectionOrderUtils::LatestMediaSectionId) ||
           sectionId == QLatin1String(DashboardSectionOrderUtils::RecommendedSectionId) ||
           sectionId == QLatin1String(DashboardSectionOrderUtils::AllLibrariesSectionId) ||
           sectionId == QLatin1String(
               DashboardSectionOrderUtils::EachLibrarySectionsSectionId);
}

} 

namespace DashboardSectionOrderUtils {

QStringList defaultSectionOrder()
{
    return {
        QString::fromLatin1(ContinueWatchingSectionId),
        QString::fromLatin1(LatestMediaSectionId),
        QString::fromLatin1(RecommendedSectionId),
        QString::fromLatin1(AllLibrariesSectionId),
        QString::fromLatin1(EachLibrarySectionsSectionId)
    };
}

QStringList normalizeSectionOrder(QStringList order)
{
    QStringList normalized;
    normalized.reserve(defaultSectionOrder().size());

    for (QString& sectionId : order) {
        sectionId = sectionId.trimmed();
        if (sectionId.isEmpty() || !isKnownSectionId(sectionId) ||
            normalized.contains(sectionId)) {
            continue;
        }
        normalized.append(sectionId);
    }

    const QStringList defaults = defaultSectionOrder();
    for (const QString& sectionId : defaults) {
        if (!normalized.contains(sectionId)) {
            normalized.append(sectionId);
        }
    }

    return normalized;
}

} 
