#include "mediaimageeditdialog.h"

#include "modernmessagebox.h"
#include "moderntoast.h"
#include "textinputdialog.h"
#include "../utils/imageutils.h"

#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>

#include <QAbstractItemView>
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeDatabase>
#include <QMimeData>
#include <QMimeType>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSaveFile>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kImageTypeRole = Qt::UserRole + 420;
const QSize kPreviewSize(240, 360);
constexpr int kPreviewPanelWidth = 292;

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

    return {QStringLiteral("Primary"),    QStringLiteral("Thumb"),
            QStringLiteral("Backdrop"),   QStringLiteral("Banner"),
            QStringLiteral("Logo"),       QStringLiteral("Art"),
            QStringLiteral("Disc"),       QStringLiteral("Box"),
            QStringLiteral("BoxRear"),    QStringLiteral("Menu"),
            QStringLiteral("Screenshot"), QStringLiteral("Chapter")};
}

QString buildListItemText(const QString& displayName,
                         const QList<ItemImageInfo>& images)
{
    QStringList lines;
    lines.append(displayName);

    if (images.isEmpty()) {
        lines.append(
            QCoreApplication::translate("MediaImageEditDialog", "No image"));
        return lines.join(QLatin1Char('\n'));
    }

    const int imageCount = images.size();
    const ItemImageInfo& previewImage = images.first();
    QString summary = QCoreApplication::translate(
                          "MediaImageEditDialog", "%1 image(s) available")
                          .arg(imageCount);
    if (previewImage.width > 0 && previewImage.height > 0) {
        summary +=
            QStringLiteral("  ·  %1 × %2")
                .arg(previewImage.width)
                .arg(previewImage.height);
    }
    lines.append(summary);
    return lines.join(QLatin1Char('\n'));
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
    segments.append(QCoreApplication::translate("MediaImageEditDialog",
                                                "%1 image(s)")
                        .arg(images.size()));

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

    if (info.imageIndex >= 0) {
        segments.append(QCoreApplication::translate("MediaImageEditDialog",
                                                    "Index %1")
                            .arg(info.imageIndex));
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
    resize(820, 620);
    setMinimumSize(760, 600);
    setTitle(tr("Edit Images"));
    contentLayout()->setSpacing(0);

    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item")
                                    : m_target.displayName.trimmed();

    m_promptLabel =
        new QLabel(tr("Manage image types for \"%1\". Changes are applied immediately.")
                       .arg(displayName),
                   this);
    m_promptLabel->setObjectName("MediaImageEditIntroText");
    m_promptLabel->setWordWrap(true);
    contentLayout()->addWidget(m_promptLabel);
    contentLayout()->addSpacing(18);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(24);

    auto* previewColumn = new QWidget(this);
    auto* previewColumnLayout = new QVBoxLayout(previewColumn);
    previewColumnLayout->setContentsMargins(0, 0, 0, 0);
    previewColumnLayout->setSpacing(0);

    auto* previewPanel = new QWidget(previewColumn);
    previewPanel->setFixedWidth(kPreviewPanelWidth);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(14);

    m_previewLabel = new QLabel(previewPanel);
    m_previewLabel->setObjectName("identify-preview-label");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setFixedSize(kPreviewSize);
    m_previewLabel->setAcceptDrops(true);
    m_previewLabel->setProperty("drag-over", false);
    m_previewLabel->setProperty("media-image-editor", true);
    m_previewLabel->setToolTip(
        tr("Drop an image file or image URL here to replace it."));
    m_previewLabel->installEventFilter(this);
    previewLayout->addWidget(m_previewLabel, 0,
                             Qt::AlignTop | Qt::AlignHCenter);

    m_previewInfoLabel = new QLabel(previewPanel);
    m_previewInfoLabel->setObjectName("MediaImageEditMetaText");
    m_previewInfoLabel->setAlignment(Qt::AlignCenter);
    m_previewInfoLabel->setWordWrap(true);
    m_previewInfoLabel->setFixedWidth(kPreviewPanelWidth);
    previewLayout->addWidget(m_previewInfoLabel, 0,
                             Qt::AlignTop | Qt::AlignHCenter);

    auto* actionPanel = new QWidget(previewPanel);
    actionPanel->setFixedWidth(kPreviewPanelWidth);
    auto* actionLayout = new QGridLayout(actionPanel);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setHorizontalSpacing(10);
    actionLayout->setVerticalSpacing(10);
    actionLayout->setColumnStretch(0, 1);
    actionLayout->setColumnStretch(1, 1);

    m_chooseButton = new QPushButton(tr("Choose Image"), previewPanel);
    m_chooseButton->setObjectName("dialog-btn-primary");
    m_chooseButton->setCursor(Qt::PointingHandCursor);
    m_chooseButton->setMinimumHeight(34);
    m_chooseButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionLayout->addWidget(m_chooseButton, 0, 0);

    m_urlButton = new QPushButton(tr("Image URL"), previewPanel);
    m_urlButton->setObjectName("dialog-btn-cancel");
    m_urlButton->setCursor(Qt::PointingHandCursor);
    m_urlButton->setMinimumHeight(34);
    m_urlButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionLayout->addWidget(m_urlButton, 0, 1);

    m_downloadButton = new QPushButton(tr("Download"), previewPanel);
    m_downloadButton->setObjectName("dialog-btn-cancel");
    m_downloadButton->setCursor(Qt::PointingHandCursor);
    m_downloadButton->setMinimumHeight(34);
    m_downloadButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionLayout->addWidget(m_downloadButton, 1, 0);

    m_removeButton = new QPushButton(tr("Remove Image"), previewPanel);
    m_removeButton->setObjectName("dialog-btn-danger");
    m_removeButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setMinimumHeight(34);
    m_removeButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    actionLayout->addWidget(m_removeButton, 1, 1);

    previewLayout->addWidget(actionPanel, 0, Qt::AlignHCenter);
    previewLayout->addStretch();
    previewColumnLayout->addWidget(previewPanel, 0, Qt::AlignTop | Qt::AlignHCenter);
    previewColumnLayout->addStretch();

    auto* listPanel = new QWidget(this);
    listPanel->setObjectName("MediaImageEditSidePanel");
    listPanel->setFixedWidth(286);
    auto* listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(14, 14, 2, 12);
    listLayout->setSpacing(8);

    auto* listTitle = new QLabel(tr("Image Types"), listPanel);
    listTitle->setObjectName("MediaImageEditSectionTitle");
    listLayout->addWidget(listTitle);

    m_statusLabel = new QLabel(tr("Loading image information..."), listPanel);
    m_statusLabel->setObjectName("MediaImageEditSideStatus");
    m_statusLabel->setWordWrap(true);
    listLayout->addWidget(m_statusLabel);

    m_typeList = new QListWidget(listPanel);
    m_typeList->setObjectName("ManageLibPathList");
    m_typeList->setProperty("media-image-list", true);
    m_typeList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_typeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_typeList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_typeList->setWordWrap(true);
    m_typeList->setMinimumHeight(420);
    listLayout->addWidget(m_typeList, 1);

    bodyLayout->addWidget(previewColumn, 1);
    bodyLayout->addWidget(listPanel, 0, Qt::AlignTop);
    bodyLayout->setStretch(0, 1);
    bodyLayout->setStretch(1, 0);

    contentLayout()->addLayout(bodyLayout);

    connect(m_typeList, &QListWidget::itemSelectionChanged, this,
            [this]() {
                refreshPreview();
                updateUiState();
            });
    connect(m_chooseButton, &QPushButton::clicked, this,
            [this]() { m_pendingTask = chooseAndUploadImage(); });
    connect(m_urlButton, &QPushButton::clicked, this,
            [this]() { m_pendingTask = promptAndUploadImageUrl(); });
    connect(m_downloadButton, &QPushButton::clicked, this,
            [this]() { m_pendingTask = saveSelectedImage(); });
    connect(m_removeButton, &QPushButton::clicked, this,
            [this]() { m_pendingTask = removeSelectedImage(); });

    updatePreviewMessage(tr("Preview unavailable"));
    updatePreviewDetails();
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
    if (watched == m_previewLabel && event) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (canAcceptDroppedImage(dragEvent->mimeData())) {
                dragEvent->acceptProposedAction();
                setPreviewDropActive(true);
            } else {
                dragEvent->ignore();
                setPreviewDropActive(false);
            }
            return true;
        }
        case QEvent::DragMove: {
            auto* dragEvent = static_cast<QDragMoveEvent*>(event);
            if (canAcceptDroppedImage(dragEvent->mimeData())) {
                dragEvent->acceptProposedAction();
                setPreviewDropActive(true);
            } else {
                dragEvent->ignore();
                setPreviewDropActive(false);
            }
            return true;
        }
        case QEvent::DragLeave:
            setPreviewDropActive(false);
            return true;
        case QEvent::Drop: {
            auto* dropEvent = static_cast<QDropEvent*>(event);
            setPreviewDropActive(false);

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

            dropEvent->acceptProposedAction();
            m_pendingTask = handleDroppedImage(payload);
            return true;
        }
        default:
            break;
        }
    }

    return ModernDialogBase::eventFilter(watched, event);
}

