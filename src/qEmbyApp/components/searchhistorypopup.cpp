#include "searchhistorypopup.h"

#include "flowlayout.h"
#include "searchhistorychip.h"
#include "../managers/thememanager.h"
#include "../../qEmbyCore/config/config_keys.h"
#include "../../qEmbyCore/config/configstore.h"
#include <QColor>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QEasingCurve>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QTransform>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace {

constexpr int kSearchHistoryPopupMinWidth = 360;
constexpr int kSearchHistoryPopupVisibleCount = 20;
constexpr int kSearchHistoryPopupOuterMargin = 4;
constexpr int kSearchHistoryPopupVerticalOffset = 3;
constexpr int kSearchHistoryPopupBodyPadding = 8;
constexpr int kSearchHistoryPopupBodyGap = 6;
constexpr int kSearchHistoryPopupControlsTop = 2;
constexpr int kSearchHistoryPopupBodyBottomPadding = 6;
constexpr int kSearchHistoryPopupEdgeScrollBarWidth = 4;
constexpr int kSearchHistoryPopupContentRightInset = 6;
constexpr int kSearchHistoryPopupCollapsedBodyMaxHeight = 228;
constexpr int kSearchHistoryPopupExpandedBodyMaxHeight = 320;
constexpr int kSearchHistoryPopupExpandDurationMs = 240;
constexpr int kSearchHistoryPopupCollapseDurationMs = 200;
constexpr int kSearchHistoryPopupSortTransitionDurationMs = 190;
constexpr qreal kSearchHistoryPopupSortTransitionMidpoint = 0.44;
constexpr qreal kSearchHistoryPopupSortTransitionMinOpacity = 0.18;
constexpr int kSearchHistoryPopupShowDurationMs = 180;
constexpr int kSearchHistoryPopupHideDurationMs = 140;
constexpr int kSearchHistoryPopupVisibilityOffsetY = 8;

QString normalizeHistoryTerm(QString term)
{
    return term.simplified().toCaseFolded();
}

