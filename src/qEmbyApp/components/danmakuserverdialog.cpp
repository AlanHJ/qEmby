#include "danmakuserverdialog.h"

#include "danmakuserverlistitemwidget.h"
#include "modernmessagebox.h"
#include "services/danmaku/danmakusettings.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <utility>

namespace {

QString fallbackServerName(const DanmakuServerDefinition &server)
{
    return server.name.trimmed().isEmpty() ? server.baseUrl.trimmed()
                                           : server.name.trimmed();
}

} 

DanmakuServerDialog::DanmakuServerDialog(QWidget *parent)
    : ModernDialogBase(parent)
{
    setTitle(tr("Danmaku Servers"));
    setMinimumSize(920, 620);

    auto *hintLabel = new QLabel(
        tr("Manage multiple danmaku servers here. Click a server card to edit "
           "it and choose the active server used for matching and loading "
           "danmaku."),
        this);
    hintLabel->setObjectName("dialog-text");
    hintLabel->setWordWrap(true);
    contentLayout()->addWidget(hintLabel);
    contentLayout()->addSpacing(8);

    auto *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(18);

    auto *listPanel = new QFrame(this);
    listPanel->setObjectName("DanmakuServerListPanel");
    listPanel->setMinimumWidth(360);
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->setContentsMargins(18, 18, 18, 18);
    listLayout->setSpacing(12);

    auto *listTitle = new QLabel(tr("Servers"), listPanel);
    listTitle->setObjectName("SettingsCardTitle");
    listLayout->addWidget(listTitle);

    auto *listDesc = new QLabel(
        tr("Each item shows the danmaku server name, address, app "
           "credentials, and description. The built-in official "
           "DandanPlay server supports anime only."),
        listPanel);
    listDesc->setObjectName("SettingsCardDesc");
    listDesc->setWordWrap(true);
    listLayout->addWidget(listDesc);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(10);

    m_addButton = new QPushButton(tr("Add"), listPanel);
    m_addButton->setObjectName("SettingsCardButton");
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setFixedHeight(32);
    actionRow->addWidget(m_addButton);
    actionRow->addStretch();
    listLayout->addLayout(actionRow);

    m_serverList = new QListWidget(listPanel);
    m_serverList->setObjectName("DanmakuServerList");
    m_serverList->setFrameShape(QFrame::NoFrame);
    m_serverList->setCursor(Qt::PointingHandCursor);
    m_serverList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_serverList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_serverList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_serverList->setSpacing(10);
    m_serverList->viewport()->installEventFilter(this);
    listLayout->addWidget(m_serverList, 1);

    bodyLayout->addWidget(listPanel, 1);

    auto *editorPanel = new QFrame(this);
    editorPanel->setObjectName("DanmakuServerEditorPanel");
    editorPanel->setMinimumWidth(420);
    auto *editorLayout = new QVBoxLayout(editorPanel);
    editorLayout->setContentsMargins(18, 18, 18, 18);
    editorLayout->setSpacing(14);

    auto *editorTitle = new QLabel(tr("Edit Selected Server"), editorPanel);
    editorTitle->setObjectName("DanmakuServerEditorTitle");
    editorLayout->addWidget(editorTitle);

    auto *editorDesc = new QLabel(
        tr("Update the server name, address, description, and app credentials for the "
           "selected danmaku server. Official DandanPlay endpoints require "
           "App ID and App Secret and support anime only."),
        editorPanel);
    editorDesc->setObjectName("DanmakuServerEditorDesc");
    editorDesc->setWordWrap(true);
    editorLayout->addWidget(editorDesc);

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 4, 0, 0);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(14);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *nameLabel = new QLabel(tr("Name"), editorPanel);
    nameLabel->setObjectName("LibraryMetadataFieldLabel");
    m_nameEdit = new QLineEdit(editorPanel);
    m_nameEdit->setObjectName("ManageLibInput");
    m_nameEdit->setClearButtonEnabled(true);
    m_nameEdit->setPlaceholderText(tr("e.g., Official DandanPlay"));
    formLayout->addRow(nameLabel, m_nameEdit);

    auto *urlLabel = new QLabel(tr("Server URL"), editorPanel);
    urlLabel->setObjectName("LibraryMetadataFieldLabel");
    m_baseUrlEdit = new QLineEdit(editorPanel);
    m_baseUrlEdit->setObjectName("ManageLibInput");
    m_baseUrlEdit->setClearButtonEnabled(true);
    m_baseUrlEdit->setPlaceholderText(
        QStringLiteral("https://api.dandanplay.net"));
    formLayout->addRow(urlLabel, m_baseUrlEdit);

    m_descriptionLabel = new QLabel(tr("Description"), editorPanel);
    m_descriptionLabel->setObjectName("LibraryMetadataFieldLabel");
    m_descriptionEdit = new QLineEdit(editorPanel);
    m_descriptionEdit->setObjectName("ManageLibInput");
    m_descriptionEdit->setClearButtonEnabled(true);
    m_descriptionEdit->setPlaceholderText(tr("e.g., Supports anime only"));
    formLayout->addRow(m_descriptionLabel, m_descriptionEdit);

    m_appIdLabel = new QLabel(tr("App ID"), editorPanel);
    m_appIdLabel->setObjectName("LibraryMetadataFieldLabel");
    m_appIdEdit = new QLineEdit(editorPanel);
    m_appIdEdit->setObjectName("ManageLibInput");
    m_appIdEdit->setClearButtonEnabled(true);
    m_appIdEdit->setPlaceholderText(tr("Required for official DandanPlay"));
    formLayout->addRow(m_appIdLabel, m_appIdEdit);

    m_appSecretLabel = new QLabel(tr("App Secret"), editorPanel);
    m_appSecretLabel->setObjectName("LibraryMetadataFieldLabel");
    m_appSecretEdit = new QLineEdit(editorPanel);
    m_appSecretEdit->setObjectName("ManageLibInput");
    m_appSecretEdit->setClearButtonEnabled(true);
    m_appSecretEdit->setEchoMode(QLineEdit::Password);
    m_appSecretEdit->setPlaceholderText(
        tr("Required for official DandanPlay"));
    formLayout->addRow(m_appSecretLabel, m_appSecretEdit);

    editorLayout->addLayout(formLayout);
    editorLayout->addStretch();
    bodyLayout->addWidget(editorPanel, 1);

    contentLayout()->addLayout(bodyLayout);
    contentLayout()->addSpacing(24);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName("dialog-btn-cancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    auto *saveButton = new QPushButton(tr("Save"), this);
    saveButton->setObjectName("dialog-btn-primary");
    saveButton->setCursor(Qt::PointingHandCursor);
    connect(saveButton, &QPushButton::clicked, this,
            [this]() { accept(); });
    buttonLayout->addWidget(saveButton);

    contentLayout()->addLayout(buttonLayout);

    connect(m_serverList, &QListWidget::currentRowChanged, this,
            [this](int row) {
              if (m_loading || row < 0 || row >= m_servers.size()) {
                return;
              }
              m_editingServerId = m_servers.at(row).id;
              if (m_servers.at(row).enabled) {
                  m_selectedServerId = m_servers.at(row).id;
              }
              loadCurrentServer();
              updateSelectionState();
              updateEditorState();
              updateActionState();
            });

    connect(m_nameEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { updateCurrentServerName(text); });
    connect(m_baseUrlEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { updateCurrentServerUrl(text); });
    connect(m_descriptionEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
              updateCurrentServerDescription(text);
            });
    connect(m_appIdEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { updateCurrentServerAppId(text); });
    connect(m_appSecretEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
              updateCurrentServerAppSecret(text);
            });

    connect(m_addButton, &QPushButton::clicked, this, [this]() {
      DanmakuServerDefinition server =
          DanmakuSettings::builtInOfficialDandanplayServer();
      server.id = createServerId();
      server.builtIn = false;
      server.contentScope.clear();
      server.description.clear();
      server.appId.clear();
      server.appSecret.clear();
      server.name = tr("Danmaku Server %1").arg(m_servers.size() + 1);
      server.enabled = true;
      m_servers.append(server);
      refreshServerList();
      selectServer(m_servers.size() - 1);
      m_nameEdit->setFocus();
      m_nameEdit->selectAll();
    });

    setServers({DanmakuSettings::builtInOfficialDandanplayServer()},
               QStringLiteral("default"));
}