QCoro::Task<void> MediaImageEditDialog::reloadImages(QString preferredType)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    if (!safeThis || !adminService || !m_target.isValid()) {
        co_return;
    }

    m_isBusy = true;
    ++m_previewLoadGeneration;
    updatePreviewMessage(tr("Loading preview..."));
    updatePreviewDetails();
    updateStatusText(tr("Loading image information..."));
    updateUiState();

    try {
        const QList<ItemImageInfo> images =
            co_await adminService->getItemImages(m_target.effectiveImageItemId());
        if (!safeThis) {
            co_return;
        }

        safeThis->m_itemImages = images;
        safeThis->rebuildTypeList(preferredType);

        int availableTypeCount = 0;
        for (const ImageSlotState& slot : std::as_const(safeThis->m_slots)) {
            if (slot.hasImage()) {
                ++availableTypeCount;
            }
        }

        safeThis->updateStatusText(
            availableTypeCount > 0
                ? tr("Loaded images for %1 type(s)").arg(availableTypeCount)
                : tr("No images found yet"));
        safeThis->m_isBusy = false;
        safeThis->refreshPreview();
        safeThis->updateUiState();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isBusy = false;
        safeThis->m_itemImages.clear();
        safeThis->m_slots.clear();
        safeThis->rebuildTypeList(preferredType);
        safeThis->updatePreviewMessage(tr("Preview unavailable"));
        safeThis->updatePreviewDetails();
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
                                                    quint64 requestGeneration)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);

    if (!safeThis || !mediaService || !slot.hasImage()) {
        co_return;
    }

    try {
        const QPixmap pixmap =
            co_await mediaService->fetchImage(m_target.effectiveImageItemId(),
                                              slot.imageType, QString(),
                                              kPreviewSize.width() * 2,
                                              slot.previewImageIndex());
        if (!safeThis || requestGeneration != m_previewLoadGeneration) {
            co_return;
        }

        if (pixmap.isNull()) {
            safeThis->updatePreviewMessage(tr("Preview unavailable"));
            co_return;
        }

        safeThis->updatePreviewPixmap(pixmap);
    } catch (const std::exception& e) {
        if (!safeThis || requestGeneration != m_previewLoadGeneration) {
            co_return;
        }

        qWarning() << "[MediaImageEditDialog] Failed to load image preview"
                   << "| itemId=" << safeThis->m_target.effectiveImageItemId()
                   << "| imageType=" << slot.imageType
                   << "| imageIndex=" << slot.previewImageIndex()
                   << "| error=" << e.what();
        safeThis->updatePreviewMessage(tr("Preview unavailable"));
    }
}

