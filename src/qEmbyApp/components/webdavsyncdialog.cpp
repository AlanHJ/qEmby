#include "webdavsyncdialog.h"

#include "modernmessagebox.h"
#include "modernswitch.h"
#include "moderntoast.h"
#include "webdavpassphraseprompt.h"

#include "../managers/thememanager.h"

#include "config/webdavprofilestore.h"
#include "services/manager/servermanager.h"

#include <QAction>
#include <QButtonGroup>
#include <QDateTime>
#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

#include <qcoro/qcorotask.h>

#include <exception>

namespace
{


QString humanReadableSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024.0)
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
    const double mb = kb / 1024.0;
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 2);
}

QString formatLocalDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid())
        return QStringLiteral("--");
    return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString displayOrDash(const QString &text)
{
    return text.isEmpty() ? QStringLiteral("--") : text;
}

QIcon makeSpinnerIcon(qreal angle)
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    constexpr qreal kPenWidth = 2.2;
    const QRectF arcRect(kPenWidth, kPenWidth,
                         pixmap.width() - kPenWidth * 2,
                         pixmap.height() - kPenWidth * 2);

    QPen trackPen(QColor(148, 163, 184, 70), kPenWidth);
    trackPen.setCapStyle(Qt::RoundCap);
    painter.setPen(trackPen);
    painter.drawArc(arcRect, 0, 360 * 16);

    QPen sweepPen(QColor(96, 165, 250, 235), kPenWidth);
    sweepPen.setCapStyle(Qt::RoundCap);
    painter.setPen(sweepPen);
    painter.drawArc(arcRect, static_cast<int>(angle * 16), 105 * 16);

    return QIcon(pixmap);
}

QIcon makeCheckIcon()
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(34, 197, 94), 2.4);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.drawLine(QPointF(4.0, 9.5), QPointF(7.5, 13.0));
    painter.drawLine(QPointF(7.5, 13.0), QPointF(14.2, 5.2));
    return QIcon(pixmap);
}

QIcon makeErrorIcon()
{
    QPixmap pixmap(18, 18);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(QColor(239, 68, 68), 2.3);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.drawLine(QPointF(5.0, 5.0), QPointF(13.0, 13.0));
    painter.drawLine(QPointF(13.0, 5.0), QPointF(5.0, 13.0));
    return QIcon(pixmap);
}

} 



WebdavSyncDialog::WebdavSyncDialog(WebdavProfileStore *store, ServerManager *serverManager,
                                   QWidget *parent)
    : ModernDialogBase(parent), m_store(store), m_serverManager(serverManager)
{
    setObjectName("webdav-sync-dialog");
    setTitle(tr("Cloud Sync (WebDAV)"));
    setMinimumSize(560, 640);
    resize(620, 700);

    m_service = new WebdavSyncService(store, this);

    buildUi();
    wireSignals();
    loadProfileToUi();
    onSnapshotSelectionChanged(); 
}

WebdavSyncDialog::~WebdavSyncDialog() = default;

void WebdavSyncDialog::showEvent(QShowEvent *event)
{
    ModernDialogBase::showEvent(event);
    if (m_baseUrlEdit && m_baseUrlEdit->text().trimmed().isEmpty())
    {
        m_baseUrlEdit->setFocus();
    }
    if (!m_autoRefreshedOnShow)
    {
        m_autoRefreshedOnShow = true;
        requestSnapshotRefresh();
    }
}





