#include "hwdecprioritydialog.h"

#include <config/config_keys.h>
#include <config/configstore.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

HwdecPriorityDialog::HwdecPriorityDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Decoder Priority"));

    m_enableCheck = new QCheckBox(tr("Enable hardware decoding"), this);
    m_enableCheck->setChecked(
        ConfigStore::instance()->get<bool>(ConfigKeys::PlayerHwDecEnabled, true));

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);

    const QString saved = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerHwDecPriority, QString());
    const QStringList chain = saved.isEmpty() ? defaultChain()
                                              : saved.split(QLatin1Char(','),
                                                            Qt::SkipEmptyParts);
    for (const QString &id : chain) {
        m_list->addItem(id.trimmed());
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }

    m_upButton = new QPushButton(tr("Move Up"), this);
    m_downButton = new QPushButton(tr("Move Down"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    auto *sideLayout = new QVBoxLayout();
    sideLayout->addWidget(m_upButton);
    sideLayout->addWidget(m_downButton);
    sideLayout->addWidget(m_removeButton);
    sideLayout->addStretch();

    m_addCombo = new QComboBox(this);
    m_addButton = new QPushButton(tr("Add"), this);
    auto *addLayout = new QHBoxLayout();
    addLayout->addWidget(m_addCombo, 1);
    addLayout->addWidget(m_addButton);

    m_summaryLabel = new QLabel(
        tr("Decoders are tried top to bottom; playback falls back to CPU "
           "(software) automatically, so it never breaks."), this);
    m_summaryLabel->setWordWrap(true);

    m_defaultsButton = new QPushButton(tr("Restore Defaults"), this);
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *listLayout = new QHBoxLayout();
    listLayout->addWidget(m_list, 1);
    listLayout->addLayout(sideLayout);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_enableCheck);
    mainLayout->addLayout(listLayout);
    mainLayout->addLayout(addLayout);
    mainLayout->addWidget(m_summaryLabel);
    auto *footerLayout = new QHBoxLayout();
    footerLayout->addWidget(m_defaultsButton);
    footerLayout->addStretch();
    footerLayout->addWidget(buttonBox);
    mainLayout->addLayout(footerLayout);

    connect(m_upButton, &QPushButton::clicked, this, [this]() { moveCurrent(-1); });
    connect(m_downButton, &QPushButton::clicked, this, [this]() { moveCurrent(1); });
    connect(m_removeButton, &QPushButton::clicked, this, &HwdecPriorityDialog::removeCurrent);
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        const QString id = m_addCombo->currentData().toString();
        if (!id.isEmpty()) {
            addDecoder(id);
        }
    });
    connect(m_defaultsButton, &QPushButton::clicked, this,
            &HwdecPriorityDialog::restoreDefaults);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_list, &QListWidget::currentRowChanged, this,
            [this](int) { updateSummary(); });

    updateSummary();
}

QString HwdecPriorityDialog::priorityChain() const
{
    QStringList ids;
    for (int i = 0; i < m_list->count(); ++i) {
        ids << m_list->item(i)->text();
    }
    return ids.join(QLatin1Char(','));
}

bool HwdecPriorityDialog::hwdecEnabled() const
{
    return m_enableCheck->isChecked();
}

void HwdecPriorityDialog::addDecoder(const QString &id)
{
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->text() == id) {
            m_list->setCurrentRow(i);
            return;
        }
    }
    m_list->addItem(id);
    m_list->setCurrentRow(m_list->count() - 1);
}

void HwdecPriorityDialog::moveCurrent(int delta)
{
    const int row = m_list->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= m_list->count()) {
        return;
    }
    QListWidgetItem *item = m_list->takeItem(row);
    m_list->insertItem(target, item);
    m_list->setCurrentRow(target);
}

void HwdecPriorityDialog::removeCurrent()
{
    const int row = m_list->currentRow();
    if (row < 0) {
        return;
    }
    delete m_list->takeItem(row);
    if (m_list->count() > 0) {
        m_list->setCurrentRow(qMin(row, m_list->count() - 1));
    }
    updateSummary();
}

void HwdecPriorityDialog::restoreDefaults()
{
    m_list->clear();
    for (const QString &id : defaultChain()) {
        m_list->addItem(id);
    }
    m_enableCheck->setChecked(true);
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }
    updateSummary();
}

void HwdecPriorityDialog::updateSummary()
{
    m_addCombo->clear();
    for (const QString &id : availableDecoders()) {
        bool present = false;
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->text() == id) {
                present = true;
                break;
            }
        }
        if (!present) {
            m_addCombo->addItem(id, id);
        }
    }
    m_addButton->setEnabled(m_addCombo->count() > 0);
    m_upButton->setEnabled(m_list->currentRow() > 0);
    m_downButton->setEnabled(m_list->currentRow() >= 0 &&
                             m_list->currentRow() < m_list->count() - 1);
    m_removeButton->setEnabled(m_list->currentRow() >= 0);
}

QStringList HwdecPriorityDialog::defaultChain()
{
    return {QStringLiteral("auto"), QStringLiteral("auto-safe")};
}

QStringList HwdecPriorityDialog::availableDecoders()
{
#ifdef Q_OS_WIN
    return {QStringLiteral("d3d11va"), QStringLiteral("dxva2"),
            QStringLiteral("cuda"),   QStringLiteral("nvdec"),
            QStringLiteral("d3d11va-copy"), QStringLiteral("dxva2-copy"),
            QStringLiteral("cuda-copy"),    QStringLiteral("auto"),
            QStringLiteral("auto-safe")};
#elif defined(Q_OS_LINUX)
    return {QStringLiteral("vaapi"),  QStringLiteral("nvdec"),
            QStringLiteral("vdpau"),  QStringLiteral("cuda"),
            QStringLiteral("vaapi-copy"), QStringLiteral("nvdec-copy"),
            QStringLiteral("auto"),   QStringLiteral("auto-safe")};
#else
    return {QStringLiteral("videotoolbox"), QStringLiteral("videotoolbox-copy"),
            QStringLiteral("auto"),         QStringLiteral("auto-safe")};
#endif
}
