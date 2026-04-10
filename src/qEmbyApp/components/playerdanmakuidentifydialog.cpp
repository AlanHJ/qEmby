#include "playerdanmakuidentifydialog.h"

#include "loadingoverlay.h"
#include "moderntoast.h"
#include "../managers/thememanager.h"

#include <qembycore.h>
#include <services/danmaku/danmakuservice.h>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSize>
#include <QSignalBlocker>
#include <QtGlobal>
#include <QVBoxLayout>
#include <exception>
#include <utility>

namespace {

constexpr int kDanmakuCandidateRole = Qt::UserRole + 410;

QString providerDisplayName(const QString &provider)
{
    if (provider == QLatin1String("local-file")) {
        return QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                           "Local File");
    }
    if (provider == QLatin1String("dandanplay")) {
        return QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                           "DandanPlay");
    }
    return provider.trimmed().isEmpty() ? QCoreApplication::translate(
                                              "PlayerDanmakuIdentifyDialog",
                                              "Unknown Source")
                                        : provider.trimmed();
}

QString candidateCountText(const DanmakuMatchCandidate &candidate)
{
    return candidate.commentCount > 0
               ? QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                             "%1 comments")
                     .arg(candidate.commentCount)
               : QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                             "Comment count unavailable");
}

QString buildResultDisplayText(const DanmakuMatchCandidate &candidate)
{
    const QString title =
        candidate.displayText().trimmed().isEmpty()
            ? QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                          "Unknown Danmaku")
            : candidate.displayText().trimmed();
    const QString detail = QStringLiteral("%1  |  %2")
                               .arg(providerDisplayName(candidate.provider),
                                    candidateCountText(candidate));
    return title + QStringLiteral("\n") + detail;
}

QString buildDetailText(const DanmakuMatchCandidate &candidate)
{
    QStringList sections;
    sections.append(
        QCoreApplication::translate("PlayerDanmakuIdentifyDialog", "Source: %1")
            .arg(providerDisplayName(candidate.provider)));
    sections.append(
        QCoreApplication::translate("PlayerDanmakuIdentifyDialog", "Title: %1")
            .arg(candidate.title.trimmed().isEmpty()
                     ? QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                                   "Unknown Danmaku")
                     : candidate.title.trimmed()));

    if (!candidate.subtitle.trimmed().isEmpty()) {
        sections.append(
            QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                        "Series: %1")
                .arg(candidate.subtitle.trimmed()));
    }

    sections.append(
        QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                    "Comments: %1")
            .arg(candidate.commentCount > 0
                     ? QString::number(candidate.commentCount)
                     : QCoreApplication::translate(
                           "PlayerDanmakuIdentifyDialog", "Unknown")));

    if (candidate.score > 0.0) {
        sections.append(
            QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                        "Score: %1")
                .arg(QString::number(candidate.score, 'f', 1)));
    }

    if (candidate.seasonNumber > 0 || candidate.episodeNumber > 0) {
        sections.append(
            QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                        "Episode: S%1E%2")
                .arg(qMax(candidate.seasonNumber, 0), 2, 10, QChar('0'))
                .arg(qMax(candidate.episodeNumber, 0), 2, 10, QChar('0')));
    }

    if (!candidate.matchReason.trimmed().isEmpty()) {
        sections.append(
            QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                        "Match Reason: %1")
                .arg(candidate.matchReason.trimmed()));
    }

    if (!candidate.targetId.trimmed().isEmpty()) {
        sections.append(
            QCoreApplication::translate("PlayerDanmakuIdentifyDialog",
                                        "Target: %1")
                .arg(candidate.targetId.trimmed()));
    }

    return sections.join(QStringLiteral("\n"));
}

} 