void WebdavSyncDialog::buildUi()
{
    auto *root = contentLayout();
    root->setSpacing(10);

    
    {
        auto *card = new QFrame(this);
        card->setObjectName("webdav-card");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(7);

        auto *title = new QLabel(tr("Server Configuration"), card);
        title->setObjectName("webdav-card-title");
        cardLayout->addWidget(title);

        constexpr int kPrimaryLabelWidth = 74;
        constexpr int kSecondaryLabelWidth = 52;

        auto *form = new QGridLayout();
        form->setContentsMargins(0, 2, 0, 0);
        form->setHorizontalSpacing(8);
        form->setVerticalSpacing(7);
        form->setColumnMinimumWidth(0, kPrimaryLabelWidth);
        form->setColumnMinimumWidth(2, kSecondaryLabelWidth);
        form->setColumnStretch(1, 1);
        form->setColumnStretch(3, 1);

        auto makeFieldLabel = [card](const QString &text, int width) {
            auto *label = new QLabel(text, card);
            label->setObjectName("webdav-field-label");
            label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            label->setMinimumWidth(width);
            label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            return label;
        };

        auto tuneEdit = [](QLineEdit *edit) {
            edit->setMinimumWidth(0);
            edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        };

        m_baseUrlEdit = new QLineEdit(card);
        m_baseUrlEdit->setPlaceholderText(QStringLiteral("https://dav.example.com/dav/"));
        tuneEdit(m_baseUrlEdit);
        form->addWidget(makeFieldLabel(tr("Server URL:"), kPrimaryLabelWidth), 0, 0);
        form->addWidget(m_baseUrlEdit, 0, 1, 1, 3);

        m_userEdit = new QLineEdit(card);
        m_userEdit->setPlaceholderText(tr("WebDAV username"));
        tuneEdit(m_userEdit);

        
        
        m_passwordEdit = new QLineEdit(card);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText(tr("WebDAV password"));
        tuneEdit(m_passwordEdit);

        m_togglePwdAction = new QAction(
            ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/eye.svg")),
            tr("Show Password"), this);
        m_togglePwdAction->setCheckable(true);
        m_passwordEdit->addAction(m_togglePwdAction, QLineEdit::TrailingPosition);

        form->addWidget(makeFieldLabel(tr("Username:"), kPrimaryLabelWidth), 1, 0);
        form->addWidget(m_userEdit, 1, 1);
        form->addWidget(makeFieldLabel(tr("Password:"), kSecondaryLabelWidth), 1, 2);
        form->addWidget(m_passwordEdit, 1, 3);

        m_rootDirEdit = new QLineEdit(card);
        m_rootDirEdit->setPlaceholderText(QStringLiteral("/qEmby"));
        tuneEdit(m_rootDirEdit);

        
        {
            auto *sslControls = new QHBoxLayout();
            sslControls->setContentsMargins(0, 0, 0, 0);
            sslControls->setSpacing(8);
            m_ignoreSslSwitch = new ModernSwitch(card);
            auto *hint = new QLabel(tr("Ignore SSL certificate errors"), card);
            hint->setObjectName("webdav-hint");
            hint->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            sslControls->addWidget(m_ignoreSslSwitch, 0, Qt::AlignVCenter);
            sslControls->addWidget(hint, 0, Qt::AlignVCenter);
            sslControls->addStretch(1);

            form->addWidget(makeFieldLabel(tr("Root directory:"), kPrimaryLabelWidth), 2, 0);
            form->addWidget(m_rootDirEdit, 2, 1);
            form->addWidget(makeFieldLabel(tr("HTTPS:"), kSecondaryLabelWidth), 2, 2);
            form->addLayout(sslControls, 2, 3);
        }

        
        {
            auto *btnRow = new QHBoxLayout();
            btnRow->setContentsMargins(0, 0, 0, 0);
            btnRow->setSpacing(8);
            btnRow->addStretch(1);

            m_testBtn = new QPushButton(tr("Test Connection"), card);
            m_testBtn->setObjectName("dialog-btn-cancel");
            m_testBtn->setCursor(Qt::PointingHandCursor);
            m_testBtn->setIconSize(QSize(16, 16));
            btnRow->addWidget(m_testBtn);

            m_saveProfileBtn = new QPushButton(tr("Save Profile"), card);
            m_saveProfileBtn->setObjectName("dialog-btn-primary");
            m_saveProfileBtn->setCursor(Qt::PointingHandCursor);
            btnRow->addWidget(m_saveProfileBtn);

            form->addLayout(btnRow, 3, 1, 1, 3);
        }

        cardLayout->addLayout(form);

        root->addWidget(card);
    }

    
    {
        auto *card = new QFrame(this);
        card->setObjectName("webdav-card");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 8, 12, 8);
        cardLayout->setSpacing(6);

        auto *title = new QLabel(tr("Upload Current Configuration"), card);
        title->setObjectName("webdav-card-title");
        cardLayout->addWidget(title);

        auto *tagRow = new QHBoxLayout();
        tagRow->setContentsMargins(0, 0, 0, 0);
        tagRow->setSpacing(8);
        auto *tagLabel = new QLabel(tr("Custom tag:"), card);
        tagLabel->setObjectName("webdav-hint");
        tagRow->addWidget(tagLabel, 0, Qt::AlignVCenter);

        m_snapshotTagEdit = new QLineEdit(card);
        m_snapshotTagEdit->setPlaceholderText(tr("Snapshot tag"));
        m_snapshotTagEdit->setText(QStringLiteral("snapshot"));
        m_snapshotTagEdit->setMinimumWidth(120);
        m_snapshotTagEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        tagRow->addWidget(m_snapshotTagEdit, 1);

        m_encryptSnapshotSwitch = new ModernSwitch(card);
        m_encryptSnapshotSwitch->setChecked(true);
        auto *encryptLabel = new QLabel(tr("Encrypt save"), card);
        encryptLabel->setObjectName("webdav-hint");
        tagRow->addWidget(m_encryptSnapshotSwitch, 0, Qt::AlignVCenter);
        tagRow->addWidget(encryptLabel, 0, Qt::AlignVCenter);

        m_uploadBtn = new QPushButton(tr("Upload Now"), card);
        m_uploadBtn->setObjectName("dialog-btn-primary");
        m_uploadBtn->setCursor(Qt::PointingHandCursor);
        tagRow->addWidget(m_uploadBtn, 0, Qt::AlignVCenter);

        cardLayout->addLayout(tagRow);

        m_proposedFileLabel = new QLabel(card);
        m_proposedFileLabel->setObjectName("webdav-hint");
        m_proposedFileLabel->setWordWrap(true);
        cardLayout->addWidget(m_proposedFileLabel);

        root->addWidget(card);
    }

    
    {
        auto *card = new QFrame(this);
        card->setObjectName("webdav-card");
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(7);

        auto *headerRow = new QHBoxLayout();
        auto *title = new QLabel(tr("Snapshots on Server"), card);
        title->setObjectName("webdav-card-title");
        headerRow->addWidget(title);

        m_lastSyncLabel = new QLabel(card);
        m_lastSyncLabel->setObjectName("webdav-hint");
        m_lastSyncLabel->setMinimumWidth(0);
        headerRow->addWidget(m_lastSyncLabel, 0, Qt::AlignVCenter);

        auto *sep = new QLabel(QStringLiteral("·"), card);
        sep->setObjectName("webdav-hint");
        headerRow->addWidget(sep, 0, Qt::AlignVCenter);

        m_snapshotStatusLabel = new QLabel(card);
        m_snapshotStatusLabel->setObjectName("webdav-hint");
        m_snapshotStatusLabel->setMinimumWidth(0);
        headerRow->addWidget(m_snapshotStatusLabel, 0, Qt::AlignVCenter);
        headerRow->addStretch();

        m_refreshBtn = new QPushButton(tr("Refresh"), card);
        m_refreshBtn->setObjectName("dialog-btn-cancel");
        m_refreshBtn->setCursor(Qt::PointingHandCursor);
        headerRow->addWidget(m_refreshBtn);
        cardLayout->addLayout(headerRow);

        m_snapshotTable = new QTableView(card);
        m_snapshotTable->setObjectName("webdav-snapshot-table");
        m_snapshotTable->viewport()->setObjectName("webdav-snapshot-viewport");
        m_snapshotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_snapshotTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_snapshotTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_snapshotTable->setTextElideMode(Qt::ElideMiddle);
        m_snapshotTable->verticalHeader()->setVisible(false);
        m_snapshotTable->horizontalHeader()->setStretchLastSection(false);
        m_snapshotTable->setMinimumHeight(160);

        m_snapshotModel = new QStandardItemModel(this);
        m_snapshotModel->setHorizontalHeaderLabels(
            {tr("Device"), tr("Version"), tr("OS"), tr("Created"),
             tr("Size"), tr("File Name")});
        m_snapshotTable->setModel(m_snapshotModel);
        m_snapshotTable->setColumnWidth(0, 105);
        m_snapshotTable->setColumnWidth(1, 78);
        m_snapshotTable->setColumnWidth(2, 82);
        m_snapshotTable->setColumnWidth(3, 150);
        m_snapshotTable->setColumnWidth(4, 78);
        m_snapshotTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

        cardLayout->addWidget(m_snapshotTable);

        
        {
            auto *strategyRow = new QHBoxLayout();
            strategyRow->setContentsMargins(0, 2, 0, 0);
            strategyRow->setSpacing(10);
            auto *label = new QLabel(tr("Apply strategy:"), card);
            label->setObjectName("webdav-strategy-label");
            strategyRow->addWidget(label, 0, Qt::AlignVCenter);

            auto *selector = new QFrame(card);
            selector->setObjectName("webdav-strategy-selector");
            selector->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            auto *selectorLayout = new QHBoxLayout(selector);
            selectorLayout->setContentsMargins(3, 3, 3, 3);
            selectorLayout->setSpacing(3);

            m_strategyGroup = new QButtonGroup(this);
            m_strategyGroup->setExclusive(true);
            m_strategyReplaceBtn = new QPushButton(tr("Replace"), selector);
            m_strategyMergeBtn = new QPushButton(tr("Merge"), selector);
            m_strategyLocalWinsBtn = new QPushButton(tr("Local wins"), selector);

            const auto tuneStrategyButton = [](QPushButton *button) {
                button->setProperty("webdavStrategyOption", true);
                button->setCheckable(true);
                button->setCursor(Qt::PointingHandCursor);
                button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            };
            tuneStrategyButton(m_strategyReplaceBtn);
            tuneStrategyButton(m_strategyMergeBtn);
            tuneStrategyButton(m_strategyLocalWinsBtn);

            m_strategyGroup->addButton(m_strategyReplaceBtn,
                                        static_cast<int>(MergeStrategy::Replace));
            m_strategyGroup->addButton(m_strategyMergeBtn,
                                        static_cast<int>(MergeStrategy::Merge));
            m_strategyGroup->addButton(m_strategyLocalWinsBtn,
                                        static_cast<int>(MergeStrategy::LocalWins));
            m_strategyReplaceBtn->setChecked(true);

            selectorLayout->addWidget(m_strategyReplaceBtn);
            selectorLayout->addWidget(m_strategyMergeBtn);
            selectorLayout->addWidget(m_strategyLocalWinsBtn);

            strategyRow->addWidget(selector, 0, Qt::AlignVCenter);
            strategyRow->addStretch();

            cardLayout->addLayout(strategyRow);
        }

        
        {
            auto *btnRow = new QHBoxLayout();
            btnRow->addStretch();

            m_deleteBtn = new QPushButton(tr("Delete Selected"), card);
            m_deleteBtn->setObjectName("dialog-btn-cancel");
            m_deleteBtn->setCursor(Qt::PointingHandCursor);
            btnRow->addWidget(m_deleteBtn);

            m_applyBtn = new QPushButton(tr("Apply to Local"), card);
            m_applyBtn->setObjectName("dialog-btn-primary");
            m_applyBtn->setCursor(Qt::PointingHandCursor);
            btnRow->addWidget(m_applyBtn);

            cardLayout->addLayout(btnRow);
        }

        root->addWidget(card, 1);
    }
}

