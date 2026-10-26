#include "mediaimageeditdialog.h"

#include "modernmessagebox.h"
#include "moderntoast.h"
#include "textinputdialog.h"
#include "imagepreviewdialog.h"
#include "flowlayout.h"
#include "../utils/layoututils.h"

#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>

#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QMimeDatabase>
#include <QMimeData>
#include <QMimeType>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QScopeGuard>
#include <QShowEvent>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kPreviewWidth = 440;
constexpr int kPreviewWorkers = 4;

QString buildItemImagePath(QString itemId, QString imageType, int imageIndex)
{
    itemId = itemId.trimmed();
    imageType = imageType.trimmed();

    QString path = QStringLiteral("/Items/%1/Images/%2").arg(itemId, imageType);
    if (imageIndex >= 0) {
        path += QStringLiteral("/%1").arg(imageIndex);
    }

    return path;
}

QString imageTypeLabel(QString imageType)
{
    imageType = imageType.trimmed();
    if (imageType == QLatin1String("Primary")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Primary");
    }
    if (imageType == QLatin1String("Backdrop")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Backdrop");
    }
    if (imageType == QLatin1String("Banner")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Banner");
    }
    if (imageType == QLatin1String("Thumb")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Thumb");
    }
    if (imageType == QLatin1String("Logo")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Logo");
    }
    if (imageType == QLatin1String("Art")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Art");
    }
    if (imageType == QLatin1String("Disc")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Disc");
    }
    if (imageType == QLatin1String("Box")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Box");
    }
    if (imageType == QLatin1String("BoxRear")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Box Rear");
    }
    if (imageType == QLatin1String("Menu")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Menu");
    }
    if (imageType == QLatin1String("Screenshot")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Screenshot");
    }
    if (imageType == QLatin1String("Chapter")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Chapter");
    }
    if (imageType == QLatin1String("Profile")) {
        return QCoreApplication::translate("MediaImageEditDialog", "Profile");
    }

    return imageType;
}

QStringList preferredImageTypeOrder(const MediaImageEditTarget& target)
{
    if (target.isLibrary) {
        return {QStringLiteral("Primary"),  QStringLiteral("Banner"),
                QStringLiteral("Thumb"),    QStringLiteral("Logo"),
                QStringLiteral("Art"),      QStringLiteral("Backdrop"),
                QStringLiteral("Box"),      QStringLiteral("BoxRear")};
    }

    const QString itemType = target.itemType.trimmed();
    const QString mediaType = target.mediaType.trimmed();

    if (itemType == QLatin1String("Person")) {
        return {QStringLiteral("Primary"), QStringLiteral("Backdrop")};
    }

    if (itemType == QLatin1String("Episode") ||
        itemType == QLatin1String("Video") ||
        itemType == QLatin1String("MusicVideo")) {
        return {QStringLiteral("Primary"),    QStringLiteral("Thumb"),
                QStringLiteral("Backdrop"),   QStringLiteral("Screenshot"),
                QStringLiteral("Banner"),     QStringLiteral("Logo"),
                QStringLiteral("Art")};
    }

    if (itemType == QLatin1String("Season")) {
        return {QStringLiteral("Primary"),  QStringLiteral("Thumb"),
                QStringLiteral("Backdrop"), QStringLiteral("Banner"),
                QStringLiteral("Logo"),     QStringLiteral("Art")};
    }

    if (itemType == QLatin1String("MusicAlbum") ||
        itemType == QLatin1String("MusicArtist") ||
        itemType == QLatin1String("Audio") ||
        mediaType == QLatin1String("Audio")) {
        return {QStringLiteral("Primary"),  QStringLiteral("Thumb"),
                QStringLiteral("Disc"),     QStringLiteral("Logo"),
                QStringLiteral("Banner"),   QStringLiteral("Art"),
                QStringLiteral("Backdrop")};
    }

    return {QStringLiteral("Primary"),    QStringLiteral("Logo"),
            QStringLiteral("Thumb"),      QStringLiteral("Banner"),
            QStringLiteral("Disc"),       QStringLiteral("Art"),
            QStringLiteral("Backdrop"),   QStringLiteral("Box"),
            QStringLiteral("BoxRear"),    QStringLiteral("Menu"),
            QStringLiteral("Screenshot"), QStringLiteral("Chapter")};
}

QString formatImageSize(qint64 size)
{
    if (size <= 0) {
        return {};
    }

    double value = static_cast<double>(size);
    QString unit = QStringLiteral("B");
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = QStringLiteral("KB");
    }
    if (value >= 1024.0) {
        value /= 1024.0;
        unit = QStringLiteral("MB");
    }

    const int precision = unit == QLatin1String("B") ? 0 : 1;
    return QCoreApplication::translate("MediaImageEditDialog", "%1 %2")
        .arg(QString::number(value, 'f', precision), unit);
}

