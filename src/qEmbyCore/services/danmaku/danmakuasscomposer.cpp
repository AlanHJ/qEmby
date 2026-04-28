#include "danmakuasscomposer.h"

#include <QDebug>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kAssCoordScale = 3;
constexpr int kPlayResX = 1920 * kAssCoordScale;
constexpr int kPlayResY = 1080 * kAssCoordScale;
constexpr int kTopMargin = 24 * kAssCoordScale;
constexpr int kBottomMargin = 48 * kAssCoordScale;
constexpr int kScrollStartPadding = 32 * kAssCoordScale;
constexpr int kScrollEndPadding = 48 * kAssCoordScale;
constexpr int kScrollLaneGapPadding = 96 * kAssCoordScale;
constexpr double kScrollReferenceWidthFactor = 8.0;
constexpr double kScrollSpeedEaseExponent = 0.55;
constexpr double kMaxScrollPixelsPerMs = 1.12;
constexpr double kScrollOutlineRatio = 0.72;
constexpr double kScrollOutlineCap = 2.4;
constexpr double kScrollShadowRatio = 0.25;
constexpr double kScrollShadowCap = 0.45;
constexpr qint64 kMinScrollDurationMs = 2200;
constexpr qint64 kMaxScrollDurationMs = 22000;
constexpr double kMaxScrollQueueDelayRatio = 0.42;
constexpr qint64 kMaxStaticQueueDelayMs = 1800;

QString assTime(qint64 milliseconds)
{
    const qint64 totalCentiseconds = std::max<qint64>(
        0, static_cast<qint64>(std::llround(milliseconds / 10.0)));
    const qint64 cs = totalCentiseconds % 100;
    const qint64 totalSeconds = totalCentiseconds / 100;
    const qint64 s = totalSeconds % 60;
    const qint64 totalMinutes = totalSeconds / 60;
    const qint64 m = totalMinutes % 60;
    const qint64 h = totalMinutes / 60;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(h)
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(cs, 2, 10, QChar('0'));
}

QString assColor(const QColor &color)
{
    return QStringLiteral("&H%1%2%3&")
        .arg(color.blue(), 2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.red(), 2, 16, QChar('0'))
        .toUpper();
}

QString assAlpha(double opacity)
{
    const int alpha = qBound(
        0, static_cast<int>(std::lround((1.0 - opacity) * 255.0)), 255);
    return QStringLiteral("&H%1&")
        .arg(alpha, 2, 16, QChar('0'))
        .toUpper();
}

QString escapeAssText(QString text)
{
    text.replace(QStringLiteral("\r"), QString());
    text.replace(QStringLiteral("\n"), QStringLiteral("\\N"));
    text.replace(QStringLiteral("{"), QStringLiteral("\\{"));
    text.replace(QStringLiteral("}"), QStringLiteral("\\}"));
    return text;
}

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

QString assNumber(double value)
{
    return stripTrailingZeros(QString::number(value, 'f', 2));
}