QCoro::Task<void> MediaImageEditDialog::chooseAndUploadImage()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Image"), QString(), buildImageFileFilter());
    if (filePath.trimmed().isEmpty()) {
        co_return;
    }

    co_await uploadLocalImageFile(filePath);
}

QCoro::Task<void> MediaImageEditDialog::promptAndUploadImageUrl()
{
    const ImageSlotState* current = currentSlot();
    if (!current) {
        co_return;
    }

    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item")
                                    : m_target.displayName.trimmed();
    bool accepted = false;
    const QString imageUrl = TextInputDialog::getText(
        this, tr("Enter Image URL"),
        tr("Paste an image address for the %1 image of \"%2\".")
            .arg(current->displayName, displayName),
        QString(), tr("https://example.com/image.jpg"),
        tr("Replace Image"), &accepted);
    if (!accepted) {
        co_return;
    }

    co_await uploadRemoteImage(imageUrl);
}

QCoro::Task<void> MediaImageEditDialog::saveSelectedImage()
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    if (!safeThis || !mediaService) {
        co_return;
    }

    const ImageSlotState* current = currentSlot();
    if (!current || !current->hasImage()) {
        co_return;
    }

    const ImageSlotState slot = *current;
    const ItemImageInfo previewImage = slot.images.first();
    const QString defaultPath =
        QDir(defaultDownloadDirectory())
            .filePath(defaultImageSaveName(m_target, slot.displayName,
                                           &previewImage));
    const QString savePath = QFileDialog::getSaveFileName(
        this, tr("Save Image"), defaultPath, buildImageFileFilter());
    if (savePath.trimmed().isEmpty()) {
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

QCoro::Task<void> MediaImageEditDialog::uploadLocalImageFile(QString filePath)
{
    const ImageSlotState* current = currentSlot();
    if (!current) {
        co_return;
    }

    const ImageSlotState slot = *current;
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

QCoro::Task<void> MediaImageEditDialog::uploadRemoteImage(QString imageUrl)
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    if (!safeThis || !mediaService) {
        co_return;
    }

    const ImageSlotState* current = currentSlot();
    if (!current) {
        co_return;
    }

    const ImageSlotState slot = *current;
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
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
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
    ++m_previewLoadGeneration;
    updateStatusText(tr("Uploading image..."));
    updateUiState();

    try {
        co_await adminService->uploadItemImage(
            m_target.effectiveImageItemId(), slot.imageType, uploadData,
            uploadMimeType, slot.previewImageIndex());
        if (!safeThis) {
            co_return;
        }

        if (mediaService) {
            mediaService->invalidateImageCache(
                m_target.effectiveImageItemId(), slot.imageType,
                slot.previewImageIndex());
            if (m_target.itemId.trimmed() !=
                m_target.effectiveImageItemId().trimmed()) {
                mediaService->invalidateImageCache(
                    m_target.itemId, slot.imageType,
                    slot.previewImageIndex());
            }
        }

        safeThis->m_hasChanges = true;
        ModernToast::showMessage(tr("Image updated"), 2000);
        co_await safeThis->reloadImages(slot.imageType);
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
            3000);
    }
}

