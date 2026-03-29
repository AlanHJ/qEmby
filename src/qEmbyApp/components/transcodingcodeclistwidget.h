#ifndef TRANSCODINGCODECLISTWIDGET_H
#define TRANSCODINGCODECLISTWIDGET_H

#include <QList>
#include <QPoint>
#include <QStringList>
#include <QWidget>

class QResizeEvent;
class QScrollArea;
class QTimer;
class TranscodingCodecRowWidget;

class TranscodingCodecListWidget : public QWidget {
    Q_OBJECT

public:
    explicit TranscodingCodecListWidget(QWidget* parent = nullptr);

    void setCodecRows(const QList<TranscodingCodecRowWidget*>& rows);
    QStringList codecOrder() const;

signals:
    void orderChanged(const QStringList& codecIds);

public slots:
    void onRowDragStart(TranscodingCodecRowWidget* row, const QPoint& globalPos);
    void onRowDragMove(TranscodingCodecRowWidget* row, const QPoint& globalPos);
    void onRowDragFinish(TranscodingCodecRowWidget* row);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    static constexpr int RowSpacing = 10;

    int rowHeightFor(TranscodingCodecRowWidget* row) const;
    int rowTopForIndex(int index) const;
    int indexForCenterY(int centerY) const;
    QScrollArea* parentScrollArea() const;
    int autoScrollDelta(const QPoint& globalPos) const;
    void updateDragPosition(TranscodingCodecRowWidget* row, const QPoint& globalPos);
    void relayout(bool animate = false);

    QList<TranscodingCodecRowWidget*> m_rows;
    TranscodingCodecRowWidget* m_dragRow = nullptr;
    int m_dragOriginalIndex = -1;
    int m_dragCurrentIndex = -1;
    int m_dragHotSpotY = 0;
    QPoint m_lastDragGlobalPos;
    QTimer* m_autoScrollTimer = nullptr;
};

#endif 
