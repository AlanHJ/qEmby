#include "modernscrollpanel.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QEvent>
#include <QFontMetrics>

ModernScrollPanel::ModernScrollPanel(QWidget *parent) : QFrame(parent), m_maxContentWidth(0) {
    setObjectName("modernScrollMenuOuter");

    setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("modernMenuScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_container = new QWidget(m_scrollArea);
    m_container->setObjectName("modernMenuContainer");
    m_container->setAttribute(Qt::WA_StyledBackground, true);

    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(1);

    m_scrollArea->setWidget(m_container);
    m_mainLayout->addWidget(m_scrollArea);
}

void ModernScrollPanel::addItem(const QString &text, const QVariant &userData, bool isSelected) {
    auto *btn = new QPushButton(m_container);
    btn->setObjectName("modernMenuItemBtn");

    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    if (isSelected) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QColor(255, 255, 255, 255));
        QFont font = painter.font();
        font.setPixelSize(14);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "✓");
    }

    btn->setIcon(QIcon(pixmap));
    btn->setText(text); 

    
    btn->setToolTip(text);

    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFixedHeight(32);

    
    QFontMetrics fm(btn->font());
    m_maxContentWidth = qMax(m_maxContentWidth, fm.horizontalAdvance(text));

    
    m_items.append({btn, text});

    connect(btn, &QPushButton::clicked, this, [this, userData, text]() {
        emit itemTriggered(userData, text); 
    });

    m_layout->addWidget(btn);
}

void ModernScrollPanel::finalizeLayout(int maxHeight, int maxWidth) {
    m_layout->addStretch();

    m_container->adjustSize();
    int contentHeight = m_container->sizeHint().height();

    if (contentHeight < 10) {
        contentHeight = 40;
    }

    int finalHeight = qMin(contentHeight, maxHeight);
    bool needsVScroll = (contentHeight > maxHeight);

    if (needsVScroll) {
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    } else {
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    
    
    
    
    int scrollBarBuffer = needsVScroll ? 12 : 0; 
    int requiredWidth = m_maxContentWidth + 45 + scrollBarBuffer;

    
    int minWidth = 130;

    
    int finalWidth = qBound(minWidth, requiredWidth, maxWidth);

    
    int textAvailableWidth = finalWidth - 45 - scrollBarBuffer;
    if (textAvailableWidth < 20) {
        textAvailableWidth = 20; 
    }

    
    for (const auto& item : m_items) {
        QFontMetrics fm(item.btn->font());
        
        QString elidedText = fm.elidedText(item.fullText, Qt::ElideRight, textAvailableWidth);
        item.btn->setText(elidedText);
    }

    
    m_scrollArea->setFixedSize(finalWidth, finalHeight);
    this->setFixedSize(finalWidth, finalHeight);
}

void ModernScrollPanel::wheelEvent(QWheelEvent *event) {
    QFrame::wheelEvent(event);
    event->accept();
}