void WebdavSyncDialog::wireSignals()
{
    connect(m_testBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onTestConnectionClicked);
    connect(m_saveProfileBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onSaveProfileClicked);
    connect(m_togglePwdAction, &QAction::toggled, this,
            &WebdavSyncDialog::onTogglePasswordVisibility);

    connect(m_uploadBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onUploadClicked);
    connect(m_snapshotTagEdit, &QLineEdit::textChanged, this,
            &WebdavSyncDialog::updateProposedFileName);
    connect(m_refreshBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onRefreshSnapshotsClicked);
    connect(m_applyBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onApplySelectedClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this,
            &WebdavSyncDialog::onDeleteSelectedClicked);

    connect(m_snapshotTable->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &WebdavSyncDialog::onSnapshotSelectionChanged);

    if (m_service)
    {
        connect(m_service, &WebdavSyncService::snapshotListChanged, this,
                &WebdavSyncDialog::requestSnapshotRefresh);
    }

    m_testSpinnerTimer = new QTimer(this);
    m_testSpinnerTimer->setInterval(40);
    connect(m_testSpinnerTimer, &QTimer::timeout, this, [this]() {
        m_testSpinnerAngle -= 18.0;
        if (m_testSpinnerAngle < -360.0)
            m_testSpinnerAngle += 360.0;
        if (m_testBtn)
            m_testBtn->setIcon(makeSpinnerIcon(m_testSpinnerAngle));
    });
}





