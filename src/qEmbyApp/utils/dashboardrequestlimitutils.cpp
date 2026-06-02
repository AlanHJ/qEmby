#include "dashboardrequestlimitutils.h"

#include <algorithm>
#include <config/config_keys.h>
#include <config/configstore.h>

namespace DashboardRequestLimitUtils {

int configuredRequestLimit(const QString &serverId, const char *configKey,
                           int defaultValue)
{
    if (serverId.trimmed().isEmpty() || !configKey) {
        return std::max(0, defaultValue);
    }

    const int value = ConfigStore::instance()->get<int>(
        ConfigKeys::forServer(serverId, configKey), defaultValue);
    return std::max(0, value);
}

int homeSectionRequestLimit(const QString &serverId, const char *configKey,
                            int defaultValue, int maximumValue)
{
    const int configured =
        configuredRequestLimit(serverId, configKey, defaultValue);
    const int cappedMaximum = std::max(1, maximumValue);
    if (configured <= 0) {
        return cappedMaximum;
    }
    return std::min(configured, cappedMaximum);
}

} 
