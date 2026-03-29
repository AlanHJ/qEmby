#ifndef HORIZONTALWIDGETGALLERY_H
#define HORIZONTALWIDGETGALLERY_H

#include <QWidget>

class QScrollArea;
class QHBoxLayout;
class QPushButton;
class QPropertyAnimation;

class HorizontalWidgetGallery : public QWidget
{
    Q_OBJECT
public:
    explicit HorizontalWidgetGallery(QWidget* parent = nullptr);
    ~HorizontalWidgetGallery() override = default;

    
    void addWidget(QWidget* widget);

    
    void clear();

    
    void setSpacing(int spacing);

    
    void adjustHeightToContent();

    
    QSize minimumSizeHint() const override {
        return QSize(1, QWidget::minimumSizeHint().height());
    }

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateButtonsVisibility();
    void updateButtonPositions();

    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QHBoxLayout* m_contentLayout;

    QPushButton* m_btnLeft;
    QPushButton* m_btnRight;

    
    QPropertyAnimation* m_hScrollAnim;
    int m_hScrollTarget;
};

#endif 