void WebdavSyncDialog::loadProfileToUi()
{
    WebdavProfile p;
    if (m_store && m_store->hasProfile())
    {
        p = m_store->profile();
    }
    else
    {
        p = WebdavProfile::defaults();
    }

    m_baseUrlEdit->setText(p.baseUrl);
    m_userEdit->setText(p.username);
    m_passwordEdit->setText(p.password);
    m_rootDirEdit->setText(p.rootDir);
    m_ignoreSslSwitch->setChecked(p.ignoreSsl);

    if (p.lastSyncAt > 0)
    {
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(p.lastSyncAt);
        m_lastSyncLabel->setText(
            tr("Last sync: %1").arg(ts.toLocalTime().toString(
                QStringLiteral("yyyy-MM-dd HH:mm"))));
    }
    else
    {
        m_lastSyncLabel->setText(tr("Last sync: never"));
    }

    updateProposedFileName();

    if (m_snapshotStatusLabel)
        m_snapshotStatusLabel->clear();
}

WebdavProfile WebdavSyncDialog::collectProfileFromUi() const
{
    WebdavProfile p;
    if (m_store && m_store->hasProfile())
    {
        p = m_store->profile(); 
    }
    else
    {
        p = WebdavProfile::defaults();
    }

    p.baseUrl = m_baseUrlEdit->text().trimmed();
    p.username = m_userEdit->text();
    p.password = m_passwordEdit->text();
    p.rootDir = m_rootDirEdit->text().trimmed();
    if (p.rootDir.isEmpty())
    {
        p.rootDir = QStringLiteral("/qEmby");
    }
    p.ignoreSsl = m_ignoreSslSwitch->isChecked();
    p.normalize();
    return p;
}

