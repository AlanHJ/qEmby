#include "pagelogs.h"

#include "../../components/elidedlabel.h"
#include "../../components/moderntoast.h"
#include "../../managers/thememanager.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>

#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QIODevice>
#include <QLabel>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QPixmap>
#include <QSaveFile>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QTextOption>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>
#include <functional>
#include <stdexcept>

namespace {

QDateTime parseIsoDateTime(QString rawDateTime) {
    if (rawDateTime.isEmpty()) {
        return {};
    }

    QDateTime parsed = QDateTime::fromString(rawDateTime, Qt::ISODate);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(rawDateTime, Qt::ISODateWithMs);
    }
    return parsed;
}

QString formatIsoDateTime(const QString& rawDateTime) {
    const QDateTime parsed = parseIsoDateTime(rawDateTime);
    if (!parsed.isValid()) {
        return rawDateTime;
    }
    return parsed.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString formatCompactDateTime(const QString& rawDateTime) {
    const QDateTime parsed = parseIsoDateTime(rawDateTime);
    if (!parsed.isValid()) {
        return QStringLiteral("--");
    }
    return parsed.toLocalTime().toString(QStringLiteral("MM-dd HH:mm"));
}

enum LogItemRoles {
    LogNameRole = Qt::UserRole,
    LogModifiedDisplayRole,
};

class LogFileItemDelegate : public QStyledItemDelegate {
public:
    using DownloadHandler = std::function<void(int, QString)>;

    explicit LogFileItemDelegate(QListWidget* listWidget, DownloadHandler handler)
        : QStyledItemDelegate(listWidget), m_listWidget(listWidget), m_onDownload(std::move(handler)) {
        if (m_listWidget && m_listWidget->viewport()) {
            m_listWidget->viewport()->installEventFilter(this);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(0, 34);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool isDarkMode = ThemeManager::instance()->isDarkMode();
        const bool isSelected = option.state.testFlag(QStyle::State_Selected);
        const bool isHovered = option.state.testFlag(QStyle::State_MouseOver);
        const QRect contentRect = option.rect.adjusted(0, 0, -10, 0);
        const QRectF rowRect = contentRect.adjusted(0, 0, -1, -1);

        QColor rowBackground = Qt::transparent;
        QColor rowBorder = Qt::transparent;
        if (isSelected) {
            rowBackground = isDarkMode ? QColor(59, 130, 246, 28) : QColor(59, 130, 246, 18);
            rowBorder = isDarkMode ? QColor(59, 130, 246, 42) : QColor(59, 130, 246, 30);
        } else if (isHovered) {
            rowBackground = isDarkMode ? QColor(255, 255, 255, 12) : QColor(15, 23, 42, 8);
            rowBorder = isDarkMode ? QColor(148, 163, 184, 20) : QColor(148, 163, 184, 20);
        }

        if (rowBackground.alpha() > 0 || rowBorder.alpha() > 0) {
            painter->setBrush(rowBackground);
            painter->setPen(QPen(rowBorder, 1));
            painter->drawRoundedRect(rowRect.adjusted(1, 1, -1, -1), 8, 8);
        }

        const QRect buttonRect = downloadButtonRect(contentRect);
        const QString modifiedText = index.data(LogModifiedDisplayRole).toString();
        const QString fileName = index.data(LogNameRole).toString();
        const bool buttonHovered = m_hoveredButtonRow == index.row();
        const bool buttonPressed = m_pressedButtonRow == index.row();

        QFont metaFont = option.font;
        metaFont.setPointSize(std::max(10, metaFont.pointSize() - 1));
        QFontMetrics metaMetrics(metaFont);
        const int timeWidth = metaMetrics.horizontalAdvance(modifiedText) + 2;
        const QRect timeRect(buttonRect.left() - timeWidth - 10, contentRect.top(), timeWidth, contentRect.height());
        const QRect nameRect(contentRect.left() + 12, contentRect.top(),
                             std::max(40, timeRect.left() - contentRect.left() - 24), contentRect.height());

        QFont nameFont = option.font;
        nameFont.setWeight(QFont::Medium);
        painter->setFont(nameFont);
        painter->setPen(isDarkMode ? QColor("#E2E8F0") : QColor("#1E293B"));
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft,
                          QFontMetrics(nameFont).elidedText(fileName, Qt::ElideRight, nameRect.width()));

        painter->setFont(metaFont);
        painter->setPen(isDarkMode ? QColor("#94A3B8") : QColor("#64748B"));
        painter->drawText(timeRect, Qt::AlignVCenter | Qt::AlignRight, modifiedText);

        QColor buttonBackground = Qt::transparent;
        QColor buttonBorder = isDarkMode ? QColor(148, 163, 184, 32) : QColor(148, 163, 184, 40);
        QColor iconColor = isSelected ? (isDarkMode ? QColor("#BFDBFE") : QColor("#2563EB"))
                                      : (isDarkMode ? QColor("#CBD5E1") : QColor("#475569"));

        if (buttonHovered) {
            buttonBackground = isDarkMode ? QColor(59, 130, 246, 26) : QColor(59, 130, 246, 16);
            buttonBorder = isDarkMode ? QColor(59, 130, 246, 52) : QColor(59, 130, 246, 42);
            iconColor = isDarkMode ? QColor("#BFDBFE") : QColor("#2563EB");
        }
        if (buttonPressed) {
            buttonBackground = isDarkMode ? QColor(59, 130, 246, 42) : QColor(59, 130, 246, 26);
            buttonBorder = isDarkMode ? QColor(59, 130, 246, 72) : QColor(59, 130, 246, 56);
        }

        painter->setBrush(buttonBackground);
        painter->setPen(QPen(buttonBorder, 1));
        painter->drawRoundedRect(buttonRect.adjusted(0, 0, -1, -1), 7, 7);

        const QIcon icon = ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/download.svg"), iconColor);
        icon.paint(painter, buttonRect.adjusted(5, 5, -5, -5));

        painter->restore();
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override {
        Q_UNUSED(model)
        Q_UNUSED(event)
        Q_UNUSED(option)
        Q_UNUSED(index)
        return false;
    }

private:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (!m_listWidget || watched != m_listWidget->viewport()) {
            return QStyledItemDelegate::eventFilter(watched, event);
        }

        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            updateHoveredButtonRow(hitDownloadButtonRow(mouseEvent->position().toPoint()));
            break;
        }
        case QEvent::MouseButtonPress: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const int hitRow = hitDownloadButtonRow(mouseEvent->position().toPoint());
                if (hitRow >= 0) {
                    m_pressedButtonRow = hitRow;
                    updateHoveredButtonRow(hitRow);
                    m_listWidget->viewport()->update();
                    return true;
                }
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const int hitRow = hitDownloadButtonRow(mouseEvent->position().toPoint());
                const int pressedRow = m_pressedButtonRow;
                m_pressedButtonRow = -1;
                updateHoveredButtonRow(hitRow);
                m_listWidget->viewport()->update();

                if (pressedRow >= 0 && pressedRow == hitRow && m_onDownload) {
                    const QModelIndex index = m_listWidget->model()->index(pressedRow, 0);
                    m_onDownload(pressedRow, index.data(LogNameRole).toString());
                    return true;
                }
            }
            break;
        }
        case QEvent::Leave:
            if (m_hoveredButtonRow >= 0 || m_pressedButtonRow >= 0) {
                m_hoveredButtonRow = -1;
                m_pressedButtonRow = -1;
                m_listWidget->viewport()->update();
            }
            break;
        default:
            break;
        }

        return QStyledItemDelegate::eventFilter(watched, event);
    }

    static QRect downloadButtonRect(const QRect& rowRect) {
        const int buttonSize = 24;
        return QRect(rowRect.right() - 12 - buttonSize, rowRect.center().y() - buttonSize / 2, buttonSize,
                     buttonSize);
    }

    QRect buttonRectForIndex(const QModelIndex& index) const {
        return downloadButtonRect(m_listWidget->visualRect(index).adjusted(0, 0, -10, 0));
    }

    int hitDownloadButtonRow(const QPoint& viewportPos) const {
        const QModelIndex index = m_listWidget->indexAt(viewportPos);
        if (!index.isValid()) {
            return -1;
        }
        return buttonRectForIndex(index).contains(viewportPos) ? index.row() : -1;
    }

    void updateHoveredButtonRow(int row) {
        if (m_hoveredButtonRow == row) {
            return;
        }
        m_hoveredButtonRow = row;
        if (m_listWidget && m_listWidget->viewport()) {
            m_listWidget->viewport()->update();
        }
    }

    QListWidget* m_listWidget = nullptr;
    DownloadHandler m_onDownload;
    int m_hoveredButtonRow = -1;
    int m_pressedButtonRow = -1;
};

} 





