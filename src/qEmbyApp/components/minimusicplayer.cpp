#include "minimusicplayer.h"
#include "elidedlabel.h"
#include "modernslider.h"
#include "../managers/musicplaybackmanager.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPointer>

MiniMusicPlayer::MiniMusicPlayer(QEmbyCore* core, QWidget* parent) : QWidget(parent), m_core(core)
{
    setObjectName("MiniMusicPlayer");
    setAttribute(Qt::WA_StyledBackground, true);
    auto* manager = MusicPlaybackManager::instance();
    manager->init(core);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 10, 20, 10);
    layout->setSpacing(12);
    m_cover = new QLabel(this);
    m_cover->setFixedSize(48, 48);
    layout->addWidget(m_cover);
    auto* labels = new QVBoxLayout;
    labels->setSpacing(2);
    m_title = new ElidedLabel(this);
    m_artist = new ElidedLabel(this);
    m_artist->setObjectName("SettingsCardDesc");
    labels->addWidget(m_title); labels->addWidget(m_artist);
    layout->addLayout(labels, 1);
    auto* previous = new QPushButton(tr("Previous"), this);
    m_play = new QPushButton(tr("Pause"), this);
    auto* next = new QPushButton(tr("Next"), this);
    layout->addWidget(previous); layout->addWidget(m_play); layout->addWidget(next);
    m_progress = new ModernSlider(Qt::Horizontal, this);
    m_progress->setRange(0, 1000);
    layout->addWidget(m_progress, 2);
    auto* volume = new ModernSlider(Qt::Horizontal, this);
    volume->setRange(0, 100); volume->setValue(manager->volume());
    volume->setFixedWidth(90); volume->setToolTip(tr("Volume"));
    layout->addWidget(volume);
    auto* close = new QPushButton(tr("Close"), this);
    layout->addWidget(close);
    connect(previous, &QPushButton::clicked, manager, &MusicPlaybackManager::previous);
    connect(next, &QPushButton::clicked, manager, &MusicPlaybackManager::next);
    connect(m_play, &QPushButton::clicked, manager, &MusicPlaybackManager::togglePause);
    connect(close, &QPushButton::clicked, manager, &MusicPlaybackManager::stop);
    connect(volume, &QSlider::valueChanged, manager, &MusicPlaybackManager::setVolume);
    connect(m_progress, &QSlider::sliderReleased, this, [this, manager]() {
        manager->seek(manager->duration() * m_progress->value() / 1000.0);
    });
    connect(manager, &MusicPlaybackManager::changed, this, &MiniMusicPlayer::refresh);
    refresh();
}

void MiniMusicPlayer::refresh()
{
    const auto* manager = MusicPlaybackManager::instance();
    const auto track = manager->currentTrack();
    setVisible(manager->state() != MusicPlaybackManager::Idle &&
               !m_suppressed);
    m_title->setFullText(track.name);
    m_artist->setFullText(track.artists.join(QStringLiteral(", ")));
    m_play->setText(manager->state() == MusicPlaybackManager::Paused ? tr("Play") : tr("Pause"));
    m_play->setEnabled(manager->state() == MusicPlaybackManager::Playing || manager->state() == MusicPlaybackManager::Paused);
    m_progress->setEnabled(m_play->isEnabled() && manager->duration() > 0);
    if (!m_progress->isSliderDown())
        m_progress->setValue(manager->duration() > 0 ? int(manager->position() * 1000 / manager->duration()) : 0);
    const QString id = track.getPrimaryImageId();
    if (id != m_coverId) {
        m_coverId = id; m_cover->clear();
        if (!id.isEmpty()) QCoro::connect(loadCover(id, track.images.primaryTag), this, []() {});
    }
}

QCoro::Task<void> MiniMusicPlayer::loadCover(QString id, QString tag)
{
    QPointer<MiniMusicPlayer> guard(this);
    try {
        const auto pix = co_await m_core->mediaService()->fetchImage(id, "Primary", tag, 96, -1,
                                           ImageRequestPriority::Normal, this);
        if (guard && guard->m_coverId == id)
            guard->m_cover->setPixmap(pix.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } catch (...) {}
}
