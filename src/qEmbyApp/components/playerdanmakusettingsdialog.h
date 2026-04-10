#ifndef PLAYERDANMAKUSETTINGSDIALOG_H
#define PLAYERDANMAKUSETTINGSDIALOG_H

#include "playeroverlaydialog.h"

class PlayerDanmakuSettingsDialog : public PlayerOverlayDialog
{
    Q_OBJECT

public:
    explicit PlayerDanmakuSettingsDialog(QWidget *parent = nullptr);

    bool requiresReload() const;
    bool danmakuEnabledChanged() const;

signals:
    void liveReloadRequested();
    void danmakuEnabledToggled(bool enabled);

private:
    void scheduleLiveReload();

    bool m_requiresReload = false;
    bool m_initialEnabled = false;
    class QTimer *m_liveReloadTimer = nullptr;
};

#endif 
