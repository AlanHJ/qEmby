#include "subtitlestyleutils.h"

#include "../components/mpvcontroller.h"
#include "subtitleoptionutils.h"

#include <QDebug>
#include <config/config_keys.h>
#include <config/configstore.h>

namespace SubtitleStyleUtils {

namespace {

int readConfigInt(const QString &key, int fallback)
{
    const QVariant value =
        ConfigStore::instance()->get<QVariant>(key, QVariant(fallback));

    bool ok = false;
    const int result = value.toInt(&ok);
    if (ok) {
        return result;
    }

    const int parsed = value.toString().trimmed().toInt(&ok);
    return ok ? parsed : fallback;
}

QString readSubtitleFontFamily()
{
    return SubtitleOptionUtils::normalizeFontFamily(
        ConfigStore::instance()->get<QString>(
            ConfigKeys::PlayerSubtitleFont,
            SubtitleOptionUtils::defaultFontFamily()));
}

} 

bool isSubtitleStyleKey(const QString &key)
{
    return key == QLatin1String(ConfigKeys::PlayerSubtitleFont) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitleDelayMs) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitleFontSize) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitlePosition) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitleOutlineSize) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitleShadowOffset) ||
           key == QLatin1String(ConfigKeys::PlayerSubtitleScale);
}

void applyToController(MpvController *controller,
                       bool protectPrimarySubtitleForDanmaku)
{
    if (!controller) {
        return;
    }

    const int delayMs = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::DelayMs,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitleDelayMs),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::DelayMs)
                .defaultValue));
    const int fontSize = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::FontSize,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitleFontSize),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::FontSize)
                .defaultValue));
    const int position = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::Position,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitlePosition),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::Position)
                .defaultValue));
    const int outlineSize = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::OutlineSize,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitleOutlineSize),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::OutlineSize)
                .defaultValue));
    const int shadowOffset = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::ShadowOffset,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitleShadowOffset),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::ShadowOffset)
                .defaultValue));
    const int scalePercent = SubtitleOptionUtils::clampSliderValue(
        SubtitleOptionUtils::SliderKind::ScalePercent,
        readConfigInt(
            QLatin1String(ConfigKeys::PlayerSubtitleScale),
            SubtitleOptionUtils::sliderSpec(
                SubtitleOptionUtils::SliderKind::ScalePercent)
                .defaultValue));

    const QString fontFamily = readSubtitleFontFamily();
    const double delaySeconds = delayMs / 1000.0;
    const double outlinePixels = outlineSize / 10.0;
    const double shadowPixels = shadowOffset / 10.0;
    const double scaleFactor = scalePercent / 100.0;

    controller->setProperty(QStringLiteral("sub-font"), fontFamily);
    controller->setProperty(QStringLiteral("sub-font-size"), fontSize);
    controller->setProperty(QStringLiteral("sub-scale"), scaleFactor);
    controller->setProperty(QStringLiteral("sub-outline-size"), outlinePixels);
    controller->setProperty(QStringLiteral("sub-shadow-offset"), shadowPixels);
    controller->setProperty(QStringLiteral("secondary-sub-font"), fontFamily);
    controller->setProperty(QStringLiteral("secondary-sub-font-size"),
                            fontSize);
    controller->setProperty(QStringLiteral("secondary-sub-scale"),
                            scaleFactor);
    controller->setProperty(QStringLiteral("secondary-sub-outline-size"),
                            outlinePixels);
    controller->setProperty(QStringLiteral("secondary-sub-shadow-offset"),
                            shadowPixels);
    controller->setProperty(QStringLiteral("secondary-sub-ass-override"),
                            QStringLiteral("force"));
    controller->setProperty(QStringLiteral("secondary-sub-pos"), position);
    controller->setProperty(QStringLiteral("secondary-sub-delay"),
                            delaySeconds);

    if (protectPrimarySubtitleForDanmaku) {
        controller->setProperty(QStringLiteral("sub-ass-override"),
                                QStringLiteral("no"));
        controller->setProperty(QStringLiteral("sub-pos"), 100);
        controller->setProperty(QStringLiteral("sub-delay"), 0.0);
    } else {
        controller->setProperty(QStringLiteral("sub-ass-override"),
                                QStringLiteral("force"));
        controller->setProperty(QStringLiteral("sub-pos"), position);
        controller->setProperty(QStringLiteral("sub-delay"), delaySeconds);
    }

    qDebug().noquote()
        << "[Subtitle][Player] Apply style"
        << "| font:" << fontFamily
        << "| delayMs:" << delayMs
        << "| fontSize:" << fontSize
        << "| position:" << position
        << "| outline:" << outlinePixels
        << "| shadow:" << shadowPixels
        << "| scale:" << scaleFactor
        << "| protectDanmakuPrimary:" << protectPrimarySubtitleForDanmaku;
}

} 
