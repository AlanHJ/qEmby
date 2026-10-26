#include "carouselnavigationwidget.h"
#include "carouselnavigationbutton.h"
#include "textoutlineeffect.h"
#include "../utils/smoothscrollcontroller.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QWheelEvent>
#include <QShowEvent>
#include <QHideEvent>

CarouselNavigationWidget::CarouselNavigationWidget(QWidget* parent)
    : QWidget(parent), m_outlineColor(palette().shadow().color())
{
    setObjectName("carousel-navigation");
    setAttribute(Qt::WA_StyledBackground, true);
    
    setAttribute(Qt::WA_NoMousePropagation, true);
    setAccessibleName(tr("Carousel navigation"));
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 1, 4, 1);
    layout->setSpacing(2);
    auto* previous = new CarouselNavigationButton(CarouselNavigationButton::Symbol::Previous, this);
    auto* next = new CarouselNavigationButton(CarouselNavigationButton::Symbol::Next, this);
    auto* refresh = new CarouselNavigationButton(CarouselNavigationButton::Symbol::Refresh, this);
    m_previous = previous;
    m_next = next;
    m_refresh = refresh;
    previous->setToolTip(tr("Previous slide"));
    next->setToolTip(tr("Next slide"));
    refresh->setToolTip(tr("Show another selection"));
    for (auto* button : {previous, next, refresh}) {
        button->setObjectName("carousel-arrow");
        button->setFixedSize(20, 20);
        button->setCursor(Qt::PointingHandCursor);
        button->setAccessibleName(button->toolTip());
        addOutline(button);
    }
    refresh->setObjectName("carousel-refresh");
    layout->addWidget(previous);
    m_dotScrollArea = new QScrollArea(this);
    m_dotScrollArea->setObjectName("carousel-dot-scroll");
    m_dotScrollArea->setFrameShape(QFrame::NoFrame);
    m_dotScrollArea->setFocusPolicy(Qt::NoFocus);
    m_dotScrollArea->setWidgetResizable(false);
    m_dotScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dotScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_dotScrollArea->viewport()->setObjectName("carousel-dot-viewport");
    m_dotScrollArea->viewport()->setAutoFillBackground(false);
    m_dotScrollArea->viewport()->installEventFilter(this);
    m_dotContainer = new QWidget();
    m_dotContainer->setObjectName("carousel-dots");
    m_dotContainer->installEventFilter(this);
    m_dotLayout = new QHBoxLayout(m_dotContainer);
    m_dotLayout->setContentsMargins(0, 0, 0, 0);
    m_dotLayout->setSpacing(0);
    m_dotLayout->setAlignment(Qt::AlignLeft);
    m_dotScrollArea->setWidget(m_dotContainer);
    m_dotContainer->setAutoFillBackground(false);
    m_dotScrollController = new SmoothScrollController(
        m_dotScrollArea->horizontalScrollBar(), this);
    m_dotScrollController->setDuration(180);
    layout->addWidget(m_dotScrollArea);
    layout->addWidget(next);
    layout->addWidget(refresh);
    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    connect(previous, &QPushButton::clicked, this,
            [this]() { Q_EMIT stepRequested(-1); });
    connect(next, &QPushButton::clicked, this,
            [this]() { Q_EMIT stepRequested(1); });
    connect(refresh, &QPushButton::clicked, this,
            &CarouselNavigationWidget::refreshRequested);
    hide();
}

void CarouselNavigationWidget::addOutline(QWidget* widget)
{
    auto* outline = new TextOutlineEffect(widget);
    outline->setOutlineColor(m_outlineColor);
    outline->setOutlineWidth(m_outlineWidth);
    widget->setGraphicsEffect(outline);
    m_outlines.append(outline);
}

void CarouselNavigationWidget::setOutlineColor(const QColor& color)
{
    m_outlineColor = color;
    for (auto* outline : m_outlines) outline->setOutlineColor(color);
}

void CarouselNavigationWidget::setOutlineWidth(qreal width)
{
    m_outlineWidth = qBound(qreal(0), width, qreal(4));
    for (auto* outline : m_outlines) outline->setOutlineWidth(m_outlineWidth);
}