QString buildPreviewDetails(const QList<ItemImageInfo>& images)
{
    if (images.isEmpty()) {
        return {};
    }

    const ItemImageInfo& info = images.first();
    QStringList segments;
    if (info.width > 0 && info.height > 0) {
        segments.append(
            QCoreApplication::translate("MediaImageEditDialog", "%1 × %2")
                .arg(info.width)
                .arg(info.height));
    }

    const QString sizeText = formatImageSize(info.size);
    if (!sizeText.isEmpty()) {
        segments.append(sizeText);
    }

    return segments.join(QStringLiteral("  ·  "));
}

QString buildImageFileFilter()
{
    QStringList patterns;
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    for (const QByteArray& format : formats) {
        const QString extension = QString::fromLatin1(format).trimmed().toLower();
        if (!extension.isEmpty()) {
            patterns.append(QStringLiteral("*.%1").arg(extension));
        }
    }

    patterns.removeDuplicates();
    std::sort(patterns.begin(), patterns.end());

    if (patterns.isEmpty()) {
        patterns = {QStringLiteral("*.jpg"),  QStringLiteral("*.jpeg"),
                    QStringLiteral("*.png"),  QStringLiteral("*.webp"),
                    QStringLiteral("*.bmp"),  QStringLiteral("*.gif")};
    }

    return QCoreApplication::translate("MediaImageEditDialog", "Image Files (%1);;All Files (*)")
        .arg(patterns.join(QLatin1Char(' ')));
}

bool loadLocalImageFile(QString filePath, QByteArray& imageData,
                        QString& mimeType)
{
    filePath = filePath.trimmed();
    if (filePath.isEmpty()) {
        return false;
    }

    QImageReader reader(filePath);
    if (!reader.canRead()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    imageData = file.readAll();
    if (imageData.isEmpty()) {
        return false;
    }

    QMimeDatabase mimeDatabase;
    mimeType = mimeDatabase.mimeTypeForFile(QFileInfo(filePath)).name().trimmed();
    if (!mimeType.startsWith(QStringLiteral("image/"))) {
        const QByteArray format = QImageReader::imageFormat(filePath);
        if (!format.isEmpty()) {
            mimeType = QStringLiteral("image/%1")
                           .arg(QString::fromLatin1(format).toLower());
        }
    }
    if (mimeType.isEmpty()) {
        mimeType = QStringLiteral("application/octet-stream");
    }

    
    
    if (QFileInfo(filePath).suffix().compare(QLatin1String("jpg"),
                                             Qt::CaseInsensitive) == 0 &&
        mimeType == QLatin1String("image/jpeg")) {
        mimeType = QStringLiteral("image/jpg");
    }

    return true;
}

bool encodeImageAsPng(const QImage& image, QByteArray& imageData,
                      QString& mimeType)
{
    if (image.isNull()) {
        return false;
    }

    QBuffer buffer(&imageData);
    if (!buffer.open(QIODevice::WriteOnly) ||
        !image.save(&buffer, "PNG")) {
        return false;
    }

    mimeType = QStringLiteral("image/png");
    return true;
}

QString sanitizeFileName(QString fileName)
{
    fileName = fileName.trimmed();
    if (fileName.isEmpty()) {
        return QStringLiteral("image");
    }

    static const QString invalidCharacters =
        QStringLiteral("<>:\"/\\|?*");
    for (QChar& character : fileName) {
        if (character.unicode() < 32 ||
            invalidCharacters.contains(character)) {
            character = QLatin1Char('_');
        }
    }

    return fileName;
}

QString preferredImageSuffix(QString mimeType, QString fallbackFileName)
{
    const QString fallbackSuffix =
        QFileInfo(fallbackFileName.trimmed()).suffix().trimmed().toLower();
    if (!fallbackSuffix.isEmpty()) {
        return fallbackSuffix;
    }

    mimeType = mimeType.trimmed().toLower();
    if (!mimeType.isEmpty()) {
        QMimeDatabase mimeDatabase;
        const QMimeType mime = mimeDatabase.mimeTypeForName(mimeType);
        if (mime.isValid()) {
            const QString preferredSuffix =
                mime.preferredSuffix().trimmed().toLower();
            if (!preferredSuffix.isEmpty()) {
                return preferredSuffix;
            }

            const QStringList suffixes = mime.suffixes();
            if (!suffixes.isEmpty()) {
                return suffixes.first().trimmed().toLower();
            }
        }
    }

    return QStringLiteral("png");
}

QString ensureImageFileSuffix(QString filePath, QString mimeType,
                              QString fallbackFileName)
{
    filePath = filePath.trimmed();
    if (filePath.isEmpty() ||
        !QFileInfo(filePath).suffix().trimmed().isEmpty()) {
        return filePath;
    }

    return QStringLiteral("%1.%2")
        .arg(filePath, preferredImageSuffix(mimeType, fallbackFileName));
}

QString defaultDownloadDirectory()
{
    const QString picturesDirectory =
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return picturesDirectory.isEmpty() ? QDir::homePath() : picturesDirectory;
}

QString defaultImageSaveName(const MediaImageEditTarget& target,
                             const QString& imageLabel,
                             const ItemImageInfo* previewImage)
{
    if (previewImage) {
        const QString originalFileName =
            previewImage->fileName.trimmed().isEmpty()
                ? QFileInfo(previewImage->path).fileName().trimmed()
                : previewImage->fileName.trimmed();
        if (!originalFileName.isEmpty()) {
            return sanitizeFileName(originalFileName);
        }
    }

    QString baseName = target.displayName.trimmed();
    if (baseName.isEmpty()) {
        baseName = target.effectiveImageItemId();
    }

    return sanitizeFileName(
        QStringLiteral("%1-%2").arg(baseName, imageLabel.trimmed()));
}

QString displayUrlForLog(QString imageUrl)
{
    imageUrl = imageUrl.trimmed();
    const QUrl url(imageUrl);
    if (!url.isValid()) {
        return imageUrl;
    }

    return url.toString(QUrl::RemoveQuery | QUrl::RemoveFragment);
}

} 