void DanmakuServerDialog::setServers(QList<DanmakuServerDefinition> servers,
                                     QString selectedServerId)
{
    if (servers.isEmpty()) {
        servers.append(DanmakuSettings::builtInOfficialDandanplayServer());
    }

    for (DanmakuServerDefinition &server : servers) {
        server.id = server.id.trimmed();
        server.name = server.name.trimmed();
        server.provider = server.provider.trimmed();
        server.baseUrl = normalizeBaseUrl(server.baseUrl);
        server.description = server.description.trimmed();
        server.appId = server.appId.trimmed();
        server.appSecret = server.appSecret.trimmed();
        server.contentScope = server.contentScope.trimmed().toLower();

        if (server.id.isEmpty()) {
            server.id = createServerId();
        }
        if (server.builtIn) {
            const DanmakuServerDefinition builtIn =
                DanmakuSettings::builtInOfficialDandanplayServer();
            server.id = builtIn.id;
            server.provider = builtIn.provider;
            server.baseUrl = builtIn.baseUrl;
            server.appId = builtIn.appId;
            server.appSecret = builtIn.appSecret;
            server.description.clear();
            server.contentScope = builtIn.contentScope;
            if (server.name.isEmpty()) {
                server.name = builtIn.name;
            }
        }
        if (server.provider.isEmpty()) {
            server.provider = QStringLiteral("dandanplay");
        }
        if (server.name.isEmpty()) {
            server.name = fallbackServerName(server);
        }
    }

    m_servers = std::move(servers);
    m_selectedServerId = selectedServerId.trimmed();
    if (m_selectedServerId.trimmed().isEmpty() && !m_servers.isEmpty()) {
        const int activeIndex = preferredServerIndex(0);
        if (activeIndex >= 0) {
            m_selectedServerId = m_servers.at(activeIndex).id;
        }
    }
    m_editingServerId = m_selectedServerId;
    refreshServerList();
    selectServer(editingServerIndex());
}

