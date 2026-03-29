#include "mediasectionwidget.h"
#include "horizontallistviewgallery.h"
#include <qembycore.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QListView>

MediaSectionWidget::MediaSectionWidget(const QString& title, QEmbyCore* core, QWidget* parent)
    : QWidget(parent), m_core(core)
{
    setObjectName("media-section");
    setStyleSheet("QWidget#media-section { background: transparent; }");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 20, 40, 0);
    layout->setSpacing(0);

    
    auto *headerContainer = new QWidget(this);
    headerContainer->setObjectName("section-header");
    headerContainer->setStyleSheet("#section-header { background: transparent; }");
    m_headerLayout = new QHBoxLayout(headerContainer);
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(12);

    m_titleLabel = new QLabel(title, headerContainer);
    m_titleLabel->setObjectName("detail-section-title");
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();

    m_gallery = new HorizontalListViewGallery(core, this);
    m_gallery->listView()->setProperty("isHorizontalListView", true);

    
    connect(m_gallery, &HorizontalListViewGallery::itemClicked, this, &MediaSectionWidget::itemClicked);
    connect(m_gallery, &HorizontalListViewGallery::playRequested, this, &MediaSectionWidget::playRequested);
    connect(m_gallery, &HorizontalListViewGallery::favoriteRequested, this, &MediaSectionWidget::favoriteRequested);
    connect(m_gallery, &HorizontalListViewGallery::moreMenuRequested, this, &MediaSectionWidget::moreMenuRequested);

    layout->addWidget(headerContainer);
    layout->addWidget(m_gallery);

    
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    
    hide();
}

HorizontalListViewGallery* MediaSectionWidget::gallery() const {
    return m_gallery;
}

void MediaSectionWidget::setTitle(const QString& title) {
    m_titleLabel->setText(title);
}

void MediaSectionWidget::setHeaderWidget(QWidget* widget) {
    
    if (m_headerWidget && m_headerWidget != widget) {
        m_headerWidget->hide();
        m_headerWidget = nullptr;
    }
    if (widget) {
        m_headerWidget = widget;
        
        if (m_headerLayout->indexOf(m_headerWidget) < 0) {
            m_headerLayout->insertWidget(1, m_headerWidget);
        }
        m_headerWidget->show();
    }
}

void MediaSectionWidget::setCardStyle(MediaCardDelegate::CardStyle style) {
    m_gallery->setCardStyle(style);
}

void MediaSectionWidget::setGalleryHeight(int height) {
    m_gallery->setFixedHeight(height);
}

void MediaSectionWidget::setTileSize(const QSize &size) {
    m_gallery->setTileSize(size);
}

void MediaSectionWidget::clear() {
    m_gallery->setItems(QList<MediaItem>());
    hide();
}

void MediaSectionWidget::setItems(const QList<MediaItem>& items) {
    if (items.isEmpty()) {
        hide();
    } else {
        m_gallery->setItems(items);
        show();
    }
}

void MediaSectionWidget::updateItem(const MediaItem& item) {
    m_gallery->updateItem(item);
}


QCoro::Task<void> MediaSectionWidget::loadAsync(FetchFunction fetcher) {
    if (!fetcher) co_return;

    
    QPointer<MediaSectionWidget> safeThis(this);

    try {
        QList<MediaItem> items = co_await fetcher();

        if (safeThis) {
            safeThis->setItems(items);
            Q_EMIT safeThis->dataLoaded(items.size());
        }
    } catch(...) {
        
        if (safeThis) {
            safeThis->hide();
        }
    }
}
