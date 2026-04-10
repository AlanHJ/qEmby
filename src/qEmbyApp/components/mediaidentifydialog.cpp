#include "mediaidentifydialog.h"

#include "loadingoverlay.h"
#include "moderntoast.h"
#include "../managers/thememanager.h"
#include "../utils/mediaidentifyutils.h"

#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QVariant>
#include <QVBoxLayout>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kRemoteSearchResultRole = Qt::UserRole + 300;
const QSize kIdentifyPreviewSize(220, 330);

QString buildResultDisplayText(const RemoteSearchResult& result)
{
    QStringList lines;
    lines.append(result.name.trimmed().isEmpty()
                     ? QCoreApplication::translate("MediaIdentifyDialog",
                                                   "Unknown Title")
                     : result.name.trimmed());

    const QString secondaryText = result.secondaryText();
    if (!secondaryText.isEmpty()) {
        lines.append(secondaryText);
    }

    return lines.join(QLatin1Char('\n'));
}

QString buildResultToolTip(const RemoteSearchResult& result)
{
    QStringList sections;

    const QString providerSummary = result.providerIdSummary();
    if (!providerSummary.isEmpty()) {
        sections.append(
            QCoreApplication::translate("MediaIdentifyDialog",
                                        "Provider IDs: %1")
                .arg(providerSummary));
    }

    if (!result.albumArtistName.trimmed().isEmpty()) {
        sections.append(QCoreApplication::translate("MediaIdentifyDialog",
                                                    "Album Artist: %1")
                            .arg(result.albumArtistName.trimmed()));
    }

    if (!result.artistNames.isEmpty()) {
        sections.append(QCoreApplication::translate("MediaIdentifyDialog",
                                                    "Artists: %1")
                            .arg(result.artistNames.join(QStringLiteral(", "))));
    }

    if (!result.overview.trimmed().isEmpty()) {
        sections.append(result.overview.trimmed());
    }

    return sections.join(QStringLiteral("\n\n"));
}

QLineEdit* createOptionalProviderIdEdit(QWidget* parent,
                                        const QString& placeholderText)
{
    auto* edit = new QLineEdit(parent);
    edit->setObjectName("PlaylistSearchEdit");
    edit->setPlaceholderText(placeholderText);
    edit->setClearButtonEnabled(true);
    return edit;
}

QString buildProviderIdSummary(const QJsonObject& providerIds)
{
    QStringList pairs;
    const QStringList keys = providerIds.keys();
    for (const QString& key : keys) {
        const QString value = providerIds.value(key).toString().trimmed();
        if (!key.trimmed().isEmpty() && !value.isEmpty()) {
            pairs.append(QStringLiteral("%1=%2").arg(key.trimmed(), value));
        }
    }

    return pairs.join(QStringLiteral(", "));
}

} 

