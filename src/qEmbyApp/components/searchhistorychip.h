#ifndef SEARCHHISTORYCHIP_H
#define SEARCHHISTORYCHIP_H

#include <QAbstractButton>
#include <QString>

class QEnterEvent;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPoint;
class QVariantAnimation;

class SearchHistoryChip : public QAbstractButton
{
    Q_OBJECT
public:
    explicit SearchHistoryChip(QWidget *parent = nullptr);

    void setTerm(QString term);
    QString term() const;

    void setSearchCount(int count);
    int searchCount() const;

    void setHeatLevel(const QString &level);
    QString heatLevel() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void removeRequested(const QString &term);

protected:
    bool hitButton(const QPoint &pos) const override;
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void updateMetrics();
    void syncHoverDecorationVisibility();
    bool shouldShowHoverDecorations() const;
    void updateRemoveHover(const QPoint &pos);
    QRect bodyRect() const;
    QRect removeButtonRect() const;
    QRect badgeRect() const;
    QString formattedCount() const;

    QString m_term;
    QString m_heatLevel = QStringLiteral("low");
    int m_searchCount = 1;
    int m_cachedWidth = 40;
    bool m_removeHovered = false;
    bool m_removePressed = false;
    bool m_hoverDecorVisible = false;
    qreal m_hoverDecorProgress = 0.0;
    QVariantAnimation *m_hoverDecorAnimation = nullptr;
};

#endif 
