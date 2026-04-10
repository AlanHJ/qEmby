#include "dashboardsectionorderstrip.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStyle>

namespace {
constexpr int StripHeight = 36;
constexpr int ChipHeight = 28;
constexpr int ChipSpacing = 8;
constexpr int ChipHorizontalPadding = 12;
constexpr int DragPreviewYOffset = 5;
constexpr int ChipAnimationDuration = 220;
constexpr qreal DragPreviewOpacity = 0.82;
}

DashboardSectionOrderStrip::DashboardSectionOrderStrip(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("DashboardSectionOrderStrip");
    setFrameShape(QFrame::NoFrame);
    setMouseTracking(true);
    setMinimumHeight(StripHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_dropIndicator = new QFrame(this);
    m_dropIndicator->setObjectName("DashboardSectionOrderDropIndicator");
    m_dropIndicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dropIndicator->setFixedHeight(ChipHeight);
    m_dropIndicator->hide();

    m_dragPreview = new QFrame(this);
    m_dragPreview->setObjectName("DashboardSectionOrderDragPreview");
    m_dragPreview->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dragPreview->hide();

    auto* previewLayout = new QHBoxLayout(m_dragPreview);
    previewLayout->setContentsMargins(ChipHorizontalPadding, 0,
                                      ChipHorizontalPadding, 0);
    previewLayout->setSpacing(0);

    m_dragPreviewLabel = new QLabel(m_dragPreview);
    m_dragPreviewLabel->setObjectName("DashboardSectionOrderDragPreviewLabel");
    m_dragPreviewLabel->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(m_dragPreviewLabel);

    m_dragPreviewOpacity = new QGraphicsOpacityEffect(m_dragPreview);
    m_dragPreviewOpacity->setOpacity(DragPreviewOpacity);
    m_dragPreview->setGraphicsEffect(m_dragPreviewOpacity);
}

void DashboardSectionOrderStrip::setSectionOrder(
    const QStringList& order, const QHash<QString, QString>& titles)
{
    if (m_dragging || m_pressedChip) {
        resetDragState(false);
    }

    m_titles = titles;
    m_order = order;
    rebuildChips();
}

QStringList DashboardSectionOrderStrip::sectionOrder() const
{
    return m_order;
}

QSize DashboardSectionOrderStrip::sizeHint() const
{
    return QSize(fullContentWidth(), StripHeight);
}

QSize DashboardSectionOrderStrip::minimumSizeHint() const
{
    return QSize(0, StripHeight);
}

bool DashboardSectionOrderStrip::eventFilter(QObject* watched, QEvent* event)
{
    auto* chip = qobject_cast<QFrame*>(watched);
    if (!chip) {
        return QFrame::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }

        m_pressedChip = chip;
        m_pressGlobalPos = mouseEvent->globalPosition().toPoint();
        m_pressChipOffset = mouseEvent->position().toPoint();
        chip->setCursor(Qt::ClosedHandCursor);
        return true;
    }
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!m_pressedChip || !(mouseEvent->buttons() & Qt::LeftButton)) {
            break;
        }

        const QPoint globalPos = mouseEvent->globalPosition().toPoint();
        if (!m_dragging &&
            (globalPos - m_pressGlobalPos).manhattanLength() >=
                QApplication::startDragDistance()) {
            beginDrag(m_pressedChip);
        }

        if (m_dragging && chip == m_draggedChip) {
            updateDrag(globalPos);
        }
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            break;
        }

        if (m_dragging && chip == m_draggedChip) {
            finishDrag();
        } else {
            chip->setCursor(Qt::OpenHandCursor);
            resetDragState();
        }
        return true;
    }
    case QEvent::Leave:
        if (!m_dragging) {
            chip->setCursor(Qt::OpenHandCursor);
        }
        break;
    default:
        break;
    }

    return QFrame::eventFilter(watched, event);
}