void repolishWidget(QWidget *widget)
{
    if (!widget)
    {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

void clearFlowLayout(FlowLayout *layout)
{
    while (layout && layout->count() > 0)
    {
        QLayoutItem *item = layout->takeAt(0);
        if (item && item->widget())
        {
            delete item->widget();
        }
        delete item;
    }
}

QColor controlIconColor(bool active)
{
    if (ThemeManager::instance()->isDarkMode())
    {
        return active ? QColor(QStringLiteral("#F8FAFC"))
                      : QColor(QStringLiteral("#8CA0B7"));
    }

    return active ? QColor(QStringLiteral("#1D4ED8"))
                  : QColor(QStringLiteral("#64748B"));
}

QColor clearIconColor(bool armed)
{
    if (ThemeManager::instance()->isDarkMode())
    {
        return armed ? QColor(QStringLiteral("#FCA5A5"))
                     : QColor(QStringLiteral("#8CA0B7"));
    }

    return armed ? QColor(QStringLiteral("#DC2626"))
                 : QColor(QStringLiteral("#64748B"));
}

} 

SearchHistoryPopup::SearchHistoryPopup(QWidget *parent)
    : QFrame(parent) {
    setObjectName("SearchHistoryPopup");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    hide();

    auto *hostLayout = new QVBoxLayout(this);
    hostLayout->setContentsMargins(kSearchHistoryPopupOuterMargin,
                                   kSearchHistoryPopupOuterMargin,
                                   kSearchHistoryPopupOuterMargin,
                                   kSearchHistoryPopupOuterMargin);
    hostLayout->setSpacing(0);

    m_card = new QFrame(this);
    m_card->setObjectName("SearchHistoryPopupCard");
    m_card->setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QVBoxLayout(m_card);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_body = new QWidget(m_card);
    m_body->setObjectName("SearchHistoryPopupBody");
    auto *bodyLayout = new QVBoxLayout(m_body);
    bodyLayout->setContentsMargins(kSearchHistoryPopupBodyPadding, kSearchHistoryPopupControlsTop,
                                   0,
                                   kSearchHistoryPopupBodyBottomPadding);
    bodyLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(m_body);
    m_scrollArea->setObjectName("SearchHistoryScrollArea");
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->setObjectName("SearchHistoryScrollViewport");
    if (auto *verticalBar = m_scrollArea->verticalScrollBar())
    {
        verticalBar->setSingleStep(20);
        verticalBar->setPageStep(56);
    }

    m_flowHost = new QWidget();
    m_flowHost->setObjectName("SearchHistoryFlowHost");
    m_flowHost->setAttribute(Qt::WA_StyledBackground, true);
    m_flowHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_flowOpacityEffect = new QGraphicsOpacityEffect(m_flowHost);
    m_flowOpacityEffect->setOpacity(1.0);
    m_flowHost->setGraphicsEffect(m_flowOpacityEffect);
    m_flowLayout = new FlowLayout(m_flowHost, 0, 2, 2);
    m_scrollArea->setWidget(m_flowHost);
    bodyLayout->addWidget(m_scrollArea);

    m_headerDivider = new QFrame(m_body);
    m_headerDivider->setObjectName("SearchHistoryHeaderDivider");
    m_headerDivider->setFixedSize(1, 13);

    m_header = new QWidget(m_body);
    m_header->setObjectName("SearchHistoryPopupHeader");
    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(2);
    m_header->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto createHeaderButton = [this](const QString &objectName) {
        auto *button = new QToolButton(m_header);
        button->setObjectName(objectName);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(16, 16);
        button->setIconSize(QSize(13, 13));
        return button;
    };

    m_recentButton = createHeaderButton(QStringLiteral("SearchHistoryModeButton"));
    connect(m_recentButton, &QToolButton::clicked, this, [this]() {
        setSortMode(SortMode::Recent);
    });
    headerLayout->addWidget(m_recentButton, 0, Qt::AlignCenter);

    m_hotButton = createHeaderButton(QStringLiteral("SearchHistoryModeButton"));
    connect(m_hotButton, &QToolButton::clicked, this, [this]() {
        setSortMode(SortMode::Count);
    });
    headerLayout->addWidget(m_hotButton, 0, Qt::AlignCenter);

    m_clearButton = new QToolButton(m_header);
    m_clearButton->setObjectName("SearchHistoryClearButton");
    m_clearButton->setCursor(Qt::PointingHandCursor);
    m_clearButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_clearButton->setAutoRaise(true);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setFixedSize(16, 16);
    m_clearButton->setIconSize(QSize(13, 13));
    connect(m_clearButton, &QToolButton::clicked, this, [this]() {
        if (!m_clearArmed)
        {
            setClearArmed(true);
            return;
        }

        setClearArmed(false);
        Q_EMIT clearHistoryRequested();
        dismiss();
    });
    headerLayout->addWidget(m_clearButton, 0, Qt::AlignCenter);

    m_expandButton = new QToolButton(m_header);
    m_expandButton->setObjectName("SearchHistoryExpandButton");
    m_expandButton->setCursor(Qt::PointingHandCursor);
    m_expandButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_expandButton->setAutoRaise(true);
    m_expandButton->setCheckable(true);
    m_expandButton->setFocusPolicy(Qt::NoFocus);
    m_expandButton->setFixedSize(16, 16);
    m_expandButton->setIconSize(QSize(13, 13));
    connect(m_expandButton, &QToolButton::clicked, this, [this]() {
        setExpanded(!m_expanded);
    });
    headerLayout->addWidget(m_expandButton, 0, Qt::AlignCenter);
    mainLayout->addWidget(m_body);
    hostLayout->addWidget(m_card);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme) { updateControlIcons(); });
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
                if (key == ConfigKeys::UiAnimations)
                {
                    syncReducedAnimationMode();
                }
            });

    m_heightAnimation = new QVariantAnimation(this);
    m_heightAnimation->setStartValue(0.0);
    m_heightAnimation->setEndValue(1.0);
    m_heightAnimation->setDuration(kSearchHistoryPopupExpandDurationMs);
    m_heightAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_heightAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                const qreal progress = value.toReal();
                const int scrollHeight =
                    qRound(m_animationStartScrollHeight +
                           (m_animationTargetScrollHeight -
                            m_animationStartScrollHeight) *
                               progress);
                const int bodyHeight =
                    qRound(m_animationStartBodyHeight +
                           (m_animationTargetBodyHeight -
                            m_animationStartBodyHeight) *
                               progress);
                const int popupHeight =
                    qRound(m_animationStartPopupHeight +
                           (m_animationTargetPopupHeight -
                            m_animationStartPopupHeight) *
                               progress);
                applyAnimatedHeights(scrollHeight, bodyHeight, popupHeight);
            });
    connect(m_heightAnimation, &QVariantAnimation::finished, this,
            [this]() {
                applyAnimatedHeights(m_animationTargetScrollHeight,
                                     m_animationTargetBodyHeight,
                                     m_animationTargetPopupHeight);
            });

    m_sortTransitionAnimation = new QVariantAnimation(this);
    m_sortTransitionAnimation->setStartValue(0.0);
    m_sortTransitionAnimation->setEndValue(1.0);
    m_sortTransitionAnimation->setDuration(kSearchHistoryPopupSortTransitionDurationMs);
    m_sortTransitionAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_sortTransitionAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                if (!m_flowOpacityEffect)
                {
                    return;
                }

                const qreal progress = value.toReal();
                if (!m_sortTransitionRebuilt &&
                    progress >= kSearchHistoryPopupSortTransitionMidpoint)
                {
                    m_sortTransitionRebuilt = true;
                    rebuildChips();
                }

                qreal opacity = 1.0;
                if (progress < kSearchHistoryPopupSortTransitionMidpoint)
                {
                    const qreal fadeOutProgress =
                        progress / kSearchHistoryPopupSortTransitionMidpoint;
                    opacity = 1.0 - (1.0 - kSearchHistoryPopupSortTransitionMinOpacity) *
                                        fadeOutProgress;
                }
                else
                {
                    const qreal fadeInProgress =
                        (progress - kSearchHistoryPopupSortTransitionMidpoint) /
                        (1.0 - kSearchHistoryPopupSortTransitionMidpoint);
                    opacity = kSearchHistoryPopupSortTransitionMinOpacity +
                              (1.0 - kSearchHistoryPopupSortTransitionMinOpacity) *
                                  fadeInProgress;
                }

                m_flowOpacityEffect->setOpacity(opacity);
            });
    connect(m_sortTransitionAnimation, &QVariantAnimation::finished, this,
            [this]() {
                if (!m_sortTransitionRebuilt)
                {
                    m_sortTransitionRebuilt = true;
                    rebuildChips();
                }
                if (m_flowOpacityEffect)
                {
                    m_flowOpacityEffect->setOpacity(1.0);
                }
            });

    m_visibilityAnimation = new QVariantAnimation(this);
    m_visibilityAnimation->setStartValue(0.0);
    m_visibilityAnimation->setEndValue(1.0);
    m_visibilityAnimation->setDuration(kSearchHistoryPopupShowDurationMs);
    m_visibilityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_visibilityAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                const qreal progress = value.toReal();
                const qreal opacity =
                    m_visibilityStartOpacity +
                    (m_visibilityTargetOpacity - m_visibilityStartOpacity) *
                        progress;
                const int offsetY =
                    qRound(m_visibilityStartOffsetY +
                           (m_visibilityTargetOffsetY -
                            m_visibilityStartOffsetY) *
                               progress);
                applyVisibilityState(opacity, offsetY);
            });
    connect(m_visibilityAnimation, &QVariantAnimation::finished, this,
            [this]() {
                applyVisibilityState(m_visibilityTargetOpacity,
                                     m_visibilityTargetOffsetY);
                if (!m_visibilityHiding)
                {
                    if (m_cardOpacityEffect)
                    {
                        m_cardOpacityEffect->setOpacity(1.0);
                        m_cardOpacityEffect->setEnabled(false);
                    }
                    return;
                }

                m_visibilityHiding = false;
                QFrame::hide();
                if (m_cardOpacityEffect)
                {
                    m_cardOpacityEffect->setOpacity(1.0);
                    m_cardOpacityEffect->setEnabled(false);
                }
                m_visibilityOffsetY = 0;
                m_anchor = nullptr;
            });

    refreshControls();
    m_lastCardWidth = kSearchHistoryPopupMinWidth;
    syncReducedAnimationMode();
}

