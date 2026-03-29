#ifndef ADMINUSERCARD_H
#define ADMINUSERCARD_H

#include <QFrame>
#include <QString>
#include <models/admin/adminmodels.h>

class QMouseEvent;

class AdminUserCard : public QFrame {
    Q_OBJECT

public:
    explicit AdminUserCard(UserInfo userInfo,
                           QString currentUserId,
                           QWidget* parent = nullptr);

signals:
    void clicked(QString userId);
    void toggleRequested(QString userId, QString userName, bool enable);
    void deleteRequested(QString userId, QString userName);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    UserInfo m_user;
    QString m_currentUserId;
    QString m_displayName;
};

#endif 
