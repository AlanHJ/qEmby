#include "taskeditdialog.h"

#include "moderncombobox.h"
#include "moderntoast.h"
#include "slidingstackedwidget.h"
#include "../utils/layoututils.h"
#include "../utils/taskutils.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>

#include <QComboBox>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QStringList>
#include <QVBoxLayout>

namespace {

constexpr qint64 TicksPerMinute = 600000000LL;
constexpr qint64 TicksPerHour = 36000000000LL;
constexpr int OverviewPageIndex = 0;
constexpr int TriggerComboWidth = 200;
QWidget *createScrollablePage(QVBoxLayout *&innerLayout, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("LibCreateScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *container = new QWidget(scroll);
    container->setAttribute(Qt::WA_StyledBackground, true);

    innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 0, 12, 0);
    innerLayout->setSpacing(8);
    innerLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    return scroll;
}

QWidget *createPanelCard(QWidget *parent, QVBoxLayout *&bodyLayout) {
    auto *card = new QWidget(parent);
    card->setObjectName("LibAdvancedPanel");
    card->setAttribute(Qt::WA_StyledBackground, true);

    bodyLayout = new QVBoxLayout(card);
    bodyLayout->setContentsMargins(12, 10, 12, 10);
    bodyLayout->setSpacing(10);
    return card;
}

QPushButton *createNavButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName("SettingsCardButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("navActive", false);
    button->setFixedHeight(30);
    return button;
}

QPushButton *createInlineButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName("SettingsCardButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(28);
    return button;
}

QWidget *createFormRow(const QString &labelText, QWidget *field, QWidget *parent,
                       int labelWidth = 118) {
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *label = new QLabel(labelText, row);
    label->setObjectName("ManageInfoKey");
    label->setMinimumWidth(labelWidth);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(label);
    layout->addStretch(1);
    layout->addWidget(field, 0, Qt::AlignRight | Qt::AlignVCenter);
    return row;
}

} 

TaskEditDialog::TaskEditDialog(QEmbyCore *core, const QString &taskId,
                               const QString &taskName, QWidget *parent)
    : ModernDialogBase(parent)
    , m_core(core)
    , m_taskId(taskId) {
    setTitle(taskName);
    setMinimumWidth(540);
    setMaximumHeight(720);
    resize(580, 640);

    contentLayout()->setContentsMargins(20, 10, 0, 20);
    contentLayout()->setSpacing(12);

    auto *navRow = new QHBoxLayout();
    navRow->setContentsMargins(0, 0, 12, 0);
    navRow->setSpacing(8);

    m_pageStack = new SlidingStackedWidget(this);
    m_pageStack->setSpeed(260);
    m_pageStack->setEasingCurve(QEasingCurve::OutCubic);

    setupOverviewPage();
    setupTriggersPage();

    const QStringList navTitles = {
        tr("Overview"),
        tr("Triggers")
    };
    for (int index = 0; index < navTitles.size(); ++index) {
        auto *button = createNavButton(navTitles[index], this);
        connect(button, &QPushButton::clicked, this,
                [this, index]() { onNavigationChanged(index); });
        m_navButtons.append(button);
        navRow->addWidget(button);
    }
    navRow->addStretch(1);

    contentLayout()->addLayout(navRow);
    contentLayout()->addWidget(m_pageStack, 1);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 12, 0);
    bottomRow->setSpacing(8);
    bottomRow->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setObjectName("SettingsCardButton");
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnCancel);

    m_btnSave = new QPushButton(tr("Save"), this);
    m_btnSave->setObjectName("SettingsCardButton");
    m_btnSave->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnSave);

    contentLayout()->addLayout(bottomRow);

    connect(m_comboTriggerType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                m_triggerOptionsStack->setCurrentIndex(index);
                updateTriggerOptionsHeight();
            });
    connect(m_btnAddTrigger, &QPushButton::clicked,
            this, &TaskEditDialog::showAddTriggerForm);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnSave, &QPushButton::clicked, this,
            [this]() { m_pendingTask = saveTriggers(); });

    m_currentPageIndex = OverviewPageIndex;
    m_pageStack->setCurrentIndex(OverviewPageIndex);
    for (int index = 1; index < m_pageStack->count(); ++index) {
        if (QWidget *page = m_pageStack->widget(index)) {
            page->hide();
        }
    }

    updateNavigationState();
    updateDialogState();
    updateTriggerOptionsHeight();
    m_pendingTask = loadTaskData();
}

