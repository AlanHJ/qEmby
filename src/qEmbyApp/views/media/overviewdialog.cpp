#include "overviewdialog.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QTextDocument>
#include <QRegularExpression>

OverviewDialog::OverviewDialog(QWidget *parent) : ModernDialogBase(parent) {
    setTitle(tr("作品简介"));
    resize(800, 450); 

    auto* mainLayout = new QHBoxLayout();
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(30);

    
    m_posterLabel = new QLabel(this);
    m_posterLabel->setObjectName("overview-poster"); 
    m_posterLabel->setFixedSize(220, 330);
    m_posterLabel->setScaledContents(true);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(25);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 15);
    m_posterLabel->setGraphicsEffect(shadow);

    mainLayout->addWidget(m_posterLabel, 0, Qt::AlignTop);

    
    auto* rightLayout = new QVBoxLayout();
    rightLayout->setSpacing(15);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("overview-title"); 
    m_titleLabel->setWordWrap(true);

    
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("overview-scroll"); 
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* scrollContent = new QWidget(scrollArea);
    scrollContent->setObjectName("overview-scroll-content");
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 15, 0);

    m_overviewLabel = new QLabel(scrollContent);
    m_overviewLabel->setObjectName("overview-text"); 
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_overviewLabel->setTextFormat(Qt::RichText);

    scrollLayout->addWidget(m_overviewLabel);
    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);

    rightLayout->addWidget(m_titleLabel);
    rightLayout->addWidget(scrollArea, 1);

    mainLayout->addLayout(rightLayout, 1);
    contentLayout()->addLayout(mainLayout);
}

void OverviewDialog::setMediaItem(const MediaItem& item, const QPixmap& posterPix) {
    if (!posterPix.isNull()) {
        m_posterLabel->setPixmap(posterPix);
    }
    m_titleLabel->setText(item.name);

    
    
    
    QString html = item.overview;

    
    html.replace("\r\n", "\n");
    
    html.replace(QRegularExpression("<br\\s*/?>\\n", QRegularExpression::CaseInsensitiveOption), "<br>");
    
    html.replace("\n", "<br>");

    
    m_overviewLabel->setTextFormat(Qt::RichText);
    m_overviewLabel->setText(html);
}
