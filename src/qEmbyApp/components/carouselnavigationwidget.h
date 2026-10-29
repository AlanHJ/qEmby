#ifndef CAROUSELNAVIGATIONWIDGET_H
#define CAROUSELNAVIGATIONWIDGET_H

#include <QColor>
#include <QStringList>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;
class QPushButton;
class TextOutlineEffect;
class CarouselNavigationButton;
class QScrollArea;
class QWheelEvent;
class QShowEvent;
class QHideEvent;
class SmoothScrollController;

class CarouselNavigationWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor outlineColor READ outlineColor WRITE setOutlineColor)
    Q_PROPERTY(qreal outlineWidth READ outlineWidth WRITE setOutlineWidth)
public:
    explicit CarouselNavigationWidget(QWidget* parent = nullptr);
    void setSlides(const QStringList& titles);
    void setCurrentIndex(int index);
    void setRefreshEnabled(bool enabled);
    QColor outlineColor() const { return m_outlineColor; }
    void setOutlineColor(const QColor& color);
    qreal outlineWidth() const { return m_outlineWidth; }
    void setOutlineWidth(qreal width);

Q_SIGNALS:
    void stepRequested(int offset);
    void slideRequested(int index);
    void refreshRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void addOutline(QWidget* widget);
    bool scrollDotsByWheel(QWheelEvent* event);
    void revealCurrentDot(bool animated);
    static constexpr int VisibleDotCount = 8;
    static constexpr int DotWidth = 10;
    static constexpr int DotHeight = 20;
    QStringList m_titles;
    QList<QPushButton*> m_dots;
    QList<TextOutlineEffect*> m_outlines;
    QButtonGroup* m_group = nullptr;
    QHBoxLayout* m_dotLayout = nullptr;
    QScrollArea* m_dotScrollArea = nullptr;
    QWidget* m_dotContainer = nullptr;
    SmoothScrollController* m_dotScrollController = nullptr;
    QPushButton* m_previous = nullptr;
    QPushButton* m_next = nullptr;
    CarouselNavigationButton* m_refresh = nullptr;
    QColor m_outlineColor;
    qreal m_outlineWidth = 1.0;
    int m_index = -1;
};

#endif 