QList<DanmakuServerDefinition> DanmakuServerDialog::servers() const
{
    return m_servers;
}

QString DanmakuServerDialog::selectedServerId() const
{
    if (enabledServerCount() <= 0) {
        return QString();
    }

    const int index = preferredServerIndex(currentServerIndex());
    if (index >= 0 && index < m_servers.size()) {
        return m_servers.at(index).id;
    }
    return m_selectedServerId.trimmed().isEmpty() && !m_servers.isEmpty()
               ? m_servers.first().id
               : m_selectedServerId;
}

void DanmakuServerDialog::accept()
{
    if (m_servers.isEmpty()) {
        ModernMessageBox::warning(
            this, tr("Danmaku Servers"),
            tr("At least one danmaku server is required."), tr("OK"));
        return;
    }

    for (const DanmakuServerDefinition &server : std::as_const(m_servers)) {
        if (server.name.trimmed().isEmpty()) {
            m_editingServerId = server.id;
            if (server.enabled) {
                m_selectedServerId = server.id;
            }
            refreshServerList();
            selectServer(editingServerIndex());
            m_nameEdit->setFocus();
            ModernMessageBox::warning(
                this, tr("Danmaku Servers"),
                tr("Every danmaku server must have a name."), tr("OK"));
            return;
        }
        if (server.baseUrl.trimmed().isEmpty()) {
            m_editingServerId = server.id;
            if (server.enabled) {
                m_selectedServerId = server.id;
            }
            refreshServerList();
            selectServer(editingServerIndex());
            m_baseUrlEdit->setFocus();
            ModernMessageBox::warning(
                this, tr("Danmaku Servers"),
                tr("Every danmaku server must have a server URL."), tr("OK"));
            return;
        }

        const QUrl url = QUrl::fromUserInput(server.baseUrl.trimmed());
        const QString scheme = url.scheme().trimmed().toLower();
        if (!url.isValid() || url.host().trimmed().isEmpty() ||
            (scheme != QStringLiteral("http") &&
             scheme != QStringLiteral("https"))) {
            m_editingServerId = server.id;
            if (server.enabled) {
                m_selectedServerId = server.id;
            }
            refreshServerList();
            selectServer(editingServerIndex());
            m_baseUrlEdit->setFocus();
            ModernMessageBox::warning(
                this, tr("Danmaku Servers"),
                tr("Server URL must be a valid http or https address."),
                tr("OK"));
            return;
        }
    }

    const QString activeServerId = selectedServerId();
    if (!activeServerId.trimmed().isEmpty()) {
        m_selectedServerId = activeServerId;
    }

    QDialog::accept();
}