QFrame* DashboardSectionOrderStrip::createChip(const QString& title)
{
    auto* chip = new QFrame(this);
    chip->setObjectName("DashboardSectionOrderChip");
    chip->setCursor(Qt::OpenHandCursor);
    chip->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    chip->installEventFilter(this);

    auto* layout = new QHBoxLayout(chip);
    layout->setContentsMargins(ChipHorizontalPadding, 0, ChipHorizontalPadding,
                               0);
    layout->setSpacing(0);

    auto* label = new QLabel(title, chip);
    label->setObjectName("DashboardSectionOrderChipLabel");
    label->setAlignment(Qt::AlignCenter);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(label);

    chip->setFixedSize(
        qMax(68, label->sizeHint().width() + ChipHorizontalPadding * 2),
        ChipHeight);
    return chip;
}

void DashboardSectionOrderStrip::rebuildChips()
{
    for (ChipItem& item : m_chipItems) {
        if (item.chip) {
            item.chip->deleteLater();
        }
    }
    m_chipItems.clear();

    for (const QString& sectionId : m_order) {
        const QString title = m_titles.value(sectionId, sectionId);
        QFrame* chip = createChip(title);
        m_chipItems.append({sectionId, chip});
    }

    relayoutChips(false);
    updateGeometry();
}

int DashboardSectionOrderStrip::indexOfChip(const QFrame* chip) const
{
    for (int index = 0; index < m_chipItems.size(); ++index) {
        if (m_chipItems[index].chip == chip) {
            return index;
        }
    }

    return -1;
}

int DashboardSectionOrderStrip::insertionIndexForPosition(int localX) const
{
    int insertionIndex = 0;

    for (const ChipItem& item : m_chipItems) {
        if (!item.chip || item.chip == m_draggedChip) {
            continue;
        }

        if (localX < item.chip->geometry().center().x()) {
            return insertionIndex;
        }

        ++insertionIndex;
    }

    return insertionIndex;
}

void DashboardSectionOrderStrip::beginDrag(QFrame* chip)
{
    m_draggedChip = chip;
    m_dragSourceIndex = indexOfChip(chip);
    if (m_dragSourceIndex < 0) {
        resetDragState();
        return;
    }

    m_dragging = true;
    setDraggingChip(chip, true);
    if (m_dragPreview && m_dragPreviewLabel) {
        if (auto* label =
                chip->findChild<QLabel*>("DashboardSectionOrderChipLabel")) {
            m_dragPreviewLabel->setText(label->text());
        } else {
            m_dragPreviewLabel->clear();
        }
        m_dragPreview->setFixedSize(chip->size());
        m_dragPreview->show();
        m_dragPreview->raise();
        updateDragPreview(m_pressGlobalPos);
    }
    grabMouse();
    chip->hide();
    setIndicatorIndex(m_dragSourceIndex, true);
}

void DashboardSectionOrderStrip::updateDrag(const QPoint& globalPos)
{
    const QPoint localPos = mapFromGlobal(globalPos);
    updateDragPreview(globalPos);
    setIndicatorIndex(insertionIndexForPosition(localPos.x()));
}

void DashboardSectionOrderStrip::updateDragPreview(const QPoint& globalPos)
{
    if (!m_dragPreview || !m_dragPreview->isVisible()) {
        return;
    }

    const QPoint localPos = mapFromGlobal(globalPos);
    const int previewWidth = m_dragPreview->width();
    const int previewHeight = m_dragPreview->height();
    int x = localPos.x() - m_pressChipOffset.x();
    if (m_dropIndicator->isVisible()) {
        const int indicatorCenterX = m_dropIndicator->geometry().center().x();
        const int previewCenterX = x + previewWidth / 2;
        const int snappedCenterX =
            qRound(previewCenterX * 0.7 + indicatorCenterX * 0.3);
        x = snappedCenterX - previewWidth / 2;
    }
    x = qBound(0, x, qMax(0, width() - previewWidth));

    const int y = qBound(0, chipY() - DragPreviewYOffset,
                         qMax(0, height() - previewHeight));

    m_dragPreview->move(x, y);
    m_dragPreview->raise();
}

