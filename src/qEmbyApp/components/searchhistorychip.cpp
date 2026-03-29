#include "searchhistorychip.h"

#include "../managers/thememanager.h"
#include "../../qEmbyCore/config/config_keys.h"
#include "../../qEmbyCore/config/configstore.h"
#include <QColor>
#include <QEnterEvent>
#include <QEasingCurve>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <utility>

namespace
{

constexpr int kChipHeight = 26;
constexpr int kChipBodyHeight = 20;
constexpr int kChipTopOverflow = 5;
constexpr int kChipSideOverflow = 4;
constexpr int kChipMinWidth = 46;
constexpr int kChipMaxWidth = 158;
constexpr int kChipHorizontalPadding = 10;
constexpr int kChipBadgeTop = 0;
constexpr int kChipBadgeHeight = 12;
constexpr int kChipRemoveTop = 0;
constexpr int kChipRemoveButtonSize = 12;
constexpr int kChipRemoveHoverSize = 12;
constexpr int kChipHoverDecorFadeInDurationMs = 150;
constexpr int kChipHoverDecorFadeOutDurationMs = 120;
constexpr qreal kChipHoverDecorMinScale = 0.9;
constexpr qreal kChipHoverDecorMaxShiftY = 1.8;
constexpr qreal kChipRemoveDecorDelayProgress = 0.18;

struct ChipPalette
{
    QColor backgroundTop;
    QColor backgroundBottom;
    QColor border;
    QColor text;
    QColor badgeBackground;
    QColor badgeBorder;
    QColor badgeText;
    QColor removeBackground;
    QColor removeIcon;
};

QColor withAlpha(QColor color, int alpha)
{
    color.setAlpha(alpha);
    return color;
}

int scaledAlpha(int alpha, qreal progress)
{
    return qBound(0, qRound(alpha * progress), 255);
}

qreal delayedProgress(qreal progress, qreal delay)
{
    if (progress <= delay)
    {
        return 0.0;
    }

    if (delay >= 0.999)
    {
        return 1.0;
    }

    return qBound(0.0, (progress - delay) / (1.0 - delay), 1.0);
}

QRectF animatedDecorationRect(const QRect &rect, qreal progress)
{
    const qreal scale =
        kChipHoverDecorMinScale + (1.0 - kChipHoverDecorMinScale) * progress;
    const qreal width = rect.width() * scale;
    const qreal height = rect.height() * scale;
    const QPointF center(rect.center().x() + 0.5,
                         rect.center().y() + 0.5 +
                             (1.0 - progress) * kChipHoverDecorMaxShiftY);
    return QRectF(center.x() - width / 2.0, center.y() - height / 2.0,
                  width, height);
}

QFont chipTextFont(const QWidget *widget)
{
    QFont font = widget->font();
    font.setPixelSize(qMax(11, qRound(12.0 * ThemeManager::fontScaleFactor())));
    font.setWeight(QFont::Medium);
    return font;
}

QFont chipBadgeFont(const QWidget *widget)
{
    QFont font = widget->font();
    font.setPixelSize(qMax(7, qRound(8.0 * ThemeManager::fontScaleFactor())));
    font.setWeight(QFont::Bold);
    return font;
}

ChipPalette basePalette(const QString &heatLevel, bool darkMode)
{
    if (darkMode)
    {
        if (heatLevel == QLatin1String("top"))
        {
            return {
                QColor(QStringLiteral("#23476D")),
                QColor(QStringLiteral("#183B5F")),
                QColor(QStringLiteral("#4F80B5")),
                QColor(QStringLiteral("#F8FBFF")),
                QColor(QStringLiteral("#315F93")),
                QColor(QStringLiteral("#6499D1")),
                QColor(QStringLiteral("#FFFFFF")),
                QColor(QStringLiteral("#284F7B")),
                QColor(QStringLiteral("#EAF3FF"))};
        }

        if (heatLevel == QLatin1String("high"))
        {
            return {
                QColor(QStringLiteral("#1B3A59")),
                QColor(QStringLiteral("#15314D")),
                QColor(QStringLiteral("#3F71A4")),
                QColor(QStringLiteral("#EAF3FF")),
                QColor(QStringLiteral("#28527E")),
                QColor(QStringLiteral("#5082B9")),
                QColor(QStringLiteral("#F5FAFF")),
                QColor(QStringLiteral("#24486F")),
                QColor(QStringLiteral("#EAF3FF"))};
        }

        if (heatLevel == QLatin1String("mid"))
        {
            return {
                QColor(QStringLiteral("#17304A")),
                QColor(QStringLiteral("#122943")),
                QColor(QStringLiteral("#355C84")),
                QColor(QStringLiteral("#D6E7FF")),
                QColor(QStringLiteral("#23486F")),
                QColor(QStringLiteral("#4472A5")),
                QColor(QStringLiteral("#E7F2FF")),
                QColor(QStringLiteral("#203F61")),
                QColor(QStringLiteral("#DDEBFF"))};
        }

        return {
            QColor(QStringLiteral("#1B2535")),
            QColor(QStringLiteral("#141E2D")),
            QColor(QStringLiteral("#2F3D54")),
            QColor(QStringLiteral("#D6E0EB")),
            QColor(QStringLiteral("#253247")),
            QColor(QStringLiteral("#43546C")),
            QColor(QStringLiteral("#C8D4E2")),
            QColor(QStringLiteral("#222F43")),
            QColor(QStringLiteral("#CBD5E1"))};
    }

    if (heatLevel == QLatin1String("top"))
    {
        return {
            QColor(QStringLiteral("#E2F0FF")),
            QColor(QStringLiteral("#CFE2FF")),
            QColor(QStringLiteral("#95B9F0")),
            QColor(QStringLiteral("#173D8C")),
            QColor(QStringLiteral("#CFE1FF")),
            QColor(QStringLiteral("#95B9F0")),
            QColor(QStringLiteral("#1D4ED8")),
            QColor(QStringLiteral("#E9F2FF")),
            QColor(QStringLiteral("#5A79B8"))};
    }

    if (heatLevel == QLatin1String("high"))
    {
        return {
            QColor(QStringLiteral("#EDF5FF")),
            QColor(QStringLiteral("#DDEBFF")),
            QColor(QStringLiteral("#B9D2F8")),
            QColor(QStringLiteral("#214AA5")),
            QColor(QStringLiteral("#DDEBFF")),
            QColor(QStringLiteral("#ABC8F6")),
            QColor(QStringLiteral("#2563EB")),
            QColor(QStringLiteral("#EEF4FC")),
            QColor(QStringLiteral("#63758F"))};
    }

    if (heatLevel == QLatin1String("mid"))
    {
        return {
            QColor(QStringLiteral("#F5F9FF")),
            QColor(QStringLiteral("#E9F1FF")),
            QColor(QStringLiteral("#CBDCF4")),
            QColor(QStringLiteral("#2558BB")),
            QColor(QStringLiteral("#EAF2FF")),
            QColor(QStringLiteral("#C5D8F7")),
            QColor(QStringLiteral("#2D69D1")),
            QColor(QStringLiteral("#F2F6FB")),
            QColor(QStringLiteral("#71839B"))};
    }

    return {
        QColor(QStringLiteral("#FBFCFE")),
        QColor(QStringLiteral("#F3F7FA")),
        QColor(QStringLiteral("#DEE6EF")),
        QColor(QStringLiteral("#334155")),
        QColor(QStringLiteral("#FFFFFF")),
        QColor(QStringLiteral("#D8E1EC")),
        QColor(QStringLiteral("#64748B")),
        QColor(QStringLiteral("#F3F6F9")),
        QColor(QStringLiteral("#8090A4"))};
}

ChipPalette statePalette(const QString &heatLevel, bool darkMode, bool hovered, bool pressed)
{
    ChipPalette palette = basePalette(heatLevel, darkMode);

    if (hovered)
    {
        palette.backgroundTop = palette.backgroundTop.lighter(darkMode ? 116 : 103);
        palette.backgroundBottom = palette.backgroundBottom.lighter(darkMode ? 118 : 104);
        palette.border = palette.border.lighter(darkMode ? 128 : 108);
        palette.badgeBackground = palette.badgeBackground.lighter(darkMode ? 108 : 103);
        palette.badgeBorder = palette.badgeBorder.lighter(darkMode ? 122 : 108);
        palette.removeBackground = palette.removeBackground.lighter(darkMode ? 110 : 103);
    }

    if (pressed)
    {
        palette.backgroundTop = palette.backgroundTop.darker(darkMode ? 112 : 104);
        palette.backgroundBottom = palette.backgroundBottom.darker(darkMode ? 116 : 106);
        palette.border = palette.border.darker(darkMode ? 112 : 108);
        palette.badgeBackground = palette.badgeBackground.darker(darkMode ? 106 : 102);
        palette.removeBackground = palette.removeBackground.darker(darkMode ? 106 : 102);
    }

    return palette;
}

} 

