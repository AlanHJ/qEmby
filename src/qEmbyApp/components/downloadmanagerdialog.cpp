#include "downloadmanagerdialog.h"

#include "../../qEmbyCore/config/config_keys.h"
#include "../../qEmbyCore/config/configstore.h"
#include "../managers/thememanager.h"
#include "../utils/layoututils.h"
#include "elidedlabel.h"
#include "modernmessagebox.h"
#include <qembycore.h>
#include <services/manager/servermanager.h>

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSize>
#include <QStringList>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QtMath>

namespace
{

QString formatBytes(qint64 bytes)
{
    if (bytes < 0)
    {
        return DownloadManagerDialog::tr("Unknown");
    }

    static const QStringList units = {DownloadManagerDialog::tr("B"), DownloadManagerDialog::tr("KB"),
                                      DownloadManagerDialog::tr("MB"), DownloadManagerDialog::tr("GB"),
                                      DownloadManagerDialog::tr("TB")};

    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < units.size() - 1)
    {
        value /= 1024.0;
        ++unitIndex;
    }

    const int precision = unitIndex == 0 ? 0 : 1;
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision), units.at(unitIndex));
}

QString formatDateTime(qint64 timestampMs)
{
    if (timestampMs <= 0)
    {
        return DownloadManagerDialog::tr("--");
    }

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString formatSize(const DownloadManager::DownloadRecord &record)
{
    if (record.bytesTotal > 0)
    {
        return formatBytes(record.bytesTotal);
    }
    if (record.bytesReceived > 0)
    {
        return formatBytes(record.bytesReceived);
    }
    return DownloadManagerDialog::tr("Unknown");
}

QString formatProgress(const DownloadManager::DownloadRecord &record)
{
    const QString downloadingText = DownloadManager::tr("Downloading");
    const QString pausingText = DownloadManager::tr("Pausing");
    const QString cancellingText = DownloadManager::tr("Cancelling");

    if (record.isActive)
    {
        QString actionText = downloadingText;
        if (record.statusText == pausingText)
        {
            actionText = pausingText;
        }
        else if (record.statusText == cancellingText)
        {
            actionText = cancellingText;
        }

        if (record.bytesTotal > 0)
        {
            const double ratio = static_cast<double>(record.bytesReceived) / static_cast<double>(record.bytesTotal);
            const int percent = qBound(0, qRound(ratio * 100.0), 100);
            return DownloadManagerDialog::tr("%1 %2% (%3 / %4)")
                .arg(actionText)
                .arg(percent)
                .arg(formatBytes(record.bytesReceived))
                .arg(formatBytes(record.bytesTotal));
        }

        if (record.bytesReceived > 0)
        {
            return DownloadManagerDialog::tr("%1 %2").arg(actionText).arg(formatBytes(record.bytesReceived));
        }

        if (record.statusText == pausingText || record.statusText == cancellingText)
        {
            return record.statusText;
        }

        return DownloadManagerDialog::tr("Waiting...");
    }

    if (record.isPaused)
    {
        if (record.bytesTotal > 0)
        {
            const double ratio = static_cast<double>(record.bytesReceived) / static_cast<double>(record.bytesTotal);
            const int percent = qBound(0, qRound(ratio * 100.0), 100);
            return DownloadManagerDialog::tr("Paused %1% (%2 / %3)")
                .arg(percent)
                .arg(formatBytes(record.bytesReceived))
                .arg(formatBytes(record.bytesTotal));
        }

        if (record.bytesReceived > 0)
        {
            return DownloadManagerDialog::tr("Paused %1").arg(formatBytes(record.bytesReceived));
        }

        return DownloadManagerDialog::tr("Paused");
    }

    if (record.isCompleted)
    {
        return DownloadManagerDialog::tr("Completed");
    }

    return record.statusText.trimmed().isEmpty() ? DownloadManagerDialog::tr("Failed") : record.statusText;
}

QString progressTone(const DownloadManager::DownloadRecord &record)
{
    if (record.isActive)
    {
        if (record.statusText == DownloadManager::tr("Cancelling"))
        {
            return QStringLiteral("danger");
        }
        return QStringLiteral("active");
    }
    if (record.isPaused)
    {
        return QStringLiteral("paused");
    }
    if (record.isCompleted)
    {
        return QStringLiteral("success");
    }
    return QStringLiteral("danger");
}

QColor accentIconColor()
{
    return ThemeManager::instance()->isDarkMode() ? QColor(QStringLiteral("#93C5FD"))
                                                  : QColor(QStringLiteral("#2563EB"));
}

QColor dangerIconColor()
{
    return ThemeManager::instance()->isDarkMode() ? QColor(QStringLiteral("#FCA5A5"))
                                                  : QColor(QStringLiteral("#DC2626"));
}

QColor mutedIconColor()
{
    return ThemeManager::instance()->isDarkMode() ? QColor(QStringLiteral("#64748B"))
                                                  : QColor(QStringLiteral("#94A3B8"));
}

constexpr int kIndexColumnWidth = 46;
constexpr int kSizeColumnWidth = 104;
constexpr int kDateColumnWidth = 150;
constexpr int kProgressColumnWidth = 220;
constexpr int kActionButtonSize = 30;
constexpr int kActionButtonsSpacing = 8;
constexpr int kRecordsTrailingInset = 16;
constexpr int kActionAreaWidth = (kActionButtonSize * 4) + (kActionButtonsSpacing * 3);

QPushButton *createIconButton(const QIcon &icon, const QString &toolTip, const QString &tone, QWidget *parent)
{
    auto *button = new QPushButton(parent);
    button->setObjectName("DownloadIconButton");
    button->setProperty("tone", tone);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    button->setFixedSize(kActionButtonSize, kActionButtonSize);
    button->setIcon(icon);
    button->setIconSize(QSize(16, 16));
    button->setToolTip(toolTip);
    return button;
}

QWidget *createActionPlaceholder(int width, QWidget *parent)
{
    auto *placeholder = new QWidget(parent);
    placeholder->setFixedWidth(width);
    return placeholder;
}

QLabel *createHeaderLabel(const QString &text, int width, Qt::Alignment alignment, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName("DownloadHeaderLabel");
    label->setAlignment(alignment);
    if (width > 0)
    {
        label->setFixedWidth(width);
    }
    return label;
}

QLabel *createValueLabel(const QString &text, const QString &objectName, int width, Qt::Alignment alignment,
                         QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setAlignment(alignment);
    if (width > 0)
    {
        label->setFixedWidth(width);
    }
    label->setToolTip(text);
    return label;
}

void setButtonTone(QPushButton *button, const QString &tone)
{
    if (!button || button->property("tone").toString() == tone)
    {
        return;
    }

    button->setProperty("tone", tone);
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

} 

void DownloadManagerDialog::showManager(QEmbyCore *core, QWidget *parent)
{
    static QPointer<DownloadManagerDialog> s_dialog;
    QWidget *owner = parent ? parent->window() : nullptr;

    DownloadManager::instance()->init(core);

    if (!s_dialog)
    {
        s_dialog = new DownloadManagerDialog(core, owner);
        s_dialog->setAttribute(Qt::WA_DeleteOnClose, true);
    }
    else
    {
        s_dialog->m_core = core;
        s_dialog->bindServerSignals();
        s_dialog->refreshUi();
    }

    s_dialog->show();
    if (s_dialog->isMinimized())
    {
        s_dialog->showNormal();
    }
    s_dialog->raise();
    s_dialog->activateWindow();
}

DownloadManagerDialog::DownloadManagerDialog(QEmbyCore *core, QWidget *parent) : ModernDialogBase(parent), m_core(core)
{
    setModal(false);
    resize(1180, 660);
    setTitle(tr("Download Manager"));

    auto *rootLayout = contentLayout();
    rootLayout->setContentsMargins(20, 10, 0, 20);
    rootLayout->setSpacing(10);

    auto *listCard = new QWidget(this);
    listCard->setObjectName("DownloadListSection");
    listCard->setAttribute(Qt::WA_StyledBackground, true);

    auto *listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(8);

    auto *summaryBar = new QWidget(listCard);
    summaryBar->setObjectName("DownloadSummaryBar");
    summaryBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *summaryLayout = new QHBoxLayout(summaryBar);
    summaryLayout->setContentsMargins(2, 0, 2, 2);
    summaryLayout->setSpacing(10);

    m_summaryLabel = new QLabel(summaryBar);
    m_summaryLabel->setObjectName("DownloadSummaryLabel");
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(m_summaryLabel);
    listLayout->addWidget(summaryBar);

    m_recordsHeader = new QWidget(listCard);
    m_recordsHeader->setObjectName("DownloadListHeader");
    m_recordsHeader->setAttribute(Qt::WA_StyledBackground, true);
    auto *headerLayout = new QHBoxLayout(m_recordsHeader);
    headerLayout->setContentsMargins(8, 6, 8 + kRecordsTrailingInset, 10);
    headerLayout->setSpacing(12);
    headerLayout->addWidget(createHeaderLabel(tr("No."), kIndexColumnWidth, Qt::AlignCenter, m_recordsHeader));
    headerLayout->addWidget(createHeaderLabel(tr("Media"), 0, Qt::AlignVCenter | Qt::AlignLeft, m_recordsHeader), 1);
    headerLayout->addWidget(createHeaderLabel(tr("Size"), kSizeColumnWidth, Qt::AlignCenter, m_recordsHeader));
    headerLayout->addWidget(createHeaderLabel(tr("Started"), kDateColumnWidth, Qt::AlignCenter, m_recordsHeader));
    headerLayout->addWidget(createHeaderLabel(tr("Finished"), kDateColumnWidth, Qt::AlignCenter, m_recordsHeader));
    headerLayout->addWidget(createHeaderLabel(tr("Progress"), kProgressColumnWidth, Qt::AlignCenter, m_recordsHeader));
    headerLayout->addWidget(createActionPlaceholder(kActionAreaWidth, m_recordsHeader));
    listLayout->addWidget(m_recordsHeader);

    m_emptyState = new QFrame(listCard);
    m_emptyState->setObjectName("DownloadEmptyState");
    m_emptyState->setAttribute(Qt::WA_StyledBackground, true);
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(28, 28, 28, 28);
    emptyLayout->setSpacing(12);
    emptyLayout->addStretch(1);

    m_emptyIconLabel = new QLabel(m_emptyState);
    m_emptyIconLabel->setObjectName("DownloadEmptyIcon");
    m_emptyIconLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyIconLabel, 0, Qt::AlignCenter);

    m_emptyTitleLabel = new QLabel(tr("No records yet"), m_emptyState);
    m_emptyTitleLabel->setObjectName("DownloadEmptyTitle");
    m_emptyTitleLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyTitleLabel, 0, Qt::AlignCenter);

    m_emptyHintLabel = new QLabel(tr("Start a download and the record will appear here."), m_emptyState);
    m_emptyHintLabel->setObjectName("DownloadEmptyHint");
    m_emptyHintLabel->setAlignment(Qt::AlignCenter);
    m_emptyHintLabel->setWordWrap(true);
    emptyLayout->addWidget(m_emptyHintLabel, 0, Qt::AlignCenter);
    emptyLayout->addStretch(1);
    listLayout->addWidget(m_emptyState, 1);

    m_recordsScrollArea = new QScrollArea(listCard);
    m_recordsScrollArea->setObjectName("DownloadRecordsArea");
    m_recordsScrollArea->setWidgetResizable(true);
    m_recordsScrollArea->setFrameShape(QFrame::NoFrame);
    m_recordsScrollArea->setFocusPolicy(Qt::NoFocus);
    m_recordsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recordsScrollArea->viewport()->setObjectName("DownloadRecordsViewport");

    m_recordsContainer = new QWidget(m_recordsScrollArea);
    m_recordsContainer->setObjectName("DownloadRecordsContainer");
    m_recordsContainer->setAttribute(Qt::WA_StyledBackground, true);

    m_recordsLayout = new QVBoxLayout(m_recordsContainer);
    m_recordsLayout->setContentsMargins(0, 0, kRecordsTrailingInset, 0);
    m_recordsLayout->setSpacing(8);
    m_recordsLayout->addStretch(1);

    m_recordsScrollArea->setWidget(m_recordsContainer);
    listLayout->addWidget(m_recordsScrollArea, 1);

    rootLayout->addWidget(listCard, 1);

    auto *locationCard = new QWidget(this);
    locationCard->setObjectName("DownloadFooterBar");
    locationCard->setAttribute(Qt::WA_StyledBackground, true);

    auto *locationLayout = new QHBoxLayout(locationCard);
    locationLayout->setContentsMargins(2, 14, kRecordsTrailingInset, 0);
    locationLayout->setSpacing(12);

    auto *locationTextLayout = new QVBoxLayout();
    locationTextLayout->setContentsMargins(0, 0, 0, 0);
    locationTextLayout->setSpacing(4);

    auto *locationTitle = new QLabel(tr("Saved To"), locationCard);
    locationTitle->setObjectName("DownloadSectionTitle");
    locationTextLayout->addWidget(locationTitle);

    m_locationValueLabel = new QLabel(locationCard);
    m_locationValueLabel->setObjectName("DownloadLocationValue");
    m_locationValueLabel->setWordWrap(true);
    m_locationValueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    locationTextLayout->addWidget(m_locationValueLabel);
    locationLayout->addLayout(locationTextLayout, 1);

    m_openDirectoryButton = new QPushButton(locationCard);
    m_openDirectoryButton->setObjectName("DownloadIconButton");
    m_openDirectoryButton->setProperty("tone", QStringLiteral("default"));
    m_openDirectoryButton->setCursor(Qt::PointingHandCursor);
    m_openDirectoryButton->setFocusPolicy(Qt::NoFocus);
    m_openDirectoryButton->setFixedSize(32, 32);
    m_openDirectoryButton->setToolTip(tr("Open Directory"));
    locationLayout->addWidget(m_openDirectoryButton, 0, Qt::AlignBottom);

    m_changeDirectoryButton = new QPushButton(locationCard);
    m_changeDirectoryButton->setObjectName("DownloadIconButton");
    m_changeDirectoryButton->setProperty("tone", QStringLiteral("accent"));
    m_changeDirectoryButton->setCursor(Qt::PointingHandCursor);
    m_changeDirectoryButton->setFocusPolicy(Qt::NoFocus);
    m_changeDirectoryButton->setFixedSize(32, 32);
    m_changeDirectoryButton->setToolTip(tr("Set Directory"));
    locationLayout->addWidget(m_changeDirectoryButton, 0, Qt::AlignBottom);

    rootLayout->addWidget(locationCard);

    connect(DownloadManager::instance(), &DownloadManager::downloadsChanged, this, [this]() { refreshUi(); });
    connect(DownloadManager::instance(), &DownloadManager::downloadDirectoryChanged, this,
            [this](const QString &) { refreshUi(); });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](ThemeManager::Theme) { refreshUi(); });

    connect(m_openDirectoryButton, &QPushButton::clicked, this,
            [this]()
            {
                const QString directory = DownloadManager::instance()->downloadDirectory();
                QDir().mkpath(directory);
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
                {
                    ModernMessageBox::warning(this, tr("Open Directory"),
                                              tr("The download folder is no longer available."));
                }
            });

    connect(m_changeDirectoryButton, &QPushButton::clicked, this,
            [this]()
            {
                const QString selectedDirectory = QFileDialog::getExistingDirectory(
                    this, tr("Select Download Directory"), DownloadManager::instance()->downloadDirectory(),
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                if (selectedDirectory.trimmed().isEmpty())
                {
                    return;
                }

                DownloadManager::instance()->setDownloadDirectory(selectedDirectory);
            });

    bindServerSignals();
    refreshUi();
}