MediaImageEditDialog::MediaImageEditDialog(QEmbyCore* core,
                                           MediaImageEditTarget target,
                                           QWidget* parent)
    : ModernDialogBase(parent), m_core(core), m_target(std::move(target))
{
    resize(1040, 740);
    setMinimumSize(520, 420);
    setTitle(tr("Edit Images"));
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    contentLayout()->setSpacing(0);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("MediaImageEditScrollArea");
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    auto* page = new QWidget(m_scrollArea);
    page->setObjectName("MediaImageEditPage");
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(20, 16, 20, 20);
    pageLayout->setSpacing(14);
    pageLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);

    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item") : m_target.displayName.trimmed();
    m_promptLabel = new QLabel(
        tr("Manage image types for \"%1\". Changes are applied immediately.").arg(displayName), page);
    m_promptLabel->setObjectName("MediaImageEditIntroText");
    m_promptLabel->setWordWrap(true);
    pageLayout->addWidget(m_promptLabel);

    auto* imagesTitle = new QLabel(tr("Images"), page);
    imagesTitle->setObjectName("MediaImageEditSectionTitle");
    pageLayout->addWidget(imagesTitle);
    auto* imagesPanel = new QWidget(page);
    m_imagesLayout = new FlowLayout(imagesPanel, 0, 12, 12);
    pageLayout->addWidget(imagesPanel);

    m_backdropsSection = new QWidget(page);
    auto* backdropLayout = new QVBoxLayout(m_backdropsSection);
    backdropLayout->setContentsMargins(0, 10, 0, 0);
    backdropLayout->setSpacing(12);
    auto* backdropTitle = new QLabel(tr("Backdrop"), m_backdropsSection);
    backdropTitle->setObjectName("MediaImageEditSectionTitle");
    backdropLayout->addWidget(backdropTitle);
    auto* backdropsPanel = new QWidget(m_backdropsSection);
    m_backdropsLayout = new FlowLayout(backdropsPanel, 0, 12, 12);
    backdropLayout->addWidget(backdropsPanel);
    pageLayout->addWidget(m_backdropsSection);
    pageLayout->addStretch();
    m_scrollArea->setWidget(page);
    contentLayout()->addWidget(m_scrollArea, 1);

    auto* footer = new QWidget(this);
    footer->setObjectName("MediaImageEditFooter");
    footer->setAttribute(Qt::WA_StyledBackground, true);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(20, 12, 20, 12);
    footerLayout->setSpacing(12);
    m_statusLabel = new QLabel(tr("Loading image information..."), footer);
    m_statusLabel->setObjectName("MediaImageEditIntroText");
    m_statusLabel->setWordWrap(true);
    footerLayout->addWidget(m_statusLabel, 1);
    m_retryButton = new QPushButton(tr("Retry"), footer);
    m_retryButton->setObjectName("dialog-btn-cancel");
    m_retryButton->setCursor(Qt::PointingHandCursor);
    m_retryButton->setAutoDefault(false);
    m_retryButton->hide();
    connect(m_retryButton, &QPushButton::clicked, this,
            [this]() { if (!m_isBusy) m_pendingTask = reloadImages(); });
    footerLayout->addWidget(m_retryButton);
    auto* closeButton = new QPushButton(tr("Close"), footer);
    closeButton->setObjectName("dialog-btn-cancel");
    closeButton->setCursor(Qt::PointingHandCursor);
    closeButton->setMinimumWidth(96);
    closeButton->setAutoDefault(false);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    footerLayout->addWidget(closeButton);
    contentLayout()->addWidget(footer);
    updateUiState();
}

void MediaImageEditDialog::showEvent(QShowEvent* event)
{
    ModernDialogBase::showEvent(event);

    if (!m_loaded) {
        m_loaded = true;
        m_pendingTask = reloadImages();
    }
}

