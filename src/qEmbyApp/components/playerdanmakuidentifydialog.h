#ifndef PLAYERDANMAKUIDENTIFYDIALOG_H
#define PLAYERDANMAKUIDENTIFYDIALOG_H

#include "playeroverlaydialog.h"

#include <models/danmaku/danmakumodels.h>

#include <QList>
#include <QString>
#include <optional>
#include <qcorotask.h>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QResizeEvent;
class QShowEvent;
class QEmbyCore;
class QWidget;
class LoadingOverlay;

class PlayerDanmakuIdentifyDialog : public PlayerOverlayDialog
{
    Q_OBJECT

public:
    explicit PlayerDanmakuIdentifyDialog(QEmbyCore *core,
                                         DanmakuMediaContext context,
                                         QString initialKeyword,
                                         QString activeTargetId,
                                         QString activeEndpointId,
                                         QWidget *parent = nullptr);

    DanmakuMatchCandidate selectedCandidate() const;

protected:
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QCoro::Task<void> searchMatches(QString queryText);
    void triggerSearch();
    void rebuildResultList();
    void applyResultFilter();
    void refreshDetail();
    void updateLoadingOverlayGeometry();
    void updateUiState();
    void updateApplyButtonState();
    void updateStatusText(const QString &text);
    void updateResultStatusText();

    QEmbyCore *m_core = nullptr;
    DanmakuMediaContext m_context;
    QString m_initialKeyword;
    QLabel *m_promptLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_resultListContainer = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QListWidget *m_resultList = nullptr;
    QLabel *m_detailLabel = nullptr;
    LoadingOverlay *m_loadingOverlay = nullptr;
    QPushButton *m_applyButton = nullptr;
    bool m_loaded = false;
    bool m_isLoading = false;
    QList<DanmakuMatchCandidate> m_results;
    QString m_filterText;
    int m_visibleResultCount = 0;
    std::optional<QCoro::Task<void>> m_pendingTask;
    QString m_activeTargetId;
    QString m_activeEndpointId;
};

#endif 
