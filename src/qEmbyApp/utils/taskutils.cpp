#include "taskutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QStringList>

namespace {

constexpr qint64 TicksPerMinute = 600000000LL;
constexpr qint64 TicksPerHour = 36000000000LL;
constexpr qint64 TicksPerDay = 864000000000LL;

QString translate(const char *sourceText, int count = -1) {
    return QCoreApplication::translate("TaskUtils", sourceText, nullptr, count);
}

QString localizedDayOfWeek(const QString &day) {
    if (day == "Sunday") {
        return translate("Sunday");
    }
    if (day == "Monday") {
        return translate("Monday");
    }
    if (day == "Tuesday") {
        return translate("Tuesday");
    }
    if (day == "Wednesday") {
        return translate("Wednesday");
    }
    if (day == "Thursday") {
        return translate("Thursday");
    }
    if (day == "Friday") {
        return translate("Friday");
    }
    if (day == "Saturday") {
        return translate("Saturday");
    }
    return day;
}

QString localizedSystemEvent(const QString &eventName) {
    if (eventName == "DisplayConfigurationChange") {
        return translate("display configuration changes");
    }
    if (eventName == "WakeFromSleep") {
        return translate("the server wakes from sleep");
    }
    if (eventName == "PluginUpdated") {
        return translate("plugins are updated");
    }
    return eventName;
}

QString ticksToClockTime(qint64 ticks) {
    const int totalMinutes = static_cast<int>(ticks / TicksPerMinute);
    const int hours = totalMinutes / 60;
    const int minutes = totalMinutes % 60;
    return QStringLiteral("%1:%2")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'));
}

} 

namespace TaskUtils {

QString formatIsoDateTime(QString isoDate) {
    if (isoDate.isEmpty()) {
        return {};
    }

    QDateTime dt = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(isoDate, Qt::ISODateWithMs);
    }
    if (!dt.isValid()) {
        return isoDate;
    }

    dt.setTimeSpec(Qt::UTC);
    return dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString formatDurationTicks(qint64 ticks) {
    if (ticks <= 0) {
        return translate("No limit");
    }

    if (ticks % TicksPerDay == 0) {
        const int days = static_cast<int>(ticks / TicksPerDay);
        return translate("%n day(s)", days);
    }

    if (ticks % TicksPerHour == 0) {
        const int hours = static_cast<int>(ticks / TicksPerHour);
        return translate("%n hour(s)", hours);
    }

    const int minutes = static_cast<int>(ticks / TicksPerMinute);
    if (minutes > 0) {
        return translate("%n minute(s)", minutes);
    }

    return translate("Less than a minute");
}

QString localizedTaskState(const QString &state) {
    if (state == "Idle") {
        return translate("Idle");
    }
    if (state == "Running") {
        return translate("Running");
    }
    if (state == "Cancelling") {
        return translate("Cancelling");
    }
    return state;
}

QString localizedTaskResult(const QString &result) {
    if (result == "Completed") {
        return translate("Completed");
    }
    if (result == "Failed") {
        return translate("Failed");
    }
    if (result == "Aborted") {
        return translate("Aborted");
    }
    if (result == "Cancelled") {
        return translate("Cancelled");
    }
    return result;
}

QString localizedTriggerType(const QString &type) {
    if (type == "DailyTrigger") {
        return translate("Daily");
    }
    if (type == "IntervalTrigger") {
        return translate("Interval");
    }
    if (type == "WeeklyTrigger") {
        return translate("Weekly");
    }
    if (type == "StartupTrigger") {
        return translate("On Startup");
    }
    if (type == "SystemEventTrigger") {
        return translate("System Event");
    }
    return type;
}

QString formatTriggerDescription(const QJsonObject &trigger) {
    const QString type = trigger.value("Type").toString();

    if (type == "DailyTrigger") {
        return translate("Daily at %1").arg(
            ticksToClockTime(trigger.value("TimeOfDayTicks").toVariant().toLongLong()));
    }

    if (type == "IntervalTrigger") {
        return translate("Every %1").arg(
            formatDurationTicks(trigger.value("IntervalTicks").toVariant().toLongLong()));
    }

    if (type == "WeeklyTrigger") {
        return translate("%1 at %2")
            .arg(localizedDayOfWeek(trigger.value("DayOfWeek").toString()),
                 ticksToClockTime(trigger.value("TimeOfDayTicks")
                                      .toVariant()
                                      .toLongLong()));
    }

    if (type == "StartupTrigger") {
        return translate("On server startup");
    }

    if (type == "SystemEventTrigger") {
        return translate("When %1")
            .arg(localizedSystemEvent(trigger.value("SystemEvent").toString()));
    }

    return localizedTriggerType(type);
}

QString formatTriggerSummary(const QJsonArray &triggers, int maxVisible) {
    if (triggers.isEmpty()) {
        return translate("Manual only");
    }

    QStringList parts;
    const int limit = qMax(1, maxVisible);
    const int visibleCount = qMin(limit, triggers.size());
    for (int index = 0; index < visibleCount; ++index) {
        parts.append(formatTriggerDescription(triggers.at(index).toObject()));
    }

    if (triggers.size() > visibleCount) {
        parts.append(translate("+%n more", triggers.size() - visibleCount));
    }

    return parts.join(QStringLiteral("  ·  "));
}

bool isEditableTriggerType(const QString &type) {
    return type == "DailyTrigger" || type == "IntervalTrigger" ||
           type == "WeeklyTrigger" || type == "StartupTrigger";
}

} 