void DownloadManagerDialog::bindServerSignals()
{
    QObject::disconnect(m_activeServerChangedConnection);
    m_activeServerChangedConnection = {};

    if (!m_core || !m_core->serverManager())
    {
        return;
    }

    m_activeServerChangedConnection =
        connect(m_core->serverManager(), &ServerManager::activeServerChanged, this,
                [this](const ServerProfile &profile)
                {
                    qDebug() << "[DownloadManagerDialog] Active server changed"
                             << "| serverUrl:" << profile.url
                             << "| userId:" << profile.userId;
                    refreshUi();
                });
}

void DownloadManagerDialog::applyIcons()
{
    if (m_openDirectoryButton)
    {
        m_openDirectoryButton->setIcon(ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/folder.svg")));
        m_openDirectoryButton->setIconSize(QSize(17, 17));
    }

    if (m_changeDirectoryButton)
    {
        m_changeDirectoryButton->setIcon(
            ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/hard-drive.svg"), accentIconColor()));
        m_changeDirectoryButton->setIconSize(QSize(17, 17));
    }

    if (m_emptyIconLabel)
    {
        const QIcon emptyIcon =
            ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/download-sidebar.svg"), mutedIconColor());
        m_emptyIconLabel->setPixmap(emptyIcon.pixmap(42, 42));
    }
}

void DownloadManagerDialog::refreshUi()
{
    const QList<DownloadManager::DownloadRecord> activeRecords =
        DownloadManager::instance()->activeDownloadsForCurrentServer();
    const QList<DownloadManager::DownloadRecord> historyRecords =
        DownloadManager::instance()->historyDownloadsForCurrentServer();

    int completedCount = 0;
    for (const auto &record : historyRecords)
    {
        if (record.isCompleted)
        {
            ++completedCount;
        }
    }
    const int failedCount = historyRecords.size() - completedCount;
    const int totalCount = activeRecords.size() + historyRecords.size();

    QString activeServerUrl;
    QString activeUserId;
    if (m_core && m_core->serverManager())
    {
        const ServerProfile profile = m_core->serverManager()->activeProfile();
        activeServerUrl = profile.url;
        activeUserId = profile.userId;
    }

    qDebug() << "[DownloadManagerDialog] Refreshing records for active server"
             << "| serverUrl:" << activeServerUrl
             << "| userId:" << activeUserId
             << "| activeCount:" << activeRecords.size()
             << "| historyCount:" << historyRecords.size();

    m_summaryLabel->setText(tr("%1 records, %2 active, %3 completed, %4 failed")
                                .arg(totalCount)
                                .arg(activeRecords.size())
                                .arg(completedCount)
                                .arg(failedCount));

    const QString directory = QDir::toNativeSeparators(DownloadManager::instance()->downloadDirectory());
    m_locationValueLabel->setText(directory);
    m_locationValueLabel->setToolTip(directory);

    QList<DownloadManager::DownloadRecord> records;
    records.reserve(activeRecords.size() + historyRecords.size());
    for (const auto &record : activeRecords)
    {
        records.append(record);
    }
    for (const auto &record : historyRecords)
    {
        records.append(record);
    }

    rebuildRecordList(records);
    applyIcons();
}

void DownloadManagerDialog::rebuildRecordList(const QList<DownloadManager::DownloadRecord> &records)
{
    QStringList orderedIds;
    orderedIds.reserve(records.size());
    int index = 1;
    for (const auto &record : records)
    {
        orderedIds.append(record.id);
        if (!m_recordRows.contains(record.id))
        {
            m_recordRows.insert(record.id, createRecordRow());
        }
        updateRecordRow(record.id, record, index++);
    }

    const QStringList existingIds = m_recordRows.keys();
    for (const QString &recordId : existingIds)
    {
        if (!orderedIds.contains(recordId))
        {
            removeRecordRow(recordId);
        }
    }

    if (orderedIds != m_rowOrder)
    {
        rebuildRecordLayout(orderedIds);
    }

    const bool hasRecords = !records.isEmpty();
    if (m_recordsHeader)
    {
        m_recordsHeader->setVisible(hasRecords);
    }
    if (m_recordsScrollArea)
    {
        m_recordsScrollArea->setVisible(hasRecords);
    }
    if (m_emptyState)
    {
        m_emptyState->setVisible(!hasRecords);
    }
}

DownloadManagerDialog::RecordRowWidgets DownloadManagerDialog::createRecordRow()
{
    RecordRowWidgets widgets;

    widgets.row = new QFrame(m_recordsContainer);
    widgets.row->setObjectName("DownloadRecordRow");
    widgets.row->setAttribute(Qt::WA_StyledBackground, true);
    widgets.row->setAttribute(Qt::WA_Hover, true);
    widgets.row->installEventFilter(this);

    auto *rowLayout = new QHBoxLayout(widgets.row);
    rowLayout->setContentsMargins(14, 12, 14, 12);
    rowLayout->setSpacing(12);

    widgets.indexLabel = createValueLabel(QString(), QStringLiteral("DownloadRecordIndex"), kIndexColumnWidth,
                                          Qt::AlignCenter, widgets.row);
    widgets.indexLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.indexLabel);

    widgets.titleLabel = new ElidedLabel(widgets.row);
    widgets.titleLabel->setObjectName("DownloadRecordTitle");
    widgets.titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    widgets.titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.titleLabel, 1);

    widgets.sizeLabel = createValueLabel(QString(), QStringLiteral("DownloadRecordValue"), kSizeColumnWidth,
                                         Qt::AlignCenter, widgets.row);
    widgets.sizeLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.sizeLabel);

    widgets.startedLabel = createValueLabel(QString(), QStringLiteral("DownloadRecordValue"), kDateColumnWidth,
                                            Qt::AlignCenter, widgets.row);
    widgets.startedLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.startedLabel);

    widgets.finishedLabel = createValueLabel(QString(), QStringLiteral("DownloadRecordValue"), kDateColumnWidth,
                                             Qt::AlignCenter, widgets.row);
    widgets.finishedLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.finishedLabel);

    widgets.progressLabel = new ElidedLabel(widgets.row);
    widgets.progressLabel->setObjectName("DownloadRecordProgress");
    widgets.progressLabel->setAlignment(Qt::AlignCenter);
    widgets.progressLabel->setFixedWidth(kProgressColumnWidth);
    widgets.progressLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    rowLayout->addWidget(widgets.progressLabel);

    widgets.actionsContainer = new QWidget(widgets.row);
    widgets.actionsContainer->setObjectName("DownloadActions");
    widgets.actionsContainer->setFixedWidth(kActionAreaWidth);
    auto *actionsLayout = new QHBoxLayout(widgets.actionsContainer);
    actionsLayout->setContentsMargins(0, 0, 0, 0);
    actionsLayout->setSpacing(kActionButtonsSpacing);

    widgets.primaryButton = createIconButton(QIcon(), QString(), QStringLiteral("accent"), widgets.actionsContainer);
    actionsLayout->addWidget(widgets.primaryButton);

    widgets.stopButton =
        createIconButton(QIcon(), tr("Stop Download"), QStringLiteral("danger"), widgets.actionsContainer);
    actionsLayout->addWidget(widgets.stopButton);

    widgets.folderButton =
        createIconButton(QIcon(), tr("Open Folder"), QStringLiteral("default"), widgets.actionsContainer);
    actionsLayout->addWidget(widgets.folderButton);

    widgets.deleteButton =
        createIconButton(QIcon(), tr("Delete Record"), QStringLiteral("danger"), widgets.actionsContainer);
    actionsLayout->addWidget(widgets.deleteButton);

    rowLayout->addWidget(widgets.actionsContainer);

    return widgets;
}