void TaskEditDialog::setupOverviewPage() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *summaryBody = nullptr;
    auto *summaryCard = createPanelCard(page, summaryBody);
    QWidget *summaryContent = summaryBody->parentWidget();

    auto *summaryTitle = new QLabel(tr("Task Summary"), summaryContent);
    summaryTitle->setObjectName("ManageCardTitle");
    summaryBody->addWidget(summaryTitle);

    m_titleLabel = new QLabel(summaryContent);
    m_titleLabel->setObjectName("ManageSessionUser");
    m_titleLabel->setWordWrap(true);
    summaryBody->addWidget(m_titleLabel);

    m_descLabel = new QLabel(summaryContent);
    m_descLabel->setObjectName("ManageInfoKey");
    m_descLabel->setWordWrap(true);
    summaryBody->addWidget(m_descLabel);

    summaryBody->addWidget(LayoutUtils::createSeparator(summaryContent));
    summaryBody->addLayout(LayoutUtils::createInfoRow(tr("State"), m_stateValueLabel));
    summaryBody->addLayout(
        LayoutUtils::createInfoRow(tr("Category"), m_categoryValueLabel));
    summaryBody->addLayout(
        LayoutUtils::createInfoRow(tr("Last Result"), m_lastResultValueLabel));
    summaryBody->addLayout(LayoutUtils::createInfoRow(tr("Last Finished"),
                                                      m_lastFinishedValueLabel));
    summaryBody->addLayout(
        LayoutUtils::createInfoRow(tr("Schedule"), m_scheduleValueLabel));
    summaryBody->addLayout(
        LayoutUtils::createInfoRow(tr("Visibility"), m_visibilityValueLabel));
    layout->addWidget(summaryCard);

    QVBoxLayout *noticeBody = nullptr;
    auto *noticeCard = createPanelCard(page, noticeBody);
    QWidget *noticeContent = noticeBody->parentWidget();

    auto *noticeTitle = new QLabel(tr("Scheduling Notes"), noticeContent);
    noticeTitle->setObjectName("ManageCardTitle");
    noticeBody->addWidget(noticeTitle);

    m_overviewNoticeLabel = new QLabel(noticeContent);
    m_overviewNoticeLabel->setObjectName("ManageInfoKey");
    m_overviewNoticeLabel->setWordWrap(true);
    noticeBody->addWidget(m_overviewNoticeLabel);
    layout->addWidget(noticeCard);

    layout->addStretch(1);
    m_pageStack->addWidget(page);
}

