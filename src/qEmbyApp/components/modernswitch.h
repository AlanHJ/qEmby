#ifndef MODERNSWITCH_H
#define MODERNSWITCH_H

#include <QAbstractButton>
#include <QPropertyAnimation>
#include <QColor>
#include <QPainter>
#include <QEvent>

class ModernSwitch : public QAbstractButton {
    Q_OBJECT
    
    Q_PROPERTY(qreal offset READ offset WRITE setOffset)
    Q_PROPERTY(QColor trackColorActive READ trackColorActive WRITE setTrackColorActive)
    Q_PROPERTY(QColor trackColorInactive READ trackColorInactive WRITE setTrackColorInactive)
    Q_PROPERTY(QColor thumbColor READ thumbColor WRITE setThumbColor)

public:
    explicit ModernSwitch(QWidget* parent = nullptr);
    ~ModernSwitch() override;

    
    qreal offset() const;
    void setOffset(qreal offset);

    QColor trackColorActive() const;
    void setTrackColorActive(const QColor& color);

    QColor trackColorInactive() const;
    void setTrackColorInactive(const QColor& color);

    QColor thumbColor() const;
    void setThumbColor(const QColor& color);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void updateAnimation(bool checked);

private:
    qreal m_offset;
    QPropertyAnimation* m_animation;

    
    QColor m_trackColorActive;
    QColor m_trackColorInactive;
    QColor m_thumbColor;

    bool m_isHovered;
};

#endif 