void DownloadManagerDialog::updateRecordRow(const QString &recordId, const DownloadManager::DownloadRecord &record,
                                            int index)
{
    auto it = m_recordRows.find(recordId);
    if (it == m_recordRows.end())
    {
        return;
    }

    RecordRowWidgets &widgets = it.value();
    const QIcon openIcon =
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/quick-play.svg"), accentIconColor());
    const QIcon pauseIcon =
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/click-pause.svg"), accentIconColor());
    const QIcon resumeIcon =
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/task-run.svg"), accentIconColor());
    const QIcon stopIcon =
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/task-stop.svg"), dangerIconColor());
    const QIcon folderIcon = ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/folder.svg"));
    const QIcon deleteIcon = ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/delete.svg"), dangerIconColor());

    const QFileInfo fileInfo(record.filePath);
    const bool canOpenFile = record.isCompleted && fileInfo.exists() && fileInfo.isFile();
    const bool canOpenFolder = !record.filePath.trimmed().isEmpty();
    const bool canPauseDownload = record.isActive && record.statusText != DownloadManager::tr("Cancelling") &&
                                  record.statusText != DownloadManager::tr("Pausing");
    const bool canStopDownload =
        (record.isActive && record.statusText != DownloadManager::tr("Cancelling")) || record.isPaused;
    const QString progressText = formatProgress(record);

    widgets.row->setProperty("recordId", record.id);
    widgets.row->setCursor(canOpenFile ? Qt::PointingHandCursor : Qt::ArrowCursor);
    widgets.indexLabel->setText(QString::number(index));
    widgets.indexLabel->setToolTip(QString::number(index));

    widgets.titleLabel->setFullText(record.itemName);
    widgets.sizeLabel->setText(formatSize(record));
    widgets.sizeLabel->setToolTip(formatSize(record));
    widgets.startedLabel->setText(formatDateTime(record.createdAtMs));
    widgets.startedLabel->setToolTip(formatDateTime(record.createdAtMs));
    widgets.finishedLabel->setText(formatDateTime(record.finishedAtMs));
    widgets.finishedLabel->setToolTip(formatDateTime(record.finishedAtMs));

    widgets.progressLabel->setFullText(progressText);
    widgets.progressLabel->setProperty("tone", progressTone(record));
    widgets.progressLabel->style()->unpolish(widgets.progressLabel);
    widgets.progressLabel->style()->polish(widgets.progressLabel);
    widgets.progressLabel->update();

    if (record.isActive)
    {
        widgets.primaryButton->setIcon(pauseIcon);
        widgets.primaryButton->setToolTip(tr("Pause Download"));
        widgets.primaryButton->setEnabled(canPauseDownload);
        setButtonTone(widgets.primaryButton, QStringLiteral("accent"));
    }
    else if (record.isPaused)
    {
        widgets.primaryButton->setIcon(resumeIcon);
        widgets.primaryButton->setToolTip(tr("Resume Download"));
        widgets.primaryButton->setEnabled(true);
        setButtonTone(widgets.primaryButton, QStringLiteral("accent"));
    }
    else
    {
        widgets.primaryButton->setIcon(openIcon);
        widgets.primaryButton->setToolTip(tr("Open Media"));
        widgets.primaryButton->setEnabled(canOpenFile);
        setButtonTone(widgets.primaryButton, QStringLiteral("accent"));
    }
    widgets.primaryButton->setIconSize(QSize(16, 16));

    widgets.stopButton->setEnabled(canStopDownload);
    widgets.stopButton->setIcon(stopIcon);
    widgets.stopButton->setIconSize(QSize(16, 16));
    widgets.stopButton->setToolTip(tr("Stop Download"));

    widgets.folderButton->setEnabled(canOpenFolder);
    widgets.folderButton->setIcon(folderIcon);
    widgets.folderButton->setIconSize(QSize(16, 16));
    widgets.folderButton->setToolTip(tr("Open Folder"));

    widgets.deleteButton->setEnabled(!(record.isActive && record.statusText == DownloadManager::tr("Cancelling")));
    widgets.deleteButton->setIcon(deleteIcon);
    widgets.deleteButton->setIconSize(QSize(16, 16));
    widgets.deleteButton->setToolTip(tr("Delete Record"));

    widgets.row->setToolTip(QString());

    widgets.primaryButton->disconnect(this);
    connect(widgets.primaryButton, &QPushButton::clicked, this,
            [this, record]()
            {
                if (record.isActive)
                {
                    DownloadManager::instance()->pauseDownload(record.id);
                    return;
                }

                if (record.isPaused)
                {
                    DownloadManager::instance()->resumeDownload(record.id);
                    return;
                }

                if (!DownloadManager::instance()->openDownloadedFile(record.id))
                {
                    ModernMessageBox::warning(this, tr("Open Media"),
                                              tr("The downloaded media file is no longer available."));
                }
            });

    widgets.folderButton->disconnect(this);
    connect(widgets.folderButton, &QPushButton::clicked, this,
            [this, record]()
            {
                if (!DownloadManager::instance()->openContainingFolder(record.id))
                {
                    ModernMessageBox::warning(this, tr("Open Folder"),
                                              tr("The download folder is no longer available."));
                }
            });

    widgets.stopButton->disconnect(this);
    connect(widgets.stopButton, &QPushButton::clicked, this, [this, record]() { handleStopRecord(record); });

    widgets.deleteButton->disconnect(this);
    connect(widgets.deleteButton, &QPushButton::clicked, this, [this, record]() { handleDeleteRecord(record); });
}