QStringList normalizeBlockedKeywords(const QStringList &keywords)
{
    QStringList result;
    for (const QString &keyword : keywords) {
        const QString trimmed = keyword.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    result.removeDuplicates();
    return result;
}

bool isBlocked(const QString &text, const QStringList &blockedKeywords)
{
    for (const QString &keyword : blockedKeywords) {
        if (text.contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

double commentScale(int fontLevel)
{
    if (fontLevel <= 0) {
        return 1.0;
    }
    return qBound(0.72, fontLevel / 25.0, 1.65);
}

double effectiveScrollSpeedScale(double configuredSpeedScale)
{
    if (configuredSpeedScale <= 1.0) {
        return configuredSpeedScale;
    }
    return std::pow(configuredSpeedScale, kScrollSpeedEaseExponent);
}

double scrollOutlineSizeForMotion(double outlineSize)
{
    if (outlineSize <= 0.0) {
        return 0.0;
    }
    const double preferred =
        qBound(0.85, outlineSize * kScrollOutlineRatio, kScrollOutlineCap);
    return std::min(outlineSize, preferred);
}

double scrollShadowOffsetForMotion(double shadowOffset)
{
    if (shadowOffset <= 0.0) {
        return 0.0;
    }
    const double preferred =
        qBound(0.0, shadowOffset * kScrollShadowRatio, kScrollShadowCap);
    return std::min(shadowOffset, preferred);
}

QString eventForScroll(qint64 startMs,
                       qint64 endMs,
                       int y,
                       int startX,
                       int endX,
                       int fontSize,
                       int fontWeight,
                       const QColor &color,
                       const QString &alpha,
                       const QString &text)
{
    return QStringLiteral(
               "Dialogue: 2,%1,%2,DanmakuScroll,,0,0,0,,{\\move(%3,%4,%5,%4)\\fs%6\\b%7\\blur0\\c%8\\alpha%9}%10")
        .arg(assTime(startMs), assTime(endMs))
        .arg(startX)
        .arg(y)
        .arg(endX)
        .arg(fontSize)
        .arg(fontWeight)
        .arg(assColor(color))
        .arg(alpha)
        .arg(text);
}

QString eventForStatic(const QString &styleName,
                       qint64 startMs,
                       qint64 endMs,
                       int y,
                       int fontSize,
                       int fontWeight,
                       const QColor &color,
                       const QString &alpha,
                       const QString &text)
{
    return QStringLiteral(
               "Dialogue: 2,%1,%2,%3,,0,0,0,,{\\pos(%4,%5)\\fs%6\\b%7\\c%8\\alpha%9}%10")
        .arg(assTime(startMs), assTime(endMs), styleName)
        .arg(kPlayResX / 2)
        .arg(y)
        .arg(fontSize)
        .arg(fontWeight)
        .arg(assColor(color))
        .arg(alpha)
        .arg(text);
}

} 

QString DanmakuAssComposer::composeAss(const QList<DanmakuComment> &comments,
                                       const DanmakuRenderOptions &options)
{
    QList<DanmakuComment> sortedComments = comments;
    std::sort(sortedComments.begin(), sortedComments.end(),
              [](const DanmakuComment &lhs, const DanmakuComment &rhs) {
                  if (lhs.timeMs == rhs.timeMs) {
                      return lhs.text < rhs.text;
                  }
                  return lhs.timeMs < rhs.timeMs;
              });

    const QStringList blockedKeywords =
        normalizeBlockedKeywords(options.blockedKeywords);
    const double opacity = qBound(0.05, options.opacity, 1.0);
    const double fontScale = qBound(0.6, options.fontScale, 2.4);
    const int fontWeight = qBound(100, options.fontWeight, 900);
    const double outlineSize = qBound(0.0, options.outlineSize, 6.0);
    const double shadowOffset = qBound(0.0, options.shadowOffset, 4.0);
    const double scrollOutlineSize = scrollOutlineSizeForMotion(outlineSize);
    const double scrollShadowOffset = scrollShadowOffsetForMotion(shadowOffset);
    const QString outlineAss = assNumber(outlineSize * kAssCoordScale);
    const QString shadowAss = assNumber(shadowOffset * kAssCoordScale);
    const QString scrollOutlineAss =
        assNumber(scrollOutlineSize * kAssCoordScale);
    const QString scrollShadowAss =
        assNumber(scrollShadowOffset * kAssCoordScale);
    const double speedScale = qBound(0.5, options.speedScale, 3.0);
    const double effectiveSpeedScale =
        effectiveScrollSpeedScale(speedScale);
    const int areaPercent = qBound(10, options.areaPercent, 100);
    const int densityPercent = qBound(20, options.density, 100);
    const int baseFontSize =
        qBound(24 * kAssCoordScale,
               static_cast<int>(std::lround(44.0 * fontScale * kAssCoordScale)),
               88 * kAssCoordScale);
    const int lineHeight = static_cast<int>(std::ceil(baseFontSize * 1.28));
    const int maxLanes = std::max(
        1, static_cast<int>(std::floor((kPlayResY * (areaPercent / 100.0)) /
                                       static_cast<double>(lineHeight))));
    const int laneCount = std::max(
        1, static_cast<int>(std::round(maxLanes * (densityPercent / 100.0))));
    const QString alpha = assAlpha(opacity);
    const qint64 referenceDurationMs = static_cast<qint64>(
        std::lround(9000.0 / effectiveSpeedScale));
    const double referenceTextWidth = std::clamp(
        baseFontSize * kScrollReferenceWidthFactor,
        220.0 * kAssCoordScale,
        1800.0 * kAssCoordScale);
    const double referenceDistance =
        kPlayResX + kScrollStartPadding + kScrollEndPadding + referenceTextWidth;
    const double scrollPixelsPerMs = std::min(
        kMaxScrollPixelsPerMs,
        std::max(0.01, referenceDistance /
                           std::max<qint64>(1, referenceDurationMs)));

    QFont font;
    font.setFamily(QFont().family());
    font.setPixelSize(baseFontSize);
    font.setWeight(static_cast<QFont::Weight>(fontWeight));
    QHash<int, QFontMetricsF> fontMetricsCache;
    auto metricsForSize = [&font, &fontMetricsCache](int fontSize) {
        const auto it = fontMetricsCache.constFind(fontSize);
        if (it != fontMetricsCache.constEnd()) {
            return it.value();
        }

        QFont sizedFont(font);
        sizedFont.setPixelSize(fontSize);
        const QFontMetricsF metrics(sizedFont);
        fontMetricsCache.insert(fontSize, metrics);
        return metrics;
    };

    QVector<qint64> scrollLaneAvailable(laneCount, 0);
    QVector<qint64> topLaneAvailable(laneCount, 0);
    QVector<qint64> bottomLaneAvailable(laneCount, 0);

    QStringList dialogueLines;
    dialogueLines.reserve(sortedComments.size());
    int skippedInvalid = 0;
    int skippedBlocked = 0;
    int skippedByMode = 0;
    int skippedByOverload = 0;
    int renderedScroll = 0;
    int renderedTop = 0;
    int renderedBottom = 0;

    for (const DanmakuComment &comment : std::as_const(sortedComments)) {
        if (!comment.isValid()) {
            ++skippedInvalid;
            continue;
        }

        if (isBlocked(comment.text, blockedKeywords)) {
            ++skippedBlocked;
            continue;
        }

        if ((comment.mode == 1 && options.hideScroll) ||
            (comment.mode == 5 && options.hideTop) ||
            (comment.mode == 4 && options.hideBottom)) {
            ++skippedByMode;
            continue;
        }

        const QString assText = escapeAssText(comment.text);
        const double widthScale = commentScale(comment.fontLevel);
        const int fontSize = std::max(
            18, static_cast<int>(std::lround(baseFontSize * widthScale)));
        const QFontMetricsF metrics = metricsForSize(fontSize);
        const double textWidth = std::max(metrics.horizontalAdvance(comment.text),
                                          static_cast<double>(fontSize));
        const qint64 startMs = std::max<qint64>(0, comment.timeMs + options.offsetMs);

        if (comment.mode == 5 || comment.mode == 4) {
            QVector<qint64> &lanes = (comment.mode == 5) ? topLaneAvailable
                                                         : bottomLaneAvailable;
            const qint64 durationMs = 4200;
            int laneIndex = 0;
            qint64 bestAvailable = lanes[0];
            for (int i = 0; i < lanes.size(); ++i) {
                if (lanes[i] <= startMs) {
                    laneIndex = i;
                    break;
                }
                if (lanes[i] < bestAvailable) {
                    bestAvailable = lanes[i];
                    laneIndex = i;
                }
            }

            const qint64 actualStartMs = std::max(startMs, lanes[laneIndex]);
            if (actualStartMs - startMs > kMaxStaticQueueDelayMs) {
                ++skippedByOverload;
                continue;
            }
            const qint64 endMs = actualStartMs + durationMs;
            lanes[laneIndex] = endMs;

            const int y = (comment.mode == 5)
                              ? (kTopMargin + laneIndex * lineHeight)
                              : (kPlayResY - kBottomMargin -
                                 laneIndex * lineHeight);
            dialogueLines.append(eventForStatic(
                comment.mode == 5 ? QStringLiteral("DanmakuTop")
                                  : QStringLiteral("DanmakuBottom"),
                actualStartMs, endMs, y, fontSize, fontWeight, comment.color,
                alpha,
                assText));
            if (comment.mode == 5) {
                ++renderedTop;
            } else {
                ++renderedBottom;
            }
            continue;
        }

        const int textWidthPixels =
            std::max(1, static_cast<int>(std::ceil(textWidth)));
        const int startX = kPlayResX + kScrollStartPadding;
        const int endX = -textWidthPixels - kScrollEndPadding;
        const double totalDistance = static_cast<double>(startX - endX);
        const qint64 durationMs = qBound<qint64>(
            kMinScrollDurationMs,
            static_cast<qint64>(std::llround(totalDistance / scrollPixelsPerMs)),
            kMaxScrollDurationMs);
        const qint64 laneGapMs =
            static_cast<qint64>(std::ceil((textWidthPixels + kScrollLaneGapPadding) /
                                          scrollPixelsPerMs));

        int laneIndex = 0;
        qint64 bestAvailable = scrollLaneAvailable[0];
        for (int i = 0; i < scrollLaneAvailable.size(); ++i) {
            if (scrollLaneAvailable[i] <= startMs) {
                laneIndex = i;
                break;
            }
            if (scrollLaneAvailable[i] < bestAvailable) {
                bestAvailable = scrollLaneAvailable[i];
                laneIndex = i;
            }
        }

        const qint64 actualStartMs = std::max(startMs, scrollLaneAvailable[laneIndex]);
        const qint64 maxQueueDelayMs = std::max<qint64>(
            300,
            static_cast<qint64>(std::lround(durationMs * kMaxScrollQueueDelayRatio)));
        if (actualStartMs - startMs > maxQueueDelayMs) {
            ++skippedByOverload;
            continue;
        }
        const qint64 endMs = actualStartMs + durationMs;
        scrollLaneAvailable[laneIndex] = actualStartMs + laneGapMs;

        const int y = kTopMargin + laneIndex * lineHeight;
        dialogueLines.append(eventForScroll(actualStartMs, endMs, y, startX, endX,
                                            fontSize, fontWeight, comment.color,
                                            alpha,
                                            assText));
        ++renderedScroll;
    }

    QString ass;
    QTextStream stream(&ass);
    stream << "[Script Info]\n";
    stream << "ScriptType: v4.00+\n";
    stream << "PlayResX: " << kPlayResX << '\n';
    stream << "PlayResY: " << kPlayResY << '\n';
    stream << "WrapStyle: 2\n";
    stream << "ScaledBorderAndShadow: yes\n";
    stream << "YCbCr Matrix: TV.601\n\n";

    stream << "[V4+ Styles]\n";
    stream << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
              "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, "
              "ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, "
              "Alignment, MarginL, MarginR, MarginV, Encoding\n";
    
    stream << "Style: DanmakuScroll," << font.family()
           << ',' << baseFontSize
            << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,"
           << scrollOutlineAss << ',' << scrollShadowAss
           << ",7,0,0,0,1\n";
    stream << "Style: DanmakuTop," << font.family()
           << ',' << baseFontSize
            << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,"
           << outlineAss << ',' << shadowAss
           << ",8,0,0,0,1\n";
    stream << "Style: DanmakuBottom," << font.family()
           << ',' << baseFontSize
            << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,"
           << outlineAss << ',' << shadowAss
           << ",2,0,0,0,1\n\n";

    stream << "[Events]\n";
    stream << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    for (const QString &line : std::as_const(dialogueLines)) {
        stream << line << '\n';
    }

    const int renderedTotal = renderedScroll + renderedTop + renderedBottom;
    qDebug().noquote()
        << "[Danmaku][ASS] Compose summary"
        << "| inputCount:" << sortedComments.size()
        << "| renderedCount:" << renderedTotal
        << "| scroll:" << renderedScroll
        << "| top:" << renderedTop
        << "| bottom:" << renderedBottom
        << "| droppedInvalid:" << skippedInvalid
        << "| droppedBlocked:" << skippedBlocked
        << "| droppedModeFiltered:" << skippedByMode
        << "| droppedQueueOverflow:" << skippedByOverload
        << "| laneCount:" << laneCount
        << "| coordScale:" << kAssCoordScale
        << "| baseFontSize:" << baseFontSize
        << "| lineHeight:" << lineHeight
        << "| scrollPixelsPerMs:" << scrollPixelsPerMs
        << "| fontWeight:" << fontWeight
        << "| effectiveSpeedScale:" << effectiveSpeedScale
        << "| scrollOutline:" << scrollOutlineAss
        << "| scrollShadow:" << scrollShadowAss
        << "| outline:" << outlineAss
        << "| shadow:" << shadowAss
        << "| speedScale:" << speedScale;

    return ass;
}