QCoro::Task<void> MediaImageEditDialog::handleDroppedImage(
    DroppedImagePayload payload)
{
    if (!payload.localFilePath.trimmed().isEmpty()) {
        qDebug() << "[MediaImageEditDialog] Handling dropped local image file"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| filePath=" << payload.localFilePath;
        co_await uploadLocalImageFile(payload.localFilePath);
        co_return;
    }

    if (!payload.imageData.isEmpty()) {
        const ImageSlotState* current = currentSlot();
        if (!current) {
            co_return;
        }

        qDebug() << "[MediaImageEditDialog] Uploading dropped in-memory image"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| imageType=" << current->imageType
                 << "| imageIndex=" << current->previewImageIndex()
                 << "| mimeType=" << payload.mimeType
                 << "| bytes=" << payload.imageData.size();
        co_await uploadImageData(*current, payload.imageData,
                                 payload.mimeType);
        co_return;
    }

    if (!payload.remoteImageUrl.trimmed().isEmpty()) {
        qDebug() << "[MediaImageEditDialog] Handling dropped remote image url"
                 << "| itemId=" << m_target.effectiveImageItemId()
                 << "| url=" << displayUrlForLog(payload.remoteImageUrl);
        co_await uploadRemoteImage(payload.remoteImageUrl);
    }
}