void SearchHistoryPopup::setEntries(
    QList<SearchHistoryManager::SearchHistoryEntry> entries)
{
    stopSortTransitionAnimation();
    const bool keepExpanded =
        m_expanded && entries.size() > kSearchHistoryPopupVisibleCount;
    m_entries = std::move(entries);
    m_expanded = keepExpanded;
    setClearArmed(false);
    rebuildChips();
}

bool SearchHistoryPopup::hasContent() const {
    return !m_entries.isEmpty();
}

void SearchHistoryPopup::showBelow(QWidget *anchor) {
    if (!anchor || !hasContent()) {
        dismiss(true);
        return;
    }

    stopVisibilityAnimation();
    m_anchor = anchor;
    relayoutPopup(qMax(kSearchHistoryPopupMinWidth, anchor->width() + 12));

    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    const bool wasVisible = isVisible();

    if (reduceAnimations)
    {
        if (!wasVisible)
        {
            QFrame::show();
        }
        if (m_cardOpacityEffect)
        {
            m_cardOpacityEffect->setOpacity(1.0);
            m_cardOpacityEffect->setEnabled(false);
        }
        m_visibilityHiding = false;
        applyVisibilityState(1.0, 0);
        raise();
        return;
    }

    if (!wasVisible)
    {
        m_visibilityStartOpacity = 0.0;
        m_visibilityStartOffsetY = kSearchHistoryPopupVisibilityOffsetY;
        if (m_cardOpacityEffect)
        {
            m_cardOpacityEffect->setEnabled(true);
            m_cardOpacityEffect->setOpacity(0.0);
        }
        m_visibilityOffsetY = kSearchHistoryPopupVisibilityOffsetY;
        QFrame::show();
        repositionToAnchor();
    }
    else
    {
        m_visibilityStartOpacity =
            m_cardOpacityEffect ? m_cardOpacityEffect->opacity() : 1.0;
        m_visibilityStartOffsetY = m_visibilityOffsetY;
        if (m_cardOpacityEffect)
        {
            m_cardOpacityEffect->setEnabled(true);
        }
    }

    m_visibilityHiding = false;
    m_visibilityTargetOpacity = 1.0;
    m_visibilityTargetOffsetY = 0;
    m_visibilityAnimation->setDuration(kSearchHistoryPopupShowDurationMs);
    m_visibilityAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_visibilityAnimation->setStartValue(0.0);
    m_visibilityAnimation->setEndValue(1.0);
    m_visibilityAnimation->start();
    raise();
}

