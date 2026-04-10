#ifndef MEDIAIMAGEEDITDIALOG_H
#define MEDIAIMAGEEDITDIALOG_H

#include "moderndialogbase.h"

#include <models/admin/adminmodels.h>

#include <QList>
#include <QString>
#include <optional>
#include <qcorotask.h>

class QLabel;
class QListWidget;
class QMimeData;
class QPixmap;
class QPushButton;
class QEvent;
class QShowEvent;
class QEmbyCore;

struct MediaImageEditTarget {
    QString itemId;
    QString imageItemId;
    QString displayName;
    QString itemType;
    QString mediaType;
    QString collectionType;
    bool isLibrary = false;

    bool isValid() const
    {
        return !effectiveImageItemId().trimmed().isEmpty();
    }

    QString effectiveImageItemId() const
    {
        return imageItemId.trimmed().isEmpty() ? itemId.trimmed()
                                               : imageItemId.trimmed();
    }
};

class MediaImageEditDialog : public ModernDialogBase
{
    Q_OBJECT

public:
    explicit MediaImageEditDialog(QEmbyCore* core, MediaImageEditTarget target,
                                  QWidget* parent = nullptr);

    bool hasChanges() const { return m_hasChanges; }

protected:
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct ImageSlotState {
        QString imageType;
        QString displayName;
        QList<ItemImageInfo> images;

        bool hasImage() const { return !images.isEmpty(); }
        int previewImageIndex() const
        {
            return hasImage() ? images.first().imageIndex : -1;
        }
    };

    struct DroppedImagePayload {
        QString localFilePath;
        QString remoteImageUrl;
        QByteArray imageData;
        QString mimeType;

        bool isValid() const
        {
            return !localFilePath.trimmed().isEmpty() ||
                   !remoteImageUrl.trimmed().isEmpty() || !imageData.isEmpty();
        }
    };

    QCoro::Task<void> reloadImages(QString preferredType = QString());
    QCoro::Task<void> loadPreview(ImageSlotState slot,
                                  quint64 requestGeneration);
    QCoro::Task<void> chooseAndUploadImage();
    QCoro::Task<void> promptAndUploadImageUrl();
    QCoro::Task<void> saveSelectedImage();
    QCoro::Task<void> uploadLocalImageFile(QString filePath);
    QCoro::Task<void> uploadRemoteImage(QString imageUrl);
    QCoro::Task<void> uploadImageData(ImageSlotState slot,
                                      QByteArray imageData, QString mimeType);
    QCoro::Task<void> handleDroppedImage(DroppedImagePayload payload);
    QCoro::Task<void> removeSelectedImage();

    const ImageSlotState* currentSlot() const;
    bool canAcceptDroppedImage(const QMimeData* mimeData) const;
    DroppedImagePayload parseDroppedImagePayload(const QMimeData* mimeData) const;
    void rebuildTypeList(QString preferredType = QString());
    void refreshPreview();
    void setPreviewDropActive(bool active);
    void updatePreviewMessage(const QString& text);
    void updatePreviewPixmap(const QPixmap& pixmap);
    void updatePreviewDetails();
    void updateStatusText(const QString& text);
    void updateUiState();

    QEmbyCore* m_core = nullptr;
    MediaImageEditTarget m_target;

    QLabel* m_promptLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_previewInfoLabel = nullptr;
    QListWidget* m_typeList = nullptr;
    QPushButton* m_chooseButton = nullptr;
    QPushButton* m_urlButton = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_removeButton = nullptr;

    bool m_loaded = false;
    bool m_isBusy = false;
    bool m_hasChanges = false;
    bool m_isPreviewDropActive = false;
    quint64 m_previewLoadGeneration = 0;
    QList<ItemImageInfo> m_itemImages;
    QList<ImageSlotState> m_slots;
    std::optional<QCoro::Task<void>> m_pendingTask;
    std::optional<QCoro::Task<void>> m_pendingPreviewTask;
};

#endif 
