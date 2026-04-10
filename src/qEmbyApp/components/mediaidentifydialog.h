#ifndef MEDIAIDENTIFYDIALOG_H
#define MEDIAIDENTIFYDIALOG_H

#include "moderndialogbase.h"

#include <models/admin/adminmodels.h>
#include <models/media/mediaitem.h>

#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QString>
#include <QtGlobal>
#include <optional>
#include <qcorotask.h>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPixmap;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QSpinBox;
class QEmbyCore;
class QWidget;
class LoadingOverlay;

class MediaIdentifyDialog : public ModernDialogBase
{
    Q_OBJECT

public:
    explicit MediaIdentifyDialog(QEmbyCore* core, MediaItem item,
                                 QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QCoro::Task<void> searchMatches(QString queryText, int productionYear,
                                    QJsonObject providerIds);
    QCoro::Task<void> applySelectedResult();
    QCoro::Task<void> loadPreviewImage(RemoteSearchResult result,
                                       quint64 requestGeneration);

    QJsonObject collectProviderIds() const;
    void syncProviderEditHeights();
    RemoteSearchResult selectedResult() const;
    void triggerSearch();
    void rebuildResultList();
    void refreshPreview();
    void updateLoadingOverlayGeometry();
    void updateUiState();
    void updateApplyButtonState();
    void updatePreviewMessage(const QString& text);
    void updatePreviewPixmap(const QPixmap& pixmap);
    void updateStatusText(const QString& text);

    QEmbyCore* m_core = nullptr;
    MediaItem m_item;
    QString m_remoteSearchType;

    QLabel* m_promptLabel = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLineEdit* m_imdbIdEdit = nullptr;
    QLineEdit* m_movieDbIdEdit = nullptr;
    QLineEdit* m_tvdbIdEdit = nullptr;
    QSpinBox* m_yearSpin = nullptr;
    QPushButton* m_searchButton = nullptr;
    QLabel* m_statusLabel = nullptr;
    QWidget* m_resultListContainer = nullptr;
    QListWidget* m_resultList = nullptr;
    QLabel* m_previewLabel = nullptr;
    LoadingOverlay* m_loadingOverlay = nullptr;
    QPushButton* m_applyButton = nullptr;

    bool m_loaded = false;
    bool m_isLoading = false;
    quint64 m_previewLoadGeneration = 0;
    QList<RemoteSearchResult> m_results;
    std::optional<QCoro::Task<void>> m_pendingTask;
    std::optional<QCoro::Task<void>> m_pendingPreviewTask;
};

#endif 