void SearchHistoryPopup::dismiss(bool immediate)
{
    stopHeightAnimation();
    stopSortTransitionAnimation();

    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    const bool shouldHideImmediately = immediate || reduceAnimations;

    if (!isVisible() && (!m_visibilityAnimation ||
                         m_visibilityAnimation->state() != QAbstractAnimation::Running))
    {
        m_anchor = nullptr;
        return;
    }

    if (shouldHideImmediately)
    {
        stopVisibilityAnimation();
        m_visibilityHiding = false;
        m_visibilityOffsetY = 0;
        if (m_cardOpacityEffect)
        {
            m_cardOpacityEffect->setOpacity(1.0);
            m_cardOpacityEffect->setEnabled(false);
        }
        QFrame::hide();
        m_anchor = nullptr;
        return;
    }

    stopVisibilityAnimation();
    if (m_cardOpacityEffect)
    {
        m_cardOpacityEffect->setEnabled(true);
    }
    m_visibilityStartOpacity =
        m_cardOpacityEffect ? m_cardOpacityEffect->opacity() : 1.0;
    m_visibilityTargetOpacity = 0.0;
    m_visibilityStartOffsetY = m_visibilityOffsetY;
    m_visibilityTargetOffsetY = kSearchHistoryPopupVisibilityOffsetY;
    m_visibilityHiding = true;

    if (!isVisible())
    {
        QFrame::show();
    }

    m_visibilityAnimation->setDuration(kSearchHistoryPopupHideDurationMs);
    m_visibilityAnimation->setEasingCurve(QEasingCurve::InCubic);
    m_visibilityAnimation->setStartValue(0.0);
    m_visibilityAnimation->setEndValue(1.0);
    m_visibilityAnimation->start();
}

void SearchHistoryPopup::rebuildChips(bool preserveScrollPosition)
{
    stopHeightAnimation();
    int preservedVerticalScrollValue = 0;
    int preservedHorizontalScrollValue = 0;
    if (preserveScrollPosition && m_scrollArea)
    {
        if (auto *verticalBar = m_scrollArea->verticalScrollBar())
        {
            preservedVerticalScrollValue = verticalBar->value();
        }

        if (auto *horizontalBar = m_scrollArea->horizontalScrollBar())
        {
            preservedHorizontalScrollValue = horizontalBar->value();
        }
    }

    clearFlowLayout(m_flowLayout);

    const auto entries = sortedEntries();
    const int visibleCount = m_expanded ? entries.size()
                                        : qMin(kSearchHistoryPopupVisibleCount,
                                               entries.size());

    for (int i = 0; i < visibleCount; ++i)
    {
        const auto &entry = entries.at(i);
        auto *chip = new SearchHistoryChip(m_flowHost);
        chip->setTerm(entry.term);
        chip->setSearchCount(entry.searchCount);
        chip->setHeatLevel(heatLevelForCount(entry.searchCount));

        connect(chip, &QAbstractButton::clicked, this, [this, term = entry.term]() {
            if (!term.isEmpty())
            {
                Q_EMIT termActivated(term);
            }
            dismiss();
        });

        connect(chip, &SearchHistoryChip::removeRequested, this,
                [this](const QString &term) {
                    QTimer::singleShot(0, this, [this, term]() {
                        const QString normalizedTerm = normalizeHistoryTerm(term);
                        m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                                       [&normalizedTerm](const auto &entry) {
                                                           return normalizeHistoryTerm(entry.term) ==
                                                                  normalizedTerm;
                                                       }),
                        m_entries.end());

                        setClearArmed(false);
                        rebuildChips(true);
                        if (!hasContent())
                        {
                            dismiss();
                        }

                        if (!term.isEmpty())
                        {
                            Q_EMIT removeHistoryTermRequested(term);
                        }
                    });
                });
        m_flowLayout->addWidget(chip);
        chip->show();
    }

    refreshControls();
    relayoutPopup();
    if (preserveScrollPosition && m_scrollArea)
    {
        if (auto *verticalBar = m_scrollArea->verticalScrollBar())
        {
            verticalBar->setValue(qBound(verticalBar->minimum(),
                                         preservedVerticalScrollValue,
                                         verticalBar->maximum()));
        }

        if (auto *horizontalBar = m_scrollArea->horizontalScrollBar())
        {
            horizontalBar->setValue(qBound(horizontalBar->minimum(),
                                           preservedHorizontalScrollValue,
                                           horizontalBar->maximum()));
        }
    }
    else
    {
        resetScrollPosition();
    }
}