bool MediaImageEditDialog::eventFilter(QObject* watched, QEvent* event)
{
    const int index = watched ? watched->property("image-slot").toInt() : -1;
    QPointer<MediaImageEditCard> card =
        index >= 0 && index < m_cards.size() ? m_cards.at(index) : QPointer<MediaImageEditCard>();
    if (card && watched == card.data() && event) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (canAcceptDroppedImage(dragEvent->mimeData())) {
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                card->setDropActive(true);
            } else {
                dragEvent->ignore();
                card->setDropActive(false);
            }
            return true;
        }
        case QEvent::DragMove: {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (canAcceptDroppedImage(dragEvent->mimeData())) {
                dragEvent->setDropAction(Qt::CopyAction);
                dragEvent->accept();
                card->setDropActive(true);
            } else {
                dragEvent->ignore();
                card->setDropActive(false);
            }
            return true;
        }
        case QEvent::DragLeave:
            card->setDropActive(false);
            return true;
        case QEvent::Drop: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            card->setDropActive(false);

            if (!canAcceptDroppedImage(dropEvent->mimeData())) {
                dropEvent->ignore();
                return true;
            }

            const DroppedImagePayload payload =
                parseDroppedImagePayload(dropEvent->mimeData());
            if (!payload.isValid()) {
                dropEvent->ignore();
                ModernToast::showMessage(
                    tr("Dropped data does not contain a supported image"),
                    3000);
                return true;
            }

            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
            m_pendingTask = handleDroppedImage(m_slots.at(index), payload);
            return true;
        }
        default:
            break;
        }
    }

    return ModernDialogBase::eventFilter(watched, event);
}

QCoro::Task<void> MediaImageEditDialog::reloadImages()
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    if (!safeThis || !adminService || !m_target.isValid()) {
        co_return;
    }

    m_isBusy = true;
    m_hasImageInfo = false;
    ++m_previewLoadGeneration;
    m_retryButton->hide();
    updateStatusText(tr("Loading image information..."));
    updateUiState();

    try {
        const QList<ItemImageInfo> images =
            co_await adminService->getItemImages(m_target.effectiveImageItemId());
        if (!safeThis) {
            co_return;
        }

        safeThis->m_hasImageInfo = true;
        safeThis->m_itemImages = images;
        safeThis->rebuildCards();

        QSet<QString> availableTypes;
        for (const ImageSlotState& slot : std::as_const(safeThis->m_slots)) {
            if (slot.hasImage())
                availableTypes.insert(slot.imageType);
        }
        const int availableTypeCount = availableTypes.size();

        safeThis->updateStatusText(
            availableTypeCount > 0
                ? tr("Loaded images for %1 type(s)").arg(availableTypeCount)
                : tr("No images found yet"));
        safeThis->m_isBusy = false;
        safeThis->startPreviews();
        safeThis->updateUiState();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        if (safeThis->m_cards.isEmpty())
            safeThis->rebuildCards();
        safeThis->m_retryButton->show();
        safeThis->updateStatusText(tr("Failed to load images"));
        safeThis->updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to load item images"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to load images: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> MediaImageEditDialog::loadPreview(ImageSlotState slot,
                                                    QPointer<MediaImageEditCard> card,
                                                    quint64 requestGeneration)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);

    if (!safeThis || !mediaService || !card || !slot.hasImage()) {
        co_return;
    }

    try {
        const QPixmap pixmap =
            co_await mediaService->fetchImage(m_target.effectiveImageItemId(),
                                              slot.imageType, QString(),
                                              kPreviewWidth,
                                              slot.previewImageIndex(),
                                              ImageRequestPriority::Normal, card.data());
        if (!safeThis || !card || requestGeneration != safeThis->m_previewLoadGeneration) {
            co_return;
        }

        if (pixmap.isNull()) {
            card->setPreviewMessage(tr("Preview unavailable"));
            co_return;
        }

        card->setPreview(pixmap);
    } catch (const std::exception& e) {
        if (!safeThis || !card || requestGeneration != safeThis->m_previewLoadGeneration) {
            co_return;
        }

        qWarning() << "[MediaImageEditDialog] Failed to load image preview"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| error=" << e.what();
        card->setPreviewMessage(tr("Preview unavailable"));
    }
}

QCoro::Task<void> MediaImageEditDialog::viewImage(ImageSlotState slot)
{
    QPointer<MediaService> service(m_core ? m_core->mediaService() : nullptr);
    if (!service || !slot.hasImage())
        co_return;

    const QString itemId = m_target.effectiveImageItemId();
    QPointer<ImagePreviewDialog> viewer = new ImagePreviewDialog(
        m_target.displayName + QStringLiteral(" · ") + slot.displayName, this);
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->open();
    try {
        qDebug() << "[MediaImageEditDialog] Loading original image"
                 << "| itemId=" << itemId << "| imageType=" << slot.imageType
                 << "| imageIndex=" << slot.previewImageIndex();
        const DownloadedImageData data = co_await service->downloadImageByUrl(
            buildItemImagePath(itemId, slot.imageType, slot.previewImageIndex()));
        if (!viewer)
            co_return;
        QPixmap image;
        if (!image.loadFromData(data.data)) {
            viewer->setMessage(tr("Failed to load image"));
            qWarning() << "[MediaImageEditDialog] Original image could not be decoded"
                       << "| itemId=" << itemId << "| imageType=" << slot.imageType;
            co_return;
        }
        viewer->setImage(image);
    } catch (const std::exception&) {
        if (viewer)
            viewer->setMessage(tr("Failed to load image"));
        qWarning() << "[MediaImageEditDialog] Original image request failed"
                   << "| itemId=" << itemId << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex();
    }
}

