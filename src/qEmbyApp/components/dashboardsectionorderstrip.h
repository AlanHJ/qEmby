#ifndef DASHBOARDSECTIONORDERSTRIP_H
#define DASHBOARDSECTIONORDERSTRIP_H

#include <QFrame>
#include <QHash>
#include <QSize>
#include <QPointer>
#include <QStringList>

class QGraphicsOpacityEffect;
class QLabel;
class QMouseEvent;
class QResizeEvent;

class DashboardSectionOrderStrip : public QFrame {
    Q_OBJECT

public:
    explicit DashboardSectionOrderStrip(QWidget* parent = nullptr);

    void setSectionOrder(const QStringList& order,
                         const QHash<QString, QString>& titles);
    QStringList sectionOrder() const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void sectionOrderChanged(const QStringList& order);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct ChipItem {
        QString sectionId;
        QFrame* chip = nullptr;
    };

    QFrame* createChip(const QString& title);
    void rebuildChips();
    int indexOfChip(const QFrame* chip) const;
    int insertionIndexForPosition(int localX) const;
    void beginDrag(QFrame* chip);
    void updateDrag(const QPoint& globalPos);
    void updateDragPreview(const QPoint& globalPos);
    void finishDrag();
    void resetDragState(bool animateDraggedChip = false);
    void setDraggingChip(QFrame* chip, bool dragging);
    void setIndicatorIndex(int index, bool animate = true);
    void relayoutChips(bool animate);
    void moveWidgetTo(QWidget* widget, const QPoint& targetPos, bool animate,
                      int duration = 180);
    void stopPosAnimations(QWidget* widget) const;
    int fullContentWidth() const;
    int contentWidthForCurrentState() const;
    int chipY() const;
    int indicatorY() const;

    QFrame* m_dropIndicator = nullptr;
    QFrame* m_dragPreview = nullptr;
    QLabel* m_dragPreviewLabel = nullptr;
    QGraphicsOpacityEffect* m_dragPreviewOpacity = nullptr;
    QList<ChipItem> m_chipItems;
    QStringList m_order;
    QHash<QString, QString> m_titles;
    QPointer<QFrame> m_pressedChip;
    QPointer<QFrame> m_draggedChip;
    QPoint m_pressGlobalPos;
    QPoint m_pressChipOffset;
    int m_dragSourceIndex = -1;
    int m_indicatorIndex = -1;
    bool m_dragging = false;
};

#endif 
