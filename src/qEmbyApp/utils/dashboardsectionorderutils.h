#ifndef DASHBOARDSECTIONORDERUTILS_H
#define DASHBOARDSECTIONORDERUTILS_H

#include <QStringList>

namespace DashboardSectionOrderUtils {

inline constexpr char ContinueWatchingSectionId[] = "continue_watching";
inline constexpr char LatestMediaSectionId[] = "latest_media";
inline constexpr char RecommendedSectionId[] = "recommended_media";
inline constexpr char CompletedWatchingSectionId[] = "completed_watching";
inline constexpr char LiveTvSectionId[] = "live_tv";
inline constexpr char MusicSectionId[] = "music";
inline constexpr char AllLibrariesSectionId[] = "all_libraries";
inline constexpr char EachLibrarySectionsSectionId[] = "each_library_sections";

QStringList defaultSectionOrder();
QStringList normalizeSectionOrder(QStringList order);

} 

#endif 