SearchHistoryChip::SearchHistoryChip(QWidget *parent)
    : QAbstractButton(parent)
{
    setObjectName(QStringLiteral("SearchHistoryChip"));
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFixedHeight(kChipHeight);

    m_hoverDecorAnimation = new QVariantAnimation(this);
    m_hoverDecorAnimation->setStartValue(0.0);
    m_hoverDecorAnimation->setEndValue(1.0);
    m_hoverDecorAnimation->setDuration(kChipHoverDecorFadeInDurationMs);
    m_hoverDecorAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverDecorAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_hoverDecorProgress = value.toReal();
                update();
            });

    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
                if (key == ConfigKeys::FontSize)
                {
                    updateMetrics();
                }
                else if (key == ConfigKeys::UiAnimations)
                {
                    if (m_hoverDecorAnimation)
                    {
                        m_hoverDecorAnimation->stop();
                    }
                    m_hoverDecorProgress = shouldShowHoverDecorations() ? 1.0 : 0.0;
                    update();
                }
            });

    updateMetrics();
}

void SearchHistoryChip::setTerm(QString term)
{
    term = term.trimmed();
    if (m_term == term)
    {
        return;
    }

    m_term = std::move(term);
    setToolTip(m_term);
    updateMetrics();
}

QString SearchHistoryChip::term() const
{
    return m_term;
}

