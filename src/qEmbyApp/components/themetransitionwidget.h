#ifndef THEMETRANSITIONWIDGET_H
#define THEMETRANSITIONWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>

class ThemeTransitionWidget : public QWidget {
    Q_OBJECT
    
    Q_PROPERTY(int radius READ radius WRITE setRadius)

public:
    explicit ThemeTransitionWidget(const QPixmap& oldSnapshot, const QPoint& center, QWidget* parent = nullptr);

    int radius() const;
    void setRadius(int r);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_oldSnapshot;
    QPoint m_center;
    int m_radius;
};

#endif 