void DownloadManagerDialog::rebuildRecordLayout(const QStringList &orderedIds)
{
    while (QLayoutItem *item = m_recordsLayout->takeAt(0))
    {
        delete item;
    }

    for (const QString &recordId : orderedIds)
    {
        const auto it = m_recordRows.constFind(recordId);
        if (it == m_recordRows.cend() || !it.value().row)
        {
            continue;
        }
        m_recordsLayout->addWidget(it.value().row);
    }

    m_recordsLayout->addStretch(1);
    m_rowOrder = orderedIds;
}

void DownloadManagerDialog::removeRecordRow(const QString &recordId)
{
    auto it = m_recordRows.find(recordId);
    if (it == m_recordRows.end())
    {
        return;
    }

    if (it.value().row)
    {
        it.value().row->deleteLater();
    }
    m_recordRows.erase(it);
    m_rowOrder.removeAll(recordId);
}

void DownloadManagerDialog::handleStopRecord(const DownloadManager::DownloadRecord &record)
{
    if (!record.isActive && !record.isPaused)
    {
        return;
    }

    const bool confirmed =
        ModernMessageBox::question(this, tr("Stop Download"), tr("Stop and remove the unfinished file"), tr("Stop"),
                                   tr("Cancel"), ModernMessageBox::Danger, ModernMessageBox::Warning);
    if (!confirmed)
    {
        return;
    }

    DownloadManager::instance()->stopDownload(record.id);
}

