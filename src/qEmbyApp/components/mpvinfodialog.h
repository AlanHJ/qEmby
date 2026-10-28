#ifndef MPVINFODIALOG_H
#define MPVINFODIALOG_H

#include "playeroverlaydialog.h"

class MpvController;
class QPlainTextEdit;
class QTimer;
class QHideEvent;
class QShowEvent;

// Read-only player overlay mirroring the mpv stats page (stats.lua "i" key):
// grouped media / video / playback / audio / cache / output properties,
// refreshed once per second while visible.
class MpvInfoDialog : public PlayerOverlayDialog
{
    Q_OBJECT

public:
    explicit MpvInfoDialog(MpvController *controller, QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void refreshInfo();
    QString buildInfoText() const;

    MpvController *m_controller = nullptr;
    QPlainTextEdit *m_textView = nullptr;
    QTimer *m_refreshTimer = nullptr;
};

#endif
