#ifndef DANMAKUSERVERLISTITEMWIDGET_H
#define DANMAKUSERVERLISTITEMWIDGET_H

#include <QFrame>

#include "models/danmaku/danmakumodels.h"

class QEvent;
class QFrame;
class QLabel;
class ModernSwitch;
class QPushButton;

class DanmakuServerListItemWidget : public QFrame
{
    Q_OBJECT
public:
    explicit DanmakuServerListItemWidget(QWidget *parent = nullptr);

    void setServer(const DanmakuServerDefinition &server);
    void setSelected(bool selected);
    void setCurrentServer(bool currentServer);
    void setRemovable(bool removable);

signals:
    void clicked();
    void enabledChanged(bool enabled);
    void removeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    static QString maskedSecret(const QString &secret);
    void refreshCredentialsLabel();
    void refreshRemoveIcon();
    void refreshSelectionState();

    DanmakuServerDefinition m_server;
    bool m_removable = true;
    QFrame *m_cardFrame = nullptr;
    QLabel *m_nameLabel = nullptr;
    QLabel *m_baseUrlLabel = nullptr;
    QLabel *m_credentialsLabel = nullptr;
    ModernSwitch *m_enabledSwitch = nullptr;
    QPushButton *m_removeButton = nullptr;
};

#endif 
