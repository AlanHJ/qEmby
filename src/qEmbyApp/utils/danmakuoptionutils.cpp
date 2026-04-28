#include "danmakuoptionutils.h"

#include <QtGlobal>

namespace DanmakuOptionUtils {

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
    case SliderKind::Opacity:
        return {5, 100, 1, 10, 72};
    case SliderKind::FontScale:
        return {60, 240, 1, 10, 100};
    case SliderKind::FontWeight:
        return {100, 900, 10, 100, 400};
    case SliderKind::OutlineSize:
        return {0, 60, 1, 5, 30};
    case SliderKind::ShadowOffset:
        return {0, 30, 1, 5, 10};
    case SliderKind::Area:
        return {10, 100, 1, 10, 70};
    case SliderKind::Density:
        return {20, 100, 1, 10, 100};
    case SliderKind::SpeedScale:
        return {50, 300, 1, 10, 100};
    case SliderKind::OffsetMs:
        return {-3000, 3000, 25, 250, 0};
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
    case SliderKind::Opacity:
    case SliderKind::FontScale:
    case SliderKind::Area:
    case SliderKind::Density:
        return QStringLiteral("%1%").arg(value);
    case SliderKind::FontWeight:
        return QString::number(value);
    case SliderKind::OutlineSize:
    case SliderKind::ShadowOffset:
        return QStringLiteral("%1 px").arg(formatDecimalTenths(value));
    case SliderKind::SpeedScale:
        return QStringLiteral("%1x").arg(
            stripTrailingZeros(QString::number(value / 100.0, 'f', 2)));
    case SliderKind::OffsetMs:
        if (value > 0) {
            return QStringLiteral("+%1 ms").arg(value);
        }
        return QStringLiteral("%1 ms").arg(value);
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
    case SliderKind::Opacity:
    case SliderKind::FontScale:
    case SliderKind::Area:
    case SliderKind::Density:
        text.remove(QLatin1Char('%'));
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        value = clampSliderValue(kind, qRound(parsed));
        return true;
    case SliderKind::FontWeight:
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
    case SliderKind::SpeedScale: {
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
    case SliderKind::OffsetMs:
        text.remove(QStringLiteral("ms"));
        parsed = text.toDouble(&ok);
        if (!ok) {
            return false;
        }
        value = clampSliderValue(kind, qRound(parsed));
        return true;
    }

    return false;
}

} 
