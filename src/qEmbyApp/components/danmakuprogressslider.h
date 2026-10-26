#ifndef DANMAKUPROGRESSSLIDER_H
#define DANMAKUPROGRESSSLIDER_H

#include "modernslider.h"
#include <QPainterPath>
#include <QPoint>
#include <QVector>
#include <models/danmaku/danmakumodels.h>

class DanmakuProgressSlider : public ModernSlider {
    Q_OBJECT
    Q_PROPERTY(QColor heatmapColor READ heatmapColor WRITE setHeatmapColor)
public:
    explicit DanmakuProgressSlider(QWidget *parent = nullptr);
    void setComments(const QList<DanmakuComment> &comments);
    void setMediaDuration(double seconds);
    void setDanmakuOffset(int milliseconds);
    void refreshHoverPosition();
    QColor heatmapColor() const;
    void setHeatmapColor(const QColor &color);
    QPainterPath heatmapPath() const { return m_heatmap; }

signals:
    void heatmapChanged(const QPainterPath &path);
    void hoverPositionChanged(int position, const QPoint &globalTrackPosition);
    void hoverEnded();

protected:
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    qreal horizontalTrackCenter() const override;
    void paintTrackBackground(QPainter &painter, const QRectF &trackRect) override;

private:
    int positionAt(qreal x) const;
    void updateHoverPosition(const QPoint &globalPosition);
    void clearHoverPosition();
    void seekAt(qreal x);
    void rebuildHeatmap();
    QVector<qint64> m_timesMs;
    QPainterPath m_heatmap;
    QColor m_heatmapColor;
    double m_durationSeconds = 0.0;
    int m_offsetMs = 0;
    bool m_hovering = false;
};

#endif
