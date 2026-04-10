#ifndef PLAYERSTATISTICSOVERLAY_H
#define PLAYERSTATISTICSOVERLAY_H

#include <QColor>
#include <QStringList>
#include <QWidget>

class QPainter;
class QPaintEvent;
class QPointF;

class PlayerStatisticsOverlay : public QWidget
{
public:
    explicit PlayerStatisticsOverlay(QWidget *parent = nullptr);

    void setLines(const QStringList &lines);
    QStringList lines() const { return m_lines; }

    QColor textColor() const { return m_textColor; }
    void setTextColor(const QColor &color);

    QColor shadowColor() const { return m_shadowColor; }
    void setShadowColor(const QColor &color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal textHeightForWidth(const QString &text, qreal width) const;
    void drawTextBlock(QPainter *painter,
                       const QString &text,
                       const QPointF &topLeft,
                       qreal width,
                       const QColor &color) const;

    QStringList m_lines;
    QColor m_textColor = Qt::white;
    QColor m_shadowColor = QColor(0, 0, 0, 230);
    int m_horizontalPadding = 4;
    int m_verticalPadding = 4;
    int m_rowSpacing = 2;
    int m_sectionSpacing = 8;
};

#endif 