QString WebdavSyncDialog::currentSnapshotTag() const
{
    return m_snapshotTagEdit ? m_snapshotTagEdit->text().trimmed() : QString();
}

void WebdavSyncDialog::updateProposedFileName()
{
    if (!m_proposedFileLabel)
    {
        return;
    }

    const ConfigBundleMetadata metadata = ConfigBundleMetadata::current();
    m_proposedFileLabel->setText(
        tr("This device will be uploaded as:\n%1")
            .arg(WebdavSyncService::proposedSnapshotFileName(metadata,
                                                             currentSnapshotTag())));
}





void WebdavSyncDialog::onTogglePasswordVisibility()
{
    if (!m_togglePwdAction || !m_passwordEdit)
        return;
    const bool visible = m_togglePwdAction->isChecked();
    m_passwordEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    m_togglePwdAction->setIcon(ThemeManager::getAdaptiveIcon(
        visible ? QStringLiteral(":/svg/dark/eye-off.svg")
                : QStringLiteral(":/svg/dark/eye.svg")));
    m_togglePwdAction->setText(visible ? tr("Hide Password") : tr("Show Password"));
}

void WebdavSyncDialog::onTestConnectionClicked()
{
    if (m_busy)
    {
        return;
    }
    const WebdavProfile pending = collectProfileFromUi();
    if (!pending.isValid())
    {
        showError(tr("Invalid profile"),
                  tr("Please fill in the server URL, username and password before testing."));
        return;
    }

    
    if (m_store)
    {
        WebdavProfile preserved = pending;
        if (m_store->hasProfile())
        {
            const auto orig = m_store->profile();
            preserved.id = orig.id;
            preserved.lastSyncAt = orig.lastSyncAt;
        }
        m_store->setProfile(preserved);
    }

    setBusy(true);
    setTestButtonLoading();

    [](WebdavSyncDialog *self) -> QCoro::Task<void>
    {
        bool success = false;
        try
        {
            const bool ok = co_await self->m_service->testConnection();
            if (ok)
            {
                success = true;
                self->showToast(self->tr("WebDAV connection succeeded"));
            }
            else
            {
                self->showError(
                    self->tr("Connection failed"),
                    self->tr("Server replied but the response is not valid WebDAV."));
            }
        }
        catch (const std::exception &e)
        {
            self->showError(self->tr("Connection failed"),
                            QString::fromUtf8(e.what()));
        }
        self->setBusy(false);
        self->setTestButtonResult(success);
        co_return;
    }(this);
}

