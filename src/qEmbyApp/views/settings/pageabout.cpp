#include "pageabout.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QScrollBar>
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QTextBrowser>
#include <QStyle>
#include <QMouseEvent>
#include <QPropertyAnimation>




class DialogCloseFilter : public QObject {
public:
    
    explicit DialogCloseFilter(QDialog* dialog)
        : QObject(dialog), m_dialog(dialog), m_isClosing(false) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (m_isClosing) {
            return true;
        }

        
        if (event->type() == QEvent::MouseButtonPress && watched == m_dialog) {
            fadeOutAndClose();
            return true;
        }
        
        
        
        
        

        return QObject::eventFilter(watched, event);
    }

private:
    void fadeOutAndClose() {
        m_isClosing = true;

        QPropertyAnimation* fadeOutAnim = new QPropertyAnimation(m_dialog, "windowOpacity", this);
        fadeOutAnim->setDuration(200);
        fadeOutAnim->setStartValue(1.0);
        fadeOutAnim->setEndValue(0.0);
        fadeOutAnim->setEasingCurve(QEasingCurve::OutQuad);

        connect(fadeOutAnim, &QPropertyAnimation::finished, m_dialog, &QDialog::accept);
        fadeOutAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    QDialog* m_dialog;
    bool m_isClosing;
};


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

    m_authorLabel = new QLabel(tr("© 2026 qEmby Contributors. All rights reserved."), this);
    m_authorLabel->setAlignment(Qt::AlignCenter);
    m_authorLabel->setObjectName("AboutAuthorLabel");
    m_mainLayout->addWidget(m_authorLabel);

    m_linkLabel = new QLabel(this);
    m_linkLabel->setText(
        QString("<a href=\"https://github.com/your-repo/qEmby\">%1</a> &nbsp;|&nbsp; "
                "<a href=\"https://github.com/your-repo/qEmby/issues\">%2</a>")
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

    m_mainLayout->addSpacing(25);

    m_licenseBtn = new QPushButton(tr("Open Source Licenses"), this);
    m_licenseBtn->setObjectName("AboutLicenseBtn");
    m_licenseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_licenseBtn, &QPushButton::clicked, this, &PageAbout::showLicensesDialog);

    m_mainLayout->addWidget(m_licenseBtn, 0, Qt::AlignHCenter);

    m_mainLayout->addStretch(2);
}


void PageAbout::showLicensesDialog()
{
    QDialog dialog(this->window());
    dialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dialog.setAttribute(Qt::WA_TranslucentBackground);
    dialog.setFixedSize(600, 520);

    dialog.setWindowOpacity(0.0);

    QFrame *container = new QFrame(&dialog);
    container->setObjectName("LicenseDialogContainer");
    container->setFixedSize(540, 460);

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(30);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 10);
    container->setGraphicsEffect(shadow);

    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(0, 0, 0, 0);
    dialogLayout->addWidget(container, 0, Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    QLabel *titleLabel = new QLabel(tr("Open Source Licenses"), container);
    titleLabel->setObjectName("LicenseDialogTitle");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QTextBrowser *browser = new QTextBrowser(container);
    browser->setObjectName("AboutLicenseBrowser");
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->document()->setDocumentMargin(14);

    QScrollBar *vBar = browser->verticalScrollBar();
    vBar->setObjectName("AboutLicenseScrollBar");

    browser->style()->unpolish(browser);
    browser->style()->polish(browser);
    vBar->style()->unpolish(vBar);
    vBar->style()->polish(vBar);
    browser->ensurePolished();

    QFile licenseFile(":/html/licenses.html");
    if (licenseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&licenseFile);
        in.setEncoding(QStringConverter::Utf8);
        browser->setHtml(in.readAll());
        licenseFile.close();
    }

    layout->addWidget(browser);

    
    
    
    DialogCloseFilter* closeFilter = new DialogCloseFilter(&dialog);
    dialog.installEventFilter(closeFilter);
    

    dialog.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, dialog.size(), this->window()->geometry()));

    QPropertyAnimation* fadeInAnim = new QPropertyAnimation(&dialog, "windowOpacity");
    fadeInAnim->setDuration(250);
    fadeInAnim->setStartValue(0.0);
    fadeInAnim->setEndValue(1.0);
    fadeInAnim->setEasingCurve(QEasingCurve::OutCubic);
    fadeInAnim->start();

    dialog.exec();
}