MediaIdentifyDialog::MediaIdentifyDialog(QEmbyCore* core, MediaItem item,
                                         QWidget* parent)
    : ModernDialogBase(parent),
      m_core(core),
      m_item(std::move(item)),
      m_remoteSearchType(MediaIdentifyUtils::identifySearchType(m_item))
{
    setMinimumWidth(760);
    resize(760, 560);
    setTitle(tr("Identify"));
    contentLayout()->setSpacing(0);

    const QString itemName =
        m_item.name.trimmed().isEmpty() ? tr("this item") : m_item.name.trimmed();

    m_promptLabel = new QLabel(
        tr("Search remote metadata matches for \"%1\" and apply the correct result.")
            .arg(itemName),
        this);
    m_promptLabel->setObjectName("dialog-text");
    m_promptLabel->setWordWrap(true);
    m_promptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_promptLabel->setCursor(Qt::IBeamCursor);
    contentLayout()->addWidget(m_promptLabel);
    contentLayout()->addSpacing(12);

    auto* searchRow = new QHBoxLayout();
    searchRow->setSpacing(12);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("PlaylistSearchEdit");
    m_searchEdit->setPlaceholderText(tr("Enter title or keyword"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/search.svg")),
        QLineEdit::LeadingPosition);
    m_searchEdit->setText(MediaIdentifyUtils::defaultSearchText(m_item));
    searchRow->addWidget(m_searchEdit, 1);

    m_yearSpin = new QSpinBox(this);
    m_yearSpin->setObjectName("ManageLibSpinBox");
    m_yearSpin->setRange(0, 9999);
    m_yearSpin->setSpecialValueText(tr("Any"));
    m_yearSpin->setValue(m_item.productionYear > 0 ? m_item.productionYear : 0);
    m_yearSpin->setMinimumWidth(96);
    searchRow->addWidget(m_yearSpin);

    m_searchButton = new QPushButton(tr("Search"), this);
    m_searchButton->setObjectName("dialog-btn-primary");
    m_searchButton->setCursor(Qt::PointingHandCursor);

    contentLayout()->addLayout(searchRow);

    auto* providerIdRow = new QHBoxLayout();
    providerIdRow->setContentsMargins(0, 0, 0, 0);
    providerIdRow->setSpacing(12);

    m_imdbIdEdit =
        createOptionalProviderIdEdit(this, tr("IMDb Id (optional)"));
    providerIdRow->addWidget(m_imdbIdEdit, 1);

    m_movieDbIdEdit =
        createOptionalProviderIdEdit(this, tr("MovieDb Id (optional)"));
    providerIdRow->addWidget(m_movieDbIdEdit, 1);

    m_tvdbIdEdit =
        createOptionalProviderIdEdit(this, tr("TheTVDB Id (optional)"));
    providerIdRow->addWidget(m_tvdbIdEdit, 1);

    providerIdRow->addWidget(m_searchButton);

    contentLayout()->addSpacing(8);
    contentLayout()->addLayout(providerIdRow);
    contentLayout()->addSpacing(10);

    m_statusLabel = new QLabel(tr("Enter a title or provider ID to search."),
                               this);
    m_statusLabel->setObjectName("dialog-text");
    m_statusLabel->setWordWrap(true);
    contentLayout()->addWidget(m_statusLabel);
    contentLayout()->addSpacing(8);

    auto* bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(20);

    m_resultListContainer = new QWidget(this);
    auto* listLayout = new QVBoxLayout(m_resultListContainer);
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

    auto* previewPanel = new QWidget(this);
    auto* previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    m_previewLabel = new QLabel(previewPanel);
    m_previewLabel->setObjectName("identify-preview-label");
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setFixedSize(kIdentifyPreviewSize);
    previewLayout->addWidget(m_previewLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
    previewLayout->addStretch();

    bodyLayout->addWidget(previewPanel, 0, Qt::AlignTop);
    bodyLayout->addWidget(m_resultListContainer, 1);

    contentLayout()->addLayout(bodyLayout);
    contentLayout()->addSpacing(24);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName("dialog-btn-cancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_applyButton = new QPushButton(tr("Apply Match"), this);
    m_applyButton->setObjectName("dialog-btn-primary");
    m_applyButton->setCursor(Qt::PointingHandCursor);
    m_applyButton->setEnabled(false);
    buttonLayout->addWidget(m_applyButton);

    contentLayout()->addLayout(buttonLayout);

    connect(m_searchButton, &QPushButton::clicked, this,
            [this]() { triggerSearch(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this,
            [this]() { triggerSearch(); });
    connect(m_imdbIdEdit, &QLineEdit::returnPressed, this,
            [this]() { triggerSearch(); });
    connect(m_movieDbIdEdit, &QLineEdit::returnPressed, this,
            [this]() { triggerSearch(); });
    connect(m_tvdbIdEdit, &QLineEdit::returnPressed, this,
            [this]() { triggerSearch(); });
    connect(m_yearSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int) { updateApplyButtonState(); });
    connect(m_resultList, &QListWidget::itemSelectionChanged, this,
            [this]() {
                updateApplyButtonState();
                refreshPreview();
            });
    connect(m_applyButton, &QPushButton::clicked, this,
            [this]() { m_pendingTask = applySelectedResult(); });

    updatePreviewMessage(tr("Preview unavailable"));
    updateUiState();
    updateLoadingOverlayGeometry();
}

void MediaIdentifyDialog::showEvent(QShowEvent* event)
{
    ModernDialogBase::showEvent(event);
    updateLoadingOverlayGeometry();
    syncProviderEditHeights();
    m_searchEdit->setFocus();
    m_searchEdit->selectAll();

    if (!m_loaded) {
        m_loaded = true;
        triggerSearch();
    }
}

void MediaIdentifyDialog::resizeEvent(QResizeEvent* event)
{
    ModernDialogBase::resizeEvent(event);
    updateLoadingOverlayGeometry();
    syncProviderEditHeights();
}

QCoro::Task<void> MediaIdentifyDialog::searchMatches(QString queryText,
                                                     int productionYear,
                                                     QJsonObject providerIds)
{
    QPointer<MediaIdentifyDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    queryText = queryText.trimmed();
    const QString providerIdSummary = buildProviderIdSummary(providerIds);
    if (!safeThis || !adminService ||
        m_remoteSearchType.trimmed().isEmpty()) {
        co_return;
    }

    if (queryText.isEmpty() && providerIds.isEmpty()) {
        m_results.clear();
        rebuildResultList();
        updatePreviewMessage(tr("Preview unavailable"));
        updateStatusText(tr("Enter a title or provider ID to search."));
        m_isLoading = false;
        updateUiState();
        co_return;
    }

    m_isLoading = true;
    ++m_previewLoadGeneration;
    updatePreviewMessage(tr("Loading preview..."));
    updateStatusText(tr("Searching..."));
    updateUiState();

    qDebug() << "[MediaIdentifyDialog] Searching remote metadata"
             << "| itemId=" << m_item.id
             << "| itemType=" << m_item.type
             << "| remoteSearchType=" << m_remoteSearchType
             << "| queryText=" << queryText
             << "| productionYear=" << productionYear
             << "| providerIds=" << providerIdSummary;

    try {
        const QJsonObject searchInfo =
            MediaIdentifyUtils::buildSearchInfo(m_item, queryText,
                                                productionYear, providerIds);
        const QList<RemoteSearchResult> results =
            co_await adminService->searchRemoteMetadata(
                m_remoteSearchType, m_item.id, searchInfo);
        if (!safeThis) {
            co_return;
        }

        safeThis->m_results = results;
        safeThis->rebuildResultList();
        safeThis->updateStatusText(
            results.isEmpty() ? tr("No matches found")
                              : tr("Found %1 matches").arg(results.size()));
        safeThis->m_isLoading = false;
        safeThis->updateUiState();

        qDebug() << "[MediaIdentifyDialog] Remote metadata search completed"
                 << "| itemId=" << safeThis->m_item.id
                 << "| resultCount=" << results.size();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_results.clear();
        safeThis->rebuildResultList();
        ++safeThis->m_previewLoadGeneration;
        safeThis->updatePreviewMessage(tr("Preview unavailable"));
        safeThis->updateStatusText(tr("Search failed"));
        safeThis->m_isLoading = false;
        safeThis->updateUiState();

        qWarning() << "[MediaIdentifyDialog] Remote metadata search failed"
                   << "| itemId=" << safeThis->m_item.id
                   << "| itemType=" << safeThis->m_item.type
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to search metadata: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> MediaIdentifyDialog::applySelectedResult()
{
    QPointer<MediaIdentifyDialog> safeThis(this);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    if (!safeThis || !adminService) {
        co_return;
    }

    const RemoteSearchResult result = selectedResult();
    if (result.name.trimmed().isEmpty()) {
        co_return;
    }

    m_isLoading = true;
    updateStatusText(tr("Applying match..."));
    updateUiState();

    qDebug() << "[MediaIdentifyDialog] Applying remote metadata match"
             << "| itemId=" << m_item.id
             << "| itemType=" << m_item.type
             << "| resultName=" << result.name
             << "| provider=" << result.searchProviderName
             << "| providerIds=" << result.providerIdSummary();

    try {
        co_await adminService->applyRemoteSearchResult(m_item.id, result);
        if (!safeThis) {
            co_return;
        }

        qDebug() << "[MediaIdentifyDialog] Remote metadata match applied"
                 << "| itemId=" << safeThis->m_item.id
                 << "| resultName=" << result.name;
        ModernToast::showMessage(tr("Metadata updated"), 2000);
        safeThis->accept();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isLoading = false;
        safeThis->updateStatusText(tr("Apply failed"));
        safeThis->updateUiState();

        qWarning() << "[MediaIdentifyDialog] Failed to apply remote metadata match"
                   << "| itemId=" << safeThis->m_item.id
                   << "| resultName=" << result.name
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to apply match: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> MediaIdentifyDialog::loadPreviewImage(
    RemoteSearchResult result, quint64 requestGeneration)
{
    QPointer<MediaIdentifyDialog> safeThis(this);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);

    if (!safeThis || !mediaService) {
        co_return;
    }

    const QString imageUrl = result.imageUrl.trimmed();
    if (imageUrl.isEmpty()) {
        if (safeThis && requestGeneration == m_previewLoadGeneration) {
            safeThis->updatePreviewMessage(tr("Preview unavailable"));
        }
        co_return;
    }

    try {
        const QPixmap pixmap = co_await mediaService->fetchImageByUrl(imageUrl);
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

        qWarning() << "[MediaIdentifyDialog] Failed to load preview image"
                   << "| imageUrl=" << imageUrl
                   << "| error=" << e.what();
        safeThis->updatePreviewMessage(tr("Preview unavailable"));
    }
}

QJsonObject MediaIdentifyDialog::collectProviderIds() const
{
    return MediaIdentifyUtils::buildProviderIds(
        m_imdbIdEdit ? m_imdbIdEdit->text() : QString(),
        m_movieDbIdEdit ? m_movieDbIdEdit->text() : QString(),
        m_tvdbIdEdit ? m_tvdbIdEdit->text() : QString());
}

void MediaIdentifyDialog::syncProviderEditHeights()
{
    if (!m_searchEdit) {
        return;
    }

    m_searchEdit->ensurePolished();
    const int referenceHeight =
        m_searchEdit->height() > 0 ? m_searchEdit->height()
                                   : m_searchEdit->sizeHint().height();
    if (referenceHeight <= 0) {
        return;
    }

    const QList<QLineEdit*> providerEdits = {m_imdbIdEdit, m_movieDbIdEdit,
                                             m_tvdbIdEdit};
    for (QLineEdit* edit : providerEdits) {
        if (!edit) {
            continue;
        }

        edit->setFixedHeight(referenceHeight);
    }
}

RemoteSearchResult MediaIdentifyDialog::selectedResult() const
{
    const QListWidgetItem* item = m_resultList ? m_resultList->currentItem() : nullptr;
    if (!item) {
        return {};
    }

    const QVariant data = item->data(kRemoteSearchResultRole);
    if (!data.canConvert<RemoteSearchResult>()) {
        return {};
    }

    return data.value<RemoteSearchResult>();
}

void MediaIdentifyDialog::triggerSearch()
{
    if (!m_searchEdit || !m_yearSpin || m_isLoading) {
        return;
    }

    m_pendingTask = searchMatches(m_searchEdit->text(), m_yearSpin->value(),
                                  collectProviderIds());
}

void MediaIdentifyDialog::rebuildResultList()
{
    if (!m_resultList) {
        return;
    }

    QSignalBlocker blocker(m_resultList);
    m_resultList->clear();

    for (const RemoteSearchResult& result : std::as_const(m_results)) {
        auto* item = new QListWidgetItem(buildResultDisplayText(result), m_resultList);
        item->setData(kRemoteSearchResultRole, QVariant::fromValue(result));
        item->setToolTip(buildResultToolTip(result));
        item->setSizeHint(QSize(0, 60));
    }

    if (m_resultList->count() > 0) {
        m_resultList->setCurrentRow(0);
    }

    refreshPreview();
    updateApplyButtonState();
}

void MediaIdentifyDialog::refreshPreview()
{
    ++m_previewLoadGeneration;

    const RemoteSearchResult result = selectedResult();
    if (result.imageUrl.trimmed().isEmpty()) {
        updatePreviewMessage(tr("Preview unavailable"));
        return;
    }

    updatePreviewMessage(tr("Loading preview..."));
    m_previewLabel->setToolTip(buildResultToolTip(result));
    m_pendingPreviewTask = loadPreviewImage(result, m_previewLoadGeneration);
}

void MediaIdentifyDialog::updateLoadingOverlayGeometry()
{
    if (!m_loadingOverlay || !m_resultListContainer) {
        return;
    }

    m_loadingOverlay->setGeometry(m_resultListContainer->rect());
}

void MediaIdentifyDialog::updateUiState()
{
    if (m_searchEdit) {
        m_searchEdit->setEnabled(!m_isLoading);
    }
    if (m_yearSpin) {
        m_yearSpin->setEnabled(!m_isLoading);
    }
    if (m_imdbIdEdit) {
        m_imdbIdEdit->setEnabled(!m_isLoading);
    }
    if (m_movieDbIdEdit) {
        m_movieDbIdEdit->setEnabled(!m_isLoading);
    }
    if (m_tvdbIdEdit) {
        m_tvdbIdEdit->setEnabled(!m_isLoading);
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

void MediaIdentifyDialog::updateApplyButtonState()
{
    if (!m_applyButton) {
        return;
    }

    const bool hasSelection =
        m_resultList && m_resultList->currentItem() != nullptr;
    m_applyButton->setEnabled(!m_isLoading && hasSelection);
}

void MediaIdentifyDialog::updatePreviewMessage(const QString& text)
{
    if (!m_previewLabel) {
        return;
    }

    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setText(text);
    m_previewLabel->setToolTip(QString());
}

void MediaIdentifyDialog::updatePreviewPixmap(const QPixmap& pixmap)
{
    if (!m_previewLabel) {
        return;
    }

    const QPixmap scaled =
        pixmap.scaled(kIdentifyPreviewSize, Qt::KeepAspectRatio,
                      Qt::SmoothTransformation);
    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(scaled);
}

void MediaIdentifyDialog::updateStatusText(const QString& text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
    }
}