bool DanmakuServerDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (m_serverList && watched == m_serverList->viewport() &&
        event && event->type() == QEvent::Resize) {
        syncAllServerListItemSizes();
    }

    return ModernDialogBase::eventFilter(watched, event);
}

void DanmakuServerDialog::showEvent(QShowEvent *event)
{
    ModernDialogBase::showEvent(event);
    syncAllServerListItemSizes();
}

int DanmakuServerDialog::currentServerIndex() const
{
    for (int index = 0; index < m_servers.size(); ++index) {
        if (m_servers.at(index).id == m_selectedServerId) {
            return index;
        }
    }
    return m_servers.isEmpty() ? -1 : 0;
}

int DanmakuServerDialog::editingServerIndex() const
{
    for (int index = 0; index < m_servers.size(); ++index) {
        if (m_servers.at(index).id == m_editingServerId) {
            return index;
        }
    }
    return m_servers.isEmpty() ? -1 : currentServerIndex();
}

int DanmakuServerDialog::enabledServerCount() const
{
    int count = 0;
    for (const DanmakuServerDefinition &server : m_servers) {
        if (server.enabled) {
            ++count;
        }
    }
    return count;
}

int DanmakuServerDialog::preferredServerIndex(int preferredIndex) const
{
    if (m_servers.isEmpty()) {
        return -1;
    }

    const int normalizedPreferred =
        qBound(0, preferredIndex < 0 ? 0 : preferredIndex, m_servers.size() - 1);

    if (enabledServerCount() > 0) {
        for (int offset = 0; offset < m_servers.size(); ++offset) {
            const int forwardIndex = normalizedPreferred + offset;
            if (forwardIndex < m_servers.size() &&
                m_servers.at(forwardIndex).enabled) {
                return forwardIndex;
            }

            const int backwardIndex = normalizedPreferred - offset;
            if (offset > 0 && backwardIndex >= 0 &&
                m_servers.at(backwardIndex).enabled) {
                return backwardIndex;
            }
        }
    }

    return normalizedPreferred;
}