PageLogs::PageLogs(QEmbyCore* core, QWidget* parent)
    : ManagePageBase(core, tr("Logs"), parent) {
    m_mainLayout->setContentsMargins(16, 30, 0, 24);
    m_mainLayout->setSpacing(16);

    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 12, 0);
    titleRow->setSpacing(8);

    m_btnRefresh = new QPushButton(this);
    m_btnRefresh->setObjectName("ManageRefreshBtn");
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnRefresh->setFixedSize(32, 32);
    m_btnRefresh->setToolTip(tr("Refresh"));
    titleRow->addWidget(m_btnRefresh);
    titleRow->addStretch(1);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName("ManageInfoValue");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_statsLabel->setText(tr("Loading..."));
    titleRow->addWidget(m_statsLabel);

    m_mainLayout->addLayout(titleRow);

    auto* fileListCard = new QFrame(this);
    fileListCard->setObjectName("ManageLogListCard");
    fileListCard->setAttribute(Qt::WA_StyledBackground, true);
    fileListCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    fileListCard->setMaximumHeight(176);

    auto* fileListLayout = new QVBoxLayout(fileListCard);
    fileListLayout->setContentsMargins(0, 0, 0, 0);
    fileListLayout->setSpacing(0);

    m_fileList = new QListWidget(fileListCard);
    m_fileList->setObjectName("ManageLogFileList");
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setTextElideMode(Qt::ElideRight);
    m_fileList->setWordWrap(false);
    m_fileList->setSpacing(0);
    m_fileList->setMouseTracking(true);
    m_fileList->setUniformItemSizes(true);
    m_fileList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_fileList->setMinimumHeight(140);
    m_fileList->setItemDelegate(new LogFileItemDelegate(
        m_fileList, [this](int row, QString logName) {
            Q_UNUSED(row)
            if (!logName.isEmpty()) {
                m_pendingDownloadTask = downloadLogFile(logName);
            }
        }));
    fileListLayout->addWidget(m_fileList, 1);

    m_emptyLabel = new QLabel(tr("No log files found"), fileListCard);
    m_emptyLabel->setObjectName("ManageEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setContentsMargins(0, 0, 0, 0);
    m_emptyLabel->hide();
    fileListLayout->addWidget(m_emptyLabel, 1);

    m_mainLayout->addWidget(fileListCard);

    auto* dividerContainer = new QWidget(this);
    auto* dividerLayout = new QHBoxLayout(dividerContainer);
    dividerLayout->setContentsMargins(0, 0, 12, 0);
    dividerLayout->setSpacing(0);

    auto* divider = new QFrame(dividerContainer);
    divider->setObjectName("ManageLogDivider");
    divider->setFixedHeight(1);
    divider->setFrameShape(QFrame::NoFrame);
    dividerLayout->addWidget(divider);

    m_mainLayout->addWidget(dividerContainer);

    auto* viewerCard = new QFrame(this);
    viewerCard->setObjectName("ManageLogViewerCard");
    viewerCard->setAttribute(Qt::WA_StyledBackground, true);
    viewerCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* viewerLayout = new QVBoxLayout(viewerCard);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    viewerLayout->setSpacing(0);

    auto* viewerHeader = new QWidget(viewerCard);
    auto* viewerHeaderLayout = new QVBoxLayout(viewerHeader);
    viewerHeaderLayout->setContentsMargins(16, 0, 16, 10);
    viewerHeaderLayout->setSpacing(6);

    m_currentFileNameLabel = new ElidedLabel(viewerHeader);
    m_currentFileNameLabel->setObjectName("ManageLogSelectedName");
    viewerHeaderLayout->addWidget(m_currentFileNameLabel);

    m_currentFileMetaLabel = new QLabel(viewerHeader);
    m_currentFileMetaLabel->setObjectName("ManageLogSelectedMeta");
    m_currentFileMetaLabel->setWordWrap(true);
    viewerHeaderLayout->addWidget(m_currentFileMetaLabel);
    viewerLayout->addWidget(viewerHeader);

    m_viewerPlaceholder = new QLabel(tr("Select a log file to inspect its contents"), viewerCard);
    m_viewerPlaceholder->setObjectName("ManageEmptyLabel");
    m_viewerPlaceholder->setAlignment(Qt::AlignCenter);
    m_viewerPlaceholder->setWordWrap(true);
    m_viewerPlaceholder->setMargin(24);
    viewerLayout->addWidget(m_viewerPlaceholder, 1);

    m_logViewer = new QPlainTextEdit(viewerCard);
    m_logViewer->setObjectName("ManageLogViewer");
    m_logViewer->setReadOnly(true);
    m_logViewer->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_logViewer->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    m_logViewer->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_logViewer->document()->setDocumentMargin(16.0);

    QFont monoFont(QStringLiteral("Consolas"), 10);
    monoFont.setStyleHint(QFont::Monospace);
    m_logViewer->setFont(monoFont);
    m_logViewer->hide();
    viewerLayout->addWidget(m_logViewer, 1);

    m_mainLayout->addWidget(viewerCard, 1);

    updateSelectionDetails(QString());

    connect(m_btnRefresh, &QPushButton::clicked, this, [this]() { m_pendingTask = loadFileList(true); });

    connect(m_fileList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= m_fileList->count()) {
            m_currentLogName.clear();
            updateSelectionDetails(QString());
            m_logViewer->clear();
            setViewerPlaceholder(tr("Select a log file to inspect its contents"));
            return;
        }

        const QString logName = m_fileList->item(row)->data(LogNameRole).toString();
        if (logName.isEmpty()) {
            return;
        }

        updateSelectionDetails(logName);
        m_pendingTask = loadLogContent(logName);
    });
}