void DashboardSectionOrderStrip::finishDrag()
{
    if (!m_dragging || m_dragSourceIndex < 0 || !m_draggedChip) {
        resetDragState();
        return;
    }

    QStringList newOrder = m_order;
    const QString draggedSectionId = newOrder.takeAt(m_dragSourceIndex);
    const int targetIndex =
        qBound(0, m_indicatorIndex, newOrder.size());
    newOrder.insert(targetIndex, draggedSectionId);

    const bool changed = newOrder != m_order;
    ChipItem draggedItem = m_chipItems.takeAt(m_dragSourceIndex);
    m_chipItems.insert(targetIndex, draggedItem);
    m_order = newOrder;

    resetDragState(true);

    if (changed) {
        emit sectionOrderChanged(m_order);
    }
}

void DashboardSectionOrderStrip::resetDragState(bool animateDraggedChip)
{
    if (QWidget::mouseGrabber() == this) {
        releaseMouse();
    }

    QPointer<QFrame> draggedChip = m_draggedChip;
    const QPoint dragPreviewPos =
        m_dragPreview ? m_dragPreview->pos() : QPoint();

    if (m_draggedChip) {
        setDraggingChip(m_draggedChip, false);
    }
    if (draggedChip) {
        if (animateDraggedChip && m_dragPreview && m_dragPreview->isVisible()) {
            draggedChip->move(dragPreviewPos);
            draggedChip->raise();
        }
        draggedChip->show();
        draggedChip->setCursor(Qt::OpenHandCursor);
    }

    if (m_pressedChip) {
        m_pressedChip->setCursor(Qt::OpenHandCursor);
    }

    if (m_dragPreview) {
        m_dragPreview->hide();
    }

    m_pressedChip.clear();
    m_draggedChip.clear();
    m_dragSourceIndex = -1;
    m_indicatorIndex = -1;
    m_dragging = false;
    relayoutChips(animateDraggedChip);
}

void DashboardSectionOrderStrip::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging) {
        updateDrag(event->globalPosition().toPoint());
        event->accept();
        return;
    }

    QFrame::mouseMoveEvent(event);
}

void DashboardSectionOrderStrip::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        finishDrag();
        event->accept();
        return;
    }

    QFrame::mouseReleaseEvent(event);
}

void DashboardSectionOrderStrip::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    relayoutChips(false);
}

void DashboardSectionOrderStrip::setDraggingChip(QFrame* chip, bool dragging)
{
    if (!chip) {
        return;
    }

    chip->setProperty("dragging", dragging);
    chip->style()->unpolish(chip);
    chip->style()->polish(chip);
    chip->update();
}

void DashboardSectionOrderStrip::setIndicatorIndex(int index, bool animate)
{
    const int maxIndex = m_dragging ? qMax(0, m_chipItems.size() - 1)
                                    : m_chipItems.size();
    const int adjustedIndex = qBound(0, index, maxIndex);

    if (m_indicatorIndex == adjustedIndex && m_dropIndicator->isVisible()) {
        return;
    }

    m_indicatorIndex = adjustedIndex;
    relayoutChips(animate);
}

