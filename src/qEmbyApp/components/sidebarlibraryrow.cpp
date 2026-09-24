#include "sidebarlibraryrow.h"
#include "elidedlabel.h"
#include "../managers/thememanager.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include <QDebug>
#include <QEnterEvent>
#include <QSizePolicy>
#include <QHBoxLayout>
#include <QPushButton>
#include <QStringList>

SidebarLibraryRow::SidebarLibraryRow(QString serverId, QString libraryId,
                                     const QString &title, QWidget *parent)
    : QWidget(parent), m_serverId(serverId), m_libraryId(libraryId)
{
    auto *layout = new QHBoxLayout(this);
    
    layout->setContentsMargins(4, 0, 2, 0);
    layout->setSpacing(2);
    auto *label = new ElidedLabel(this);
    label->setObjectName("sidebar-library-name");
    label->setFullText(title);
    label->setMinimumWidth(0);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(label, 1);
    m_visibilityButton = new QPushButton(this);
    m_visibilityButton->setObjectName("sidebar-library-visibility");
    m_visibilityButton->setFixedSize(20, 20);
    m_visibilityButton->setIconSize(QSize(14, 14));
    m_visibilityButton->setCursor(Qt::PointingHandCursor);
    m_visibilityButton->setFocusPolicy(Qt::NoFocus);
    auto buttonPolicy = m_visibilityButton->sizePolicy();
    buttonPolicy.setRetainSizeWhenHidden(true);
    m_visibilityButton->setSizePolicy(buttonPolicy);
    m_visibilityButton->hide();
    layout->addWidget(m_visibilityButton, 0, Qt::AlignVCenter);
    connect(m_visibilityButton, &QPushButton::clicked, this, [this]() {
        const QString key = ConfigKeys::forServer(m_serverId, ConfigKeys::HiddenHomeLibraries);
        auto *store = ConfigStore::instance();
        QStringList hidden = store->get<QStringList>(key);
        const bool visible = hidden.contains(m_libraryId);
        hidden.removeAll(m_libraryId);
        if (!visible) hidden.append(m_libraryId);
        qDebug() << "[SidebarLibraryRow] Home visibility changed"
                 << "| libraryId=" << m_libraryId << "| visible=" << visible;
        store->set(key, hidden);
    });
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
        if (key == ConfigKeys::forServer(m_serverId, ConfigKeys::HiddenHomeLibraries))
            refreshVisibility();
    });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this]() { refreshVisibility(); });
    refreshVisibility();
}

void SidebarLibraryRow::refreshVisibility()
{
    const bool hidden = ConfigStore::instance()->get<QStringList>(
        ConfigKeys::forServer(m_serverId, ConfigKeys::HiddenHomeLibraries)).contains(m_libraryId);
    m_visibilityButton->setIcon(ThemeManager::getAdaptiveIcon(
        hidden ? ":/svg/light/eye-off.svg" : ":/svg/light/eye.svg"));
    const QString action = hidden ? tr("Show on home page") : tr("Hide from home page");
    m_visibilityButton->setToolTip(action);
    m_visibilityButton->setAccessibleName(action);
}

void SidebarLibraryRow::enterEvent(QEnterEvent *event)
{
    QWidget::enterEvent(event);
    m_visibilityButton->show();
}

void SidebarLibraryRow::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_visibilityButton->hide();
}