void SearchHistoryChip::setSearchCount(int count)
{
    const int safeCount = qMax(1, count);
    if (m_searchCount == safeCount)
    {
        return;
    }

    m_searchCount = safeCount;
    update();
}

int SearchHistoryChip::searchCount() const
{
    return m_searchCount;
}

void SearchHistoryChip::setHeatLevel(const QString &level)
{
    const QString normalizedLevel = level.trimmed().isEmpty() ? QStringLiteral("low")
                                                              : level.trimmed();
    if (m_heatLevel == normalizedLevel)
    {
        return;
    }

    m_heatLevel = normalizedLevel;
    update();
}

QString SearchHistoryChip::heatLevel() const
{
    return m_heatLevel;
}

QSize SearchHistoryChip::sizeHint() const
{
    return {m_cachedWidth, kChipHeight};
}

QSize SearchHistoryChip::minimumSizeHint() const
{
    return sizeHint();
}

bool SearchHistoryChip::hitButton(const QPoint &pos) const
{
    return bodyRect().contains(pos) || badgeRect().contains(pos) ||
           removeButtonRect().contains(pos);
}

void SearchHistoryChip::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const ChipPalette palette = statePalette(m_heatLevel,
                                             ThemeManager::instance()->isDarkMode(),
                                             underMouse(), isDown());
    const qreal decorProgress = qBound(0.0, m_hoverDecorProgress, 1.0);
    const qreal removeBackgroundProgress = decorProgress;
    const qreal removeIconProgress =
        delayedProgress(decorProgress, kChipRemoveDecorDelayProgress);

    const QRect chipBody = bodyRect();
    const QRectF outerRect = QRectF(chipBody).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = outerRect.height() / 2.0;

    QPainterPath chipPath;
    chipPath.addRoundedRect(outerRect, radius, radius);

    QLinearGradient backgroundGradient(0, outerRect.top(), 0, outerRect.bottom());
    backgroundGradient.setColorAt(0.0, palette.backgroundTop);
    backgroundGradient.setColorAt(1.0, palette.backgroundBottom);

    painter.fillPath(chipPath, backgroundGradient);
    painter.setPen(QPen(palette.border, 0.85));
    painter.drawPath(chipPath);

    painter.setPen(palette.text);
    painter.setFont(chipTextFont(this));

    const QRect badgeBox = badgeRect();
    const QRect removeRect = removeButtonRect();
    const QRect textRect(chipBody.left() + kChipHorizontalPadding,
                         chipBody.top() + 1,
                         qMax(1, chipBody.width() - kChipHorizontalPadding * 2),
                         chipBody.height() - 2);

    const QFontMetrics textMetrics(chipTextFont(this));
    const QString displayText =
        textMetrics.elidedText(m_term, Qt::ElideRight, textRect.width());
    const bool shouldCenterText =
        textMetrics.horizontalAdvance(displayText) <= textRect.width();
    painter.drawText(textRect,
                     (shouldCenterText ? Qt::AlignHCenter : Qt::AlignLeft) |
                         Qt::AlignVCenter,
                     displayText);

    if (decorProgress <= 0.001)
    {
        return;
    }

    const QRectF animatedBadgeRect = animatedDecorationRect(badgeBox, decorProgress);
    QPainterPath badgePath;
    badgePath.addEllipse(animatedBadgeRect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.fillPath(badgePath,
                     withAlpha(palette.badgeBackground,
                               scaledAlpha(235, decorProgress)));
    painter.setPen(QPen(withAlpha(palette.badgeBorder,
                                  scaledAlpha(190, decorProgress)),
                        0.85));
    painter.drawPath(badgePath);

    painter.setFont(chipBadgeFont(this));
    painter.setPen(withAlpha(palette.badgeText, scaledAlpha(255, decorProgress)));
    painter.drawText(animatedBadgeRect, Qt::AlignCenter, formattedCount());

    if (removeBackgroundProgress <= 0.001)
    {
        return;
    }

    const QRect hoverRect(removeRect.center().x() - kChipRemoveHoverSize / 2,
                          removeRect.center().y() - kChipRemoveHoverSize / 2,
                          kChipRemoveHoverSize, kChipRemoveHoverSize);
    const QRectF animatedRemoveRect =
        animatedDecorationRect(hoverRect, removeBackgroundProgress);
    QPainterPath removePath;
    removePath.addEllipse(animatedRemoveRect.adjusted(0.5, 0.5, -0.5, -0.5));
    painter.fillPath(removePath, withAlpha(palette.removeBackground,
                                           scaledAlpha((m_removeHovered || m_removePressed) ? 226
                                                                                              : 188,
                                                        removeBackgroundProgress)));
    painter.setPen(QPen(withAlpha(palette.border,
                                  scaledAlpha(175, removeBackgroundProgress)),
                        0.85));
    painter.drawPath(removePath);

    if (removeIconProgress <= 0.001)
    {
        return;
    }

    painter.setPen(QPen(withAlpha(palette.removeIcon,
                                  scaledAlpha(255, removeIconProgress)),
                        1.15, Qt::SolidLine, Qt::RoundCap));
    const QPointF removeCenter = animatedRemoveRect.center();
    const qreal removeHalfSize = 2.55;
    painter.drawLine(QPointF(removeCenter.x() - removeHalfSize,
                             removeCenter.y() - removeHalfSize),
                     QPointF(removeCenter.x() + removeHalfSize,
                             removeCenter.y() + removeHalfSize));
    painter.drawLine(QPointF(removeCenter.x() - removeHalfSize,
                             removeCenter.y() + removeHalfSize),
                     QPointF(removeCenter.x() + removeHalfSize,
                             removeCenter.y() - removeHalfSize));
}

