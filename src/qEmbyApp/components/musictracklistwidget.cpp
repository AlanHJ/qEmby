#include "musictracklistwidget.h"
#include "../managers/musicplaybackmanager.h"
#include "../managers/playbackmanager.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QPointer>
#include <algorithm>
#include <climits>

MusicTrackListWidget::MusicTrackListWidget(QEmbyCore* core, QWidget* parent) : QWidget(parent), m_core(core)
{
    setObjectName("MusicTrackListWidget");
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 12, 30, 12);
    auto* play = new QPushButton(tr("Play Album"), this);
    layout->addWidget(play, 0, Qt::AlignLeft);
    auto* externalStopped = new QPushButton(tr("I have stopped the external player"), this);
    externalStopped->setToolTip(tr("Use after closing an external player whose completion cannot be detected."));
    externalStopped->setVisible(PlaybackManager::instance()->hasUnconfirmedExternalPlayback());
    layout->addWidget(externalStopped, 0, Qt::AlignLeft);
    connect(PlaybackManager::instance(), &PlaybackManager::externalCompletionUnconfirmed,
            externalStopped, &QWidget::setVisible);
    connect(externalStopped, &QPushButton::clicked, PlaybackManager::instance(), &PlaybackManager::acknowledgeExternalStop);
    m_status = new QLabel(this); layout->addWidget(m_status);
    m_table = new QTreeWidget(this);
    m_table->setRootIsDecorated(false);
    m_table->setHeaderLabels({tr("Status"), tr("Track"), tr("Title"), tr("Artist"), tr("Duration")});
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->setMinimumHeight(280);
    layout->addWidget(m_table);
    connect(play, &QPushButton::clicked, this, [this]() { MusicPlaybackManager::instance()->playQueue(m_tracks); });
    connect(m_table, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* row) {
        MusicPlaybackManager::instance()->playQueue(m_tracks, m_table->indexOfTopLevelItem(row));
    });
    connect(MusicPlaybackManager::instance(), &MusicPlaybackManager::changed, this, &MusicTrackListWidget::updatePlayingTrack);
}

void MusicTrackListWidget::loadAlbum(QString albumId)
{
    if (albumId == m_albumId) return;
    const quint64 generation = ++m_generation;
    delete m_requestContext;
    m_requestContext = new QObject(this);
    m_albumId = albumId; m_tracks.clear(); m_table->clear();
    m_displayedState = -1;
    m_status->setText(tr("Loading tracks…"));
    QCoro::connect(fetchTracks(albumId, generation), this, []() {});
}

QCoro::Task<void> MusicTrackListWidget::fetchTracks(QString albumId, quint64 generation)
{
    QPointer<MusicTrackListWidget> guard(this);
    try {
        auto tracks = co_await m_core->mediaService()->getAlbumTracks(albumId, m_requestContext);
        if (!guard || guard->m_generation != generation) co_return;
        
        std::stable_sort(tracks.begin(), tracks.end(), [](const MediaItem& a, const MediaItem& b) {
            const int discA = qMax(1, a.parentIndexNumber), discB = qMax(1, b.parentIndexNumber);
            if (discA != discB) return discA < discB;
            const int trackA = a.indexNumber < 0 ? INT_MAX : a.indexNumber;
            const int trackB = b.indexNumber < 0 ? INT_MAX : b.indexNumber;
            return trackA < trackB;
        });
        guard->m_tracks = tracks;
        for (const auto& track : tracks) {
            const qint64 seconds = qMax<qint64>(0, track.runTimeTicks / 10000000);
            auto* row = new QTreeWidgetItem(guard->m_table, {QString(),
                track.indexNumber >= 0 ? QStringLiteral("%1.%2").arg(qMax(1, track.parentIndexNumber)).arg(track.indexNumber) : QString(),
                track.name, track.artists.join(QStringLiteral(", ")),
                QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'))});
            row->setTextAlignment(1, Qt::AlignCenter);
            row->setTextAlignment(4, Qt::AlignRight | Qt::AlignVCenter);
        }
        guard->m_status->setText(tracks.isEmpty() ? tr("No tracks available.") : QString());
        guard->updatePlayingTrack();
    } catch (...) {
        if (guard && guard->m_generation == generation) guard->m_status->setText(tr("Unable to load tracks."));
    }
}

void MusicTrackListWidget::updatePlayingTrack()
{
    const auto* manager = MusicPlaybackManager::instance();
    const QString id = manager->currentTrack().id;
    if (id == m_displayedTrack && m_displayedState == int(manager->state())) return;
    m_displayedTrack = id;
    m_displayedState = int(manager->state());
    QString status;
    switch (manager->state()) {
    case MusicPlaybackManager::Loading: status = tr("Loading"); break;
    case MusicPlaybackManager::Playing: status = tr("Playing"); break;
    case MusicPlaybackManager::Paused: status = tr("Paused"); break;
    case MusicPlaybackManager::Error: status = tr("Error"); break;
    default: break;
    }
    for (int i = 0; i < m_tracks.size(); ++i)
        m_table->topLevelItem(i)->setText(0, m_tracks[i].id == id ? status : QString());
}