void PageLogs::showEvent(QShowEvent* event) {
    ManagePageBase::showEvent(event);
    if (!m_loaded) {
        m_pendingTask = loadFileList();
        m_loaded = true;
    }
}

QString PageLogs::currentSelectedLogName() const {
    if (!m_fileList || !m_fileList->currentItem()) {
        return {};
    }
    return m_fileList->currentItem()->data(LogNameRole).toString();
}

QString PageLogs::rebuildFileList(const QString& preferredLogName) {
    QSignalBlocker blocker(m_fileList);
    m_fileList->clear();

    int selectedRow = -1;
    for (int i = 0; i < m_logFiles.size(); ++i) {
        const LogFileInfo& logFile = m_logFiles.at(i);

        auto* item = new QListWidgetItem(m_fileList);
        item->setData(LogNameRole, logFile.name);
        item->setData(LogModifiedDisplayRole, formatCompactDateTime(logFile.dateModified));
        item->setText(logFile.name);
        item->setToolTip(logFile.name);
        item->setSizeHint(QSize(0, 34));

        if (logFile.name == preferredLogName) {
            selectedRow = i;
        }
    }

    const bool hasFiles = !m_logFiles.isEmpty();
    m_fileList->setVisible(hasFiles);
    m_emptyLabel->setVisible(!hasFiles);

    if (!hasFiles) {
        m_fileList->setCurrentRow(-1);
        m_currentLogName.clear();
        updateSelectionDetails(QString());
        return {};
    }

    if (selectedRow < 0) {
        selectedRow = 0;
    }

    m_fileList->setCurrentRow(selectedRow);
    const QString selectedLogName = m_logFiles.at(selectedRow).name;
    updateSelectionDetails(selectedLogName);
    return selectedLogName;
}