int SearchHistoryPopup::resolvedCardWidth() const
{
    if (m_anchor)
    {
        return qMax(kSearchHistoryPopupMinWidth, m_anchor->width() + 12);
    }

    if (m_body && m_body->width() > 0)
    {
        return qMax(kSearchHistoryPopupMinWidth, m_body->width());
    }

    if (m_card && m_card->width() > 0)
    {
        return qMax(kSearchHistoryPopupMinWidth, m_card->width());
    }

    if (width() > kSearchHistoryPopupOuterMargin * 2)
    {
        return qMax(kSearchHistoryPopupMinWidth,
                    width() - kSearchHistoryPopupOuterMargin * 2);
    }

    return qMax(kSearchHistoryPopupMinWidth, m_lastCardWidth);
}

void SearchHistoryPopup::relayoutPopup(int preferredCardWidth)
{
    const int requestedCardWidth = preferredCardWidth > 0
                                       ? qMax(kSearchHistoryPopupMinWidth,
                                              preferredCardWidth)
                                       : resolvedCardWidth();
    updateLayoutMetrics(requestedCardWidth);

    if (m_card && m_card->layout())
    {
        m_card->layout()->activate();
    }
    if (layout())
    {
        layout()->activate();
    }

    const int totalWidth = requestedCardWidth + kSearchHistoryPopupOuterMargin * 2;
    const int totalHeight = m_card
                                ? m_card->height() + kSearchHistoryPopupOuterMargin * 2
                                : sizeHint().height();
    resize(totalWidth, totalHeight);

    if (m_card && m_card->layout())
    {
        m_card->layout()->activate();
    }
    if (layout())
    {
        layout()->activate();
    }

    const int actualCardWidth = qMax(kSearchHistoryPopupMinWidth,
                                     width() - kSearchHistoryPopupOuterMargin * 2);
    updateLayoutMetrics(actualCardWidth);
    if (m_card && m_card->layout())
    {
        m_card->layout()->activate();
    }
    if (layout())
    {
        layout()->activate();
    }
    const int actualTotalHeight = m_card
                                      ? m_card->height() + kSearchHistoryPopupOuterMargin * 2
                                      : sizeHint().height();
    resize(actualCardWidth + kSearchHistoryPopupOuterMargin * 2,
           actualTotalHeight);

    m_lastCardWidth = actualCardWidth;

    if (m_animateNextRelayout && isVisible() && m_scrollArea && m_body && m_card &&
        m_heightAnimation)
    {
        m_animationTargetScrollHeight = m_scrollArea->height();
        m_animationTargetBodyHeight = m_body->height();
        m_animationTargetPopupHeight = height();

        const bool hasHeightDelta =
            m_animationStartScrollHeight != m_animationTargetScrollHeight ||
            m_animationStartBodyHeight != m_animationTargetBodyHeight ||
            m_animationStartPopupHeight != m_animationTargetPopupHeight;

        if (hasHeightDelta)
        {
            applyAnimatedHeights(m_animationStartScrollHeight,
                                 m_animationStartBodyHeight,
                                 m_animationStartPopupHeight);
            m_heightAnimation->stop();
            m_heightAnimation->setStartValue(0.0);
            m_heightAnimation->setEndValue(1.0);
            m_heightAnimation->start();
        }
    }
    m_animateNextRelayout = false;

    if (isVisible())
    {
        repositionToAnchor();
        raise();
    }
}

void SearchHistoryPopup::repositionToAnchor()
{
    QWidget *host = parentWidget();
    if (!host || !m_anchor)
    {
        return;
    }

    const int popupWidth = qMax(width(), resolvedCardWidth() +
                                             kSearchHistoryPopupOuterMargin * 2);
    const int popupHeight = qMax(height(), sizeHint().height());
    const QPoint anchorGlobal =
        m_anchor->mapToGlobal(QPoint(m_anchor->width() / 2,
                                     m_anchor->height() +
                                         kSearchHistoryPopupVerticalOffset));
    QPoint popupPos = host->mapFromGlobal(
        QPoint(anchorGlobal.x() - popupWidth / 2, anchorGlobal.y()));

    const QRect hostRect = host->rect();
    popupPos.setX(qBound(6, popupPos.x(), qMax(6, hostRect.width() - popupWidth - 6)));
    popupPos.setY(qBound(0, popupPos.y() + m_visibilityOffsetY,
                         qMax(0, hostRect.height() - popupHeight)));

    move(popupPos);
}