void WebdavSyncDialog::onSaveProfileClicked()
{
    const WebdavProfile pending = collectProfileFromUi();
    if (!pending.isValid())
    {
        showError(tr("Invalid profile"),
                  tr("Please fill in the server URL, username and password before saving."));
        return;
    }
    if (!m_store)
    {
        showError(tr("Internal error"), tr("Profile store is unavailable."));
        return;
    }

    
    WebdavProfile toSave = pending;
    if (m_store->hasProfile())
    {
        const auto orig = m_store->profile();
        toSave.id = orig.id;
        toSave.lastSyncAt = orig.lastSyncAt;
    }
    m_store->setProfile(toSave);
    if (m_store->save())
    {
        showToast(tr("Profile saved"));
        loadProfileToUi();
        requestSnapshotRefresh();
    }
    else
    {
        showError(tr("Save failed"),
                  tr("qEmby could not write to %1.").arg(WebdavProfileStore::filePath()));
    }
}

void WebdavSyncDialog::onUploadClicked()
{
    if (m_busy)
        return;
    if (!m_store || !m_store->hasProfile())
    {
        showError(tr("Profile not configured"),
                  tr("Please save the WebDAV profile before uploading."));
        return;
    }

    updateProposedFileName();

    const bool encrypt = !m_encryptSnapshotSwitch || m_encryptSnapshotSwitch->isChecked();
    QString passphrase;
    if (encrypt)
    {
        bool ok = false;
        passphrase = WebdavPassphrasePrompt::getPassphrase(
            this, tr("Encrypt Snapshot"),
            tr("Enter a passphrase to encrypt this snapshot.\n"
               "You will need the same passphrase to restore it on another device."),
            tr("Upload"), &ok);
        if (!ok || passphrase.isEmpty())
        {
            return;
        }
    }

    setBusy(true, tr("Uploading snapshot..."));

    [](WebdavSyncDialog *self, QString customTag, bool encrypt, QString pp) -> QCoro::Task<void>
    {
        try
        {
            const QString fileName = co_await self->m_service->uploadSnapshot(
                std::move(customTag), encrypt, std::move(pp));
            self->showToast(self->tr("Uploaded: %1").arg(fileName));
            self->loadProfileToUi(); 
            self->m_pendingSnapshotRefresh = true;
        }
        catch (const std::exception &e)
        {
            self->showError(self->tr("Upload failed"), QString::fromUtf8(e.what()));
        }
        self->setBusy(false);
        co_return;
    }(this, currentSnapshotTag(), encrypt, passphrase);
}

void WebdavSyncDialog::onRefreshSnapshotsClicked()
{
    requestSnapshotRefresh();
}

void WebdavSyncDialog::requestSnapshotRefresh()
{
    m_pendingSnapshotRefresh = true;
    if (m_busy)
    {
        return;
    }

    refreshSnapshotList();
}

void WebdavSyncDialog::refreshSnapshotList()
{
    if (m_busy)
        return;
    if (!m_store || !m_store->hasProfile())
    {
        m_pendingSnapshotRefresh = false;
        clearSnapshotModel();
        return;
    }

    m_pendingSnapshotRefresh = false;
    setBusy(true, tr("Loading snapshot list..."));

    [](WebdavSyncDialog *self) -> QCoro::Task<void>
    {
        try
        {
            const QList<WebdavSnapshot> snapshots = co_await self->m_service->listSnapshots();
            self->clearSnapshotModel();
            for (const auto &s : snapshots)
            {
                self->appendSnapshotRow(s);
            }
            if (self->m_snapshotStatusLabel)
                self->m_snapshotStatusLabel->clear();
            self->onSnapshotSelectionChanged();
        }
        catch (const std::exception &e)
        {
            if (self->m_snapshotStatusLabel)
                self->m_snapshotStatusLabel->setText(self->tr("Failed"));
            self->showError(self->tr("Failed to list snapshots"),
                            QString::fromUtf8(e.what()));
        }
        self->setBusy(false);
        co_return;
    }(this);
}

