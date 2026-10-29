#include "artworkviewbutton.h"
#include <QAction>
#include <QActionGroup>
#include <QMenu>

ArtworkViewButton::ArtworkViewButton(QWidget* parent) : QPushButton(parent)
{
    setObjectName("modern-sort-btn");
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("View Mode"));
    m_menu = new QMenu(this);
    auto* group = new QActionGroup(m_menu);
    m_followDefault = m_menu->addAction(tr("Follow default"));
    m_portrait = m_menu->addAction(tr("Portrait posters"));
    m_landscape = m_menu->addAction(tr("Landscape tiles"));
    for (auto* action : {m_followDefault, m_portrait, m_landscape}) {
        action->setCheckable(true);
        group->addAction(action);
    }
    connect(m_menu, &QMenu::triggered, this, [this](QAction* action) {
        const Mode mode = action == m_followDefault ? FollowDefault : action == m_portrait ? Portrait : Landscape;
        setMode(mode);
        Q_EMIT modeSelected(mode);
    });
    connect(this, &QPushButton::clicked, this, [this]() {
        m_menu->adjustSize();
        QPoint pos = mapToGlobal(QPoint(width() / 2, height() + 6));
        pos.rx() -= m_menu->sizeHint().width() / 2;
        m_menu->popup(pos);
    });
    updateAppearance();
}

void ArtworkViewButton::setMode(Mode mode)
{
    m_mode = mode;
    updateAppearance();
}

void ArtworkViewButton::setAlbumOnly(bool albumOnly)
{
    m_albumOnly = albumOnly;
    setEnabled(!albumOnly);
    updateAppearance();
}

void ArtworkViewButton::setFollowDefaultAvailable(bool available)
{
    m_followDefault->setEnabled(available);
}

void ArtworkViewButton::updateAppearance()
{
    m_followDefault->setChecked(m_mode == FollowDefault);
    m_portrait->setChecked(m_mode == Portrait);
    m_landscape->setChecked(m_mode == Landscape);
    const QString label = m_albumOnly ? tr("Album covers") : m_mode == FollowDefault ? tr("Follow default") :
        m_mode == Landscape ? tr("Landscape tiles") : tr("Portrait posters");
    setText(label + QStringLiteral(" ▾"));
}
