#include "horizontalwidgetgallery.h"
#include <QScrollArea>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QEvent>
#include <QWheelEvent>
#include <QTimer>
#include <QCursor>
#include <QScroller>           
#include <QScrollerProperties> 

HorizontalWidgetGallery::HorizontalWidgetGallery(QWidget* parent)
    : QWidget(parent), m_hScrollAnim(nullptr), m_hScrollTarget(0)
{
    setObjectName("horizontal-widget-gallery");
    setAttribute(Qt::WA_StyledBackground, true);

    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    this->setMinimumWidth(1);
    this->setMouseTracking(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setMinimumWidth(1);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_scrollArea->viewport()->setObjectName("gallery-viewport");
    m_scrollArea->viewport()->setStyleSheet("#gallery-viewport { background: transparent; }");
    m_scrollArea->viewport()->setMouseTracking(true);

    
    
    
    QScroller::grabGesture(m_scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller* scroller = QScroller::scroller(m_scrollArea->viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    scroller->setScrollerProperties(props);

    
    m_hScrollAnim = new QPropertyAnimation(m_scrollArea->horizontalScrollBar(), "value", this);
    m_hScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_hScrollAnim->setDuration(450);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setObjectName("gallery-content-widget");
    m_contentWidget->setStyleSheet("#gallery-content-widget { background: transparent; }");
    m_contentWidget->setMouseTracking(true);

    m_contentLayout = new QHBoxLayout(m_contentWidget);
    m_contentLayout->setSizeConstraint(QLayout::SetMinimumSize);
    m_contentLayout->setContentsMargins(0, 0, 100, 0);
    m_contentLayout->setSpacing(16);
    m_contentLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_scrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_scrollArea);

    
    
    
    m_btnLeft = new QPushButton("❮", this);
    m_btnRight = new QPushButton("❯", this);

    QString btnStyle = "QPushButton { background-color: rgba(0,0,0,120); color: white; font-size: 20px; border: none; border-radius: 8px; }"
                       "QPushButton:hover { background-color: rgba(60,60,60,200); }";
    m_btnLeft->setStyleSheet(btnStyle);
    m_btnRight->setStyleSheet(btnStyle);

    m_btnLeft->setFixedSize(40, 60);
    m_btnRight->setFixedSize(40, 60);
    m_btnLeft->setCursor(Qt::PointingHandCursor);
    m_btnRight->setCursor(Qt::PointingHandCursor);
    m_btnLeft->setFocusPolicy(Qt::NoFocus);
    m_btnRight->setFocusPolicy(Qt::NoFocus);

    m_btnLeft->hide();
    m_btnRight->hide();

    auto scrollAction = [this](int directionMultiplier) {
        QScrollBar* bar = m_scrollArea->horizontalScrollBar();
        int step = this->width() / 2;
        int targetValue = bar->value() + directionMultiplier * step;
        targetValue = qBound(0, targetValue, bar->maximum());

        auto* anim = new QPropertyAnimation(bar, "value", this);
        anim->setDuration(400);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setStartValue(bar->value());
        anim->setEndValue(targetValue);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };

    connect(m_btnLeft, &QPushButton::clicked, [scrollAction]() { scrollAction(-1); });
    connect(m_btnRight, &QPushButton::clicked, [scrollAction]() { scrollAction(1); });

    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, this, &HorizontalWidgetGallery::updateButtonsVisibility);
    connect(m_scrollArea->horizontalScrollBar(), &QScrollBar::rangeChanged, this, &HorizontalWidgetGallery::updateButtonsVisibility);

    m_scrollArea->viewport()->installEventFilter(this);
    this->installEventFilter(this);
}

void HorizontalWidgetGallery::addWidget(QWidget* widget)
{
    if (!widget) return;
    m_contentLayout->addWidget(widget);
}

void HorizontalWidgetGallery::clear()
{
    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->hide();
            child->widget()->deleteLater();
        }
        delete child;
    }
}

void HorizontalWidgetGallery::setSpacing(int spacing)
{
    m_contentLayout->setSpacing(spacing);
}

void HorizontalWidgetGallery::updateButtonPositions()
{
    int currentWidth = this->width();
    int btnY = (this->height() - m_btnLeft->height()) / 2;

    m_btnLeft->move(10, btnY);
    m_btnRight->move(currentWidth - m_btnRight->width() - 10, btnY);

    m_btnLeft->raise();
    m_btnRight->raise();
}

void HorizontalWidgetGallery::adjustHeightToContent()
{
    QTimer::singleShot(0, this, [this]() {
        m_contentWidget->adjustSize();
        int contentHeight = m_contentWidget->sizeHint().height();
        this->setFixedHeight(contentHeight + 16);
        updateButtonPositions();
    });
}

void HorizontalWidgetGallery::resizeEvent(QResizeEvent* event)
{
    QScrollBar* bar = m_scrollArea->horizontalScrollBar();
    bool wasAtEnd = (bar->maximum() > 0 && bar->value() >= bar->maximum() - 5);

    QWidget::resizeEvent(event);
    updateButtonPositions();
    updateButtonsVisibility();

    if (wasAtEnd) {
        QTimer::singleShot(0, this, [bar]() {
            bar->setValue(bar->maximum());
        });
    }
}

bool HorizontalWidgetGallery::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove) {
        updateButtonsVisibility();
    } else if (event->type() == QEvent::Leave) {
        QPoint globalPos = QCursor::pos();
        if (!this->rect().contains(this->mapFromGlobal(globalPos))) {
            m_btnLeft->hide();
            m_btnRight->hide();
        }
    } else if (event->type() == QEvent::Wheel) {
        
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
        if (qAbs(wheelEvent->angleDelta().x()) > qAbs(wheelEvent->angleDelta().y())) {
            
            QScrollBar* hBar = m_scrollArea->horizontalScrollBar();
            if (hBar) {
                int currentVal = hBar->value();
                if (m_hScrollAnim->state() == QAbstractAnimation::Running) {
                    currentVal = m_hScrollTarget;
                }
                int step = wheelEvent->angleDelta().x();
                int newTarget = currentVal - step;
                newTarget = qBound(hBar->minimum(), newTarget, hBar->maximum());

                if (newTarget != hBar->value()) {
                    m_hScrollTarget = newTarget;
                    m_hScrollAnim->stop();
                    m_hScrollAnim->setStartValue(hBar->value());
                    m_hScrollAnim->setEndValue(m_hScrollTarget);
                    m_hScrollAnim->start();
                }
            }
            return true; 
        } else {
            
            wheelEvent->ignore();
            return false;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void HorizontalWidgetGallery::wheelEvent(QWheelEvent *event) {
    if (qAbs(event->angleDelta().x()) <= qAbs(event->angleDelta().y())) {
        event->ignore();
    }
}

void HorizontalWidgetGallery::updateButtonsVisibility()
{
    QPoint globalPos = QCursor::pos();
    QPoint localPos = this->mapFromGlobal(globalPos);

    if (!this->rect().contains(localPos)) {
        m_btnLeft->hide();
        m_btnRight->hide();
        return;
    }

    int currentWidth = this->width();
    QScrollBar* bar = m_scrollArea->horizontalScrollBar();

    bool isLeftHalf = localPos.x() < (currentWidth / 2);

    m_btnLeft->setVisible(isLeftHalf && bar->value() > 0);
    m_btnRight->setVisible(!isLeftHalf && bar->value() < bar->maximum());
}