void TaskEditDialog::setupTriggersPage() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *summaryBody = nullptr;
    auto *summaryCard = createPanelCard(page, summaryBody);
    QWidget *summaryContent = summaryBody->parentWidget();

    auto *summaryTitle = new QLabel(tr("Trigger Rules"), summaryContent);
    summaryTitle->setObjectName("ManageCardTitle");
    summaryBody->addWidget(summaryTitle);

    m_triggerSummaryLabel =
        new QLabel(tr("Loading task details..."), summaryContent);
    m_triggerSummaryLabel->setObjectName("ManageInfoValue");
    m_triggerSummaryLabel->setWordWrap(true);
    summaryBody->addWidget(m_triggerSummaryLabel);

    m_triggerHelperLabel = new QLabel(summaryContent);
    m_triggerHelperLabel->setObjectName("ManageInfoKey");
    m_triggerHelperLabel->setWordWrap(true);
    summaryBody->addWidget(m_triggerHelperLabel);
    layout->addWidget(summaryCard);

    QVBoxLayout *listBody = nullptr;
    auto *listCard = createPanelCard(page, listBody);
    QWidget *listContent = listBody->parentWidget();

    auto *listTitle = new QLabel(tr("Configured Triggers"), listContent);
    listTitle->setObjectName("ManageCardTitle");
    listBody->addWidget(listTitle);

    m_triggerListLayout = new QVBoxLayout();
    m_triggerListLayout->setContentsMargins(0, 0, 0, 0);
    m_triggerListLayout->setSpacing(8);
    listBody->addLayout(m_triggerListLayout);
    layout->addWidget(listCard);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    m_btnAddTrigger = createInlineButton(tr("Add Trigger"), page);
    actionRow->addWidget(m_btnAddTrigger);
    actionRow->addStretch(1);
    layout->addLayout(actionRow);

    QVBoxLayout *formBody = nullptr;
    m_addFormContainer = createPanelCard(page, formBody);
    QWidget *formContent = formBody->parentWidget();

    auto *formTitle = new QLabel(tr("Add Trigger"), formContent);
    formTitle->setObjectName("ManageCardTitle");
    formBody->addWidget(formTitle);

    m_comboTriggerType = LayoutUtils::createStyledCombo(formContent);
    m_comboTriggerType->addItem(tr("Daily"), QStringLiteral("DailyTrigger"));
    m_comboTriggerType->addItem(tr("Interval"), QStringLiteral("IntervalTrigger"));
    m_comboTriggerType->addItem(tr("Weekly"), QStringLiteral("WeeklyTrigger"));
    m_comboTriggerType->addItem(tr("On Startup"), QStringLiteral("StartupTrigger"));
    m_comboTriggerType->setFixedWidth(TriggerComboWidth);
    formBody->addWidget(
        createFormRow(tr("Trigger Type"), m_comboTriggerType, formContent));

    m_triggerOptionsStack = new QStackedWidget(formContent);
    m_triggerOptionsStack->setSizePolicy(QSizePolicy::Expanding,
                                         QSizePolicy::Fixed);

    auto *dailyPage = new QWidget();
    auto *dailyLayout = new QVBoxLayout(dailyPage);
    dailyLayout->setContentsMargins(0, 8, 0, 0);
    dailyLayout->setSpacing(8);
    m_comboDailyTime = LayoutUtils::createStyledCombo(dailyPage);
    m_comboDailyTime->setFixedWidth(TriggerComboWidth);
    for (int hour = 0; hour < 24; ++hour) {
        for (int minute = 0; minute < 60; minute += 30) {
            const QString timeLabel = QStringLiteral("%1:%2")
                                          .arg(hour, 2, 10, QChar('0'))
                                          .arg(minute, 2, 10, QChar('0'));
            m_comboDailyTime->addItem(
                timeLabel, (hour * 60 + minute) * TicksPerMinute);
        }
    }
    dailyLayout->addWidget(createFormRow(tr("Time"), m_comboDailyTime, dailyPage));
    m_triggerOptionsStack->addWidget(dailyPage);

    auto *intervalPage = new QWidget();
    auto *intervalLayout = new QVBoxLayout(intervalPage);
    intervalLayout->setContentsMargins(0, 8, 0, 0);
    intervalLayout->setSpacing(8);
    m_comboInterval = LayoutUtils::createStyledCombo(intervalPage);
    m_comboInterval->setFixedWidth(TriggerComboWidth);
    const QList<QPair<QString, qint64>> intervals = {
        {tr("15 minutes"), 15 * TicksPerMinute},
        {tr("30 minutes"), 30 * TicksPerMinute},
        {tr("45 minutes"), 45 * TicksPerMinute},
        {tr("1 hour"), TicksPerHour},
        {tr("2 hours"), 2 * TicksPerHour},
        {tr("3 hours"), 3 * TicksPerHour},
        {tr("4 hours"), 4 * TicksPerHour},
        {tr("6 hours"), 6 * TicksPerHour},
        {tr("8 hours"), 8 * TicksPerHour},
        {tr("12 hours"), 12 * TicksPerHour},
        {tr("24 hours"), 24 * TicksPerHour}
    };
    for (const auto &option : intervals) {
        m_comboInterval->addItem(option.first, option.second);
    }
    intervalLayout->addWidget(
        createFormRow(tr("Every"), m_comboInterval, intervalPage));
    m_triggerOptionsStack->addWidget(intervalPage);

    auto *weeklyPage = new QWidget();
    auto *weeklyLayout = new QVBoxLayout(weeklyPage);
    weeklyLayout->setContentsMargins(0, 8, 0, 0);
    weeklyLayout->setSpacing(8);
    m_comboWeekDay = LayoutUtils::createStyledCombo(weeklyPage);
    m_comboWeekDay->setFixedWidth(TriggerComboWidth);
    m_comboWeekDay->addItem(tr("Sunday"), QStringLiteral("Sunday"));
    m_comboWeekDay->addItem(tr("Monday"), QStringLiteral("Monday"));
    m_comboWeekDay->addItem(tr("Tuesday"), QStringLiteral("Tuesday"));
    m_comboWeekDay->addItem(tr("Wednesday"), QStringLiteral("Wednesday"));
    m_comboWeekDay->addItem(tr("Thursday"), QStringLiteral("Thursday"));
    m_comboWeekDay->addItem(tr("Friday"), QStringLiteral("Friday"));
    m_comboWeekDay->addItem(tr("Saturday"), QStringLiteral("Saturday"));
    weeklyLayout->addWidget(
        createFormRow(tr("Day of Week"), m_comboWeekDay, weeklyPage));

    m_comboWeeklyTime = LayoutUtils::createStyledCombo(weeklyPage);
    m_comboWeeklyTime->setFixedWidth(TriggerComboWidth);
    for (int hour = 0; hour < 24; ++hour) {
        for (int minute = 0; minute < 60; minute += 30) {
            const QString timeLabel = QStringLiteral("%1:%2")
                                          .arg(hour, 2, 10, QChar('0'))
                                          .arg(minute, 2, 10, QChar('0'));
            m_comboWeeklyTime->addItem(
                timeLabel, (hour * 60 + minute) * TicksPerMinute);
        }
    }
    weeklyLayout->addWidget(
        createFormRow(tr("Time"), m_comboWeeklyTime, weeklyPage));
    m_triggerOptionsStack->addWidget(weeklyPage);

    auto *startupPage = new QWidget();
    auto *startupLayout = new QVBoxLayout(startupPage);
    startupLayout->setContentsMargins(0, 8, 0, 0);
    startupLayout->setSpacing(8);
    auto *startupNote = new QLabel(tr("Task will run when the server starts."),
                                   startupPage);
    startupNote->setObjectName("ManageInfoKey");
    startupNote->setWordWrap(true);
    startupLayout->addWidget(startupNote);
    m_triggerOptionsStack->addWidget(startupPage);

    formBody->addWidget(m_triggerOptionsStack);

    m_editTimeLimit = new QLineEdit(formContent);
    m_editTimeLimit->setObjectName("ManageLibInput");
    m_editTimeLimit->setPlaceholderText(tr("Optional"));
    m_editTimeLimit->setClearButtonEnabled(true);
    m_editTimeLimit->setFixedWidth(TriggerComboWidth);
    formBody->addWidget(createFormRow(
        tr("Time Limit (hours)"), m_editTimeLimit, formContent));

    auto *formButtonRow = new QHBoxLayout();
    formButtonRow->setContentsMargins(0, 0, 0, 0);
    formButtonRow->setSpacing(8);

    auto *cancelAddBtn = createInlineButton(tr("Cancel"), formContent);
    auto *confirmAddBtn = createInlineButton(tr("Add Trigger"), formContent);
    formButtonRow->addStretch(1);
    formButtonRow->addWidget(cancelAddBtn);
    formButtonRow->addWidget(confirmAddBtn);
    formBody->addLayout(formButtonRow);

    connect(cancelAddBtn, &QPushButton::clicked,
            this, &TaskEditDialog::hideAddTriggerForm);
    connect(confirmAddBtn, &QPushButton::clicked,
            this, &TaskEditDialog::addTriggerFromForm);

    m_addFormContainer->hide();
    layout->addWidget(m_addFormContainer);

    layout->addStretch(1);
    m_pageStack->addWidget(page);
}

