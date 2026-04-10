#include "danmakuasscomposer.h"

#include <QFont>
#include <QFontMetricsF>
#include <QTextStream>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kPlayResX = 1920;
constexpr int kPlayResY = 1080;
constexpr int kTopMargin = 24;
constexpr int kBottomMargin = 48;

QString assTime(qint64 milliseconds)
{
    const qint64 totalCentiseconds = std::max<qint64>(0, milliseconds / 10);
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

QString eventForScroll(qint64 startMs,
                       qint64 endMs,
                       int y,
                       double textWidth,
                       int fontSize,
                       const QColor &color,
                       const QString &alpha,
                       const QString &text)
{
    const int startX = kPlayResX + static_cast<int>(std::ceil(textWidth)) + 32;
    const int endX = -static_cast<int>(std::ceil(textWidth)) - 48;
    return QStringLiteral(
               "Dialogue: 2,%1,%2,DanmakuScroll,,0,0,0,,{\\move(%3,%4,%5,%4)\\fs%6\\c%7\\alpha%8}%9")
        .arg(assTime(startMs), assTime(endMs))
        .arg(startX)
        .arg(y)
        .arg(endX)
        .arg(fontSize)
        .arg(assColor(color))
        .arg(alpha, text);
}

QString eventForStatic(const QString &styleName,
                       qint64 startMs,
                       qint64 endMs,
                       int y,
                       int fontSize,
                       const QColor &color,
                       const QString &alpha,
                       const QString &text)
{
    return QStringLiteral(
               "Dialogue: 2,%1,%2,%3,,0,0,0,,{\\pos(%4,%5)\\fs%6\\c%7\\alpha%8}%9")
        .arg(assTime(startMs), assTime(endMs), styleName)
        .arg(kPlayResX / 2)
        .arg(y)
        .arg(fontSize)
        .arg(assColor(color))
        .arg(alpha, text);
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
    const double speedScale = qBound(0.5, options.speedScale, 3.0);
    const int areaPercent = qBound(10, options.areaPercent, 100);
    const int densityPercent = qBound(20, options.density, 100);
    const int baseFontSize =
        qBound(24, static_cast<int>(std::lround(44.0 * fontScale)), 88);
    const int lineHeight = static_cast<int>(std::ceil(baseFontSize * 1.28));
    const int maxLanes = std::max(
        1, static_cast<int>(std::floor((kPlayResY * (areaPercent / 100.0)) /
                                       static_cast<double>(lineHeight))));
    const int laneCount = std::max(
        1, static_cast<int>(std::round(maxLanes * (densityPercent / 100.0))));
    const QString alpha = assAlpha(opacity);

    QFont font;
    font.setFamily(QFont().family());
    font.setPixelSize(baseFontSize);
    const QFontMetricsF baseMetrics(font);

    QVector<qint64> scrollLaneAvailable(laneCount, 0);
    QVector<qint64> topLaneAvailable(laneCount, 0);
    QVector<qint64> bottomLaneAvailable(laneCount, 0);

    QStringList dialogueLines;
    dialogueLines.reserve(sortedComments.size());

    for (const DanmakuComment &comment : std::as_const(sortedComments)) {
        if (!comment.isValid()) {
            continue;
        }

        if (isBlocked(comment.text, blockedKeywords)) {
            continue;
        }

        if ((comment.mode == 1 && options.hideScroll) ||
            (comment.mode == 5 && options.hideTop) ||
            (comment.mode == 4 && options.hideBottom)) {
            continue;
        }

        const QString assText = escapeAssText(comment.text);
        const double widthScale = commentScale(comment.fontLevel);
        const int fontSize = std::max(
            18, static_cast<int>(std::lround(baseFontSize * widthScale)));
        const double textWidth =
            std::max(baseMetrics.horizontalAdvance(comment.text) * widthScale,
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
            const qint64 endMs = actualStartMs + durationMs;
            lanes[laneIndex] = endMs;

            const int y = (comment.mode == 5)
                              ? (kTopMargin + laneIndex * lineHeight)
                              : (kPlayResY - kBottomMargin -
                                 laneIndex * lineHeight);
            dialogueLines.append(eventForStatic(
                comment.mode == 5 ? QStringLiteral("DanmakuTop")
                                  : QStringLiteral("DanmakuBottom"),
                actualStartMs, endMs, y, fontSize, comment.color, alpha,
                assText));
            continue;
        }

        const qint64 durationMs =
            static_cast<qint64>(std::lround(9000.0 / speedScale));
        const double totalDistance = kPlayResX + textWidth + 96.0;
        const double pixelsPerMs = totalDistance / durationMs;
        const qint64 laneGapMs =
            static_cast<qint64>(std::ceil((textWidth + 96.0) / pixelsPerMs));

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
        const qint64 endMs = actualStartMs + durationMs;
        scrollLaneAvailable[laneIndex] = actualStartMs + laneGapMs;

        const int y = kTopMargin + laneIndex * lineHeight;
        dialogueLines.append(eventForScroll(actualStartMs, endMs, y, textWidth,
                                            fontSize, comment.color, alpha,
                                            assText));
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
           << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,3,1,7,0,0,0,1\n";
    stream << "Style: DanmakuTop," << font.family()
           << ',' << baseFontSize
           << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,3,1,8,0,0,0,1\n";
    stream << "Style: DanmakuBottom," << font.family()
           << ',' << baseFontSize
           << ",&H00FFFFFF,&H00FFFFFF,&H32000000,&H82000000,0,0,0,0,100,100,0,0,1,3,1,2,0,0,0,1\n\n";

    stream << "[Events]\n";
    stream << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    for (const QString &line : std::as_const(dialogueLines)) {
        stream << line << '\n';
    }

    return ass;
}