QCoro::Task<void> MediaImageEditDialog::chooseAndUploadImage(ImageSlotState slot)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Image"), QString(), buildImageFileFilter());
    if (!safeThis || filePath.trimmed().isEmpty()) {
        co_return;
    }

    co_await uploadLocalImageFile(slot, filePath);
}

QCoro::Task<void> MediaImageEditDialog::promptAndUploadImageUrl(ImageSlotState slot)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item")
                                    : m_target.displayName.trimmed();
    bool accepted = false;
    const QString imageUrl = TextInputDialog::getText(
        this, tr("Enter Image URL"),
        tr("Paste an image address for the %1 image of \"%2\".")
            .arg(slot.displayName, displayName),
        QString(), tr("https://example.com/image.jpg"),
        slot.hasImage() ? tr("Replace Image") : tr("Upload Image"), &accepted);
    if (!safeThis || !accepted) {
        co_return;
    }

    co_await uploadRemoteImage(slot, imageUrl);
}

QCoro::Task<void> MediaImageEditDialog::saveImage(ImageSlotState slot)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    if (!safeThis || !mediaService) {
        co_return;
    }

    if (!slot.hasImage()) {
        co_return;
    }

    const ItemImageInfo previewImage = slot.images.first();
    const QString defaultPath =
        QDir(defaultDownloadDirectory())
            .filePath(defaultImageSaveName(m_target, slot.displayName,
                                           &previewImage));
    const QString savePath = QFileDialog::getSaveFileName(
        this, tr("Save Image"), defaultPath, buildImageFileFilter());
    if (!safeThis || !mediaService || savePath.trimmed().isEmpty()) {
        co_return;
    }

    m_isBusy = true;
    updateStatusText(tr("Downloading image..."));
    updateUiState();

    DownloadedImageData downloadedImage;
    try {
        const QString imagePath = buildItemImagePath(
            m_target.effectiveImageItemId(), slot.imageType,
            slot.previewImageIndex());
        qDebug() << "[MediaImageEditDialog] Downloading current image"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| imageType=" << slot.imageType
                 << "| imageIndex=" << slot.previewImageIndex()
                 << "| savePath=" << savePath;
        downloadedImage = co_await mediaService->downloadImageByUrl(imagePath);
        if (!safeThis) {
            co_return;
        }
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        safeThis->updateStatusText(tr("Download failed"));
        safeThis->updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to download image"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to download image: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
        co_return;
    }

    const QString finalSavePath = ensureImageFileSuffix(
        savePath, downloadedImage.mimeType, previewImage.fileName);
    QSaveFile file(finalSavePath);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(downloadedImage.data) != downloadedImage.data.size() ||
        !file.commit()) {
        const QString errorText = file.errorString().trimmed().isEmpty()
                                      ? tr("Unknown error")
                                      : file.errorString().trimmed();
        m_isBusy = false;
        updateStatusText(tr("Save failed"));
        updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to save image"
                   << "| path=" << finalSavePath
                   << "| error=" << errorText;
        ModernToast::showMessage(
            tr("Failed to save image: %1").arg(errorText), 3000);
        co_return;
    }

    m_isBusy = false;
    updateStatusText(tr("Image downloaded"));
    updateUiState();
    ModernToast::showMessage(tr("Image downloaded"), 2000);
}

QCoro::Task<void> MediaImageEditDialog::uploadLocalImageFile(ImageSlotState slot, QString filePath)
{
    QByteArray imageData;
    QString mimeType;
    if (!loadLocalImageFile(filePath, imageData, mimeType)) {
        ModernToast::showMessage(tr("Failed to read image file"), 3000);
        co_return;
    }

    qDebug() << "[MediaImageEditDialog] Uploading local image"
             << "| itemId=" << m_target.effectiveImageItemId()
             << "| imageType=" << slot.imageType
             << "| imageIndex=" << slot.previewImageIndex()
             << "| filePath=" << filePath
             << "| mimeType=" << mimeType
             << "| bytes=" << imageData.size();

    co_await uploadImageData(slot, imageData, mimeType);
}

