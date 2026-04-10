#include "mediagridwidget.h"
#include "shimmerwidget.h"
#include "../views/media/medialistmodel.h"
#include <QVBoxLayout>
#include <QListView>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QScroller>
#include <QScrollerProperties>
#include <QPropertyAnimation>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStyleOptionViewItem>

MediaGridWidget::MediaGridWidget(QEmbyCore* core, QWidget* parent)
    : QWidget(parent), m_core(core), m_basePadding(20), m_currentStyle(MediaCardDelegate::Poster),
    m_vScrollAnim(nullptr), m_vScrollTarget(0)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_listView = new QListView(this);
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);
    m_listView->setViewMode(QListView::IconMode);
    m_listView->setResizeMode(QListView::Adjust);
    m_listView->setMovement(QListView::Static);
    m_listView->setSpacing(0);
    m_listView->setUniformItemSizes(true);
    m_listView->setWrapping(true);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setMouseTracking(true);
    
    
    m_listView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_listView->viewport()->setAttribute(Qt::WA_Hover);

    
    
    
    
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    
    QScroller::grabGesture(m_listView->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller* scroller = QScroller::scroller(m_listView->viewport());
    QScrollerProperties props = scroller->scrollerProperties();
    
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
    
    props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
    scroller->setScrollerProperties(props);

    
    m_vScrollAnim = new QPropertyAnimation(m_listView->verticalScrollBar(), "value", this);
    m_vScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_vScrollAnim->setDuration(450);

    
    m_listView->viewport()->installEventFilter(this);

    m_listModel = new MediaListModel(400, m_core, this);
    m_listDelegate = new MediaCardDelegate(m_currentStyle, this);

    m_listView->setModel(m_listModel);
    m_listView->setItemDelegate(m_listDelegate);
    layout->addWidget(m_listView);

    
    m_shimmer = new ShimmerWidget(this);
    m_shimmer->hide();

    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex& index) {
        Q_EMIT itemClicked(m_listModel->getItem(index));
    });

    
    
    
    connect(m_listDelegate, &MediaCardDelegate::playRequested, this, &MediaGridWidget::playRequested);
    connect(m_listDelegate, &MediaCardDelegate::favoriteRequested, this, &MediaGridWidget::favoriteRequested);
    connect(m_listDelegate, &MediaCardDelegate::moreMenuRequested, this, &MediaGridWidget::moreMenuRequested);
    
}

void MediaGridWidget::setBasePadding(int padding) {
    m_basePadding = padding;
    adjustGrid();
}

void MediaGridWidget::setCardStyle(MediaCardDelegate::CardStyle style) {
    if (m_currentStyle == style) return;
    m_currentStyle = style;

    
    m_listDelegate->setStyle(style);
    m_listModel->setPreferThumb(style == MediaCardDelegate::LibraryTile || style == MediaCardDelegate::EpisodeList);

    
    if (style == MediaCardDelegate::EpisodeList) {
        m_listView->setViewMode(QListView::ListMode);
        m_listView->setFlow(QListView::TopToBottom);
        m_listView->setWrapping(false);
        m_listView->setSpacing(5); 
    } else {
        m_listView->setViewMode(QListView::IconMode);
        m_listView->setFlow(QListView::LeftToRight);
        m_listView->setWrapping(true);
        m_listView->setSpacing(0);
    }

    m_listModel->setItems(QList<MediaItem>());
    adjustGrid();
}

void MediaGridWidget::setLoading(bool loading)
{
    if (!m_shimmer) {
        return;
    }

    if (loading) {
        
        QStyleOptionViewItem opt;
        const QSize cardSize = m_listDelegate->sizeHint(opt, QModelIndex());
        m_shimmer->setCardSize(cardSize);
        m_shimmer->setShowSubtitle(false);
        m_shimmer->setGeometry(m_listView->geometry());
        m_shimmer->raise();
        m_shimmer->show();
        m_shimmer->startAnimation();
    } else {
        m_shimmer->stopAnimation();
        m_shimmer->hide();
    }
}

void MediaGridWidget::setItems(const QList<MediaItem>& items) {
    m_listModel->setItems(items);
    adjustGrid();
    
    
    if (m_shimmer && m_shimmer->isVisible() && !items.isEmpty()) {
        m_shimmer->stopAnimation();
        m_shimmer->hide();
    }
}




