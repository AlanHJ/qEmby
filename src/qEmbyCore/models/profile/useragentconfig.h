#ifndef USERAGENTCONFIG_H
#define USERAGENTCONFIG_H

#include "../../qEmbyCore_global.h"
#include <QJsonObject>
#include <QString>

struct QEMBYCORE_EXPORT UserAgentConfig {
    bool enabled = false;
    QString value;

    bool isEffective() const { return enabled && !value.trimmed().isEmpty(); }

    QJsonObject toJson() const {
        return {{QStringLiteral("enabled"), enabled},
                {QStringLiteral("value"), value}};
    }

    static UserAgentConfig fromJson(const QJsonObject &json) {
        UserAgentConfig config;
        config.enabled = json.value(QStringLiteral("enabled")).toBool(false);
        config.value = json.value(QStringLiteral("value")).toString();
        return config;
    }

    bool operator==(const UserAgentConfig &other) const {
        return enabled == other.enabled && value == other.value;
    }
    bool operator!=(const UserAgentConfig &other) const {
        return !(*this == other);
    }
};

#endif 