void SearchHistoryPopup::resetScrollPosition()
{
    if (!m_scrollArea)
    {
        return;
    }

    if (auto *verticalBar = m_scrollArea->verticalScrollBar())
    {
        verticalBar->setValue(verticalBar->minimum());
    }

    if (auto *horizontalBar = m_scrollArea->horizontalScrollBar())
    {
        horizontalBar->setValue(horizontalBar->minimum());
    }
}

void SearchHistoryPopup::stopHeightAnimation()
{
    if (m_heightAnimation &&
        m_heightAnimation->state() == QAbstractAnimation::Running)
    {
        m_heightAnimation->stop();
    }
}

void SearchHistoryPopup::stopSortTransitionAnimation()
{
    if (m_sortTransitionAnimation &&
        m_sortTransitionAnimation->state() == QAbstractAnimation::Running)
    {
        m_sortTransitionAnimation->stop();
    }

    m_sortTransitionRebuilt = false;
    if (m_flowOpacityEffect)
    {
        m_flowOpacityEffect->setOpacity(1.0);
    }
}

void SearchHistoryPopup::stopVisibilityAnimation()
{
    if (m_visibilityAnimation &&
        m_visibilityAnimation->state() == QAbstractAnimation::Running)
    {
        m_visibilityAnimation->stop();
    }
}

void SearchHistoryPopup::syncReducedAnimationMode()
{
    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (!reduceAnimations)
    {
        return;
    }

    const bool wasHeightAnimating =
        m_heightAnimation &&
        m_heightAnimation->state() == QAbstractAnimation::Running;
    const bool wasSortAnimating =
        m_sortTransitionAnimation &&
        m_sortTransitionAnimation->state() == QAbstractAnimation::Running;
    const bool needsSortRebuild = wasSortAnimating && !m_sortTransitionRebuilt;
    const bool wasVisibilityAnimating =
        m_visibilityAnimation &&
        m_visibilityAnimation->state() == QAbstractAnimation::Running;
    const bool wasVisibilityHiding = m_visibilityHiding;
    const bool preserveScrollPosition = isVisible();

    stopHeightAnimation();
    stopSortTransitionAnimation();
    stopVisibilityAnimation();

    if (needsSortRebuild)
    {
        rebuildChips(preserveScrollPosition);
    }
    else if (wasHeightAnimating)
    {
        m_animateNextRelayout = false;
        relayoutPopup();
    }

    if (wasVisibilityAnimating || isVisible())
    {
        if (wasVisibilityHiding)
        {
            m_visibilityHiding = false;
            m_visibilityOffsetY = 0;
            if (m_cardOpacityEffect)
            {
                m_cardOpacityEffect->setOpacity(1.0);
                m_cardOpacityEffect->setEnabled(false);
            }
            QFrame::hide();
            m_anchor = nullptr;
            return;
        }

        m_visibilityHiding = false;
        applyVisibilityState(1.0, 0);
        if (m_cardOpacityEffect)
        {
            m_cardOpacityEffect->setOpacity(1.0);
            m_cardOpacityEffect->setEnabled(false);
        }
        raise();
    }
}

void SearchHistoryPopup::applyVisibilityState(qreal opacity, int offsetY)
{
    m_visibilityOffsetY = offsetY;
    if (m_cardOpacityEffect)
    {
        m_cardOpacityEffect->setOpacity(qBound(0.0, opacity, 1.0));
    }

    if (isVisible())
    {
        repositionToAnchor();
    }
}

void SearchHistoryPopup::applyAnimatedHeights(int scrollHeight, int bodyHeight,
                                              int popupHeight)
{
    const int safeScrollHeight = qMax(0, scrollHeight);
    const int safeBodyHeight = qMax(0, bodyHeight);
    const int safePopupHeight =
        qMax(safeBodyHeight + kSearchHistoryPopupOuterMargin * 2, popupHeight);

    if (m_scrollArea)
    {
        m_scrollArea->setFixedHeight(safeScrollHeight);
    }

    if (m_body)
    {
        m_body->setFixedHeight(safeBodyHeight);
    }

    if (m_card)
    {
        m_card->setFixedHeight(safeBodyHeight);
    }

    resize(width(), safePopupHeight);
    if (isVisible())
    {
        repositionToAnchor();
    }
}

