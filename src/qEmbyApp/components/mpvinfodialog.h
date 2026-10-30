#ifndef MPVINFODIALOG_H
#define MPVINFODIALOG_H

#include "mpvcontroller.h"

#include <QPointer>
#include <QWidget>

class QPlainTextEdit;
class QTimer;
class QCloseEvent;
class QHideEvent;
class QMoveEvent;
class QShowEvent;

// Free-floating MPV info window mirroring the mpv stats page (stats.lua "i"
// key): the active glsl-shader chain / preset and video / audio filters plus
// grouped media / video / playback / audio / cache / output properties,
// refreshed once per second while visible. It is a plain top-level window,
// not parented to the player, so it survives player teardown; a dead
// controller is reported as "No playback" instead of crashing.
class MpvInfoDialog : public QWidget
{
    Q_OBJECT

public:
    // playerWindow is only used to place the window next to the player when no
    // geometry was saved; it is never set as the Qt parent.
    explicit MpvInfoDialog(MpvController *controller, QWidget *playerWindow = nullptr);

    void setController(MpvController *controller);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    void refreshInfo();
    QString buildInfoText() const;
    void restoreOrPlaceWindow(const QWidget *playerWindow);
    void saveGeometryToSettings() const;

    QPointer<MpvController> m_controller;
    QPlainTextEdit *m_textView = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QTimer *m_geometrySaveTimer = nullptr;
};

#endif
