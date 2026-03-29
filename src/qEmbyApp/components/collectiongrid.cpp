#include "collectiongrid.h"
#include "collectioncard.h"

#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QTimer>

namespace {

void stopPosAnimations(CollectionCard* card)
{
    const auto children = card->children();
    for (auto* child : children) {
        if (auto* anim = qobject_cast<QPropertyAnimation*>(child)) {
            if (anim->propertyName() == "pos") {
                anim->stop();
                anim->deleteLater();
            }
        }
    }
}

} 




CollectionGrid::CollectionGrid(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(100);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(16);
    connect(m_autoScrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_dragCard) {
            m_autoScrollTimer->stop();
            return;
        }

        auto* scrollArea = parentScrollArea();
        if (!scrollArea) {
            return;
        }

        auto* scrollBar = scrollArea->verticalScrollBar();
        const int delta = autoScrollDelta(m_lastDragGlobalPos);
        if (delta == 0) {
            return;
        }

        const int oldValue = scrollBar->value();
        scrollBar->setValue(
            qBound(scrollBar->minimum(), oldValue + delta, scrollBar->maximum()));
        if (scrollBar->value() != oldValue) {
            updateDragPosition(m_dragCard, m_lastDragGlobalPos);
        }
    });
}

int CollectionGrid::cardHeightForWidth(int width) const
{
    const int imageSide = qMax(0, width - 8);
    const int footerHeight = 84;
    return qMax(CardHeight, imageSide + footerHeight);
}

void CollectionGrid::setCards(const QList<CollectionCard*>& cards)
{
    m_dragCard = nullptr;
    m_dragOriginalIndex = -1;
    m_dragCurrentIndex = -1;
    if (m_autoScrollTimer) {
        m_autoScrollTimer->stop();
    }

    QSet<CollectionCard*> retainedCards;
    retainedCards.reserve(cards.size());
    for (auto* card : cards) {
        retainedCards.insert(card);
    }

    for (auto* card : m_cards) {
        if (!retainedCards.contains(card)) {
            card->hide();
            card->deleteLater();
        }
    }

    m_cards = cards;
    for (auto* card : m_cards) {
        card->setParent(this);
        card->show();
    }
    relayout();
}

int CollectionGrid::columnsForWidth(int w) const
{
    if (w <= 0) return 1;
    const int cw = qMax(0, w - LeftPadding - RightPadding);
    int cols = (cw + CardSpacing) / (CardWidth + CardSpacing);
    return qMax(1, cols);
}

int CollectionGrid::contentWidth() const
{
    return qMax(0, width() - LeftPadding - RightPadding);
}

int CollectionGrid::cardWidthForColumn(int column, int columns) const
{
    if (columns <= 0) return CardWidth;

    const int availableWidth =
        qMax(0, contentWidth() - (columns - 1) * CardSpacing);
    const int baseWidth = qMax(1, availableWidth / columns);
    const int extraPixels = qMax(0, availableWidth % columns);
    return baseWidth + (column < extraPixels ? 1 : 0);
}

int CollectionGrid::columnX(int column, int columns) const
{
    int x = LeftPadding;
    for (int i = 0; i < column; ++i) {
        x += cardWidthForColumn(i, columns) + CardSpacing;
    }
    return x;
}

int CollectionGrid::columnForX(int x, int columns) const
{
    if (columns <= 0) return 0;

    for (int col = 0; col < columns; ++col) {
        const int rightEdge = columnX(col, columns) + cardWidthForColumn(col, columns);
        if (col == columns - 1 || x < rightEdge + CardSpacing / 2) {
            return col;
        }
    }

    return columns - 1;
}

QPoint CollectionGrid::posForIndex(int index, int columns) const
{
    if (columns <= 0) columns = 1;

    const int col = index % columns;
    const int row = index / columns;
    return QPoint(columnX(col, columns), row * (m_rowHeight + CardSpacing));
}

int CollectionGrid::indexForPos(const QPoint& pos, int columns) const
{
    if (m_cards.isEmpty()) return 0;

    const int col = columnForX(pos.x(), columns);
    const int row = qMax(0, pos.y() / (m_rowHeight + CardSpacing));
    const int idx = row * columns + col;
    return qBound(0, idx, m_cards.size() - 1);
}

QScrollArea* CollectionGrid::parentScrollArea() const
{
    QWidget* current = parentWidget();
    while (current) {
        if (auto* scrollArea = qobject_cast<QScrollArea*>(current)) {
            return scrollArea;
        }
        current = current->parentWidget();
    }

    return nullptr;
}

