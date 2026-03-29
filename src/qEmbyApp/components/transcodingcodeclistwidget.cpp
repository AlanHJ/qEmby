#include "transcodingcodeclistwidget.h"

#include "transcodingcodecrowwidget.h"

#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

namespace {

void stopPosAnimations(TranscodingCodecRowWidget* row) {
    const auto children = row->children();
    for (auto* child : children) {
        if (auto* animation = qobject_cast<QPropertyAnimation*>(child)) {
            if (animation->propertyName() == "pos") {
                animation->stop();
                animation->deleteLater();
            }
        }
    }
}

} 

TranscodingCodecListWidget::TranscodingCodecListWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(16);
    connect(m_autoScrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_dragRow) {
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
            updateDragPosition(m_dragRow, m_lastDragGlobalPos);
        }
    });
}

void TranscodingCodecListWidget::setCodecRows(
    const QList<TranscodingCodecRowWidget*>& rows) {
    for (auto* row : m_rows) {
        if (row && !rows.contains(row)) {
            row->hide();
            row->deleteLater();
        }
    }

    m_rows = rows;
    for (auto* row : m_rows) {
        if (!row) {
            continue;
        }

        row->setParent(this);
        row->show();
        connect(row, &TranscodingCodecRowWidget::dragStarted, this,
                &TranscodingCodecListWidget::onRowDragStart, Qt::UniqueConnection);
        connect(row, &TranscodingCodecRowWidget::dragMoved, this,
                &TranscodingCodecListWidget::onRowDragMove, Qt::UniqueConnection);
        connect(row, &TranscodingCodecRowWidget::dragFinished, this,
                &TranscodingCodecListWidget::onRowDragFinish, Qt::UniqueConnection);
    }

    relayout(false);
}

QStringList TranscodingCodecListWidget::codecOrder() const {
    QStringList codecIds;
    codecIds.reserve(m_rows.size());
    for (auto* row : m_rows) {
        if (row) {
            codecIds.append(row->codecId());
        }
    }
    return codecIds;
}

void TranscodingCodecListWidget::onRowDragStart(TranscodingCodecRowWidget* row,
                                                const QPoint& globalPos) {
    if (!row) {
        return;
    }

    m_dragRow = row;
    m_dragOriginalIndex = m_rows.indexOf(row);
    if (m_dragOriginalIndex < 0) {
        m_dragRow = nullptr;
        return;
    }
    m_dragCurrentIndex = m_dragOriginalIndex;
    m_lastDragGlobalPos = globalPos;
    m_dragHotSpotY = row->mapFromGlobal(globalPos).y();

    for (auto* currentRow : m_rows) {
        if (currentRow) {
            stopPosAnimations(currentRow);
        }
    }

    row->setDragging(true);
    row->raise();
    m_autoScrollTimer->start();
}

void TranscodingCodecListWidget::onRowDragMove(TranscodingCodecRowWidget* row,
                                               const QPoint& globalPos) {
    if (!row || row != m_dragRow) {
        return;
    }

    m_lastDragGlobalPos = globalPos;
    updateDragPosition(row, globalPos);
}

void TranscodingCodecListWidget::onRowDragFinish(TranscodingCodecRowWidget* row) {
    if (!row || row != m_dragRow) {
        return;
    }

    row->setDragging(false);
    m_autoScrollTimer->stop();

    const int finalIndex = qMax(0, m_dragCurrentIndex);
    const bool changed = (m_dragOriginalIndex != m_dragCurrentIndex);

    m_dragRow = nullptr;
    m_dragOriginalIndex = -1;
    m_dragCurrentIndex = -1;
    m_dragHotSpotY = 0;

    stopPosAnimations(row);
    auto* animation = new QPropertyAnimation(row, "pos", row);
    animation->setDuration(180);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setEndValue(QPoint(0, rowTopForIndex(finalIndex)));
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    if (changed) {
        emit orderChanged(codecOrder());
    }
}

void TranscodingCodecListWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout(false);
}

int TranscodingCodecListWidget::rowHeightFor(TranscodingCodecRowWidget* row) const {
    return row ? row->sizeHint().height() : 0;
}

int TranscodingCodecListWidget::rowTopForIndex(int index) const {
    int top = 0;
    for (int i = 0; i < index && i < m_rows.size(); ++i) {
        top += rowHeightFor(m_rows.at(i)) + RowSpacing;
    }
    return top;
}

int TranscodingCodecListWidget::indexForCenterY(int centerY) const {
    QList<TranscodingCodecRowWidget*> orderedRows = m_rows;
    orderedRows.removeOne(m_dragRow);

    int top = 0;
    for (int index = 0; index < orderedRows.size(); ++index) {
        const int rowHeight = rowHeightFor(orderedRows.at(index));
        if (centerY < top + rowHeight / 2) {
            return index;
        }
        top += rowHeight + RowSpacing;
    }

    return orderedRows.size();
}

QScrollArea* TranscodingCodecListWidget::parentScrollArea() const {
    QWidget* current = parentWidget();
    while (current) {
        if (auto* scrollArea = qobject_cast<QScrollArea*>(current)) {
            return scrollArea;
        }
        current = current->parentWidget();
    }

    return nullptr;
}

int TranscodingCodecListWidget::autoScrollDelta(const QPoint& globalPos) const {
    auto* scrollArea = parentScrollArea();
    if (!scrollArea) {
        return 0;
    }

    auto* scrollBar = scrollArea->verticalScrollBar();
    if (!scrollBar || scrollBar->maximum() <= scrollBar->minimum()) {
        return 0;
    }

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

void TranscodingCodecListWidget::updateDragPosition(TranscodingCodecRowWidget* row,
                                                    const QPoint& globalPos) {
    if (!row) {
        return;
    }

    const QPoint localPos = mapFromGlobal(globalPos);
    const int rowY = localPos.y() - m_dragHotSpotY;
    row->move(0, rowY);

    const int centerY = rowY + row->height() / 2;
    const int targetIndex = indexForCenterY(centerY);
    if (targetIndex == m_dragCurrentIndex) {
        return;
    }

    m_rows.removeOne(row);
    m_rows.insert(qBound(0, targetIndex, m_rows.size()), row);
    m_dragCurrentIndex = m_rows.indexOf(row);
    relayout(true);
}

void TranscodingCodecListWidget::relayout(bool animate) {
    int top = 0;
    const int contentWidth = qMax(0, width());

    for (int index = 0; index < m_rows.size(); ++index) {
        auto* row = m_rows.at(index);
        if (!row) {
            continue;
        }

        const int rowHeight = rowHeightFor(row);
        row->setFixedSize(contentWidth, rowHeight);

        if (row == m_dragRow) {
            top += rowHeight + RowSpacing;
            continue;
        }

        const QPoint targetPos(0, top);
        if (animate && row->pos() != targetPos) {
            stopPosAnimations(row);

            auto* animation = new QPropertyAnimation(row, "pos", row);
            animation->setDuration(220);
            animation->setEasingCurve(QEasingCurve::OutCubic);
            animation->setEndValue(targetPos);
            animation->start(QAbstractAnimation::DeleteWhenStopped);
        } else {
            stopPosAnimations(row);
            row->move(targetPos);
        }

        top += rowHeight + RowSpacing;
    }

    if (top > 0) {
        top -= RowSpacing;
    }
    setFixedHeight(qMax(top, 0));
}