void SearchHistoryPopup::updateLayoutMetrics(int cardWidth)
{
    if (!m_flowHost || !m_flowLayout || !m_scrollArea)
    {
        return;
    }

    m_flowLayout->invalidate();

    if (m_header && m_header->layout())
    {
        m_header->layout()->activate();
        m_header->adjustSize();
    }

    const int effectiveCardWidth = qMax(kSearchHistoryPopupMinWidth, cardWidth);
    const bool headerVisible = m_header && !m_entries.isEmpty();
    const int controlWidth = headerVisible ? m_header->sizeHint().width() : 0;
    const int controlHeight = headerVisible ? m_header->sizeHint().height() : 0;
    const bool dividerVisible = headerVisible && m_headerDivider;
    const int dividerWidth = dividerVisible ? m_headerDivider->width() : 0;
    const int dividerHeight = dividerVisible ? m_headerDivider->height() : 0;

    const int firstLineTrailingSpacing =
        headerVisible ? controlWidth + dividerWidth + kSearchHistoryPopupBodyGap * 2 : 0;
    int flowWidth = qMax(220, effectiveCardWidth - kSearchHistoryPopupBodyPadding -
                                  kSearchHistoryPopupContentRightInset);
    m_flowLayout->setFirstLineTrailingSpacing(firstLineTrailingSpacing);

    const int maxHeight = m_expanded ? kSearchHistoryPopupExpandedBodyMaxHeight
                                     : kSearchHistoryPopupCollapsedBodyMaxHeight;
    int contentHeight = m_flowLayout->heightForWidth(flowWidth);
    bool needsScrollBar = (contentHeight + 2) > maxHeight;
    if (needsScrollBar)
    {
        const int adjustedFlowWidth =
            qMax(220, flowWidth - kSearchHistoryPopupEdgeScrollBarWidth - 2);
        if (adjustedFlowWidth != flowWidth)
        {
            flowWidth = adjustedFlowWidth;
            contentHeight = m_flowLayout->heightForWidth(flowWidth);
        }
    }

    const int paddedContentHeight = qMax(0, contentHeight) + 2;
    const int scrollHeight = qMin(paddedContentHeight, maxHeight);
    const int scrollAreaWidth =
        qMax(220, effectiveCardWidth - kSearchHistoryPopupBodyPadding);
    const int bodyHeight =
        kSearchHistoryPopupControlsTop + scrollHeight + kSearchHistoryPopupBodyBottomPadding;

    m_flowHost->setFixedWidth(flowWidth);
    m_flowHost->setFixedHeight(paddedContentHeight);
    if (m_flowHost->layout())
    {
        m_flowHost->layout()->activate();
        m_flowHost->layout()->setGeometry(m_flowHost->rect());
    }
    m_scrollArea->setFixedWidth(scrollAreaWidth);
    m_scrollArea->setFixedHeight(scrollHeight);
    m_scrollArea->setVerticalScrollBarPolicy(
        paddedContentHeight > maxHeight ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    if (m_body)
    {
        m_body->setFixedSize(effectiveCardWidth, bodyHeight);
    }
    if (m_card)
    {
        m_card->setFixedSize(effectiveCardWidth, bodyHeight);
    }

    if (headerVisible)
    {
        const int headerX =
            effectiveCardWidth - kSearchHistoryPopupBodyPadding - controlWidth;
        const int headerY = kSearchHistoryPopupControlsTop;
        m_header->setGeometry(headerX, headerY, controlWidth, controlHeight);
        m_header->raise();
    }

    if (dividerVisible)
    {
        const int dividerX = effectiveCardWidth - kSearchHistoryPopupBodyPadding -
                             controlWidth - kSearchHistoryPopupBodyGap - dividerWidth;
        const int dividerY = kSearchHistoryPopupControlsTop +
                             qMax(0, (controlHeight - dividerHeight) / 2);
        m_headerDivider->move(dividerX, dividerY);
        m_headerDivider->raise();
    }
}

void SearchHistoryPopup::refreshControls()
{
    if (m_recentButton)
    {
        m_recentButton->setChecked(m_sortMode == SortMode::Recent);
        m_recentButton->setToolTip(tr("Sort by recent searches"));
    }

    if (m_hotButton)
    {
        m_hotButton->setChecked(m_sortMode == SortMode::Count);
        m_hotButton->setToolTip(tr("Sort by most searched"));
    }

    if (m_expandButton)
    {
        const bool canExpand = m_entries.size() > kSearchHistoryPopupVisibleCount;
        m_expandButton->setVisible(canExpand);
        m_expandButton->setChecked(m_expanded);
        m_expandButton->setToolTip(m_expanded ? tr("Show Less") : tr("Show More"));
    }

    if (m_clearButton)
    {
        m_clearButton->setVisible(!m_entries.isEmpty());
        m_clearButton->setToolTip(m_clearArmed ? tr("Confirm clear all history")
                                               : tr("Clear search history"));
        m_clearButton->setProperty("armed", m_clearArmed);
        repolishWidget(m_clearButton);
    }

    if (m_headerDivider)
    {
        m_headerDivider->setVisible(!m_entries.isEmpty());
    }

    if (m_header)
    {
        m_header->setVisible(!m_entries.isEmpty());
        if (m_header->layout())
        {
            m_header->layout()->activate();
            m_header->adjustSize();
        }
    }

    updateControlIcons();
}

void SearchHistoryPopup::updateControlIcons()
{
    if (m_recentButton)
    {
        m_recentButton->setIcon(ThemeManager::getAdaptiveIcon(
            QStringLiteral(":/svg/dark/search-history-recent.svg"),
            controlIconColor(m_sortMode == SortMode::Recent)));
    }

    if (m_hotButton)
    {
        m_hotButton->setIcon(ThemeManager::getAdaptiveIcon(
            QStringLiteral(":/svg/dark/search-history-hot.svg"),
            controlIconColor(m_sortMode == SortMode::Count)));
    }

    if (m_clearButton)
    {
        m_clearButton->setIcon(ThemeManager::getAdaptiveIcon(
            m_clearArmed ? QStringLiteral(":/svg/dark/check.svg")
                         : QStringLiteral(":/svg/dark/delete.svg"),
            clearIconColor(m_clearArmed)));
    }

    if (m_expandButton)
    {
        const bool expanded = m_expandButton->isChecked();
        QPixmap pixmap = ThemeManager::getAdaptiveIcon(
                             QStringLiteral(":/svg/dark/chevron-down.svg"),
                             controlIconColor(expanded))
                             .pixmap(13, 13);

        if (expanded)
        {
            QTransform transform;
            transform.rotate(180);
            pixmap = pixmap.transformed(transform, Qt::SmoothTransformation);
        }

        m_expandButton->setIcon(QIcon(pixmap));
    }
}

void SearchHistoryPopup::setClearArmed(bool armed)
{
    if (m_clearArmed == armed)
    {
        refreshControls();
        return;
    }

    m_clearArmed = armed;
    refreshControls();
}

void SearchHistoryPopup::setExpanded(bool expanded)
{
    if (m_expanded == expanded)
    {
        return;
    }

    stopHeightAnimation();

    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    m_animateNextRelayout =
        isVisible() && !reduceAnimations && m_scrollArea && m_body && m_card;
    if (m_animateNextRelayout)
    {
        m_animationStartScrollHeight = m_scrollArea->height();
        m_animationStartBodyHeight = m_body->height();
        m_animationStartPopupHeight = height();
        if (m_heightAnimation)
        {
            m_heightAnimation->setDuration(expanded ? kSearchHistoryPopupExpandDurationMs
                                                   : kSearchHistoryPopupCollapseDurationMs);
            m_heightAnimation->setEasingCurve(expanded ? QEasingCurve::OutCubic
                                                       : QEasingCurve::InOutCubic);
        }
    }

    setClearArmed(false);
    m_expanded = expanded;
    rebuildChips();
}

void SearchHistoryPopup::setSortMode(SortMode mode)
{
    if (m_sortMode == mode)
    {
        refreshControls();
        return;
    }

    stopSortTransitionAnimation();
    setClearArmed(false);
    m_sortMode = mode;
    refreshControls();

    const bool reduceAnimations =
        ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    const bool canAnimate =
        isVisible() && !reduceAnimations && hasContent() && m_sortTransitionAnimation;

    if (!canAnimate)
    {
        rebuildChips();
        return;
    }

    m_sortTransitionRebuilt = false;
    m_sortTransitionAnimation->setStartValue(0.0);
    m_sortTransitionAnimation->setEndValue(1.0);
    m_sortTransitionAnimation->start();
}

QList<SearchHistoryManager::SearchHistoryEntry>
SearchHistoryPopup::sortedEntries() const
{
    auto entries = m_entries;
    std::sort(entries.begin(), entries.end(),
              [this](const SearchHistoryManager::SearchHistoryEntry &lhs,
                     const SearchHistoryManager::SearchHistoryEntry &rhs) {
                  if (m_sortMode == SortMode::Count)
                  {
                      if (lhs.searchCount != rhs.searchCount)
                      {
                          return lhs.searchCount > rhs.searchCount;
                      }
                      if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
                      {
                          return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
                      }
                      return lhs.term.localeAwareCompare(rhs.term) < 0;
                  }

                  if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
                  {
                      return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
                  }
                  if (lhs.searchCount != rhs.searchCount)
                  {
                      return lhs.searchCount > rhs.searchCount;
                  }
                  return lhs.term.localeAwareCompare(rhs.term) < 0;
              });
    return entries;
}

QString SearchHistoryPopup::heatLevelForCount(int count)
{
    if (count >= 10)
    {
        return QStringLiteral("top");
    }
    if (count >= 6)
    {
        return QStringLiteral("high");
    }
    if (count >= 3)
    {
        return QStringLiteral("mid");
    }
    return QStringLiteral("low");
}