void MediaGridWidget::updateItem(const MediaItem& item) {
    if (m_listModel) {
        m_listModel->updateItem(item);
    }
}


void MediaGridWidget::removeItem(const QString& itemId) {
    if (m_listModel) {
        m_listModel->removeItem(itemId);
    }
}


int MediaGridWidget::itemCount() const {
    return m_listModel ? m_listModel->rowCount() : 0;
}


int MediaGridWidget::saveScrollPosition() const {
    if (m_listView && m_listView->verticalScrollBar())
        return m_listView->verticalScrollBar()->value();
    return 0;
}


void MediaGridWidget::restoreScrollPosition(int pos) {
    if (m_listView && m_listView->verticalScrollBar()) {
        m_listView->verticalScrollBar()->setValue(pos);
        m_vScrollTarget = pos; 
    }
}

void MediaGridWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    adjustGrid();
    
    if (m_shimmer && m_shimmer->isVisible()) {
        m_shimmer->setGeometry(m_listView->geometry());
    }
}

bool MediaGridWidget::eventFilter(QObject* obj, QEvent* event) {
    
    if (event->type() == QEvent::Wheel && obj == m_listView->viewport()) {
        QWheelEvent* we = static_cast<QWheelEvent*>(event);
        if (qAbs(we->angleDelta().y()) >= qAbs(we->angleDelta().x())) {
            QScrollBar* vBar = m_listView->verticalScrollBar();
            if (vBar) {
                int currentVal = vBar->value();
                if (m_vScrollAnim->state() == QAbstractAnimation::Running) {
                    currentVal = m_vScrollTarget;
                }
                int step = we->angleDelta().y();
                int newTarget = currentVal - step;
                newTarget = qBound(vBar->minimum(), newTarget, vBar->maximum());

                if (newTarget != vBar->value()) {
                    m_vScrollTarget = newTarget;
                    m_vScrollAnim->stop();
                    m_vScrollAnim->setStartValue(vBar->value());
                    m_vScrollAnim->setEndValue(m_vScrollTarget);
                    m_vScrollAnim->start();
                }
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void MediaGridWidget::adjustGrid() {
    if (!m_listView || !m_listModel) return;

    int scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int availableWidth = this->width() - scrollBarWidth;
    if (availableWidth < 100) availableWidth = 100;

    
    if (m_currentStyle == MediaCardDelegate::EpisodeList) {
        int padding = m_basePadding;
        
        
        
        
        m_listView->setStyleSheet(QString("QListView { background: transparent; border: none; outline: none; padding-left: %1px; padding-right: 0px; }").arg(padding));
        
        
        
        
        int cellWidth = availableWidth - padding - padding; 
        if (cellWidth < 100) cellWidth = 100;
        
        m_listDelegate->setTileSize(QSize(cellWidth, 160));
    } else {
        availableWidth -= (m_basePadding * 2);
        int defaultCellWidth = 150;
        if (m_currentStyle == MediaCardDelegate::LibraryTile) {
            defaultCellWidth = 250; 
        } else if (m_currentStyle == MediaCardDelegate::Cast) {
            defaultCellWidth = 140; 
        }

        int tolerance = 5;
        int cols = (availableWidth + tolerance) / defaultCellWidth;
        if (cols < 1) cols = 1;

        int cellWidth = availableWidth / cols;
        int remainder = availableWidth - (cols * cellWidth);
        int leftPad = m_basePadding + remainder / 2;

        m_listView->setStyleSheet(QString("QListView { background: transparent; border: none; outline: none; padding-left: %1px; padding-right: 0px; }").arg(leftPad));

        int imgWidth = cellWidth - 16;
        int imgHeight = imgWidth;

        if (m_currentStyle == MediaCardDelegate::Poster || m_currentStyle == MediaCardDelegate::Cast) {
            imgHeight = qRound(imgWidth * 1.5);        
        } else if (m_currentStyle == MediaCardDelegate::LibraryTile) {
            imgHeight = qRound(imgWidth * 9.0 / 16.0); 
        }

        int cellHeight = imgHeight + 60;
        m_listDelegate->setTileSize(QSize(cellWidth, cellHeight));
    }
    
    m_listView->doItemsLayout();
}











