QCoro::Task<void> MediaImageEditDialog::uploadRemoteImage(ImageSlotState slot, QString imageUrl)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    if (!safeThis || !mediaService) {
        co_return;
    }

    imageUrl = imageUrl.trimmed();
    const QUrl parsedUrl(imageUrl);
    const QString scheme = parsedUrl.scheme().trimmed().toLower();
    const bool isServerRelativePath = imageUrl.startsWith(QLatin1Char('/'));
    if (imageUrl.isEmpty() ||
        (!isServerRelativePath && scheme != QLatin1String("http") &&
         scheme != QLatin1String("https"))) {
        ModernToast::showMessage(tr("Invalid image URL"), 3000);
        co_return;
    }

    m_isBusy = true;
    updateStatusText(tr("Downloading image..."));
    updateUiState();

    try {
        qDebug() << "[MediaImageEditDialog] Downloading remote image for upload"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| imageType=" << slot.imageType
                 << "| imageIndex=" << slot.previewImageIndex()
                 << "| url=" << displayUrlForLog(imageUrl);
        const DownloadedImageData downloadedImage =
            co_await mediaService->downloadImageByUrl(imageUrl);
        if (!safeThis) {
            co_return;
        }

        co_await safeThis->uploadImageData(slot, downloadedImage.data,
                                           downloadedImage.mimeType);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        safeThis->updateStatusText(tr("Download failed"));
        safeThis->updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to download remote image"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| url=" << displayUrlForLog(imageUrl)
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to download image: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> MediaImageEditDialog::uploadImageData(ImageSlotState slot,
                                                        QByteArray imageData,
                                                        QString mimeType)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    const auto resetBusy = qScopeGuard([safeThis]() {
        if (safeThis) {
            safeThis->m_isBusy = false;
            safeThis->updateUiState();
        }
    });
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);
    if (!safeThis || !adminService || slot.imageType.trimmed().isEmpty()) {
        co_return;
    }

    if (imageData.isEmpty()) {
        ModernToast::showMessage(tr("Failed to read image file"), 3000);
        co_return;
    }

    QImage decodedImage;
    if (!decodedImage.loadFromData(imageData)) {
        qWarning() << "[MediaImageEditDialog] uploadImageData skipped invalid image data"
                   << "| itemId=" << m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| mimeType=" << mimeType
                   << "| bytes=" << imageData.size();
        ModernToast::showMessage(tr("Failed to read image file"), 3000);
        co_return;
    }

    QByteArray uploadData = imageData;
    QString uploadMimeType = mimeType.trimmed().toLower();
    const bool keepOriginalBytes =
        uploadMimeType == QLatin1String("image/png") ||
        uploadMimeType == QLatin1String("image/jpeg") ||
        uploadMimeType == QLatin1String("image/jpg");
    if (!keepOriginalBytes) {
        QByteArray normalizedData;
        QString normalizedMimeType;
        if (!encodeImageAsPng(decodedImage, normalizedData, normalizedMimeType)) {
            qWarning() << "[MediaImageEditDialog] Failed to normalize image data"
                       << "| itemId=" << m_target.effectiveImageItemId()
                       << "| imageType=" << slot.imageType
                       << "| imageIndex=" << slot.previewImageIndex()
                       << "| sourceMimeType=" << mimeType
                       << "| bytes=" << imageData.size();
            ModernToast::showMessage(tr("Failed to read image file"), 3000);
            co_return;
        }

        uploadData = std::move(normalizedData);
        uploadMimeType = std::move(normalizedMimeType);
    }

    m_isBusy = true;
    updateStatusText(tr("Uploading image..."));
    updateUiState();

    try {
        co_await adminService->uploadItemImage(
            m_target.effectiveImageItemId(), slot.imageType, uploadData,
            uploadMimeType, slot.previewImageIndex());
        if (!safeThis) {
            co_return;
        }

        safeThis->invalidateEditedImages(slot.imageType);

        safeThis->m_hasChanges = true;
        ModernToast::showMessage(tr("Image updated"), 2000);
        co_await safeThis->reloadImages();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        safeThis->updateStatusText(tr("Upload failed"));
        safeThis->updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to upload image"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| mimeType=" << uploadMimeType
                   << "| bytes=" << uploadData.size()
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to upload image: %1")
                .arg(QString::fromUtf8(e.what())),
            6000);
    }
}

QCoro::Task<void> MediaImageEditDialog::handleDroppedImage(
    ImageSlotState slot, DroppedImagePayload payload)
{
    if (!payload.localFilePath.trimmed().isEmpty()) {
        qDebug() << "[MediaImageEditDialog] Handling dropped local image file"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| filePath=" << payload.localFilePath;
        co_await uploadLocalImageFile(slot, payload.localFilePath);
        co_return;
    }

    if (!payload.imageData.isEmpty()) {
        qDebug() << "[MediaImageEditDialog] Uploading dropped in-memory image"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| imageType=" << slot.imageType
                 << "| imageIndex=" << slot.previewImageIndex()
                 << "| mimeType=" << payload.mimeType
                 << "| bytes=" << payload.imageData.size();
        co_await uploadImageData(slot, payload.imageData,
                                 payload.mimeType);
        co_return;
    }

    if (!payload.remoteImageUrl.trimmed().isEmpty()) {
        qDebug() << "[MediaImageEditDialog] Handling dropped remote image url"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| url=" << displayUrlForLog(payload.remoteImageUrl);
        co_await uploadRemoteImage(slot, payload.remoteImageUrl);
    }
}

