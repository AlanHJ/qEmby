#ifndef PAGEUSERS_H
#define PAGEUSERS_H

#include "managepagebase.h"
#include <qcorotask.h>
#include <optional>

class FlowLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QWidget;

class PageUsers : public ManagePageBase {
    Q_OBJECT
public:
    explicit PageUsers(QEmbyCore* core, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    QCoro::Task<void> loadData(bool isManual = false);
    QCoro::Task<void> onToggleUserEnabled(QString userId, QString userName, bool enable);
    QCoro::Task<void> onDeleteUser(QString userId, QString userName);
    void onCardClicked(const QString &userId);
    void clearUserCards();
    void updateContentState(int userCount);

    QPushButton* m_btnRefresh;
    QPushButton* m_btnAddUser;
    QLabel* m_statsLabel;

    QScrollArea* m_scrollArea;
    QWidget* m_cardsContainer;
    FlowLayout* m_userCardsLayout;
    QWidget* m_emptyStateContainer;
    QLabel* m_emptyLabel;

    bool m_loaded = false;
    bool m_isLoading = false;

    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
