#include "updatedialog.h"

#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

QLabel *createSummaryLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("UpdateDialogSummaryLabel"));
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

QLabel *createSummaryValue(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("UpdateDialogSummaryValue"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

} 

UpdateDialog::UpdateDialog(const UpdateInfo &info,
                           Mode mode,
                           QWidget *parent)
    : ModernDialogBase(parent)
{
    setTitle(tr("qEmby Update"));
    setMinimumSize(540, 430);
    resize(640, 600);

    auto *layout = contentLayout();
    layout->setSpacing(0);

    auto *headline = new QLabel(tr("A new version of qEmby is available."), this);
    headline->setObjectName(QStringLiteral("UpdateDialogHeadline"));
    headline->setWordWrap(true);
    layout->addWidget(headline);
    layout->addSpacing(10);

    auto *summary = new QFrame(this);
    summary->setObjectName(QStringLiteral("UpdateDialogSummary"));
    auto *summaryLayout = new QGridLayout(summary);
    summaryLayout->setContentsMargins(14, 12, 14, 12);
    summaryLayout->setHorizontalSpacing(12);
    summaryLayout->setVerticalSpacing(6);
    summaryLayout->setColumnStretch(1, 1);

    const QString releaseName = info.name.isEmpty() ? info.version : info.name;
    const QString published = info.publishedAt.isValid()
                                  ? info.publishedAt.toLocalTime().date().toString(
                                        Qt::ISODate)
                                  : tr("Unknown");
    const QString currentVersion = QCoreApplication::applicationVersion();

    summaryLayout->addWidget(createSummaryLabel(tr("Current version"), summary),
                             0, 0);
    summaryLayout->addWidget(createSummaryValue(currentVersion, summary), 0, 1);
    summaryLayout->addWidget(createSummaryLabel(tr("New version"), summary), 1,
                             0);
    summaryLayout->addWidget(createSummaryValue(info.version, summary), 1, 1);
    summaryLayout->addWidget(createSummaryLabel(tr("Release"), summary), 2, 0);
    summaryLayout->addWidget(createSummaryValue(releaseName, summary), 2, 1);
    summaryLayout->addWidget(createSummaryLabel(tr("Published"), summary), 3, 0);
    summaryLayout->addWidget(createSummaryValue(published, summary), 3, 1);
    layout->addWidget(summary);
    layout->addSpacing(14);

    auto *notesTitle = new QLabel(tr("Release notes"), this);
    notesTitle->setObjectName(QStringLiteral("UpdateDialogSectionTitle"));
    layout->addWidget(notesTitle);
    layout->addSpacing(8);

    auto *notesBrowser = new QTextBrowser(this);
    notesBrowser->setObjectName(QStringLiteral("UpdateDialogReleaseNotes"));
    notesBrowser->setOpenExternalLinks(true);
    notesBrowser->setReadOnly(true);
    notesBrowser->document()->setDocumentMargin(14);
    notesBrowser->setMarkdown(info.releaseNotes.trimmed().isEmpty()
                                  ? tr("No release notes were provided.")
                                  : info.releaseNotes);
    notesBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    notesBrowser->setMinimumHeight(180);
    layout->addWidget(notesBrowser, 1);
    layout->addSpacing(12);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    if (mode == Mode::Automatic) {
        auto *laterButton = new QPushButton(tr("Remind Me Later"), this);
        laterButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
        laterButton->setCursor(Qt::PointingHandCursor);
        laterButton->setToolTip(
            tr("Hide this reminder until the next application startup"));
        connect(laterButton, &QPushButton::clicked, this, [this]() {
            m_decision = Decision::RemindLater;
            reject();
        });
        buttonLayout->addWidget(laterButton);

        auto *ignoreVersionButton =
            new QPushButton(tr("Ignore This Version"), this);
        ignoreVersionButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
        ignoreVersionButton->setCursor(Qt::PointingHandCursor);
        ignoreVersionButton->setToolTip(
            tr("Do not remind me again for this version"));
        connect(ignoreVersionButton, &QPushButton::clicked, this, [this]() {
            m_decision = Decision::IgnoreVersion;
            reject();
        });
        buttonLayout->addWidget(ignoreVersionButton);
    } else {
        auto *cancelButton = new QPushButton(tr("Cancel"), this);
        cancelButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
        cancelButton->setCursor(Qt::PointingHandCursor);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        buttonLayout->addWidget(cancelButton);
    }

    auto *updateButton = new QPushButton(tr("Update Now"), this);
    updateButton->setObjectName(QStringLiteral("dialog-btn-primary"));
    updateButton->setCursor(Qt::PointingHandCursor);
    updateButton->setDefault(true);
    connect(updateButton, &QPushButton::clicked, this, [this]() {
        m_decision = Decision::Update;
        accept();
    });
    buttonLayout->addWidget(updateButton);
    layout->addLayout(buttonLayout);
}

UpdateDialog::Decision UpdateDialog::decision() const
{
    return m_decision;
}