void CarouselNavigationWidget::setSlides(const QStringList& titles)
{
    
    if (m_titles == titles) return;
    m_dotScrollController->stop();
    m_titles = titles;
    const bool multipleSlides = titles.size() > 1;
    m_previous->setVisible(multipleSlides);
    m_next->setVisible(multipleSlides);
    while (m_dots.size() < titles.size()) {
        const int index = m_dots.size();
        auto* dot = new CarouselNavigationButton(CarouselNavigationButton::Symbol::Dot, m_dotContainer);
        dot->setObjectName("carousel-dot");
        dot->setFixedSize(DotWidth, DotHeight);
        dot->setCursor(Qt::PointingHandCursor);
        dot->setCheckable(true);
        m_group->addButton(dot, index);
        m_dotLayout->addWidget(dot);
        addOutline(dot);
        m_dots.append(dot);
        dot->installEventFilter(this);
        connect(dot, &QPushButton::clicked, this,
                [this, index]() { Q_EMIT slideRequested(index); });
    }
    for (int i = 0; i < m_dots.size(); ++i) {
        const bool exists = i < titles.size();
        m_dots.at(i)->setVisible(exists && multipleSlides);
        if (exists) {
            const QString description = tr("Go to slide %1: %2")
                .arg(i + 1).arg(titles.at(i));
            m_dots.at(i)->setToolTip(description);
            m_dots.at(i)->setAccessibleName(description);
        }
    }
    
    
    const int count = int(titles.size());
    const int visibleWidth = qMin(count, VisibleDotCount) * DotWidth;
    m_dotScrollArea->setFixedSize(visibleWidth, DotHeight);
    m_dotContainer->setFixedSize(count * DotWidth, DotHeight);
    if (count > VisibleDotCount) {
        
        
        m_dotScrollArea->viewport()->setMask(QRegion(2, 0, visibleWidth - 4, DotHeight));
        m_dotScrollArea->setToolTip(tr("Scroll to browse slides"));
    } else {
        m_dotScrollArea->viewport()->clearMask();
        m_dotScrollArea->setToolTip({});
    }
    m_dotLayout->activate();
    m_dotScrollArea->horizontalScrollBar()->setRange(0, qMax(0, count * DotWidth - visibleWidth));
    m_dotScrollArea->horizontalScrollBar()->setPageStep(visibleWidth);
    m_dotScrollArea->setVisible(multipleSlides);
    setCurrentIndex(titles.isEmpty() ? -1 : qBound(0, m_index, int(titles.size()) - 1));
    revealCurrentDot(false);
    
    setVisible(!titles.isEmpty());
}

void CarouselNavigationWidget::setRefreshEnabled(bool enabled)
{
    m_refresh->setEnabled(enabled);
    m_refresh->setSpinning(!enabled);
}

void CarouselNavigationWidget::setCurrentIndex(int index)
{
    m_index = index >= 0 && index < m_titles.size() ? index : -1;
    
    m_group->setExclusive(false);
    for (int i = 0; i < m_dots.size(); ++i) m_dots.at(i)->setChecked(i == m_index);
    m_group->setExclusive(true);
    revealCurrentDot(isVisible());
}

void CarouselNavigationWidget::revealCurrentDot(bool animated)
{
    
    
    const int first = m_index < 0 ? 0 : qBound(0,
        m_index - (VisibleDotCount - 1) / 2,
        qMax(0, int(m_titles.size()) - VisibleDotCount));
    m_dotScrollController->scrollTo(first * DotWidth, animated);
}

bool CarouselNavigationWidget::scrollDotsByWheel(QWheelEvent* event)
{
    if (m_titles.size() <= VisibleDotCount) return false;
    const bool hasPixels = !event->pixelDelta().isNull();
    const QPoint delta = hasPixels ? event->pixelDelta() : event->angleDelta();
    const Qt::Orientation axis = qAbs(delta.x()) > qAbs(delta.y())
        ? Qt::Horizontal : Qt::Vertical;
    
    
    m_dotScrollController->setWheelMultiplier(hasPixels ? 1.0 : DotWidth * 3.0 / 120.0);
    m_dotScrollController->scrollByWheelEvent(event, axis);
    
    
    event->accept();
    return true;
}

bool CarouselNavigationWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel && scrollDotsByWheel(static_cast<QWheelEvent*>(event)))
        return true;
    return QWidget::eventFilter(watched, event);
}

void CarouselNavigationWidget::wheelEvent(QWheelEvent* event)
{
    if (!scrollDotsByWheel(event)) QWidget::wheelEvent(event);
}

void CarouselNavigationWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    revealCurrentDot(false);
}

void CarouselNavigationWidget::hideEvent(QHideEvent* event)
{
    m_dotScrollController->stop();
    QWidget::hideEvent(event);
}
