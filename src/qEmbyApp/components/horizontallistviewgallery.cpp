#include "horizontallistviewgallery.h"
#include "../views/media/medialistmodel.h"
#include <QListView>
#include <QVBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QScroller>           
#include <QScrollerProperties> 
#include <QStyleOptionViewItem>

HorizontalListViewGallery::HorizontalListViewGallery(QEmbyCore* core, QWidget* parent)
    : QWidget(parent), m_core(core), m_hScrollAnim(nullptr), m_hScrollTarget(0), m_cardStyle(MediaCardDelegate::Poster)
{
    setObjectName("horizontal-listview-gallery");
    setAttribute(Qt::WA_StyledBackground, true);
    setMouseTracking(true);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_listView = new QListView(this);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setFlow(QListView::LeftToRight);
    m_listView->setWrapping(false);
    m_listView->setSpacing(0);
    m_listView->setMovement(QListView::Static);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setUniformItemSizes(true);

    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setMouseTracking(true);
    m_listView->viewport()->setAttribute(Qt::WA_Hover);
    m_listView->setStyleSheet("QListView { background: transparent; outline: none; border: none; }");
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);

    
    
    
    
    m_listView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    
    QScroller::grabGesture(m_listView->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller* scroller = QScroller::scroller(m_listView->viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    scroller->setScrollerProperties(props);

    
    m_hScrollAnim = new QPropertyAnimation(m_listView->horizontalScrollBar(), "value", this);
    m_hScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_hScrollAnim->setDuration(450);

    mainLayout->addWidget(m_listView);

    
    
    
    m_listModel = new MediaListModel(400, m_core, this);
    m_listDelegate = new MediaCardDelegate(MediaCardDelegate::Poster, this);

    m_listView->setModel(m_listModel);
    m_listView->setItemDelegate(m_listDelegate);

    
    connect(m_listDelegate, &MediaCardDelegate::playRequested, this, &HorizontalListViewGallery::playRequested);
    connect(m_listDelegate, &MediaCardDelegate::favoriteRequested, this, &HorizontalListViewGallery::favoriteRequested);
    connect(m_listDelegate, &MediaCardDelegate::moreMenuRequested, this, &HorizontalListViewGallery::moreMenuRequested);

    
    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& index) {
        if (m_listModel) {
            Q_EMIT itemClicked(m_listModel->getItem(index));
        }
    });

    
    
    
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
        QScrollBar* bar = m_listView->horizontalScrollBar();
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

    connect(m_listView->horizontalScrollBar(), &QScrollBar::valueChanged, this, &HorizontalListViewGallery::updateButtonsVisibility);
    connect(m_listView->horizontalScrollBar(), &QScrollBar::rangeChanged, this, &HorizontalListViewGallery::updateButtonsVisibility);

    m_listView->viewport()->installEventFilter(this);
    this->installEventFilter(this);
}


void HorizontalListViewGallery::setItems(const QList<MediaItem>& items)
{
    if (m_listModel) {
        m_listModel->setItems(items);
    }
}


void HorizontalListViewGallery::updateItem(const MediaItem& item)
{
    if (m_listModel) {
        m_listModel->updateItem(item);
    }
}


void HorizontalListViewGallery::removeItem(const QString& itemId)
{
    if (m_listModel) {
        m_listModel->removeItem(itemId);
    }
}

void HorizontalListViewGallery::setCardStyle(MediaCardDelegate::CardStyle style)
{
    bool styleChanged = (m_cardStyle != style);
    m_cardStyle = style; 
    if (m_listDelegate) {
        m_listDelegate->setStyle(style);

        
        if (style == MediaCardDelegate::LibraryTile) {
            int imgHeight = 160;
            int imgWidth = qRound(imgHeight * 16.0 / 9.0); 
            int cardWidth = imgWidth + 16;  
            int cardHeight = 8 + imgHeight + 6 + 20; 
            m_listDelegate->setTileSize(QSize(cardWidth, cardHeight));
        } else if (style == MediaCardDelegate::Poster) {
            m_listDelegate->setTileSize(QSize(160, 270));
        }

        
        m_listView->doItemsLayout();
        m_listView->viewport()->update();
    }
    
    if (m_listModel) {
        m_listModel->setPreferThumb(style == MediaCardDelegate::LibraryTile || style == MediaCardDelegate::EpisodeList);
        
        if (styleChanged) {
            m_listModel->clearImageCache();
        }
    }
    
    updateButtonPositions();
}

void HorizontalListViewGallery::setTileSize(const QSize &size)
{
    if (m_listDelegate) {
        m_listDelegate->setTileSize(size);
        m_listView->doItemsLayout();
        m_listView->viewport()->update();
    }
    updateButtonPositions();
}

void HorizontalListViewGallery::setHighlightedItemId(const QString &id)
{
    if (m_listDelegate) {
        m_listDelegate->setHighlightedItemId(id);
        m_listView->viewport()->update();
    }
}

void HorizontalListViewGallery::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateButtonPositions();
    updateButtonsVisibility();
}

void HorizontalListViewGallery::updateButtonPositions()
{
    int currentWidth = this->width();
    
    
    QStyleOptionViewItem dummyOption;
    QModelIndex dummyIndex;
    QSize itemSize = m_listDelegate->sizeHint(dummyOption, dummyIndex);
    
    int padding = 8; 
    int imgWidth = itemSize.width() - padding * 2;
    int imgHeight = 0;
    
    
    if (m_cardStyle == MediaCardDelegate::LibraryTile) {
        imgHeight = qRound(imgWidth * 9.0 / 16.0);
    } else {
        
        imgHeight = qRound(imgWidth * 1.5);
    }
    
    
    int imageCenterY = padding + (imgHeight / 2);
    int btnY = imageCenterY - (m_btnLeft->height() / 2);

    m_btnLeft->move(10, btnY);
    m_btnRight->move(currentWidth - m_btnRight->width() - 10, btnY);

    m_btnLeft->raise();
    m_btnRight->raise();
}

bool HorizontalListViewGallery::eventFilter(QObject* obj, QEvent* event)
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
            
            QScrollBar* hBar = m_listView->horizontalScrollBar();
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

void HorizontalListViewGallery::updateButtonsVisibility()
{
    QPoint globalPos = QCursor::pos();
    QPoint localPos = this->mapFromGlobal(globalPos);

    if (!this->rect().contains(localPos)) {
        m_btnLeft->hide();
        m_btnRight->hide();
        return;
    }

    int currentWidth = this->width();
    QScrollBar* bar = m_listView->horizontalScrollBar();

    bool isLeftHalf = localPos.x() < (currentWidth / 2);

    m_btnLeft->setVisible(isLeftHalf && bar->value() > 0);
    m_btnRight->setVisible(!isLeftHalf && bar->value() < bar->maximum());
}
