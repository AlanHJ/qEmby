#ifndef DANMAKUHEATMAPWIDGET_H
#define DANMAKUHEATMAPWIDGET_H

#include <QColor>
#include <QPainterPath>
#include <QWidget>


class DanmakuHeatmapWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor heatmapColor READ heatmapColor WRITE setHeatmapColor)
public:
    explicit DanmakuHeatmapWidget(QWidget *parent = nullptr);
    void setHeatmapPath(const QPainterPath &path);
    QColor heatmapColor() const;
    void setHeatmapColor(const QColor &color);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QPainterPath m_path;
    QColor m_color;
};

#endif