void TaskEditDialog::setupTriggerRow(const QJsonObject &trigger, int index) {
    auto *row = new QWidget(this);
    row->setObjectName("TaskEditTriggerRow");
    row->setAttribute(Qt::WA_StyledBackground, true);

    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 10, 12, 10);
    rowLayout->setSpacing(10);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    const QString type = trigger.value("Type").toString();
    const bool editable = TaskUtils::isEditableTriggerType(type);

    auto *titleLabel = new QLabel(TaskUtils::localizedTriggerType(type), row);
    titleLabel->setObjectName("ManageCardTitle");
    textLayout->addWidget(titleLabel);

    auto *descLabel = new QLabel(TaskUtils::formatTriggerDescription(trigger), row);
    descLabel->setObjectName("ManageInfoValue");
    descLabel->setWordWrap(true);
    textLayout->addWidget(descLabel);

    QStringList metaParts;
    const qint64 maxRuntimeTicks =
        trigger.value("MaxRuntimeTicks").toVariant().toLongLong();
    if (maxRuntimeTicks > 0) {
        metaParts.append(tr("Max runtime %1")
                             .arg(TaskUtils::formatDurationTicks(maxRuntimeTicks)));
    }
    if (!editable) {
        metaParts.append(tr("Read-only"));
    }

    if (!metaParts.isEmpty()) {
        auto *metaLabel = new QLabel(metaParts.join(QStringLiteral("  ·  ")), row);
        metaLabel->setObjectName("TaskTriggerMetaLabel");
        metaLabel->setWordWrap(true);
        textLayout->addWidget(metaLabel);
    }

    rowLayout->addLayout(textLayout, 1);

    if (editable) {
        auto *deleteBtn = createInlineButton(tr("Remove"), row);
        rowLayout->addWidget(deleteBtn, 0, Qt::AlignVCenter);
        connect(deleteBtn, &QPushButton::clicked, this,
                [this, index]() { removeTrigger(index); });
    } else {
        auto *badge = new QLabel(tr("Built-in"), row);
        badge->setObjectName("TaskReadOnlyBadge");
        rowLayout->addWidget(badge, 0, Qt::AlignVCenter);
    }

    m_triggerListLayout->addWidget(row);
}

