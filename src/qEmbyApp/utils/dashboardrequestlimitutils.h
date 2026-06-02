#ifndef DASHBOARDREQUESTLIMITUTILS_H
#define DASHBOARDREQUESTLIMITUTILS_H

#include <QString>

namespace DashboardRequestLimitUtils {

int configuredRequestLimit(const QString &serverId, const char *configKey,
                           int defaultValue);
int homeSectionRequestLimit(const QString &serverId, const char *configKey,
                            int defaultValue = 50, int maximumValue = 50);

} 

#endif 
