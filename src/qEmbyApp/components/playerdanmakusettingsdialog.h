#ifndef PLAYERDANMAKUSETTINGSDIALOG_H
#define PLAYERDANMAKUSETTINGSDIALOG_H

#include "playeroverlaydialog.h"

class PlayerDanmakuSettingsDialog : public PlayerOverlayDialog
{
    Q_OBJECT

public:
    explicit PlayerDanmakuSettingsDialog(QWidget *parent = nullptr);

    bool requiresReload() const;

signals:
    void liveReloadRequested();

private:
    void scheduleLiveReload();

    bool m_requiresReload = false;
    class QTimer *m_liveReloadTimer = nullptr;
};

#endif 
