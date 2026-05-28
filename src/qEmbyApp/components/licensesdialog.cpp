#include "licensesdialog.h"

#include <QFile>
#include <QFrame>
#include <QScrollBar>
#include <QStyle>
#include <QTextBrowser>
#include <QTextStream>
#include <QVBoxLayout>

LicensesDialog::LicensesDialog(QWidget *parent)
    : ModernDialogBase(parent) {
    setObjectName(QStringLiteral("LicensesDialog"));
    setTitle(tr("Open Source Licenses"));
    setModal(true);
    setMinimumSize(480, 400);
    resize(560, 520);

    auto *content = contentLayout();
    
    
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);

    auto *browser = new QTextBrowser(this);
    browser->setObjectName(QStringLiteral("AboutLicenseBrowser"));
    browser->setOpenExternalLinks(true);
    browser->setReadOnly(true);
    browser->setFrameShape(QFrame::NoFrame);
    browser->document()->setDocumentMargin(20);

    QScrollBar *vBar = browser->verticalScrollBar();
    vBar->setObjectName(QStringLiteral("AboutLicenseScrollBar"));
    
    
    vBar->style()->unpolish(vBar);
    vBar->style()->polish(vBar);

    QFile licenseFile(QStringLiteral(":/html/licenses.html"));
    if (licenseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&licenseFile);
        in.setEncoding(QStringConverter::Utf8);
        browser->setHtml(in.readAll());
        licenseFile.close();
    }

    content->addWidget(browser);
}