int CollectionGrid::autoScrollDelta(const QPoint& globalPos) const
{
    auto* scrollArea = parentScrollArea();
    if (!scrollArea) return 0;

    auto* scrollBar = scrollArea->verticalScrollBar();
    if (!scrollBar || scrollBar->maximum() <= scrollBar->minimum()) return 0;

    const QRect viewportRect(scrollArea->viewport()->mapToGlobal(QPoint(0, 0)),
                             scrollArea->viewport()->size());
    static constexpr int EdgeThreshold = 56;
    static constexpr int MaxScrollStep = 22;

    const int topHotZone = viewportRect.top() + EdgeThreshold;
    if (globalPos.y() < topHotZone) {
        const int distance = topHotZone - globalPos.y();
        return -qBound(4, distance / 2, MaxScrollStep);
    }

    const int bottomHotZone = viewportRect.bottom() - EdgeThreshold;
    if (globalPos.y() > bottomHotZone) {
        const int distance = globalPos.y() - bottomHotZone;
        return qBound(4, distance / 2, MaxScrollStep);
    }

    return 0;
}

void CollectionGrid::updateDragPosition(CollectionCard* card, const QPoint& globalPos)
{
    QPoint localPos =
        mapFromGlobal(globalPos) - QPoint(card->width() / 2, card->height() / 2);
    card->move(localPos);

    const QPoint center = mapFromGlobal(globalPos);
    int targetIndex = indexForPos(center, m_columns);

    if (targetIndex != m_dragCurrentIndex &&
        targetIndex >= 0 && targetIndex < m_cards.size()) {
        m_cards.removeOne(card);
        targetIndex = qBound(0, targetIndex, m_cards.size());
        m_cards.insert(targetIndex, card);
        m_dragCurrentIndex = targetIndex;
        relayout(true);
    }
}

void CollectionGrid::relayout(bool animate)
{
    m_columns = columnsForWidth(width());
    const int availableWidth =
        qMax(0, contentWidth() - (m_columns - 1) * CardSpacing);
    const int baseWidth = qMax(1, availableWidth / m_columns);
    const int extraPixels = qMax(0, availableWidth % m_columns);
    const int maxCardWidth = baseWidth + (extraPixels > 0 ? 1 : 0);

    m_rowHeight = cardHeightForWidth(maxCardWidth);

    int rows = m_cards.isEmpty() ? 0 : (m_cards.size() + m_columns - 1) / m_columns;
    int totalH = rows * (m_rowHeight + CardSpacing) - (rows > 0 ? CardSpacing : 0);
    setMinimumHeight(qMax(totalH, 100));

    for (int i = 0; i < m_cards.size(); ++i) {
        auto* card = m_cards[i];
        const int col = i % m_columns;
        const int cardW = baseWidth + (col < extraPixels ? 1 : 0);
        card->setFixedSize(cardW, m_rowHeight);
        if (card == m_dragCard) {
            continue;
        }

        QPoint target = posForIndex(i, m_columns);
        if (animate && card->pos() != target) {
            stopPosAnimations(card);

            auto* anim = new QPropertyAnimation(card, "pos", card);
            anim->setDuration(220);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->setEndValue(target);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            stopPosAnimations(card);
            card->move(target);
        }
    }
}

void CollectionGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    relayout(false);
}

void CollectionGrid::onCardDragStart(CollectionCard* card, const QPoint& globalPos)
{
    if (!card) {
        return;
    }

    m_dragCard = card;
    m_dragOriginalIndex = m_cards.indexOf(card);
    m_dragCurrentIndex = m_dragOriginalIndex;
    m_lastDragGlobalPos = globalPos;

    for (auto* currentCard : m_cards) {
        stopPosAnimations(currentCard);
    }

    card->setDragging(true);
    card->raise();
    m_autoScrollTimer->start();
}

void CollectionGrid::onCardDragMove(CollectionCard* card, const QPoint& globalPos)
{
    if (!card || card != m_dragCard) {
        return;
    }

    m_lastDragGlobalPos = globalPos;
    updateDragPosition(card, globalPos);
}

void CollectionGrid::onCardDragEnd(CollectionCard* card)
{
    if (!card) {
        return;
    }

    card->setDragging(false);
    m_autoScrollTimer->stop();

    CollectionCard* dragCard = m_dragCard;
    const int finalIndex = qMax(0, m_dragCurrentIndex);
    const bool changed = (m_dragOriginalIndex != m_dragCurrentIndex);

    m_dragCard = nullptr;
    m_dragOriginalIndex = -1;
    m_dragCurrentIndex = -1;

    stopPosAnimations(card);
    const QPoint target = posForIndex(finalIndex, m_columns);
    auto* anim = new QPropertyAnimation(card, "pos", card);
    anim->setDuration(180);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setEndValue(target);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    if (changed && dragCard) {
        QStringList newOrderIds;
        for (auto* currentCard : m_cards) {
            newOrderIds.append(currentCard->itemId());
        }
        emit orderChanged(newOrderIds);
    }
}