void DanmakuServerDialog::removeServer(int index)
{
    if (index < 0 || index >= m_servers.size()) {
        return;
    }

    if (m_servers.at(index).builtIn) {
        return;
    }

    if (m_servers.size() <= 1) {
        ModernMessageBox::warning(
            this, tr("Danmaku Servers"),
            tr("At least one danmaku server is required."), tr("OK"));
        return;
    }

    const QString removedServerId = m_servers.at(index).id;
    const QString serverName = fallbackServerName(m_servers.at(index));
    if (!ModernMessageBox::question(
            this, tr("Remove Danmaku Server"),
            tr("Remove \"%1\" from the danmaku server list?").arg(serverName),
            tr("Remove"), tr("Cancel"), ModernMessageBox::Danger,
            ModernMessageBox::Warning)) {
        return;
    }

    const bool removingSelected = removedServerId == m_selectedServerId;
    const bool removingEditing = removedServerId == m_editingServerId;
    m_servers.removeAt(index);

    if (m_servers.isEmpty()) {
        refreshServerList();
        return;
    }

    if (removingSelected) {
        const int nextActiveIndex =
            preferredServerIndex(qBound(0, index, m_servers.size() - 1));
        if (nextActiveIndex >= 0 && nextActiveIndex < m_servers.size()) {
            m_selectedServerId = m_servers.at(nextActiveIndex).id;
        }
    }

    if (removingEditing) {
        const int nextEditingIndex =
            qBound(0, index, m_servers.size() - 1);
        m_editingServerId = m_servers.at(nextEditingIndex).id;
    }

    refreshServerList();
}

void DanmakuServerDialog::selectServer(int index)
{
    if (index < 0 || index >= m_servers.size()) {
        updateEditorState();
        updateActionState();
        return;
    }

    m_loading = true;
    {
        const QSignalBlocker blocker(m_serverList);
        m_serverList->setCurrentRow(index);
    }
    m_editingServerId = m_servers.at(index).id;
    if (m_servers.at(index).enabled) {
        m_selectedServerId = m_servers.at(index).id;
    }
    m_loading = false;

    loadCurrentServer();
    updateSelectionState();
    updateEditorState();
    updateActionState();
}

void DanmakuServerDialog::refreshServerList()
{
    const int activeIndex = preferredServerIndex(currentServerIndex());
    const int editorIndex = editingServerIndex();

    m_loading = true;
    m_serverList->blockSignals(true);
    m_serverList->clear();

    for (int index = 0; index < m_servers.size(); ++index) {
        const DanmakuServerDefinition &server = m_servers.at(index);
        auto *item = new QListWidgetItem();
        auto *widget = new DanmakuServerListItemWidget(m_serverList);
        widget->setServer(server);
        widget->setRemovable(m_servers.size() > 1 && !server.builtIn);
        item->setData(Qt::UserRole, server.id);
        m_serverList->addItem(item);
        m_serverList->setItemWidget(item, widget);

        connect(widget, &DanmakuServerListItemWidget::clicked, this,
                [this, item]() {
                  if (!item) {
                      return;
                  }

                  const int row = m_serverList->row(item);
                  if (row < 0 || row >= m_servers.size()) {
                      return;
                  }
                  selectServer(row);
                });
        connect(widget, &DanmakuServerListItemWidget::enabledChanged, this,
                [this, item](bool enabled) {
                  if (!item) {
                      return;
                  }

                  const int row = m_serverList->row(item);
                  if (row < 0 || row >= m_servers.size()) {
                      return;
                  }
                  updateServerEnabled(row, enabled);
                });
        connect(widget, &DanmakuServerListItemWidget::removeRequested, this,
                [this, item]() {
                  if (!item) {
                      return;
                  }

                  removeServer(m_serverList->row(item));
                });
    }

    if (!m_servers.isEmpty()) {
        const int row = qBound(
            0, editorIndex < 0 ? activeIndex : editorIndex, m_servers.size() - 1);
        m_serverList->setCurrentRow(row);
        m_editingServerId = m_servers.at(row).id;

        const int normalizedActiveIndex =
            activeIndex >= 0 ? activeIndex : preferredServerIndex(row);
        if (normalizedActiveIndex >= 0) {
            m_selectedServerId = m_servers.at(normalizedActiveIndex).id;
        }
    }

    m_serverList->blockSignals(false);
    m_loading = false;
    syncAllServerListItemSizes();
    loadCurrentServer();
    updateSelectionState();
    updateEditorState();
    updateActionState();
}