QCoro::Task<void> MediaImageEditDialog::removeSelectedImage()
{
    QPointer<MediaImageEditDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    if (!safeThis || !adminService) {
        co_return;
    }

    const ImageSlotState* current = currentSlot();
    if (!current || !current->hasImage()) {
        co_return;
    }

    const ImageSlotState slot = *current;
    const QString displayName = m_target.displayName.trimmed().isEmpty()
                                    ? tr("this item")
                                    : m_target.displayName.trimmed();
    const bool confirmed = ModernMessageBox::question(
        this, tr("Remove Image"),
        tr("Are you sure you want to remove the %1 image from \"%2\"?")
            .arg(slot.displayName, displayName),
        tr("Remove"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!confirmed) {
        co_return;
    }

    m_isBusy = true;
    ++m_previewLoadGeneration;
    updateStatusText(tr("Removing image..."));
    updateUiState();

    try {
        co_await adminService->deleteItemImage(m_target.effectiveImageItemId(),
                                               slot.imageType,
                                               slot.previewImageIndex());
        if (!safeThis) {
            co_return;
        }

        if (mediaService) {
            mediaService->invalidateImageCache(
                m_target.effectiveImageItemId(), slot.imageType,
                slot.previewImageIndex());
            if (m_target.itemId.trimmed() !=
                m_target.effectiveImageItemId().trimmed()) {
                mediaService->invalidateImageCache(
                    m_target.itemId, slot.imageType,
                    slot.previewImageIndex());
            }
        }

        safeThis->m_hasChanges = true;
        ModernToast::showMessage(tr("Image removed"), 2000);
        co_await safeThis->reloadImages(slot.imageType);
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

const MediaImageEditDialog::ImageSlotState* MediaImageEditDialog::currentSlot() const
{
    const QListWidgetItem* currentItem =
        m_typeList ? m_typeList->currentItem() : nullptr;
    if (!currentItem) {
        return nullptr;
    }

    const QString imageType =
        currentItem->data(kImageTypeRole).toString().trimmed();
    if (imageType.isEmpty()) {
        return nullptr;
    }

    for (const ImageSlotState& slot : m_slots) {
        if (slot.imageType == imageType) {
            return &slot;
        }
    }

    return nullptr;
}

bool MediaImageEditDialog::canAcceptDroppedImage(const QMimeData* mimeData) const
{
    if (!mimeData || m_isBusy || !currentSlot()) {
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

void MediaImageEditDialog::rebuildTypeList(QString preferredType)
{
    if (!m_typeList) {
        return;
    }

    QHash<QString, QList<ItemImageInfo>> imageMap;
    for (const ItemImageInfo& image : std::as_const(m_itemImages)) {
        const QString imageType = image.imageType.trimmed();
        if (imageType.isEmpty()) {
            continue;
        }
        imageMap[imageType].append(image);
    }

    for (auto it = imageMap.begin(); it != imageMap.end(); ++it) {
        std::sort(it.value().begin(), it.value().end(),
                  [](const ItemImageInfo& lhs, const ItemImageInfo& rhs) {
                      const int leftIndex = lhs.imageIndex >= 0 ? lhs.imageIndex
                                                                : std::numeric_limits<int>::max();
                      const int rightIndex = rhs.imageIndex >= 0 ? rhs.imageIndex
                                                                 : std::numeric_limits<int>::max();
                      if (leftIndex != rightIndex) {
                          return leftIndex < rightIndex;
                      }
                      return lhs.path < rhs.path;
                  });
    }

    m_slots.clear();
    const QStringList preferredOrder = preferredImageTypeOrder(m_target);
    for (const QString& imageType : preferredOrder) {
        ImageSlotState slot;
        slot.imageType = imageType;
        slot.displayName = imageTypeLabel(imageType);
        slot.images = imageMap.take(imageType);
        m_slots.append(slot);
    }

    QStringList remainingTypes = imageMap.keys();
    std::sort(remainingTypes.begin(), remainingTypes.end());
    for (const QString& imageType : remainingTypes) {
        ImageSlotState slot;
        slot.imageType = imageType;
        slot.displayName = imageTypeLabel(imageType);
        slot.images = imageMap.value(imageType);
        m_slots.append(slot);
    }

    QSignalBlocker blocker(m_typeList);
    m_typeList->clear();

    int preferredIndex = -1;
    int firstAvailableIndex = -1;
    for (int i = 0; i < m_slots.size(); ++i) {
        const ImageSlotState& slot = m_slots.at(i);
        auto* item = new QListWidgetItem(
            buildListItemText(slot.displayName, slot.images), m_typeList);
        item->setData(kImageTypeRole, slot.imageType);
        item->setSizeHint(QSize(0, 60));

        if (preferredType == slot.imageType) {
            preferredIndex = i;
        }
        if (firstAvailableIndex < 0 && slot.hasImage()) {
            firstAvailableIndex = i;
        }
    }

    if (preferredIndex >= 0) {
        m_typeList->setCurrentRow(preferredIndex);
    } else if (firstAvailableIndex >= 0) {
        m_typeList->setCurrentRow(firstAvailableIndex);
    } else if (m_typeList->count() > 0) {
        m_typeList->setCurrentRow(0);
    }
}

void MediaImageEditDialog::refreshPreview()
{
    ++m_previewLoadGeneration;
    updatePreviewDetails();

    const ImageSlotState* slot = currentSlot();
    if (!slot) {
        updatePreviewMessage(tr("Preview unavailable"));
        return;
    }

    if (!slot->hasImage()) {
        updatePreviewMessage(tr("No image for this type"));
        return;
    }

    updatePreviewMessage(tr("Loading preview..."));
    m_pendingPreviewTask = loadPreview(*slot, m_previewLoadGeneration);
}

void MediaImageEditDialog::setPreviewDropActive(bool active)
{
    if (!m_previewLabel || m_isPreviewDropActive == active) {
        return;
    }

    m_isPreviewDropActive = active;
    m_previewLabel->setProperty("drag-over", active);
    m_previewLabel->style()->unpolish(m_previewLabel);
    m_previewLabel->style()->polish(m_previewLabel);
    m_previewLabel->update();
}

void MediaImageEditDialog::updatePreviewMessage(const QString& text)
{
    if (!m_previewLabel) {
        return;
    }

    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setText(text);
}

void MediaImageEditDialog::updatePreviewPixmap(const QPixmap& pixmap)
{
    if (!m_previewLabel) {
        return;
    }

    const QPixmap scaled = ImageUtils::containRoundedPixmap(
        pixmap, m_previewLabel->size(), 12);
    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(scaled);
}

void MediaImageEditDialog::updatePreviewDetails()
{
    if (!m_previewInfoLabel) {
        return;
    }

    const ImageSlotState* slot = currentSlot();
    if (!slot) {
        m_previewInfoLabel->clear();
        return;
    }

    if (!slot->hasImage()) {
        m_previewInfoLabel->setText(
            tr("No image uploaded for this type.\nChoose a file, paste an image URL, or drag one onto the preview."));
        return;
    }

    m_previewInfoLabel->setText(buildPreviewDetails(slot->images));
}

void MediaImageEditDialog::updateStatusText(const QString& text)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setText(text);
}

void MediaImageEditDialog::updateUiState()
{
    const ImageSlotState* slot = currentSlot();
    const bool hasSelection = slot != nullptr;
    const bool hasImage = slot && slot->hasImage();

    if (m_typeList) {
        m_typeList->setEnabled(!m_isBusy);
    }
    if (m_chooseButton) {
        m_chooseButton->setEnabled(!m_isBusy && hasSelection);
    }
    if (m_urlButton) {
        m_urlButton->setEnabled(!m_isBusy && hasSelection);
    }
    if (m_downloadButton) {
        m_downloadButton->setEnabled(!m_isBusy && hasImage);
    }
    if (m_removeButton) {
        m_removeButton->setEnabled(!m_isBusy && hasImage);
    }
}