void SearchHistoryChip::enterEvent(QEnterEvent *event)
{
    QAbstractButton::enterEvent(event);
    syncHoverDecorationVisibility();
}

void SearchHistoryChip::leaveEvent(QEvent *event)
{
    QAbstractButton::leaveEvent(event);
    m_removeHovered = false;
    m_removePressed = false;
    syncHoverDecorationVisibility();
}

void SearchHistoryChip::mouseMoveEvent(QMouseEvent *event)
{
    updateRemoveHover(event->position().toPoint());
    QAbstractButton::mouseMoveEvent(event);
}

void SearchHistoryChip::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton &&
        removeButtonRect().contains(event->position().toPoint()))
    {
        m_removePressed = true;
        m_removeHovered = true;
        syncHoverDecorationVisibility();
        event->accept();
        return;
    }

    m_removePressed = false;
    QAbstractButton::mousePressEvent(event);
}

void SearchHistoryChip::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_removePressed)
    {
        const bool triggered = event->button() == Qt::LeftButton &&
                               removeButtonRect().contains(event->position().toPoint());
        m_removePressed = false;
        updateRemoveHover(event->position().toPoint());
        syncHoverDecorationVisibility();
        event->accept();

        if (triggered && !m_term.isEmpty())
        {
            Q_EMIT removeRequested(m_term);
        }
        return;
    }

    QAbstractButton::mouseReleaseEvent(event);
}