void TaskEditDialog::onNavigationChanged(int index) {
    if (index < 0 || index >= m_pageStack->count() ||
        index == m_currentPageIndex) {
        return;
    }

    qDebug() << "[TaskEditDialog] Navigate page"
             << "| from=" << m_currentPageIndex
             << "| to=" << index;

    m_currentPageIndex = index;
    m_pageStack->slideInIdx(index, SlidingStackedWidget::Automatic);
    updateNavigationState();
}

void TaskEditDialog::updateNavigationState() {
    for (int index = 0; index < m_navButtons.size(); ++index) {
        QPushButton *button = m_navButtons[index];
        if (!button) {
            continue;
        }

        const bool active = index == m_currentPageIndex;
        button->setProperty("navActive", active);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void TaskEditDialog::updateDialogState() {
    if (m_btnAddTrigger) {
        m_btnAddTrigger->setEnabled(!m_isLoading);
    }
    if (m_addFormContainer) {
        m_addFormContainer->setEnabled(!m_isLoading);
    }
    if (m_btnSave) {
        m_btnSave->setEnabled(!m_isLoading && m_hasChanges);
    }
}

void TaskEditDialog::updateOverviewFields() {
    m_scheduleValueLabel->setText(TaskUtils::formatTriggerSummary(m_triggers, 3));
    m_triggerSummaryLabel->setText(TaskUtils::formatTriggerSummary(m_triggers, 3));

    bool hasUnsupportedTriggers = false;
    for (const QJsonValue &value : m_triggers) {
        if (!TaskUtils::isEditableTriggerType(
                value.toObject().value("Type").toString())) {
            hasUnsupportedTriggers = true;
            break;
        }
    }

    QStringList overviewNotes;
    if (m_isHidden) {
        overviewNotes.append(tr("This is a built-in system task reported by the server. Some properties may be managed by the server."));
    }
    if (hasUnsupportedTriggers) {
        overviewNotes.append(tr("Built-in trigger types are shown as read-only and will be preserved when you save."));
    }
    if (overviewNotes.isEmpty()) {
        overviewNotes.append(tr("Changes in this dialog only update trigger rules. Start or stop a task from the tasks page."));
    }
    m_overviewNoticeLabel->setText(overviewNotes.join(QStringLiteral("\n\n")));

    if (hasUnsupportedTriggers) {
        m_triggerHelperLabel->setText(tr("Built-in server triggers are shown as read-only and will be preserved when you save."));
    } else {
        m_triggerHelperLabel->setText(
            tr("Add, remove, and save trigger rules for this task."));
    }
}

void TaskEditDialog::updateTriggerOptionsHeight() {
    if (!m_triggerOptionsStack) {
        return;
    }

    QWidget *currentPage = m_triggerOptionsStack->currentWidget();
    if (!currentPage) {
        return;
    }

    m_triggerOptionsStack->setFixedHeight(currentPage->sizeHint().height());
}

void TaskEditDialog::showAddTriggerForm() {
    m_addFormContainer->show();
    m_btnAddTrigger->hide();
    m_comboTriggerType->setCurrentIndex(0);
    m_triggerOptionsStack->setCurrentIndex(0);
    m_editTimeLimit->clear();
}

void TaskEditDialog::hideAddTriggerForm() {
    m_addFormContainer->hide();
    m_btnAddTrigger->show();
}

void TaskEditDialog::addTriggerFromForm() {
    const QString limitText = m_editTimeLimit->text().trimmed();
    if (!limitText.isEmpty()) {
        bool ok = false;
        const double hours = limitText.toDouble(&ok);
        if (!ok || hours <= 0) {
            ModernToast::showMessage(
                tr("Please enter a valid time limit."), 2500);
            return;
        }
    }

    const QJsonObject newTrigger = buildTriggerJson();
    const QByteArray newFingerprint =
        QJsonDocument(newTrigger).toJson(QJsonDocument::Compact);
    for (const QJsonValue &value : m_triggers) {
        if (QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact) ==
            newFingerprint) {
            ModernToast::showMessage(tr("This trigger already exists."), 2500);
            return;
        }
    }

    qDebug() << "[TaskEditDialog] Adding trigger"
             << "| type=" << newTrigger.value("Type").toString();

    m_triggers.append(newTrigger);
    rebuildTriggerList();
    updateOverviewFields();
    m_hasChanges = (m_triggers != m_loadedTriggers);
    updateDialogState();
    hideAddTriggerForm();
}

void TaskEditDialog::removeTrigger(int index) {
    if (index < 0 || index >= m_triggers.size()) {
        return;
    }

    qDebug() << "[TaskEditDialog] Removing trigger at index" << index;

    m_triggers.removeAt(index);
    rebuildTriggerList();
    updateOverviewFields();
    m_hasChanges = (m_triggers != m_loadedTriggers);
    updateDialogState();
}

void TaskEditDialog::rebuildTriggerList() {
    LayoutUtils::clearLayout(m_triggerListLayout);

    if (m_triggers.isEmpty()) {
        auto *emptyLabel = new QLabel(tr("No triggers configured"), this);
        emptyLabel->setObjectName("ManageInfoValue");
        emptyLabel->setAlignment(Qt::AlignCenter);
        m_triggerListLayout->addWidget(emptyLabel);
        return;
    }

    for (int index = 0; index < m_triggers.size(); ++index) {
        setupTriggerRow(m_triggers.at(index).toObject(), index);
    }
}

QJsonObject TaskEditDialog::buildTriggerJson() const {
    QJsonObject trigger;
    const int typeIndex = m_comboTriggerType->currentIndex();
    const QString type = m_comboTriggerType->currentData().toString();
    trigger["Type"] = type;

    switch (typeIndex) {
    case 0:
        trigger["TimeOfDayTicks"] = m_comboDailyTime->currentData().toLongLong();
        break;
    case 1:
        trigger["IntervalTicks"] = m_comboInterval->currentData().toLongLong();
        break;
    case 2:
        trigger["DayOfWeek"] = m_comboWeekDay->currentData().toString();
        trigger["TimeOfDayTicks"] =
            m_comboWeeklyTime->currentData().toLongLong();
        break;
    case 3:
    default:
        break;
    }

    const QString limitText = m_editTimeLimit->text().trimmed();
    if (!limitText.isEmpty()) {
        bool ok = false;
        const double hours = limitText.toDouble(&ok);
        if (ok && hours > 0) {
            trigger["MaxRuntimeTicks"] =
                static_cast<qint64>(hours * TicksPerHour);
        }
    }

    return trigger;
}

QCoro::Task<void> TaskEditDialog::loadTaskData() {
    QPointer<TaskEditDialog> safeThis(this);
    m_isLoading = true;
    updateDialogState();

    try {
        const QJsonObject taskJson =
            co_await m_core->adminService()->getScheduledTaskById(m_taskId);
        if (!safeThis) {
            co_return;
        }

        const QString name = taskJson.value("Name").toString();
        m_titleLabel->setText(name);
        setTitle(name);

        const QString description = taskJson.value("Description").toString();
        m_descLabel->setText(description.isEmpty()
                                 ? tr("No description provided.")
                                 : description);

        m_isHidden = taskJson.value("IsHidden").toBool(false);
        m_stateValueLabel->setText(
            TaskUtils::localizedTaskState(taskJson.value("State").toString()));
        m_categoryValueLabel->setText(
            taskJson.value("Category").toString(tr("Other")));
        m_visibilityValueLabel->setText(
            m_isHidden ? tr("System task") : tr("Visible task"));

        const QJsonObject lastResult =
            taskJson.value("LastExecutionResult").toObject();
        const QString status = lastResult.value("Status").toString();
        const QString endTime =
            TaskUtils::formatIsoDateTime(lastResult.value("EndTimeUtc").toString());
        m_lastResultValueLabel->setText(
            status.isEmpty() ? tr("Never run")
                             : TaskUtils::localizedTaskResult(status));
        m_lastFinishedValueLabel->setText(
            endTime.isEmpty() ? tr("Not available") : endTime);

        m_triggers = taskJson.value("Triggers").toArray();
        m_loadedTriggers = m_triggers;
        m_hasChanges = false;

        qDebug() << "[TaskEditDialog] Loaded task"
                 << "| name=" << name
                 << "| isHidden=" << m_isHidden
                 << "| triggers=" << m_triggers.size();

        rebuildTriggerList();
        updateOverviewFields();
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        ModernToast::showMessage(
            tr("Failed to load task: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isLoading = false;
    updateDialogState();
}

QCoro::Task<void> TaskEditDialog::saveTriggers() {
    QPointer<TaskEditDialog> safeThis(this);
    m_isLoading = true;
    updateDialogState();

    try {
        co_await m_core->adminService()->updateTaskTriggers(m_taskId, m_triggers);
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(tr("Triggers saved"), 2000);
        m_loadedTriggers = m_triggers;
        m_hasChanges = false;
        emit triggersUpdated();
        close();
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        ModernToast::showMessage(
            tr("Failed to save triggers: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (safeThis) {
        m_isLoading = false;
        updateDialogState();
    }
}