QCoro::Task<void> MediaImageEditDialog::removeImage(ImageSlotState slot)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);
    if (!safeThis || !adminService) {
        co_return;
    }

    if (!slot.hasImage()) {
        co_return;
    }

    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item")
                                    : m_target.displayName.trimmed();
    const bool confirmed = ModernMessageBox::question(
        this, tr("Remove Image"),
        tr("Are you sure you want to remove the %1 image from \"%2\"?")
            .arg(slot.displayName, displayName),
        tr("Remove"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!safeThis || !adminService || !confirmed) {
        co_return;
    }

    m_isBusy = true;
    updateStatusText(tr("Removing image..."));
    updateUiState();

    try {
        co_await adminService->deleteItemImage(m_target.effectiveImageItemId(),
                                               slot.imageType,
                                               slot.previewImageIndex());
        if (!safeThis) {
            co_return;
        }

        safeThis->invalidateEditedImages(slot.imageType);

        safeThis->m_hasChanges = true;
        ModernToast::showMessage(tr("Image removed"), 2000);
        co_await safeThis->reloadImages();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        safeThis->updateStatusText(tr("Remove failed"));
        safeThis->updateUiState();

        qWarning() << "[MediaImageEditDialog] Failed to remove image"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to remove image: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

bool MediaImageEditDialog::canAcceptDroppedImage(const QMimeData* mimeData) const
{
    if (!mimeData || m_isBusy || !m_hasImageInfo) {
        return false;
    }

    if (mimeData->hasImage()) {
        return true;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            QImageReader reader(url.toLocalFile());
            if (reader.canRead()) {
                return true;
            }
            continue;
        }

        const QString scheme = url.scheme().trimmed().toLower();
        if (scheme == QLatin1String("http") ||
            scheme == QLatin1String("https")) {
            return true;
        }
    }

    const QString text = mimeData->text().trimmed();
    if (!text.isEmpty()) {
        if (QFileInfo(text).exists()) {
            QImageReader reader(text);
            if (reader.canRead()) {
                return true;
            }
        }

        const QUrl url = QUrl::fromUserInput(text);
        if (url.isLocalFile()) {
            QImageReader reader(url.toLocalFile());
            if (reader.canRead()) {
                return true;
            }
        }

        const QString scheme = url.scheme().trimmed().toLower();
        if (scheme == QLatin1String("http") ||
            scheme == QLatin1String("https")) {
            return true;
        }
    }

    return false;
}

MediaImageEditDialog::DroppedImagePayload
MediaImageEditDialog::parseDroppedImagePayload(const QMimeData* mimeData) const
{
    DroppedImagePayload payload;
    if (!mimeData) {
        return payload;
    }

    const QList<QUrl> urls = mimeData->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            const QString filePath = url.toLocalFile().trimmed();
            QImageReader reader(filePath);
            if (reader.canRead()) {
                payload.localFilePath = filePath;
                return payload;
            }
        }
    }

    if (mimeData->hasImage()) {
        const QVariant imageVariant = mimeData->imageData();
        QImage image = qvariant_cast<QImage>(imageVariant);
        if (image.isNull()) {
            const QPixmap pixmap = qvariant_cast<QPixmap>(imageVariant);
            image = pixmap.toImage();
        }

        if (encodeImageAsPng(image, payload.imageData, payload.mimeType)) {
            return payload;
        }
    }

    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            continue;
        }

        const QString scheme = url.scheme().trimmed().toLower();
        if (scheme == QLatin1String("http") ||
            scheme == QLatin1String("https")) {
            payload.remoteImageUrl = url.toString();
            return payload;
        }
    }

    const QString text = mimeData->text().trimmed();
    if (text.isEmpty()) {
        return payload;
    }

    if (QFileInfo(text).exists()) {
        QImageReader reader(text);
        if (reader.canRead()) {
            payload.localFilePath = text;
            return payload;
        }
    }

    const QUrl url = QUrl::fromUserInput(text);
    if (url.isLocalFile()) {
        const QString filePath = url.toLocalFile().trimmed();
        QImageReader reader(filePath);
        if (reader.canRead()) {
            payload.localFilePath = filePath;
            return payload;
        }
    }

    const QString scheme = url.scheme().trimmed().toLower();
    if (scheme == QLatin1String("http") ||
        scheme == QLatin1String("https")) {
        payload.remoteImageUrl = text;
    }

    return payload;
}