void SearchHistoryChip::syncHoverDecorationVisibility()
{
    const bool visible = shouldShowHoverDecorations();
    const qreal targetProgress = visible ? 1.0 : 0.0;
    if (m_hoverDecorVisible == visible)
    {
        if (m_hoverDecorAnimation &&
            m_hoverDecorAnimation->state() == QAbstractAnimation::Running)
        {
            return;
        }
        if (qAbs(m_hoverDecorProgress - targetProgress) < 0.001)
        {
            return;
        }
    }

    m_hoverDecorVisible = visible;

    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (!m_hoverDecorAnimation || reduceAnimations)
    {
        if (m_hoverDecorAnimation)
        {
            m_hoverDecorAnimation->stop();
        }
        m_hoverDecorProgress = targetProgress;
        update();
        return;
    }

    m_hoverDecorAnimation->stop();
    m_hoverDecorAnimation->setDuration(visible ? kChipHoverDecorFadeInDurationMs
                                               : kChipHoverDecorFadeOutDurationMs);
    m_hoverDecorAnimation->setEasingCurve(visible ? QEasingCurve::OutCubic
                                                  : QEasingCurve::InCubic);
    m_hoverDecorAnimation->setStartValue(m_hoverDecorProgress);
    m_hoverDecorAnimation->setEndValue(targetProgress);
    m_hoverDecorAnimation->start();
}

bool SearchHistoryChip::shouldShowHoverDecorations() const
{
    return underMouse() || m_removeHovered || m_removePressed;
}

void SearchHistoryChip::updateRemoveHover(const QPoint &pos)
{
    const bool hovered = removeButtonRect().contains(pos);
    if (m_removeHovered == hovered)
    {
        return;
    }

    m_removeHovered = hovered;
    syncHoverDecorationVisibility();
}

void SearchHistoryChip::updateMetrics()
{
    ensurePolished();

    const QFontMetrics metrics(chipTextFont(this));
    const int naturalTextWidth = metrics.horizontalAdvance(m_term);
    m_cachedWidth = qBound(kChipMinWidth,
                           naturalTextWidth + kChipHorizontalPadding * 2 +
                               kChipSideOverflow * 2 + 2,
                           kChipMaxWidth);

    setFixedSize(m_cachedWidth, kChipHeight);
    updateGeometry();
    update();
}

QString SearchHistoryChip::formattedCount() const
{
    return (m_searchCount > 999) ? QStringLiteral("999+")
                                 : QString::number(qMax(1, m_searchCount));
}

QRect SearchHistoryChip::bodyRect() const
{
    return QRect(kChipSideOverflow, kChipTopOverflow,
                 qMax(1, width() - kChipSideOverflow * 2), kChipBodyHeight);
}

QRect SearchHistoryChip::removeButtonRect() const
{
    return QRect(width() - kChipRemoveButtonSize - 1,
                 kChipRemoveTop, kChipRemoveButtonSize, kChipRemoveButtonSize);
}

QRect SearchHistoryChip::badgeRect() const
{
    const QFontMetrics metrics(chipBadgeFont(this));
    const int badgeWidth = qBound(kChipBadgeHeight,
                                  metrics.horizontalAdvance(formattedCount()) + 6, 18);
    return QRect(1, kChipBadgeTop, badgeWidth, kChipBadgeHeight);
}
