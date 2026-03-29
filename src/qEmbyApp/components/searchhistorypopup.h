#ifndef SEARCHHISTORYPOPUP_H
#define SEARCHHISTORYPOPUP_H

#include "../managers/searchhistorymanager.h"
#include <QFrame>
#include <QList>

class FlowLayout;
class QGraphicsOpacityEffect;
class QScrollArea;
class QToolButton;
class QVariantAnimation;
class QWidget;

class SearchHistoryPopup : public QFrame {
    Q_OBJECT
public:
    explicit SearchHistoryPopup(QWidget *parent = nullptr);

    void setEntries(QList<SearchHistoryManager::SearchHistoryEntry> entries);
    bool hasContent() const;
    void showBelow(QWidget *anchor);
    void dismiss(bool immediate = false);

Q_SIGNALS:
    void termActivated(const QString &term);
    void clearHistoryRequested();
    void removeHistoryTermRequested(const QString &term);

private:
    enum class SortMode {
        Recent,
        Count
    };

    void rebuildChips(bool preserveScrollPosition = false);
    int resolvedCardWidth() const;
    void relayoutPopup(int preferredCardWidth = -1);
    void repositionToAnchor();
    void resetScrollPosition();
    void stopHeightAnimation();
    void stopSortTransitionAnimation();
    void stopVisibilityAnimation();
    void syncReducedAnimationMode();
    void applyVisibilityState(qreal opacity, int offsetY);
    void applyAnimatedHeights(int scrollHeight, int bodyHeight, int popupHeight);
    void updateLayoutMetrics(int cardWidth);
    void refreshControls();
    void updateControlIcons();
    void setClearArmed(bool armed);
    void setExpanded(bool expanded);
    void setSortMode(SortMode mode);
    QList<SearchHistoryManager::SearchHistoryEntry> sortedEntries() const;
    static QString heatLevelForCount(int count);

    QFrame *m_card = nullptr;
    QWidget *m_body = nullptr;
    QWidget *m_header = nullptr;
    QFrame *m_headerDivider = nullptr;
    QToolButton *m_recentButton = nullptr;
    QToolButton *m_hotButton = nullptr;
    QToolButton *m_clearButton = nullptr;
    QToolButton *m_expandButton = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_flowHost = nullptr;
    FlowLayout *m_flowLayout = nullptr;
    QGraphicsOpacityEffect *m_cardOpacityEffect = nullptr;
    QGraphicsOpacityEffect *m_flowOpacityEffect = nullptr;
    QVariantAnimation *m_heightAnimation = nullptr;
    QVariantAnimation *m_sortTransitionAnimation = nullptr;
    QVariantAnimation *m_visibilityAnimation = nullptr;
    QWidget *m_anchor = nullptr;
    int m_lastCardWidth = 0;
    int m_animationStartScrollHeight = 0;
    int m_animationStartBodyHeight = 0;
    int m_animationStartPopupHeight = 0;
    int m_animationTargetScrollHeight = 0;
    int m_animationTargetBodyHeight = 0;
    int m_animationTargetPopupHeight = 0;
    qreal m_visibilityStartOpacity = 1.0;
    qreal m_visibilityTargetOpacity = 1.0;
    int m_visibilityOffsetY = 0;
    int m_visibilityStartOffsetY = 0;
    int m_visibilityTargetOffsetY = 0;
    bool m_visibilityHiding = false;
    bool m_animateNextRelayout = false;
    bool m_sortTransitionRebuilt = false;

    QList<SearchHistoryManager::SearchHistoryEntry> m_entries;
    SortMode m_sortMode = SortMode::Recent;
    bool m_expanded = false;
    bool m_clearArmed = false;
};

#endif 
