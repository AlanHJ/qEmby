#ifndef TASKEDITDIALOG_H
#define TASKEDITDIALOG_H

#include "moderndialogbase.h"

#include <qcorotask.h>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <optional>

class QComboBox;
class QEmbyCore;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
class SlidingStackedWidget;

class TaskEditDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit TaskEditDialog(QEmbyCore *core, const QString &taskId,
                            const QString &taskName,
                            QWidget *parent = nullptr);

signals:
    void triggersUpdated();

private:
    void setupOverviewPage();
    void setupTriggersPage();
    void setupTriggerRow(const QJsonObject &trigger, int index);
    void onNavigationChanged(int index);
    void updateNavigationState();
    void updateDialogState();
    void updateOverviewFields();
    void updateTriggerOptionsHeight();
    void showAddTriggerForm();
    void hideAddTriggerForm();
    void addTriggerFromForm();
    void removeTrigger(int index);
    void rebuildTriggerList();

    QJsonObject buildTriggerJson() const;

    QCoro::Task<void> loadTaskData();
    QCoro::Task<void> saveTriggers();

    QEmbyCore *m_core;
    QString m_taskId;
    bool m_isHidden = false;
    bool m_hasChanges = false;
    bool m_isLoading = false;
    int m_currentPageIndex = 0;

    SlidingStackedWidget *m_pageStack = nullptr;
    QList<QPushButton *> m_navButtons;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnSave = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_descLabel = nullptr;
    QLabel *m_stateValueLabel = nullptr;
    QLabel *m_categoryValueLabel = nullptr;
    QLabel *m_lastResultValueLabel = nullptr;
    QLabel *m_lastFinishedValueLabel = nullptr;
    QLabel *m_scheduleValueLabel = nullptr;
    QLabel *m_visibilityValueLabel = nullptr;
    QLabel *m_overviewNoticeLabel = nullptr;
    QLabel *m_triggerSummaryLabel = nullptr;
    QLabel *m_triggerHelperLabel = nullptr;

    QVBoxLayout *m_triggerListLayout = nullptr;
    QJsonArray m_triggers;
    QJsonArray m_loadedTriggers;

    QWidget *m_addFormContainer = nullptr;
    QComboBox *m_comboTriggerType = nullptr;
    QStackedWidget *m_triggerOptionsStack = nullptr;
    QComboBox *m_comboDailyTime = nullptr;
    QComboBox *m_comboWeekDay = nullptr;
    QComboBox *m_comboWeeklyTime = nullptr;
    QComboBox *m_comboInterval = nullptr;
    QLineEdit *m_editTimeLimit = nullptr;

    QPushButton *m_btnAddTrigger = nullptr;

    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