void DanmakuServerDialog::refreshServerListItem(int index)
{
    if (index < 0 || index >= m_servers.size()) {
        return;
    }

    auto *item = m_serverList->item(index);
    if (!item) {
        return;
    }

    auto *widget =
        qobject_cast<DanmakuServerListItemWidget *>(
            m_serverList->itemWidget(item));
    if (!widget) {
        return;
    }

    widget->setServer(m_servers.at(index));
    widget->setRemovable(m_servers.size() > 1 &&
                         !m_servers.at(index).builtIn);
    syncServerListItemSize(index);
    updateSelectionState();
}

void DanmakuServerDialog::syncServerListItemSize(int index)
{
    constexpr int kListItemRightInset = 8;

    if (!m_serverList || index < 0 || index >= m_serverList->count() ||
        index >= m_servers.size()) {
        return;
    }

    auto *item = m_serverList->item(index);
    if (!item) {
        return;
    }

    auto *widget = qobject_cast<DanmakuServerListItemWidget *>(
        m_serverList->itemWidget(item));
    if (!widget || !m_serverList->viewport()) {
        return;
    }

    const int viewportWidth = m_serverList->viewport()->width();
    const int targetWidth = viewportWidth - kListItemRightInset;
    if (targetWidth <= 0) {
        return;
    }

    widget->updateGeometry();

    int targetHeight = widget->hasHeightForWidth()
                           ? widget->heightForWidth(targetWidth)
                           : widget->sizeHint().height();
    if (targetHeight <= 0) {
        widget->adjustSize();
        targetHeight = widget->sizeHint().height();
    }

    targetHeight = qMax(targetHeight, widget->minimumSizeHint().height());
    item->setSizeHint(QSize(targetWidth, targetHeight));
    widget->resize(targetWidth, targetHeight);
}

void DanmakuServerDialog::syncAllServerListItemSizes()
{
    for (int index = 0; index < m_serverList->count(); ++index) {
        syncServerListItemSize(index);
    }
}

void DanmakuServerDialog::loadCurrentServer()
{
    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }

    m_loading = true;
    const DanmakuServerDefinition &server = m_servers.at(index);
    m_nameEdit->setText(server.name);
    m_baseUrlEdit->setText(server.baseUrl);
    m_descriptionEdit->setText(server.description);
    m_appIdEdit->setText(server.appId);
    m_appSecretEdit->setText(server.appSecret);
    m_loading = false;
}

void DanmakuServerDialog::updateSelectionState()
{
    const int currentRow = m_serverList->currentRow();
    const QString activeServerId = selectedServerId();
    for (int row = 0; row < m_serverList->count(); ++row) {
        auto *widget = qobject_cast<DanmakuServerListItemWidget *>(
            m_serverList->itemWidget(m_serverList->item(row)));
        if (widget) {
            widget->setSelected(row == currentRow);
            widget->setCurrentServer(m_servers.at(row).id == activeServerId);
        }
    }
}

void DanmakuServerDialog::updateEditorState()
{
    const int index = editingServerIndex();
    const bool hasCurrentServer = index >= 0 && index < m_servers.size();
    const bool isBuiltIn = hasCurrentServer && m_servers.at(index).builtIn;
    m_nameEdit->setEnabled(hasCurrentServer && !isBuiltIn);
    m_baseUrlEdit->setEnabled(hasCurrentServer && !isBuiltIn);
    m_descriptionLabel->setVisible(hasCurrentServer && !isBuiltIn);
    m_descriptionEdit->setVisible(hasCurrentServer && !isBuiltIn);
    m_descriptionEdit->setEnabled(hasCurrentServer && !isBuiltIn);
    m_appIdLabel->setVisible(hasCurrentServer && !isBuiltIn);
    m_appIdEdit->setVisible(hasCurrentServer && !isBuiltIn);
    m_appIdEdit->setEnabled(hasCurrentServer && !isBuiltIn);
    m_appSecretLabel->setVisible(hasCurrentServer && !isBuiltIn);
    m_appSecretEdit->setVisible(hasCurrentServer && !isBuiltIn);
    m_appSecretEdit->setEnabled(hasCurrentServer && !isBuiltIn);
}