void DashboardSectionOrderStrip::relayoutChips(bool animate)
{
    if (m_chipItems.isEmpty()) {
        if (m_dropIndicator) {
            stopPosAnimations(m_dropIndicator);
            m_dropIndicator->hide();
        }
        return;
    }

    const bool showDropSlot =
        m_dragging && m_draggedChip && m_indicatorIndex >= 0;
    const int slotWidth =
        (showDropSlot && m_draggedChip) ? m_draggedChip->width() : 0;
    const int layoutWidth = showDropSlot ? fullContentWidth()
                                         : contentWidthForCurrentState();
    const int startX = qMax(0, width() - layoutWidth);
    int x = startX;
    int visibleIndex = 0;
    bool indicatorPlaced = false;

    for (int index = 0; index < m_chipItems.size(); ++index) {
        auto* chip = m_chipItems[index].chip;
        if (!chip || (m_dragging && chip == m_draggedChip)) {
            continue;
        }

        if (showDropSlot && m_indicatorIndex == visibleIndex) {
            m_dropIndicator->setFixedSize(slotWidth, ChipHeight);
            m_dropIndicator->show();
            moveWidgetTo(m_dropIndicator, QPoint(x, indicatorY()), animate,
                         ChipAnimationDuration);
            m_dropIndicator->raise();
            indicatorPlaced = true;
            x += slotWidth + ChipSpacing;
        }

        chip->show();
        moveWidgetTo(chip, QPoint(x, chipY()), animate, ChipAnimationDuration);
        x += chip->width();

        const bool hasTrailingVisibleChip = [&]() {
            for (int next = index + 1; next < m_chipItems.size(); ++next) {
                if (!m_chipItems[next].chip ||
                    (m_dragging && m_chipItems[next].chip == m_draggedChip)) {
                    continue;
                }
                return true;
            }
            return false;
        }();

        if (hasTrailingVisibleChip || (showDropSlot && m_indicatorIndex > visibleIndex)) {
            x += ChipSpacing;
        }

        ++visibleIndex;
    }

    if (showDropSlot && !indicatorPlaced) {
        m_dropIndicator->setFixedSize(slotWidth, ChipHeight);
        m_dropIndicator->show();
        moveWidgetTo(m_dropIndicator, QPoint(x, indicatorY()), animate,
                     ChipAnimationDuration);
        m_dropIndicator->raise();
        indicatorPlaced = true;
    }

    if (!indicatorPlaced) {
        stopPosAnimations(m_dropIndicator);
        m_dropIndicator->hide();
    }

    if (m_dragPreview && m_dragPreview->isVisible()) {
        m_dragPreview->raise();
    }
}

void DashboardSectionOrderStrip::moveWidgetTo(QWidget* widget,
                                              const QPoint& targetPos,
                                              bool animate, int duration)
{
    if (!widget) {
        return;
    }

    stopPosAnimations(widget);
    if (!animate || widget->pos() == targetPos) {
        widget->move(targetPos);
        return;
    }

    auto* animation = new QPropertyAnimation(widget, "pos", widget);
    animation->setDuration(duration);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setEndValue(targetPos);
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}

void DashboardSectionOrderStrip::stopPosAnimations(QWidget* widget) const
{
    if (!widget) {
        return;
    }

    const auto children = widget->children();
    for (auto* child : children) {
        auto* animation = qobject_cast<QPropertyAnimation*>(child);
        if (!animation || animation->propertyName() != "pos") {
            continue;
        }

        animation->stop();
        animation->deleteLater();
    }
}

int DashboardSectionOrderStrip::fullContentWidth() const
{
    if (m_chipItems.isEmpty()) {
        return 0;
    }

    int totalWidth = 0;
    for (const ChipItem& item : m_chipItems) {
        if (item.chip) {
            totalWidth += item.chip->width();
        }
    }

    totalWidth += ChipSpacing * qMax(0, m_chipItems.size() - 1);
    return totalWidth;
}

int DashboardSectionOrderStrip::contentWidthForCurrentState() const
{
    int totalWidth = 0;
    int visibleCount = 0;
    for (const ChipItem& item : m_chipItems) {
        if (!item.chip || (m_dragging && item.chip == m_draggedChip)) {
            continue;
        }

        totalWidth += item.chip->width();
        ++visibleCount;
    }

    totalWidth += ChipSpacing * qMax(0, visibleCount - 1);
    return totalWidth;
}

int DashboardSectionOrderStrip::chipY() const
{
    return qMax(0, (height() - ChipHeight) / 2);
}

int DashboardSectionOrderStrip::indicatorY() const
{
    return chipY();
}
