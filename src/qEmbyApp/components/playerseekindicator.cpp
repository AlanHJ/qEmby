#include "playerseekindicator.h"
#include "../utils/imageutils.h"

#include <QLabel>

namespace {
constexpr int kLabelMinWidth = 80;
constexpr int kLabelMaxWidth = 148;
constexpr int kLabelHeight = 28;
constexpr int kHorizontalMargin = 10;
constexpr int kMarkerSize = 7;
constexpr int kIndicatorHeight = 42;
constexpr int kMarkerCenterY = kIndicatorHeight - kMarkerSize + kMarkerSize / 2;
}

PlayerSeekIndicator::PlayerSeekIndicator(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("playerSeekIndicator");
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    m_timeLabel = new QLabel(this);
    m_timeLabel->setObjectName("playerSeekTimeLabel");
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel = new QLabel(this);
    m_imageLabel->setObjectName("playerSeekPreviewImage");
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->hide();
    m_stem = new QWidget(this);
    m_stem->setObjectName("playerSeekStem");
    m_marker = new QWidget(this);
    m_marker->setObjectName("playerSeekMarker");
    hide();
}

void PlayerSeekIndicator::setTimeText(const QString &text)
{
    if (m_timeLabel->text() == text) return;
    m_timeLabel->setText(text);
    updateIndicatorLayout();
}

void PlayerSeekIndicator::setAnchor(const QPoint &anchor)
{
    m_anchor = anchor;
    updateIndicatorLayout();
}

void PlayerSeekIndicator::setPreviewImage(const QImage &image)
{
    if (m_previewImage.cacheKey() == image.cacheKey()) return;
    m_previewImage = image;
    m_previewSize = QSize();
    if (image.isNull()) m_imageLabel->clear();
    updateIndicatorLayout();
}

void PlayerSeekIndicator::updateIndicatorLayout()
{
    QWidget *host = parentWidget();
    if (!host || host->width() <= 0 || host->height() <= 0) return;

    m_timeLabel->ensurePolished();
    const int labelWidth = qMin(host->width(),
        qBound(kLabelMinWidth, m_timeLabel->sizeHint().width(), kLabelMaxWidth));
    const int margin = qMin(kHorizontalMargin, (host->width() - labelWidth) / 2);
    int labelX = qBound(margin, m_anchor.x() - labelWidth / 2,
                              host->width() - labelWidth - margin);
    const int imageWidth = qMin(224, qMax(0, host->width() - 2 * kHorizontalMargin));
    const int imageHeight = qMin(imageWidth * 9 / 16,
        qMax(0, m_anchor.y() - kIndicatorHeight - 6 - kHorizontalMargin));
    const bool showImage = !m_previewImage.isNull() && imageWidth >= 80 && imageHeight >= 48;
    const int imageOffset = showImage ? imageHeight + 6 : 0;
    const int indicatorHeight = kIndicatorHeight + imageOffset;
    const int y = qBound(0, m_anchor.y() - kMarkerCenterY - imageOffset,
                         qMax(0, host->height() - indicatorHeight));
    setGeometry(0, y, host->width(), indicatorHeight);
    m_imageLabel->setVisible(showImage);
    if (showImage) {
        const int imageX = qBound(kHorizontalMargin, m_anchor.x() - imageWidth / 2,
                                  host->width() - imageWidth - kHorizontalMargin);
        m_imageLabel->setGeometry(imageX, 0, imageWidth, imageHeight);
        labelX = imageX + (imageWidth - labelWidth) / 2;
        const QSize previewSize(imageWidth - 2, imageHeight - 2);
        if (previewSize != m_previewSize) {
            m_previewSize = previewSize;
            m_imageLabel->setPixmap(ImageUtils::containRoundedPixmap(
                QPixmap::fromImage(m_previewImage), previewSize, 5));
        }
    }
    m_timeLabel->setGeometry(labelX, imageOffset, labelWidth, kLabelHeight);

    const int markerX = qBound(0, m_anchor.x() - kMarkerSize / 2,
                               qMax(0, width() - kMarkerSize));
    const int markerY = qBound(0, m_anchor.y() - y - kMarkerSize / 2,
                               indicatorHeight - kMarkerSize);
    m_marker->setGeometry(markerX, markerY, kMarkerSize, kMarkerSize);
    const int stemTop = imageOffset + kLabelHeight + 2;
    const int stemHeight = qMax(0, markerY - 1 - stemTop);
    m_stem->setGeometry(qBound(0, m_anchor.x(), width() - 1), stemTop, 1, stemHeight);
    m_stem->setVisible(stemHeight > 0);
}