void WebdavSyncDialog::onApplySelectedClicked()
{
    if (m_busy)
        return;
    const QString fileName = currentSelectedSnapshotName();
    if (fileName.isEmpty())
    {
        return;
    }

    const MergeStrategy strategy = currentMergeStrategy();
    const QString strategyText = (strategy == MergeStrategy::Replace
                                      ? tr("Replace local config entirely")
                                  : strategy == MergeStrategy::Merge
                                      ? tr("Merge (cloud wins on conflict)")
                                      : tr("Local wins (only fill missing keys)"));

    if (!ModernMessageBox::question(
            this, tr("Confirm apply"),
            tr("Apply snapshot \"%1\" with strategy:\n%2\n\nThis will overwrite parts of "
               "your local configuration. Continue?")
                .arg(fileName)
                .arg(strategyText),
            tr("Apply"), tr("Cancel"), ModernMessageBox::Primary,
            ModernMessageBox::Question))
    {
        return;
    }

    setBusy(true, tr("Downloading snapshot..."));

    [](WebdavSyncDialog *self, QString fileName, MergeStrategy strategy) -> QCoro::Task<void>
    {
        try
        {
            ConfigBundle bundle;
            bool needsPassphrase = false;
            try
            {
                bundle = co_await self->m_service->downloadSnapshot(fileName);
            }
            catch (const WebdavPassphraseRequiredError &)
            {
                needsPassphrase = true;
            }

            if (needsPassphrase)
            {
                bool ok = false;
                QString passphrase = WebdavPassphrasePrompt::getPassphrase(
                    self, self->tr("Decrypt Snapshot"),
                    self->tr("Enter the passphrase used when this snapshot was created."),
                    self->tr("Apply"), &ok);
                if (!ok || passphrase.isEmpty())
                {
                    self->setBusy(false);
                    co_return;
                }
                bundle = co_await self->m_service->downloadSnapshot(fileName,
                                                                    std::move(passphrase));
            }

            const bool applied = self->m_service->applyBundle(bundle, strategy);
            if (applied)
            {
                if (self->m_serverManager)
                {
                    self->m_serverManager->loadSettings();
                }
                self->showToast(self->tr("Snapshot applied"));
                self->loadProfileToUi();
            }
            else
            {
                self->showError(self->tr("Apply failed"),
                                self->tr("Bundle decoded but failed to write to local."));
            }
        }
        catch (const std::exception &e)
        {
            self->showError(self->tr("Apply failed"), QString::fromUtf8(e.what()));
        }
        self->setBusy(false);
        co_return;
    }(this, fileName, strategy);
}

void WebdavSyncDialog::onDeleteSelectedClicked()
{
    if (m_busy)
        return;
    const QString fileName = currentSelectedSnapshotName();
    if (fileName.isEmpty())
    {
        return;
    }

    if (!ModernMessageBox::question(
            this, tr("Delete snapshot"),
            tr("Delete \"%1\" from the WebDAV server?\n\nThis cannot be undone.").arg(fileName),
            tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
            ModernMessageBox::Warning))
    {
        return;
    }

    setBusy(true, tr("Deleting snapshot..."));

    [](WebdavSyncDialog *self, QString fileName) -> QCoro::Task<void>
    {
        try
        {
            const bool ok = co_await self->m_service->deleteSnapshot(fileName);
            if (ok)
            {
                self->showToast(self->tr("Snapshot deleted"));
                self->m_pendingSnapshotRefresh = true;
            }
        }
        catch (const std::exception &e)
        {
            self->showError(self->tr("Delete failed"), QString::fromUtf8(e.what()));
        }
        self->setBusy(false);
        co_return;
    }(this, fileName);
}

void WebdavSyncDialog::onSnapshotSelectionChanged()
{
    const bool selected = !currentSelectedSnapshotName().isEmpty();
    m_applyBtn->setEnabled(selected && !m_busy);
    m_deleteBtn->setEnabled(selected && !m_busy);
}





void WebdavSyncDialog::clearSnapshotModel()
{
    m_snapshotModel->removeRows(0, m_snapshotModel->rowCount());
}