void DanmakuServerDialog::updateServerEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_servers.size()) {
        return;
    }

    const QString previousActiveServerId = selectedServerId();
    m_servers[index].enabled = enabled;

    if (enabled && m_servers.at(index).id == m_editingServerId) {
        m_selectedServerId = m_servers.at(index).id;
    } else if (!enabled && m_servers.at(index).id == m_selectedServerId) {
        const int nextIndex = preferredServerIndex(index);
        if (nextIndex >= 0 && nextIndex != index) {
            m_selectedServerId = m_servers.at(nextIndex).id;
        }
    }

    refreshServerListItem(index);
    const QString activeServerId = selectedServerId();
    if (activeServerId != previousActiveServerId) {
        for (int row = 0; row < m_servers.size(); ++row) {
            if (m_servers.at(row).id == previousActiveServerId ||
                m_servers.at(row).id == activeServerId) {
                refreshServerListItem(row);
            }
        }
    }

    updateSelectionState();
    updateEditorState();
    updateActionState();
}

void DanmakuServerDialog::updateCurrentServerName(const QString &name)
{
    if (m_loading) {
        return;
    }

    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }
    if (m_servers.at(index).builtIn) {
        return;
    }

    m_servers[index].name = name.trimmed();
    refreshServerListItem(index);
}

void DanmakuServerDialog::updateCurrentServerUrl(const QString &baseUrl)
{
    if (m_loading) {
        return;
    }

    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }
    if (m_servers.at(index).builtIn) {
        return;
    }

    m_servers[index].baseUrl = baseUrl.trimmed();
    refreshServerListItem(index);
}

void DanmakuServerDialog::updateCurrentServerDescription(
    const QString &description)
{
    if (m_loading) {
        return;
    }

    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }
    if (m_servers.at(index).builtIn) {
        return;
    }

    m_servers[index].description = description.trimmed();
    refreshServerListItem(index);
}

void DanmakuServerDialog::updateCurrentServerAppId(const QString &appId)
{
    if (m_loading) {
        return;
    }

    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }
    if (m_servers.at(index).builtIn) {
        return;
    }

    m_servers[index].appId = appId.trimmed();
    refreshServerListItem(index);
}

void DanmakuServerDialog::updateCurrentServerAppSecret(
    const QString &appSecret)
{
    if (m_loading) {
        return;
    }

    const int index = editingServerIndex();
    if (index < 0 || index >= m_servers.size()) {
        return;
    }
    if (m_servers.at(index).builtIn) {
        return;
    }

    m_servers[index].appSecret = appSecret.trimmed();
    refreshServerListItem(index);
}

void DanmakuServerDialog::updateActionState()
{
    for (int row = 0; row < m_serverList->count(); ++row) {
        auto *widget = qobject_cast<DanmakuServerListItemWidget *>(
            m_serverList->itemWidget(m_serverList->item(row)));
        if (widget) {
            const bool removable =
                row < m_servers.size() && m_servers.size() > 1 &&
                !m_servers.at(row).builtIn;
            widget->setRemovable(removable);
        }
    }
}

QString DanmakuServerDialog::createServerId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString DanmakuServerDialog::normalizeBaseUrl(QString baseUrl)
{
    baseUrl = baseUrl.trimmed();
    while (baseUrl.endsWith('/') &&
           !baseUrl.endsWith(QStringLiteral("://"))) {
        baseUrl.chop(1);
    }
    return baseUrl;
}