void MediaImageEditDialog::rebuildCards()
{
    const int scrollPosition = m_scrollArea->verticalScrollBar()->value();
    for (const auto& card : std::as_const(m_cards)) {
        if (card)
            card->hide();
    }
    m_cards.clear();
    LayoutUtils::clearLayout(m_imagesLayout);
    LayoutUtils::clearLayout(m_backdropsLayout);
    m_slots.clear();

    QHash<QString, QList<ItemImageInfo>> imageMap;
    for (const ItemImageInfo& image : std::as_const(m_itemImages)) {
        if (!image.imageType.trimmed().isEmpty())
            imageMap[image.imageType.trimmed()].append(image);
    }
    QStringList types = preferredImageTypeOrder(m_target);
    QStringList extraTypes = imageMap.keys();
    std::sort(extraTypes.begin(), extraTypes.end());
    for (const QString& type : extraTypes) {
        if (!types.contains(type))
            types.append(type);
    }

    for (const QString& type : types) {
        QList<ItemImageInfo> images = imageMap.value(type);
        std::sort(images.begin(), images.end(), [](const ItemImageInfo& a, const ItemImageInfo& b) {
            return a.imageIndex < b.imageIndex;
        });
        const bool isBackdrop = type == QLatin1String("Backdrop");
        
        for (const ItemImageInfo& image : images) {
            ImageSlotState slot;
            slot.imageType = type;
            slot.displayName = imageTypeLabel(type);
            slot.images.append(image);
            m_slots.append(slot);
        }
        if (images.isEmpty() || isBackdrop) {
            ImageSlotState slot;
            slot.imageType = type;
            slot.displayName = imageTypeLabel(type);
            m_slots.append(slot); 
        }
    }

    bool hasBackdrops = false;
    for (int index = 0; index < m_slots.size(); ++index) {
        const ImageSlotState slot = m_slots.at(index);
        const bool isBackdrop = slot.imageType == QLatin1String("Backdrop");
        hasBackdrops = hasBackdrops || isBackdrop;
        QString title = slot.displayName;
        if (isBackdrop && slot.hasImage())
            title = tr("%1 %2").arg(title).arg(slot.previewImageIndex() + 1);
        else if (isBackdrop)
            title = tr("Add Backdrop");
        auto* card = new MediaImageEditCard(title, buildPreviewDetails(slot.images), slot.hasImage(), this);
        card->setProperty("image-slot", index);
        card->installEventFilter(this);
        m_cards.append(card);
        (isBackdrop ? m_backdropsLayout : m_imagesLayout)->addWidget(card);
        card->show();
        connect(card, &MediaImageEditCard::actionRequested, this,
                [this, index, safeCard = QPointer<MediaImageEditCard>(card)](MediaImageEditCard::Action action) {
                    if (safeCard && index < m_cards.size() && m_cards.at(index) == safeCard)
                        handleCardAction(index, action);
                });
    }
    m_backdropsSection->setVisible(hasBackdrops);
    QTimer::singleShot(0, this, [this, scrollPosition]() {
        m_scrollArea->verticalScrollBar()->setValue(scrollPosition);
    });
    qDebug() << "[MediaImageEditDialog] Image cards rebuilt"
             << "| itemId=" << m_target.effectiveImageItemId()
             << "| imageCount=" << m_itemImages.size() << "| cardCount=" << m_cards.size();
}

void MediaImageEditDialog::handleCardAction(int index, MediaImageEditCard::Action action)
{
    if (m_isBusy || !m_hasImageInfo || index < 0 || index >= m_slots.size())
        return;
    const ImageSlotState slot = m_slots.at(index);
    switch (action) {
    case MediaImageEditCard::Action::View:
        QCoro::connect(viewImage(slot), this, []() {});
        break;
    case MediaImageEditCard::Action::ChooseFile:
        m_pendingTask = chooseAndUploadImage(slot);
        break;
    case MediaImageEditCard::Action::ImageUrl:
        m_pendingTask = promptAndUploadImageUrl(slot);
        break;
    case MediaImageEditCard::Action::Download:
        m_pendingTask = saveImage(slot);
        break;
    case MediaImageEditCard::Action::Remove:
        m_pendingTask = removeImage(slot);
        break;
    }
}

void MediaImageEditDialog::startPreviews()
{
    const quint64 generation = ++m_previewLoadGeneration;
    for (int worker = 0; worker < kPreviewWorkers; ++worker)
        QCoro::connect(loadPreviews(worker, generation), this, []() {});
}

void MediaImageEditDialog::invalidateEditedImages(QString imageType)
{
    if (!m_core || !m_core->mediaService())
        return;
    QSet<int> indices{-1};
    for (const ItemImageInfo& image : std::as_const(m_itemImages)) {
        if (image.imageType == imageType)
            indices.insert(image.imageIndex);
    }
    
    QStringList itemIds{m_target.effectiveImageItemId(), m_target.itemId};
    itemIds.removeAll(QString());
    itemIds.removeDuplicates();
    for (const QString& itemId : itemIds) {
        for (int index : indices)
            m_core->mediaService()->invalidateImageCache(itemId, imageType, index);
    }
}

QCoro::Task<void> MediaImageEditDialog::loadPreviews(int worker, quint64 requestGeneration)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    for (int index = worker; safeThis && requestGeneration == safeThis->m_previewLoadGeneration &&
                             index < safeThis->m_slots.size(); index += kPreviewWorkers) {
        const ImageSlotState slot = safeThis->m_slots.at(index);
        const QPointer<MediaImageEditCard> card = safeThis->m_cards.at(index);
        if (slot.hasImage() && card)
            co_await safeThis->loadPreview(slot, card, requestGeneration);
    }
}

void MediaImageEditDialog::updateStatusText(const QString& text)
{
    if (m_statusLabel)
        m_statusLabel->setText(text);
}

void MediaImageEditDialog::updateUiState()
{
    for (const auto& card : std::as_const(m_cards)) {
        if (card)
            card->setBusy(m_isBusy || !m_hasImageInfo);
    }
    if (m_retryButton)
        m_retryButton->setEnabled(!m_isBusy);
}
