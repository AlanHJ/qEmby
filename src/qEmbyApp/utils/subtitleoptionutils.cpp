#include "subtitleoptionutils.h"

#include <QFontDatabase>
#include <QtGlobal>

namespace SubtitleOptionUtils {

namespace {

QString stripTrailingZeros(QString text)
{
    if (!text.contains(QLatin1Char('.'))) {
        return text;
    }

    while (text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

QString formatDecimalTenths(int value)
{
    return stripTrailingZeros(QString::number(value / 10.0, 'f', 1));
}

} 

SliderSpec sliderSpec(SliderKind kind)
{
    switch (kind) {
    case SliderKind::DelayMs:
        return {-3000, 3000, 25, 250, 0};
    case SliderKind::FontSize:
        return {18, 72, 1, 4, 38};
    case SliderKind::Position:
        return {60, 100, 1, 5, 92};
    case SliderKind::OutlineSize:
        return {0, 40, 1, 5, 17};
    case SliderKind::ShadowOffset:
        return {0, 30, 1, 5, 0};
    case SliderKind::ScalePercent:
        return {50, 300, 1, 10, 100};
    }

    return {};
}

int clampSliderValue(SliderKind kind, int value)
{
    const SliderSpec spec = sliderSpec(kind);
    return qBound(spec.minimum, value, spec.maximum);
}

QString formatSliderValue(SliderKind kind, int value)
{
    value = clampSliderValue(kind, value);

    switch (kind) {
    case SliderKind::DelayMs:
        if (value > 0) {
            return QStringLiteral("+%1 ms").arg(value);
        }
        return QStringLiteral("%1 ms").arg(value);
    case SliderKind::FontSize:
        return QStringLiteral("%1 px").arg(value);
    case SliderKind::Position:
        return QStringLiteral("%1%").arg(value);
    case SliderKind::OutlineSize:
    case SliderKind::ShadowOffset:
        return QStringLiteral("%1 px").arg(formatDecimalTenths(value));
    case SliderKind::ScalePercent:
        return QStringLiteral("%1x").arg(
            stripTrailingZeros(QString::number(value / 100.0, 'f', 2)));
    }

    return QString::number(value);
}

bool parseSliderValue(SliderKind kind, QString text, int &value)
{
    text = text.trimmed().toLower();
    text.remove(QLatin1Char(' '));

    bool ok = false;
    double parsed = 0.0;

    switch (kind) {
    case SliderKind::DelayMs:
        text.remove(QStringLiteral("ms"));
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        value = clampSliderValue(kind, qRound(parsed));
        return true;
    case SliderKind::FontSize:
    case SliderKind::Position:
        text.remove(QStringLiteral("px"));
        text.remove(QLatin1Char('%'));
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        value = clampSliderValue(kind, qRound(parsed));
        return true;
    case SliderKind::OutlineSize:
    case SliderKind::ShadowOffset:
        text.remove(QStringLiteral("px"));
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        value = clampSliderValue(kind, qRound(parsed * 10.0));
        return true;
    case SliderKind::ScalePercent: {
        const bool explicitScale =
            text.endsWith(QLatin1Char('x')) || text.contains(QLatin1Char('.'));
        if (text.endsWith(QLatin1Char('x'))) {
            text.chop(1);
        }
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        const int rawValue =
            explicitScale || (parsed > 0.0 && parsed <= 10.0)
                ? qRound(parsed * 100.0)
                : qRound(parsed);
        value = clampSliderValue(kind, rawValue);
        return true;
    }
    }

    return false;
}

QString defaultFontFamily()
{
    return QStringLiteral("sans-serif");
}

QString normalizeFontFamily(QString value)
{
    value = value.trimmed();
    return value.isEmpty() ? defaultFontFamily() : value;
}

QStringList availableFontFamilies()
{
    QFontDatabase fontDatabase;
    QStringList families = fontDatabase.families();
    families.removeAll(QString());
    families.removeDuplicates();
    families.sort(Qt::CaseInsensitive);

    const QStringList preferredFamilies = {
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei"),
        QStringLiteral("Segoe UI"),
        QStringLiteral("PingFang SC"),
        QStringLiteral("Source Han Sans SC"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Arial"),
        QStringLiteral("Tahoma"),
        QStringLiteral("Helvetica"),
        QStringLiteral("Times New Roman")};

    QStringList orderedFamilies;
    for (const QString &family : preferredFamilies) {
        if (families.contains(family) && !orderedFamilies.contains(family)) {
            orderedFamilies.append(family);
        }
    }

    for (const QString &family : families) {
        if (!orderedFamilies.contains(family)) {
            orderedFamilies.append(family);
        }
    }

    return orderedFamilies;
}

} 