void WebdavSyncDialog::appendSnapshotRow(const WebdavSnapshot &s)
{
    const QString deviceText = displayOrDash(s.deviceHint);
    const QString versionText = displayOrDash(s.appVersion);
    const QString osText = displayOrDash(s.osName);
    const QString createdText = formatLocalDateTime(s.createdAt);
    const QString sizeText = humanReadableSize(s.size);
    const QString tooltip = tr("Device: %1\nVersion: %2\nOS: %3\nCreated: %4\nSize: %5\nFile Name: %6")
                                .arg(deviceText, versionText, osText, createdText,
                                     sizeText, s.fileName);

    auto *colDevice = new QStandardItem(deviceText);
    auto *colVersion = new QStandardItem(versionText);
    auto *colOs = new QStandardItem(osText);
    auto *colCreated = new QStandardItem(createdText);
    auto *colSize = new QStandardItem(sizeText);
    auto *colName = new QStandardItem(s.fileName);

    for (QStandardItem *item : {colDevice, colVersion, colOs, colCreated, colSize, colName})
    {
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(tooltip);
    }

    
    colDevice->setData(s.fileName, Qt::UserRole + 1);

    m_snapshotModel->appendRow({colDevice, colVersion, colOs, colCreated, colSize, colName});
}

QString WebdavSyncDialog::currentSelectedSnapshotName() const
{
    if (!m_snapshotTable || !m_snapshotModel)
        return QString();
    const auto idx = m_snapshotTable->currentIndex();
    if (!idx.isValid())
        return QString();
    return m_snapshotModel->item(idx.row(), 0)
        ->data(Qt::UserRole + 1)
        .toString();
}

MergeStrategy WebdavSyncDialog::currentMergeStrategy() const
{
    if (!m_strategyGroup)
        return MergeStrategy::Replace;
    const int id = m_strategyGroup->checkedId();
    if (id < 0)
        return MergeStrategy::Replace;
    return static_cast<MergeStrategy>(id);
}

void WebdavSyncDialog::setBusy(bool busy, const QString &hint)
{
    m_busy = busy;
    m_testBtn->setEnabled(!busy);
    m_saveProfileBtn->setEnabled(!busy);
    m_uploadBtn->setEnabled(!busy);
    if (m_snapshotTagEdit)
        m_snapshotTagEdit->setEnabled(!busy);
    if (m_encryptSnapshotSwitch)
        m_encryptSnapshotSwitch->setEnabled(!busy);
    m_refreshBtn->setEnabled(!busy);
    onSnapshotSelectionChanged();

    if (!hint.isEmpty())
    {
        if (m_snapshotStatusLabel)
            m_snapshotStatusLabel->setText(hint);
    }

    if (!busy && m_pendingSnapshotRefresh)
    {
        requestSnapshotRefresh();
    }
}

void WebdavSyncDialog::showError(const QString &title, const QString &text)
{
    qWarning() << "[WebdavSyncDialog] error |" << title << "|" << text;
    ModernMessageBox::critical(this, title, text);
}

void WebdavSyncDialog::showToast(const QString &text)
{
    ModernToast::showMessage(text);
}

void WebdavSyncDialog::setTestButtonIdle()
{
    if (m_testSpinnerTimer)
        m_testSpinnerTimer->stop();
    if (!m_testBtn)
        return;
    m_testBtn->setIcon(QIcon());
    m_testBtn->setText(tr("Test Connection"));
}

void WebdavSyncDialog::setTestButtonLoading()
{
    if (!m_testBtn)
        return;

    m_testSpinnerAngle = 0.0;
    m_testBtn->setText(tr("Testing connection..."));
    m_testBtn->setIcon(makeSpinnerIcon(m_testSpinnerAngle));
    if (m_testSpinnerTimer)
        m_testSpinnerTimer->start();
}

void WebdavSyncDialog::setTestButtonResult(bool success)
{
    if (m_testSpinnerTimer)
        m_testSpinnerTimer->stop();
    if (!m_testBtn)
        return;

    m_testBtn->setIcon(success ? makeCheckIcon() : makeErrorIcon());
    m_testBtn->setText(success ? tr("Connection OK") : tr("Failed"));

    QPointer<WebdavSyncDialog> safeThis(this);
    QTimer::singleShot(1400, this, [safeThis]() {
        if (!safeThis)
            return;
        safeThis->setTestButtonIdle();
    });
}
