#pragma once

#include <QPushButton>

class QAction;
class QMenu;

class ArtworkViewButton : public QPushButton {
    Q_OBJECT
public:
    enum Mode { FollowDefault, Portrait, Landscape };
    Q_ENUM(Mode)
    explicit ArtworkViewButton(QWidget* parent = nullptr);
    void setMode(Mode mode);
    void setAlbumOnly(bool albumOnly);
    void setFollowDefaultAvailable(bool available);
signals:
    void modeSelected(ArtworkViewButton::Mode mode);
private:
    void updateAppearance();
    QMenu* m_menu;
    QAction* m_followDefault;
    QAction* m_portrait;
    QAction* m_landscape;
    Mode m_mode = FollowDefault;
    bool m_albumOnly = false;
};
