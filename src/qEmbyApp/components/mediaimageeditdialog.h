#ifndef MEDIAIMAGEEDITDIALOG_H
#define MEDIAIMAGEEDITDIALOG_H

#include "moderndialogbase.h"
#include "mediaimageeditcard.h"

#include <models/admin/adminmodels.h>

#include <QList>
#include <QString>
#include <QPointer>
#include <optional>
#include <qcorotask.h>

class QLabel;
class QScrollArea;
class FlowLayout;
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

    QCoro::Task<void> reloadImages();
    QCoro::Task<void> loadPreview(ImageSlotState slot,
                                  QPointer<MediaImageEditCard> card,
                                  quint64 requestGeneration);
    QCoro::Task<void> loadPreviews(int worker, quint64 requestGeneration);
    QCoro::Task<void> chooseAndUploadImage(ImageSlotState slot);
    QCoro::Task<void> viewImage(ImageSlotState slot);
    QCoro::Task<void> promptAndUploadImageUrl(ImageSlotState slot);
    QCoro::Task<void> saveImage(ImageSlotState slot);
    QCoro::Task<void> uploadLocalImageFile(ImageSlotState slot, QString filePath);
    QCoro::Task<void> uploadRemoteImage(ImageSlotState slot, QString imageUrl);
    QCoro::Task<void> uploadImageData(ImageSlotState slot,
                                      QByteArray imageData, QString mimeType);
    QCoro::Task<void> handleDroppedImage(ImageSlotState slot, DroppedImagePayload payload);
    QCoro::Task<void> removeImage(ImageSlotState slot);

    bool canAcceptDroppedImage(const QMimeData* mimeData) const;
    DroppedImagePayload parseDroppedImagePayload(const QMimeData* mimeData) const;
    void rebuildCards();
    void startPreviews();
    void handleCardAction(int index, MediaImageEditCard::Action action);
    void invalidateEditedImages(QString imageType);
    void updateStatusText(const QString& text);
    void updateUiState();

    QEmbyCore* m_core = nullptr;
    MediaImageEditTarget m_target;

    QLabel* m_promptLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    FlowLayout* m_imagesLayout = nullptr;
    FlowLayout* m_backdropsLayout = nullptr;
    QWidget* m_backdropsSection = nullptr;
    QPushButton* m_retryButton = nullptr;
    QList<QPointer<MediaImageEditCard>> m_cards;

    bool m_loaded = false;
    bool m_hasImageInfo = false;
    bool m_isBusy = false;
    bool m_hasChanges = false;
    quint64 m_previewLoadGeneration = 0;
    QList<ItemImageInfo> m_itemImages;
    QList<ImageSlotState> m_slots;
    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
