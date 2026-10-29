#include "imagepreviewdialog.h"
#include "elidedlabel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QVBoxLayout>
#include <utility>

ImagePreviewDialog::ImagePreviewDialog(QString imageName, QWidget* parent)
    : ModernDialogBase(parent)
{
    setTitle(tr("View Image"));
    setMinimumSize(420, 320);
    const QSize available = screen() ? screen()->availableGeometry().size() : QSize(1000, 800);
    resize(QSize(900, 700).boundedTo(available - QSize(40, 40)));
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("ImagePreviewScrollArea");
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->viewport()->setObjectName("ImagePreviewViewport");
    m_scrollArea->viewport()->installEventFilter(this);
    m_imageLabel = new QLabel();
    m_imageLabel->setObjectName("ImagePreviewContent");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setTextFormat(Qt::PlainText);
    m_scrollArea->setWidget(m_imageLabel);
    contentLayout()->addWidget(m_scrollArea, 1);

    auto* footer = new QWidget(this);
    footer->setObjectName("MediaImageEditFooter");
    footer->setAttribute(Qt::WA_StyledBackground, true);
    auto* buttons = new QHBoxLayout(footer);
    buttons->setContentsMargins(20, 12, 20, 12);
    buttons->setSpacing(12);
    auto* name = new ElidedLabel(footer);
    name->setFullText(imageName);
    name->setObjectName("MediaImageEditIntroText");
    name->setTextFormat(Qt::PlainText);
    buttons->addWidget(name, 1);
    m_sizeButton = new QPushButton(tr("Original Size"), footer);
    m_sizeButton->setObjectName("dialog-btn-cancel");
    m_sizeButton->setAutoDefault(false);
    m_sizeButton->setCursor(Qt::PointingHandCursor);
    connect(m_sizeButton, &QPushButton::clicked, this, [this]() {
        m_fitToWindow = !m_fitToWindow;
        updateImage();
    });
    buttons->addWidget(m_sizeButton);
    auto* close = new QPushButton(tr("Close"), footer);
    close->setObjectName("dialog-btn-cancel");
    close->setAutoDefault(false);
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    contentLayout()->addWidget(footer);
    setMessage(tr("Loading image..."));
}

void ImagePreviewDialog::setMessage(const QString& message)
{
    m_image = QPixmap();
    m_imageLabel->setPixmap(QPixmap());
    m_imageLabel->setText(message);
    m_imageLabel->setFixedSize(m_scrollArea->viewport()->size());
    m_sizeButton->setEnabled(false);
}

void ImagePreviewDialog::setImage(QPixmap image)
{
    m_image = std::move(image);
    m_sizeButton->setEnabled(!m_image.isNull());
    updateImage();
}

void ImagePreviewDialog::updateImage()
{
    if (m_image.isNull())
        return;
    m_sizeButton->setText(m_fitToWindow ? tr("Original Size") : tr("Fit to Window"));
    m_scrollArea->setHorizontalScrollBarPolicy(m_fitToWindow ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(m_fitToWindow ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded);
    const qreal dpr = devicePixelRatioF();
    QSize physicalSize = m_image.size();
    if (m_fitToWindow) {
        const QSize available = (m_scrollArea->viewport()->size() - QSize(24, 24)).expandedTo(QSize(1, 1));
        if (physicalSize.width() > available.width() * dpr || physicalSize.height() > available.height() * dpr)
            physicalSize.scale(available * dpr, Qt::KeepAspectRatio);
    }
    QPixmap displayed = m_image.scaled(physicalSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    displayed.setDevicePixelRatio(dpr);
    m_imageLabel->setText(QString());
    m_imageLabel->setPixmap(displayed);
    m_imageLabel->setFixedSize(displayed.deviceIndependentSize().toSize());
}

bool ImagePreviewDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_scrollArea->viewport() && event->type() == QEvent::Resize) {
        if (m_image.isNull())
            m_imageLabel->setFixedSize(m_scrollArea->viewport()->size());
        else if (m_fitToWindow)
            updateImage();
    }
    return ModernDialogBase::eventFilter(watched, event);
}
