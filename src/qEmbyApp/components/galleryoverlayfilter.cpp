#include "galleryoverlayfilter.h"
#include <QListView>
#include <QPushButton>
#include <QEvent>
#include <QResizeEvent>
#include <QScrollBar>


GalleryOverlayFilter::GalleryOverlayFilter(QListView *list, QPushButton *left, QPushButton *right, QObject *parent)
    : QObject(parent), m_list(list), m_left(left), m_right(right) {}

bool GalleryOverlayFilter::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::Resize) {
        
        int btnWidth = 40;
        int btnHeight = 80;

        
        int y = m_list->y() + (m_list->height() - btnHeight) / 2 - 20;
        if (y < 0) y = 0;

        
        m_left->setGeometry(m_list->x() + 10, y, btnWidth, btnHeight);
        m_right->setGeometry(m_list->x() + m_list->width() - btnWidth - 10, y, btnWidth, btnHeight);
    }
    return QObject::eventFilter(obj, event);
}
