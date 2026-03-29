#ifndef TASKUTILS_H
#define TASKUTILS_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace TaskUtils {

QString formatIsoDateTime(QString isoDate);
QString formatDurationTicks(qint64 ticks);

QString localizedTaskState(const QString &state);
QString localizedTaskResult(const QString &result);
QString localizedTriggerType(const QString &type);

QString formatTriggerDescription(const QJsonObject &trigger);
QString formatTriggerSummary(const QJsonArray &triggers, int maxVisible = 2);

bool isEditableTriggerType(const QString &type);

} 

#endif 