bool DownloadManagerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event && event->type() == QEvent::MouseButtonDblClick)
    {
        if (auto *widget = qobject_cast<QWidget *>(watched))
        {
            const QString recordId = widget->property("recordId").toString();
            if (!recordId.trimmed().isEmpty())
            {
                const auto record = DownloadManager::instance()->recordById(recordId);
                if (record && record->isCompleted && !DownloadManager::instance()->openDownloadedFile(recordId))
                {
                    ModernMessageBox::warning(this, tr("Open Media"),
                                              tr("The downloaded media file is no longer available."));
                }
                return true;
            }
        }
    }

    return ModernDialogBase::eventFilter(watched, event);
}

void DownloadManagerDialog::handleDeleteRecord(const DownloadManager::DownloadRecord &record)
{
    if (record.isActive)
    {
        const bool confirmed = ModernMessageBox::question(
            this, tr("Delete Record"),
            tr("Cancel active download for \"%1\" and remove the unfinished file?").arg(record.itemName), tr("Delete"),
            tr("Cancel"), ModernMessageBox::Danger, ModernMessageBox::Warning);
        if (!confirmed)
        {
            return;
        }

        DownloadManager::instance()->cancelDownload(record.id, true, false);
        return;
    }

    const bool hasLocalFile = !record.filePath.trimmed().isEmpty() && QFileInfo::exists(record.filePath);
    bool deleteFileWithRecord = ConfigStore::instance()->get<bool>(ConfigKeys::DownloadDeleteFileWithRecord, false);

    const bool confirmed =
        hasLocalFile
            ? ModernMessageBox::questionWithToggle(this, tr("Delete Record"),
                                                   tr("Delete download record for \"%1\"?").arg(record.itemName),
                                                   tr("Also Delete File"), &deleteFileWithRecord, tr("Delete"),
                                                   tr("Cancel"), ModernMessageBox::Danger, ModernMessageBox::Warning)
            : ModernMessageBox::question(this, tr("Delete Record"),
                                         tr("Delete download record for \"%1\"?").arg(record.itemName), tr("Delete"),
                                         tr("Cancel"), ModernMessageBox::Danger, ModernMessageBox::Warning);
    if (hasLocalFile)
    {
        ConfigStore::instance()->set(ConfigKeys::DownloadDeleteFileWithRecord, deleteFileWithRecord);
    }
    if (!confirmed)
    {
        return;
    }

    const bool deleteFile = hasLocalFile && deleteFileWithRecord;
    DownloadManager::instance()->removeHistoryRecord(record.id, deleteFile);
}