void PageLogs::updateSelectionDetails(const QString& logName) {
    m_currentLogName = logName;

    const LogFileInfo* selectedInfo = nullptr;
    for (const LogFileInfo& logFile : m_logFiles) {
        if (logFile.name == logName) {
            selectedInfo = &logFile;
            break;
        }
    }

    if (!selectedInfo) {
        m_currentFileNameLabel->setFullText(tr("No log file selected"));
        m_currentFileMetaLabel->setText(tr("Choose a log file from the list to inspect the latest server output."));
        return;
    }

    m_currentFileNameLabel->setFullText(selectedInfo->name);

    QStringList metaParts;
    metaParts.append(selectedInfo->readableSize());
    const QString modifiedText = formatIsoDateTime(selectedInfo->dateModified);
    if (!modifiedText.isEmpty()) {
        metaParts.append(tr("Modified %1").arg(modifiedText));
    }
    m_currentFileMetaLabel->setText(metaParts.join(QStringLiteral("  ·  ")));
}

void PageLogs::setViewerPlaceholder(const QString& text) {
    m_viewerPlaceholder->setText(text);
    m_viewerPlaceholder->show();
    m_logViewer->hide();
}

QCoro::Task<void> PageLogs::loadFileList(bool isManual) {
    QPointer<PageLogs> safeThis(this);

    if (m_isLoading && !isManual) {
        co_return;
    }

    m_isLoading = true;
    m_btnRefresh->setEnabled(false);
    m_fileList->setEnabled(false);

    if (m_logFiles.isEmpty()) {
        m_statsLabel->setText(tr("Loading..."));
        setViewerPlaceholder(tr("Loading..."));
    }

    if (isManual) {
        const QIcon originalIcon = m_btnRefresh->icon();
        const QSize iconSize = m_btnRefresh->iconSize();
        const QPixmap basePixmap = originalIcon.pixmap(iconSize);

        auto* rotationAnimation = new QVariantAnimation(this);
        rotationAnimation->setStartValue(0.0);
        rotationAnimation->setEndValue(360.0);
        rotationAnimation->setDuration(800);
        rotationAnimation->setEasingCurve(QEasingCurve::InOutCubic);

        connect(rotationAnimation, &QVariantAnimation::valueChanged, this,
                [this, basePixmap, iconSize](const QVariant& value) {
                    const qreal angle = value.toReal();
                    QTransform transform;
                    transform.translate(iconSize.width() / 2.0, iconSize.height() / 2.0);
                    transform.rotate(angle);
                    transform.translate(-iconSize.width() / 2.0, -iconSize.height() / 2.0);

                    const QPixmap rotated = basePixmap.transformed(transform, Qt::SmoothTransformation);
                    const int xOffset = (rotated.width() - iconSize.width()) / 2;
                    const int yOffset = (rotated.height() - iconSize.height()) / 2;
                    m_btnRefresh->setIcon(
                        QIcon(rotated.copy(xOffset, yOffset, iconSize.width(), iconSize.height())));
                });
        connect(rotationAnimation, &QVariantAnimation::finished, this,
                [this, originalIcon]() { m_btnRefresh->setIcon(originalIcon); });
        rotationAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    }

    try {
        QList<LogFileInfo> logFiles = co_await m_core->adminService()->getLogFiles();
        if (!safeThis) {
            co_return;
        }

        std::sort(logFiles.begin(), logFiles.end(), [](const LogFileInfo& lhs, const LogFileInfo& rhs) {
            const QDateTime lhsModified = parseIsoDateTime(lhs.dateModified);
            const QDateTime rhsModified = parseIsoDateTime(rhs.dateModified);

            if (lhsModified.isValid() != rhsModified.isValid()) {
                return lhsModified.isValid();
            }
            if (lhsModified.isValid() && lhsModified != rhsModified) {
                return lhsModified > rhsModified;
            }
            return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
        });

        qDebug() << "[PageLogs] Loaded log files"
                 << "| count=" << logFiles.size();

        const QString currentSelectedLog = currentSelectedLogName();
        const QString preferredLogName = currentSelectedLog.isEmpty() ? m_currentLogName : currentSelectedLog;
        m_logFiles = logFiles;
        m_statsLabel->setText(m_logFiles.isEmpty() ? tr("No files") : tr("%n file(s)", "", m_logFiles.size()));

        const QString selectedLogName = rebuildFileList(preferredLogName);
        if (selectedLogName.isEmpty()) {
            m_logViewer->clear();
            setViewerPlaceholder(tr("No log files found"));
        } else {
            co_await loadLogContent(selectedLogName);
            if (!safeThis) {
                co_return;
            }
        }
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        if (m_logFiles.isEmpty()) {
            m_statsLabel->setText(tr("No files"));
            setViewerPlaceholder(tr("Failed to load: %1").arg(QString::fromUtf8(e.what())));
        }
        ModernToast::showMessage(tr("Failed to load log files: %1").arg(QString::fromUtf8(e.what())), 3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isLoading = false;
    m_btnRefresh->setEnabled(true);
    m_fileList->setEnabled(true);
}

QCoro::Task<void> PageLogs::loadLogContent(QString logName) {
    QPointer<PageLogs> safeThis(this);

    m_currentLogName = logName;
    updateSelectionDetails(logName);

    qDebug() << "[PageLogs] Loading log content"
             << "| logName=" << logName;

    setViewerPlaceholder(tr("Loading..."));

    try {
        const QString content = co_await m_core->adminService()->getLogFileContent(logName);
        if (!safeThis) {
            co_return;
        }

        if (m_currentLogName != logName) {
            qDebug() << "[PageLogs] Ignoring stale log content"
                     << "| requested=" << logName
                     << "| current=" << m_currentLogName;
            co_return;
        }

        qDebug() << "[PageLogs] Log content loaded"
                 << "| logName=" << logName
                 << "| contentLength=" << content.length();

        m_logViewer->setPlainText(content);
        m_viewerPlaceholder->hide();
        m_logViewer->show();

        if (QScrollBar* verticalBar = m_logViewer->verticalScrollBar()) {
            verticalBar->setValue(verticalBar->maximum());
        }
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        if (m_currentLogName != logName) {
            co_return;
        }

        setViewerPlaceholder(tr("Failed to load: %1").arg(QString::fromUtf8(e.what())));
    }
}

QCoro::Task<void> PageLogs::downloadLogFile(QString logName) {
    QPointer<PageLogs> safeThis(this);

    QString defaultDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultDirectory.isEmpty()) {
        defaultDirectory = QDir::homePath();
    }

    const QString suggestedPath = QDir(defaultDirectory).filePath(logName);
    const QString savePath = QFileDialog::getSaveFileName(this, tr("Save Log File"), suggestedPath,
                                                          tr("Text Files (*.txt);;All Files (*)"));
    if (savePath.isEmpty()) {
        co_return;
    }

    qDebug() << "[PageLogs] Saving log file"
             << "| logName=" << logName
             << "| savePath=" << savePath;

    try {
        const QString content = co_await m_core->adminService()->getLogFileContent(logName);
        if (!safeThis) {
            co_return;
        }

        QSaveFile saveFile(savePath);
        if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            throw std::runtime_error(saveFile.errorString().toStdString());
        }

        const QByteArray data = content.toUtf8();
        if (saveFile.write(data) != data.size()) {
            throw std::runtime_error(saveFile.errorString().toStdString());
        }

        if (!saveFile.commit()) {
            throw std::runtime_error(saveFile.errorString().toStdString());
        }

        ModernToast::showMessage(tr("Log file \"%1\" saved").arg(QFileInfo(savePath).fileName()), 2200);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(tr("Failed to save log file: %1").arg(QString::fromUtf8(e.what())), 3200);
    }
}
