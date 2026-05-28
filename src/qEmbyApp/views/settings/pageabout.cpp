#include "pageabout.h"
#include "../../components/flowlayout.h"
#include "../../components/licensesdialog.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>

namespace {

struct ThanksEntry {
    QString name;
    QString contribution;
    QString initials;
};

} 

PageAbout::PageAbout(QEmbyCore* core, QWidget *parent)
    : SettingsPageBase(core, tr("About"), parent)
{
    m_mainLayout->addStretch(1);

    m_logoLabel = new QLabel(this);
    QPixmap logoPixmap(":/svg/qemby_logo.svg");
    if (!logoPixmap.isNull()) {
        m_logoLabel->setPixmap(logoPixmap.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setObjectName("AboutLogoLabel");
    m_mainLayout->addWidget(m_logoLabel);

    m_appNameLabel = new QLabel(tr("qEmby"), this);
    m_appNameLabel->setAlignment(Qt::AlignCenter);
    m_appNameLabel->setObjectName("AboutAppNameLabel");
    m_mainLayout->addWidget(m_appNameLabel);

    QString versionStr = QCoreApplication::applicationVersion();
    if (versionStr.isEmpty()) {
        versionStr = "0.0.1";
    }
    m_versionLabel = new QLabel(tr("Version %1").arg(versionStr), this);
    m_versionLabel->setAlignment(Qt::AlignCenter);
    m_versionLabel->setObjectName("AboutVersionLabel");
    m_mainLayout->addWidget(m_versionLabel);

    m_authorLabel = new QLabel(tr("© 2026 AlanHJ. All rights reserved."), this);
    m_authorLabel->setAlignment(Qt::AlignCenter);
    m_authorLabel->setObjectName("AboutAuthorLabel");
    m_mainLayout->addWidget(m_authorLabel);

    m_linkLabel = new QLabel(this);
    m_linkLabel->setText(
        QString("<a href=\"https://github.com/AlanHJ/qEmby\">%1</a> &nbsp;|&nbsp; "
                "<a href=\"https://github.com/AlanHJ/qEmby/issues\">%2</a>")
            .arg(tr("GitHub Repository"))
            .arg(tr("Report an Issue"))
        );
    m_linkLabel->setOpenExternalLinks(true);
    m_linkLabel->setAlignment(Qt::AlignCenter);
    m_linkLabel->setObjectName("AboutLinkLabel");
    m_mainLayout->addWidget(m_linkLabel);

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setObjectName("AboutSeparatorLine");
    m_mainLayout->addWidget(line);

    m_licenseLabel = new QLabel(
        tr("This project is open-source software built with Qt 6 and modern C++.\n"
           "Special thanks to the Emby and Jellyfin communities for their fantastic APIs."), this);
    m_licenseLabel->setAlignment(Qt::AlignCenter);
    m_licenseLabel->setWordWrap(true);
    m_licenseLabel->setObjectName("AboutIntroLabel");
    m_mainLayout->addWidget(m_licenseLabel);

    m_mainLayout->addWidget(createAcknowledgementsPanel(), 0, Qt::AlignHCenter);

    m_mainLayout->addSpacing(25);

    m_licenseBtn = new QPushButton(tr("Open Source Licenses"), this);
    m_licenseBtn->setObjectName("AboutLicenseBtn");
    m_licenseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_licenseBtn, &QPushButton::clicked, this, &PageAbout::showLicensesDialog);

    m_mainLayout->addWidget(m_licenseBtn, 0, Qt::AlignHCenter);

    m_mainLayout->addStretch(2);
}

QWidget *PageAbout::createAcknowledgementsPanel()
{
    auto *thanksPanel = new QFrame(this);
    thanksPanel->setObjectName("AboutThanksPanel");

    auto *thanksLayout = new QVBoxLayout(thanksPanel);
    thanksLayout->setContentsMargins(0, 0, 0, 0);
    thanksLayout->setSpacing(10);

    auto *thanksTitle = new QLabel(tr("Acknowledgements"), thanksPanel);
    thanksTitle->setAlignment(Qt::AlignCenter);
    thanksTitle->setObjectName("AboutThanksTitle");
    thanksLayout->addWidget(thanksTitle);

    auto *thanksSubtitle = new QLabel(tr("People who help qEmby reach more users."), thanksPanel);
    thanksSubtitle->setAlignment(Qt::AlignCenter);
    thanksSubtitle->setObjectName("AboutThanksSubtitle");
    thanksLayout->addWidget(thanksSubtitle);

    auto *cardsWidget = new QWidget(thanksPanel);
    cardsWidget->setObjectName("AboutThanksCards");
    auto *cardsLayout = new FlowLayout(cardsWidget, 0, 6, 6);

    const QVector<ThanksEntry> thanksEntries = {
        {QStringLiteral("Arkoth79"), tr("French translation"), QStringLiteral("A")},
    };
    for (const auto &entry : thanksEntries) {
        auto *card = new QFrame(cardsWidget);
        card->setObjectName("AboutThanksCard");
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(5, 3, 8, 3);
        cardLayout->setSpacing(6);

        auto *avatarLabel = new QLabel(entry.initials, card);
        avatarLabel->setFixedSize(22, 22);
        avatarLabel->setAlignment(Qt::AlignCenter);
        avatarLabel->setObjectName("AboutThanksAvatar");
        cardLayout->addWidget(avatarLabel, 0, Qt::AlignVCenter);

        auto *textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(1);

        auto *nameLabel = new QLabel(entry.name, card);
        nameLabel->setObjectName("AboutThanksName");
        textLayout->addWidget(nameLabel);

        auto *roleLabel = new QLabel(entry.contribution, card);
        roleLabel->setObjectName("AboutThanksRole");
        textLayout->addWidget(roleLabel);

        cardLayout->addLayout(textLayout);
        cardsLayout->addWidget(card);
    }

    thanksLayout->addWidget(cardsWidget, 0, Qt::AlignCenter);
    return thanksPanel;
}

void PageAbout::showLicensesDialog()
{
    LicensesDialog dialog(this->window());
    dialog.exec();
}
