#include "pagetasks.h"

#include "../../components/moderntoast.h"
#include "../../components/taskeditdialog.h"
#include "../../utils/layoututils.h"
#include "../../utils/taskutils.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>

#include <QDebug>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <algorithm>

namespace
{

constexpr int AutoRefreshIntervalMs = 3000;

} 





PageTasks::PageTasks(QEmbyCore *core, QWidget *parent) : ManagePageBase(core, tr("Tasks"), parent)
{
    m_mainLayout->setContentsMargins(16, 30, 0, 0);

    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 25, 0);
    titleRow->setSpacing(8);

    m_btnRefresh = new QPushButton(this);
    m_btnRefresh->setObjectName("ManageRefreshBtn");
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnRefresh->setFixedSize(32, 32);
    m_btnRefresh->setToolTip(tr("Refresh"));
    titleRow->addWidget(m_btnRefresh);

    titleRow->addStretch(2);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName("ManageInfoValue");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    titleRow->addWidget(m_statsLabel);

    m_mainLayout->addLayout(titleRow);

    m_scrollContentWidget = new QWidget(this);
    m_scrollContentWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_scrollContentLayout = new QVBoxLayout(m_scrollContentWidget);
    m_scrollContentLayout->setContentsMargins(0, 0, 12, 40);
    m_scrollContentLayout->setSpacing(16);

    auto *summaryGrid = new QGridLayout();
    summaryGrid->setContentsMargins(0, 0, 0, 0);
    summaryGrid->setHorizontalSpacing(10);
    summaryGrid->setVerticalSpacing(10);
    summaryGrid->addWidget(createSummaryCard(tr("All Tasks"), m_totalValueLabel, m_scrollContentWidget), 0, 0);
    summaryGrid->addWidget(createSummaryCard(tr("Scheduled"), m_scheduledValueLabel, m_scrollContentWidget), 0, 1);
    summaryGrid->addWidget(createSummaryCard(tr("Running"), m_runningValueLabel, m_scrollContentWidget), 0, 2);
    summaryGrid->addWidget(createSummaryCard(tr("Manual"), m_manualValueLabel, m_scrollContentWidget), 0, 3);
    summaryGrid->setColumnStretch(0, 1);
    summaryGrid->setColumnStretch(1, 1);
    summaryGrid->setColumnStretch(2, 1);
    summaryGrid->setColumnStretch(3, 1);
    m_scrollContentLayout->addLayout(summaryGrid);

    m_taskSectionsLayout = new QVBoxLayout();
    m_taskSectionsLayout->setContentsMargins(0, 0, 0, 0);
    m_taskSectionsLayout->setSpacing(16);
    m_scrollContentLayout->addLayout(m_taskSectionsLayout);

    m_emptyStateContainer = new QWidget(m_scrollContentWidget);
    m_emptyStateContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto *emptyLayout = new QVBoxLayout(m_emptyStateContainer);
    emptyLayout->setContentsMargins(0, 20, 0, 0);
    emptyLayout->setSpacing(0);
    emptyLayout->addStretch(1);
    m_emptyLabel = new QLabel(tr("No tasks found"), m_emptyStateContainer);
    m_emptyLabel->setObjectName("ManageEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyStateContainer->hide();
    emptyLayout->addWidget(m_emptyLabel);
    emptyLayout->addStretch(1);
    m_scrollContentLayout->addWidget(m_emptyStateContainer, 1);

    m_bottomSpacer = new QWidget(m_scrollContentWidget);
    m_bottomSpacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_scrollContentLayout->addWidget(m_bottomSpacer, 1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("SettingsScrollArea");
    m_scrollArea->setWidget(m_scrollContentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->viewport()->setAutoFillBackground(false);

    m_mainLayout->addWidget(m_scrollArea, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(AutoRefreshIntervalMs);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() { m_pendingTask = loadData(false); });

    connect(m_btnRefresh, &QPushButton::clicked, this, [this]() { m_pendingTask = loadData(true); });
}

void PageTasks::showEvent(QShowEvent *event)
{
    ManagePageBase::showEvent(event);
    if (!m_loaded)
    {
        m_pendingTask = loadData();
        m_loaded = true;
    }
    m_refreshTimer->start();
}

void PageTasks::hideEvent(QHideEvent *event)
{
    m_refreshTimer->stop();
    ManagePageBase::hideEvent(event);
}

QCoro::Task<void> PageTasks::loadData(bool isManual)
{
    QPointer<PageTasks> safeThis(this);

    if (m_isLoading && !isManual)
    {
        co_return;
    }

    m_isLoading = true;
    m_btnRefresh->setEnabled(false);
    if (isManual || !m_hasLoadedData)
    {
        m_statsLabel->setText(tr("Loading..."));
    }

    if (isManual)
    {
        QIcon originalIcon = m_btnRefresh->icon();
        QSize iconSz = m_btnRefresh->iconSize();
        QPixmap basePx = originalIcon.pixmap(iconSz);

        auto *rotAnim = new QVariantAnimation(this);
        rotAnim->setStartValue(0.0);
        rotAnim->setEndValue(360.0);
        rotAnim->setDuration(800);
        rotAnim->setEasingCurve(QEasingCurve::InOutCubic);

        connect(rotAnim, &QVariantAnimation::valueChanged, this,
                [this, basePx, iconSz](const QVariant &val)
                {
                    qreal angle = val.toReal();
                    QTransform t;
                    t.translate(iconSz.width() / 2.0, iconSz.height() / 2.0);
                    t.rotate(angle);
                    t.translate(-iconSz.width() / 2.0, -iconSz.height() / 2.0);
                    QPixmap rotated = basePx.transformed(t, Qt::SmoothTransformation);
                    int xOff = (rotated.width() - iconSz.width()) / 2;
                    int yOff = (rotated.height() - iconSz.height()) / 2;
                    QPixmap cropped = rotated.copy(xOff, yOff, iconSz.width(), iconSz.height());
                    m_btnRefresh->setIcon(QIcon(cropped));
                });
        connect(rotAnim, &QVariantAnimation::finished, this,
                [this, originalIcon]() { m_btnRefresh->setIcon(originalIcon); });
        rotAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    try
    {
        auto tasks = co_await m_core->adminService()->getScheduledTasks();
        if (!safeThis)
        {
            co_return;
        }

        qDebug() << "[PageTasks] Loaded tasks"
                 << "| count=" << tasks.size();

        std::sort(tasks.begin(), tasks.end(),
                  [](const ScheduledTaskInfo &lhs, const ScheduledTaskInfo &rhs)
                  {
                      if (lhs.isRunning() != rhs.isRunning())
                      {
                          return lhs.isRunning() && !rhs.isRunning();
                      }
                      if (lhs.isHidden != rhs.isHidden)
                      {
                          return !lhs.isHidden && rhs.isHidden;
                      }

                      const QString leftCategory = lhs.category.isEmpty() ? QStringLiteral("~") : lhs.category;
                      const QString rightCategory = rhs.category.isEmpty() ? QStringLiteral("~") : rhs.category;
                      const int categoryCompare = QString::localeAwareCompare(leftCategory, rightCategory);
                      if (categoryCompare != 0)
                      {
                          return categoryCompare < 0;
                      }

                      return QString::localeAwareCompare(lhs.name, rhs.name) < 0;
                  });

        updateSummaryCards(tasks);
        rebuildTaskSections(tasks);

        if (tasks.isEmpty())
        {
            m_emptyStateContainer->show();
            m_bottomSpacer->hide();
            m_statsLabel->setText(tr("No tasks"));
        }
        else
        {
            m_emptyStateContainer->hide();
            m_bottomSpacer->show();
        }
        m_hasLoadedData = true;
    }
    catch (const std::exception &e)
    {
        if (!safeThis)
        {
            co_return;
        }
        if (isManual || !m_hasLoadedData)
        {
            m_statsLabel->setText(tr("Refresh failed"));
        }
        ModernToast::showMessage(tr("Failed to load tasks: %1").arg(QString::fromUtf8(e.what())), 3000);
    }

    if (!safeThis)
    {
        co_return;
    }

    m_isLoading = false;
    m_btnRefresh->setEnabled(true);
}

QCoro::Task<void> PageTasks::onRunClicked(QString taskId, QString taskName)
{
    QPointer<PageTasks> safeThis(this);

    qDebug() << "[PageTasks] Running task"
             << "| taskId=" << taskId << "| taskName=" << taskName;

    try
    {
        co_await m_core->adminService()->runScheduledTask(taskId);
        if (!safeThis)
        {
            co_return;
        }
        ModernToast::showMessage(tr("Task \"%1\" started").arg(taskName), 2000);
        co_await loadData(false);
    }
    catch (const std::exception &e)
    {
        if (!safeThis)
        {
            co_return;
        }
        ModernToast::showMessage(tr("Failed to run task: %1").arg(QString::fromUtf8(e.what())), 3000);
    }
}

QCoro::Task<void> PageTasks::onStopClicked(QString taskId, QString taskName)
{
    QPointer<PageTasks> safeThis(this);

    qDebug() << "[PageTasks] Stopping task"
             << "| taskId=" << taskId << "| taskName=" << taskName;

    try
    {
        co_await m_core->adminService()->stopScheduledTask(taskId);
        if (!safeThis)
        {
            co_return;
        }
        ModernToast::showMessage(tr("Task \"%1\" stopped").arg(taskName), 2000);
        co_await loadData(false);
    }
    catch (const std::exception &e)
    {
        if (!safeThis)
        {
            co_return;
        }
        ModernToast::showMessage(tr("Failed to stop task: %1").arg(QString::fromUtf8(e.what())), 3000);
    }
}

void PageTasks::onCardClicked(const QString &taskId, const QString &taskName)
{
    auto *dialog = new TaskEditDialog(m_core, taskId, taskName, this);
    connect(dialog, &TaskEditDialog::triggersUpdated, this, [this]() { m_pendingTask = loadData(false); });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

QWidget *PageTasks::createSummaryCard(const QString &title, QLabel *&valueLabel, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("ManageInfoCard");
    card->setAttribute(Qt::WA_StyledBackground, true);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    valueLabel = new QLabel(QStringLiteral("0"), card);
    valueLabel->setObjectName("TaskSummaryValue");
    layout->addWidget(valueLabel);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("TaskSummaryCaption");
    titleLabel->setWordWrap(false);
    layout->addWidget(titleLabel);
    layout->addStretch(1);

    return card;
}

void PageTasks::clearTaskSections()
{
    LayoutUtils::clearLayout(m_taskSectionsLayout);
}

void PageTasks::updateSummaryCards(const QList<ScheduledTaskInfo> &tasks)
{
    int runningCount = 0;
    int scheduledCount = 0;
    int manualCount = 0;
    int systemCount = 0;

    for (const ScheduledTaskInfo &task : tasks)
    {
        if (task.isRunning())
        {
            ++runningCount;
        }

        if (task.isHidden)
        {
            ++systemCount;
        }
        else if (task.hasTriggers())
        {
            ++scheduledCount;
        }
        else
        {
            ++manualCount;
        }
    }

    m_totalValueLabel->setText(QString::number(tasks.size()));
    m_runningValueLabel->setText(QString::number(runningCount));
    m_scheduledValueLabel->setText(QString::number(scheduledCount));
    m_manualValueLabel->setText(QString::number(manualCount));

    if (tasks.isEmpty())
    {
        m_statsLabel->setText(tr("No tasks"));
        return;
    }

    QStringList parts;
    parts.append(tr("%n task(s)", "", tasks.size()));
    if (runningCount > 0)
    {
        parts.append(tr("%n running", "", runningCount));
    }
    if (systemCount > 0)
    {
        parts.append(tr("%n system", "", systemCount));
    }
    m_statsLabel->setText(parts.join(QStringLiteral("  ·  ")));
}

void PageTasks::rebuildTaskSections(const QList<ScheduledTaskInfo> &tasks)
{
    clearTaskSections();

    QList<ScheduledTaskInfo> runningTasks;
    QList<ScheduledTaskInfo> scheduledTasks;
    QList<ScheduledTaskInfo> manualTasks;
    QList<ScheduledTaskInfo> systemTasks;

    for (const ScheduledTaskInfo &task : tasks)
    {
        if (task.isRunning())
        {
            runningTasks.append(task);
        }
        else if (task.isHidden)
        {
            systemTasks.append(task);
        }
        else if (task.hasTriggers())
        {
            scheduledTasks.append(task);
        }
        else
        {
            manualTasks.append(task);
        }
    }

    if (!runningTasks.isEmpty())
    {
        addTaskSection(tr("Running Now"), tr("Tasks that are currently executing. Progress refreshes automatically."),
                       runningTasks);
    }
    if (!scheduledTasks.isEmpty())
    {
        addTaskSection(tr("Scheduled Tasks"), tr("Tasks with one or more configured triggers."), scheduledTasks);
    }
    if (!manualTasks.isEmpty())
    {
        addTaskSection(tr("Manual Tasks"), tr("Tasks that only run when started manually."), manualTasks);
    }
    if (!systemTasks.isEmpty())
    {
        addTaskSection(tr("System Tasks"), tr("Hidden or built-in tasks reported by the server."), systemTasks);
    }

    m_emptyStateContainer->setVisible(tasks.isEmpty());
}

void PageTasks::addTaskSection(const QString &title, const QString &subtitle, const QList<ScheduledTaskInfo> &tasks)
{
    auto *sectionWidget = new QWidget(m_scrollContentWidget);
    auto *sectionLayout = new QVBoxLayout(sectionWidget);
    sectionLayout->setContentsMargins(0, 0, 0, 0);
    sectionLayout->setSpacing(10);

    auto *titleLabel = new QLabel(title, sectionWidget);
    titleLabel->setObjectName("ManageCardTitle");
    sectionLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel(subtitle, sectionWidget);
    subtitleLabel->setObjectName("ManageInfoKey");
    subtitleLabel->setWordWrap(true);
    sectionLayout->addWidget(subtitleLabel);

    auto *cardsLayout = new QVBoxLayout();
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(8);
    for (const ScheduledTaskInfo &task : tasks)
    {
        addTaskCard(task, cardsLayout);
    }
    sectionLayout->addLayout(cardsLayout);

    m_taskSectionsLayout->addWidget(sectionWidget);
}

void PageTasks::addTaskCard(const ScheduledTaskInfo &task, QVBoxLayout *parentLayout)
{
    auto *card = new QFrame(m_scrollContentWidget);
    card->setObjectName("ManageInfoCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(6);

    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(4);

    auto *nameLabel = new QLabel(task.name, card);
    nameLabel->setObjectName("ManageCardTitle");
    nameLabel->setWordWrap(true);
    titleColumn->addWidget(nameLabel);

    auto *badgeRow = new QHBoxLayout();
    badgeRow->setContentsMargins(0, 0, 0, 0);
    badgeRow->setSpacing(5);

    auto *categoryBadge = new QLabel(task.category.isEmpty() ? tr("Other") : task.category, card);
    categoryBadge->setObjectName("TaskMetaBadge");
    badgeRow->addWidget(categoryBadge);

    auto *modeBadge =
        new QLabel(task.isHidden ? tr("System") : (task.hasTriggers() ? tr("Scheduled") : tr("Manual")), card);
    modeBadge->setObjectName(task.isHidden ? "TaskMutedBadge" : "TaskMetaBadge");
    badgeRow->addWidget(modeBadge);
    badgeRow->addStretch(1);
    titleColumn->addLayout(badgeRow);

    topRow->addLayout(titleColumn, 1);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(6);

    auto *detailsBtn = new QPushButton(card);
    detailsBtn->setObjectName("ManageTaskDetailsBtn");
    detailsBtn->setCursor(Qt::PointingHandCursor);
    detailsBtn->setFixedSize(32, 32);
    detailsBtn->setToolTip(tr("Details"));
    connect(detailsBtn, &QPushButton::clicked, this, [this, task]() { onCardClicked(task.id, task.name); });
    buttonRow->addWidget(detailsBtn);

    auto *actionBtn = new QPushButton(card);
    actionBtn->setFixedSize(32, 32);
    actionBtn->setCursor(Qt::PointingHandCursor);
    if (task.isRunning())
    {
        actionBtn->setObjectName("ManageTaskStopBtn");
        actionBtn->setToolTip(tr("Stop task"));
        connect(actionBtn, &QPushButton::clicked, this,
                [this, task]() { m_pendingTask = onStopClicked(task.id, task.name); });
    }
    else
    {
        actionBtn->setObjectName("ManageTaskRunBtn");
        actionBtn->setToolTip(tr("Run task"));
        connect(actionBtn, &QPushButton::clicked, this,
                [this, task]() { m_pendingTask = onRunClicked(task.id, task.name); });
    }
    buttonRow->addWidget(actionBtn);

    topRow->addLayout(buttonRow);
    cardLayout->addLayout(topRow);

    if (!task.description.isEmpty())
    {
        auto *descLabel = new QLabel(task.description, card);
        descLabel->setObjectName("ManageInfoKey");
        descLabel->setWordWrap(true);
        cardLayout->addWidget(descLabel);
    }

    auto *scheduleLabel = new QLabel(tr("Schedule: %1").arg(TaskUtils::formatTriggerSummary(task.triggers, 2)), card);
    scheduleLabel->setObjectName("ManageInfoValue");
    scheduleLabel->setWordWrap(true);
    cardLayout->addWidget(scheduleLabel);

    QStringList statusParts;
    statusParts.append(tr("State: %1").arg(TaskUtils::localizedTaskState(task.state)));
    if (!task.lastExecutionResult.isEmpty())
    {
        QString lastResult = tr("Last result: %1").arg(TaskUtils::localizedTaskResult(task.lastExecutionResult));
        if (!task.lastEndTime.isEmpty())
        {
            lastResult += QStringLiteral("  ·  %1").arg(TaskUtils::formatIsoDateTime(task.lastEndTime));
        }
        statusParts.append(lastResult);
    }

    auto *statusLabel = new QLabel(statusParts.join(QStringLiteral("  ·  ")), card);
    statusLabel->setObjectName("ManageInfoValue");
    statusLabel->setWordWrap(true);
    cardLayout->addWidget(statusLabel);

    if (task.isRunning())
    {
        auto *progressBar = new QProgressBar(card);
        progressBar->setObjectName("ManageTaskProgress");
        progressBar->setRange(0, 100);
        progressBar->setValue(static_cast<int>(task.currentProgressPercentage));
        progressBar->setTextVisible(false);
        progressBar->setFixedHeight(4);
        cardLayout->addWidget(progressBar);

        auto *progressLabel = new QLabel(tr("%1 · %2%")
                                             .arg(TaskUtils::localizedTaskState(task.state))
                                             .arg(task.currentProgressPercentage, 0, 'f', 1),
                                         card);
        progressLabel->setObjectName("ManageInfoKey");
        cardLayout->addWidget(progressLabel);
    }

    parentLayout->addWidget(card);
}