PlayerDanmakuIdentifyDialog::PlayerDanmakuIdentifyDialog(
    QEmbyCore *core, DanmakuMediaContext context, QString initialKeyword,
    QString activeTargetId, QWidget *parent)
    : PlayerOverlayDialog(parent),
      m_core(core),
      m_context(std::move(context)),
      m_initialKeyword(std::move(initialKeyword)),
      m_activeTargetId(std::move(activeTargetId))
{
    setSurfaceObjectName("playerDanmakuIdentifyDialog");
    setSurfacePreferredSize(QSize(820, 560));
    setTitle(tr("Search Danmaku"));

    const QString itemName =
        m_context.displayTitle().trimmed().isEmpty()
            ? tr("this video")
            : m_context.displayTitle().trimmed();

    m_promptLabel = new QLabel(
        tr("Search danmaku sources for \"%1\" and choose the best match.")
            .arg(itemName),
        this);
    m_promptLabel->setObjectName("dialog-text");
    m_promptLabel->setWordWrap(true);
    contentLayout()->addWidget(m_promptLabel);
    contentLayout()->addSpacing(12);

    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("PlaylistSearchEdit");
    m_searchEdit->setPlaceholderText(tr("Enter title or keyword"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/search.svg")),
        QLineEdit::LeadingPosition);
    
    
    QString defaultKeyword = m_initialKeyword.trimmed();
    if (defaultKeyword.isEmpty()) {
        if (m_context.isEpisode() && !m_context.seriesName.trimmed().isEmpty()) {
            defaultKeyword = m_context.seriesName.trimmed();
        } else {
            defaultKeyword = m_context.title.trimmed();
        }
    }
    m_searchEdit->setText(defaultKeyword);
    searchRow->addWidget(m_searchEdit, 1);

    m_searchButton = new QPushButton(tr("Search"), this);
    m_searchButton->setObjectName("dialog-btn-primary");
    m_searchButton->setCursor(Qt::PointingHandCursor);
    searchRow->addWidget(m_searchButton);

    contentLayout()->addLayout(searchRow);
    contentLayout()->addSpacing(10);

    m_statusLabel = new QLabel(tr("Search for local or online danmaku sources."),
                               this);
    m_statusLabel->setObjectName("dialog-text");
    m_statusLabel->setWordWrap(true);
    contentLayout()->addWidget(m_statusLabel);
    contentLayout()->addSpacing(8);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(20);

    m_resultListContainer = new QWidget(this);
    auto *listLayout = new QVBoxLayout(m_resultListContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_resultList = new QListWidget(m_resultListContainer);
    m_resultList->setObjectName("ManageLibPathList");
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultList->setAlternatingRowColors(false);
    m_resultList->setWordWrap(true);
    m_resultList->setUniformItemSizes(false);
    m_resultList->setMinimumHeight(280);
    listLayout->addWidget(m_resultList);

    m_loadingOverlay = new LoadingOverlay(m_resultListContainer);
    m_loadingOverlay->setHudPanelVisible(false);
    m_loadingOverlay->setSubtleOverlay(true);

    auto *detailPanel = new QWidget(this);
    auto *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->setSpacing(8);

    auto *detailTitle =
        new QLabel(tr("Danmaku Details"), detailPanel);
    detailTitle->setObjectName("dialog-title");
    detailLayout->addWidget(detailTitle);

    m_detailLabel = new QLabel(detailPanel);
    m_detailLabel->setObjectName("dialog-text");
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detailLabel->setMinimumWidth(260);
    m_detailLabel->setText(tr("Select a danmaku source to view details."));
    detailLayout->addWidget(m_detailLabel, 1);

    bodyLayout->addWidget(detailPanel, 0);
    bodyLayout->addWidget(m_resultListContainer, 1);

    contentLayout()->addLayout(bodyLayout, 1);
    contentLayout()->addSpacing(24);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName("dialog-btn-cancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this,
            &PlayerOverlayDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_applyButton = new QPushButton(tr("Load Danmaku"), this);
    m_applyButton->setObjectName("dialog-btn-primary");
    m_applyButton->setCursor(Qt::PointingHandCursor);
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        if (selectedCandidate().isValid()) {
            accept();
        }
    });
    buttonLayout->addWidget(m_applyButton);

    contentLayout()->addLayout(buttonLayout);

    connect(m_searchButton, &QPushButton::clicked, this,
            [this]() { triggerSearch(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this,
            [this]() { triggerSearch(); });
    connect(m_resultList, &QListWidget::itemSelectionChanged, this,
            [this]() {
                refreshDetail();
                updateApplyButtonState();
            });

    updateLoadingOverlayGeometry();
    updateUiState();
}

DanmakuMatchCandidate PlayerDanmakuIdentifyDialog::selectedCandidate() const
{
    const QListWidgetItem *item =
        m_resultList ? m_resultList->currentItem() : nullptr;
    if (!item) {
        return {};
    }

    const QVariant data = item->data(kDanmakuCandidateRole);
    if (!data.canConvert<DanmakuMatchCandidate>()) {
        return {};
    }
    return data.value<DanmakuMatchCandidate>();
}

void PlayerDanmakuIdentifyDialog::showEvent(QShowEvent *event)
{
    PlayerOverlayDialog::showEvent(event);
    updateLoadingOverlayGeometry();
    if (m_searchEdit) {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }

    if (!m_loaded) {
        m_loaded = true;
        triggerSearch();
    }
}

void PlayerDanmakuIdentifyDialog::resizeEvent(QResizeEvent *event)
{
    PlayerOverlayDialog::resizeEvent(event);
    updateLoadingOverlayGeometry();
}

QCoro::Task<void> PlayerDanmakuIdentifyDialog::searchMatches(QString queryText)
{
    QPointer<PlayerDanmakuIdentifyDialog> safeThis(this);
    QPointer<QEmbyCore> core(m_core);
    const DanmakuMediaContext context = m_context;
    queryText = queryText.trimmed();
    if (queryText.isEmpty()) {
        queryText = context.displayTitle().trimmed();
    }
    if (queryText.isEmpty()) {
        queryText = context.title.trimmed();
    }

    if (!safeThis || !core || !core->danmakuService()) {
        co_return;
    }

    m_isLoading = true;
    updateStatusText(tr("Searching..."));
    updateUiState();

    qDebug().noquote()
        << "[Danmaku][IdentifyDialog] Search start"
        << "| mediaId:" << context.mediaId
        << "| keyword:" << queryText;

    try {
        const QList<DanmakuMatchCandidate> results =
            co_await core->danmakuService()->searchAllCandidates(context,
                                                                 queryText);
        if (!safeThis) {
            co_return;
        }

        safeThis->m_results = results;
        safeThis->m_isLoading = false;
        safeThis->rebuildResultList();
        safeThis->updateStatusText(
            results.isEmpty() ? tr("No matches found")
                              : tr("Found %1 matches").arg(results.size()));
        safeThis->updateUiState();

        qDebug().noquote()
            << "[Danmaku][IdentifyDialog] Search finished"
            << "| mediaId:" << context.mediaId
            << "| resultCount:" << results.size();
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_results.clear();
        safeThis->m_isLoading = false;
        safeThis->rebuildResultList();
        safeThis->updateStatusText(tr("Search failed"));
        safeThis->updateUiState();

        qWarning().noquote()
            << "[Danmaku][IdentifyDialog] Search failed"
            << "| mediaId:" << context.mediaId
            << "| keyword:" << queryText
            << "| error:" << e.what();
        ModernToast::showMessage(
            tr("Failed to search danmaku: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

void PlayerDanmakuIdentifyDialog::triggerSearch()
{
    if (!m_searchEdit || m_isLoading) {
        return;
    }
    m_pendingTask = searchMatches(m_searchEdit->text());
}

void PlayerDanmakuIdentifyDialog::rebuildResultList()
{
    if (!m_resultList) {
        return;
    }

    QSignalBlocker blocker(m_resultList);
    m_resultList->clear();

    for (const DanmakuMatchCandidate &candidate : std::as_const(m_results)) {
        auto *item =
            new QListWidgetItem(buildResultDisplayText(candidate), m_resultList);
        item->setData(kDanmakuCandidateRole, QVariant::fromValue(candidate));
        item->setToolTip(buildDetailText(candidate));
        item->setSizeHint(QSize(0, 64));
    }

    if (m_resultList->count() > 0) {
        
        int targetRow = 0;
        if (!m_activeTargetId.isEmpty()) {
            for (int i = 0; i < m_resultList->count(); ++i) {
                const auto data =
                    m_resultList->item(i)->data(kDanmakuCandidateRole);
                if (data.canConvert<DanmakuMatchCandidate>() &&
                    data.value<DanmakuMatchCandidate>().targetId ==
                        m_activeTargetId) {
                    targetRow = i;
                    break;
                }
            }
        }
        m_resultList->setCurrentRow(targetRow);
    }

    refreshDetail();
    updateApplyButtonState();
}

void PlayerDanmakuIdentifyDialog::refreshDetail()
{
    if (!m_detailLabel) {
        return;
    }

    const DanmakuMatchCandidate candidate = selectedCandidate();
    if (!candidate.isValid()) {
        m_detailLabel->setText(tr("Select a danmaku source to view details."));
        return;
    }

    m_detailLabel->setText(buildDetailText(candidate));
}

void PlayerDanmakuIdentifyDialog::updateLoadingOverlayGeometry()
{
    if (!m_loadingOverlay || !m_resultListContainer) {
        return;
    }
    m_loadingOverlay->setGeometry(m_resultListContainer->rect());
}

void PlayerDanmakuIdentifyDialog::updateUiState()
{
    if (m_searchEdit) {
        m_searchEdit->setEnabled(!m_isLoading);
    }
    if (m_searchButton) {
        m_searchButton->setEnabled(!m_isLoading);
    }
    if (m_resultList) {
        m_resultList->setEnabled(!m_isLoading);
    }
    if (m_loadingOverlay) {
        updateLoadingOverlayGeometry();
        if (m_isLoading) {
            m_loadingOverlay->start();
        } else {
            m_loadingOverlay->stop();
        }
    }

    updateApplyButtonState();
}

void PlayerDanmakuIdentifyDialog::updateApplyButtonState()
{
    if (!m_applyButton) {
        return;
    }

    m_applyButton->setEnabled(!m_isLoading && selectedCandidate().isValid());
}

void PlayerDanmakuIdentifyDialog::updateStatusText(const QString &text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}
