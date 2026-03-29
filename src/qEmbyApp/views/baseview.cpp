#include "baseview.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h> 
#include <models/profile/serverprofile.h>
#include <config/config_keys.h>
#include <config/configstore.h>
#include "../components/mediaactionmenu.h"
#include "../components/moderntoast.h" 
#include "../managers/playbackmanager.h" 
#include <QDebug>
#include <QPointer> 

BaseView::BaseView(QEmbyCore* core, QWidget *parent)
    : QWidget(parent), m_core(core)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("showGlobalSearch", true);

    connect(this, &BaseView::_internalFavoriteTask, this, [this](MediaItem item) -> QCoro::Task<void> {
        
        QPointer<BaseView> guard(this);
        try {
            
            bool targetState = !item.isFavorite();
            bool actualState = co_await m_core->mediaService()->toggleFavorite(item.id, targetState);
            
            if (!guard) co_return; 

            
            item.userData.isFavorite = actualState;
            
            
            guard->onMediaItemUpdated(item); 
            ModernToast::showMessage(actualState ? tr("Added to Favorites") : tr("Removed from Favorites"));
        } catch (const std::exception& e) {
            qDebug() << "BaseView toggle favorite failed:" << e.what();
            ModernToast::showMessage(tr("Failed to update favorite status"));
        }
    });

    
    connect(this, &BaseView::_internalPlayTask, this, [this](MediaItem item) -> QCoro::Task<void> {
        if (!m_core || !m_core->mediaService()) {
            co_return;
        }

        QPointer<BaseView> safeThis(this);

        try {
            
            MediaItem detail = item;

            
            if (detail.type == "Series") {
                
                QList<MediaItem> nextUpList = co_await m_core->mediaService()->getNextUp(detail.id);
                if (!safeThis) co_return;

                if (!nextUpList.isEmpty()) {
                    detail = nextUpList.first();
                } else {
                    
                    QList<MediaItem> seasons = co_await m_core->mediaService()->getSeasons(detail.id);
                    if (!safeThis) co_return;

                    if (!seasons.isEmpty()) {
                        QList<MediaItem> episodes = co_await m_core->mediaService()->getEpisodes(
                            detail.id, seasons.first().id);
                        if (!safeThis) co_return;

                        if (!episodes.isEmpty()) {
                            detail = episodes.first();
                        } else {
                            qDebug() << "Series has no episodes, cannot play.";
                            co_return;
                        }
                    } else {
                        qDebug() << "Series has no seasons, cannot play.";
                        co_return;
                    }
                }
            }

            if (detail.mediaSources.isEmpty()) {
                detail = co_await m_core->mediaService()->getItemDetail(detail.id);
            }

            if (!safeThis) co_return; 

            
            QString playTitle = detail.name;
            if (detail.type == "Episode" && !detail.seriesName.isEmpty()) {
                playTitle = QString("%1 - S%2E%3 - %4")
                    .arg(detail.seriesName)
                    .arg(detail.parentIndexNumber, 2, 10, QChar('0'))
                    .arg(detail.indexNumber, 2, 10, QChar('0'))
                    .arg(detail.name);
            }

            
            QString mediaSourceId = detail.id;
            QVariant sourceInfoVar;
            QString streamUrl;

            if (!detail.mediaSources.isEmpty()) {
                
                int bestSourceIdx = 0;
                if (detail.mediaSources.size() > 1) {
                    QString versionPref = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed();
                    if (!versionPref.isEmpty()) {
                        QStringList keywords = versionPref.split(',', Qt::SkipEmptyParts);
                        for (const QString& kw : keywords) {
                            QString keyword = kw.trimmed();
                            if (keyword.isEmpty()) continue;
                            bool found = false;
                            for (int i = 0; i < detail.mediaSources.size(); ++i) {
                                if (detail.mediaSources[i].name.contains(keyword, Qt::CaseInsensitive)) {
                                    bestSourceIdx = i;
                                    found = true;
                                    break;
                                }
                            }
                            if (found) break;
                        }
                    }
                }

                MediaSourceInfo selectedSource = detail.mediaSources[bestSourceIdx];

                
                QString prefAudioLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto");
                QString prefSubLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");

                {
                    int bestAudioIdx = -1;
                    int bestSubIdx = -1;
                    int firstSubIdx = -1;   
                    bool hasDefaultSub = false; 
                    bool audioFound = false, subFound = false;

                    for (auto& stream : selectedSource.mediaStreams) {
                        if (stream.type == "Audio") {
                            if (!audioFound && prefAudioLang != "auto" &&
                                stream.language.compare(prefAudioLang, Qt::CaseInsensitive) == 0) {
                                bestAudioIdx = stream.index;
                                audioFound = true;
                            }
                        } else if (stream.type == "Subtitle") {
                            if (firstSubIdx < 0) firstSubIdx = stream.index;
                            if (stream.isDefault) hasDefaultSub = true;
                            if (!subFound && prefSubLang != "auto" && prefSubLang != "none" &&
                                stream.language.compare(prefSubLang, Qt::CaseInsensitive) == 0) {
                                bestSubIdx = stream.index;
                                subFound = true;
                            }
                        }
                    }

                    
                    if (bestAudioIdx >= 0) {
                        for (auto& stream : selectedSource.mediaStreams) {
                            if (stream.type == "Audio")
                                stream.isDefault = (stream.index == bestAudioIdx);
                        }
                    }

                    
                    if (prefSubLang == "none") {
                        
                        for (auto& stream : selectedSource.mediaStreams) {
                            if (stream.type == "Subtitle")
                                stream.isDefault = false;
                        }
                    } else if (bestSubIdx >= 0) {
                        
                        for (auto& stream : selectedSource.mediaStreams) {
                            if (stream.type == "Subtitle")
                                stream.isDefault = (stream.index == bestSubIdx);
                        }
                    } else if (firstSubIdx >= 0 && !hasDefaultSub) {
                        
                        for (auto& stream : selectedSource.mediaStreams) {
                            if (stream.type == "Subtitle")
                                stream.isDefault = (stream.index == firstSubIdx);
                        }
                    }
                    
                }

                mediaSourceId = selectedSource.id;
                sourceInfoVar = QVariant::fromValue(selectedSource);
                streamUrl = safeThis->m_core->mediaService()->getStreamUrl(detail.id, selectedSource);
            } else {
                
                streamUrl = safeThis->m_core->mediaService()->getStreamUrl(detail.id, mediaSourceId);
            }

            
            long long ticks = detail.userData.playbackPositionTicks;

            
            Q_EMIT safeThis->navigateToPlayer(detail.id, playTitle, streamUrl, ticks, sourceInfoVar);

        } catch (const std::exception& e) {
            qDebug() << "BaseView play task failed:" << e.what();
        }
    });


    
    
    
    
    connect(this, &BaseView::_internalMarkPlayedTask, this, [this](MediaItem item) -> QCoro::Task<void> {
        QPointer<BaseView> safeThis(this);
        try {
            
            co_await m_core->mediaService()->markAsPlayed(item.id);

            bool isJellyfin = false;
            if (m_core && m_core->serverManager() && m_core->serverManager()->activeProfile().isValid()) {
                isJellyfin = (m_core->serverManager()->activeProfile().type == ServerProfile::Jellyfin);
            }
            if (!isJellyfin)
            {
                
                co_await m_core->mediaService()->removeFromResume(item.id);
            }

            if (!safeThis) co_return;
            
            
            item.userData.played = true;
            item.userData.playbackPositionTicks = 0;
            item.userData.playedPercentage = 100.0;
            
            safeThis->onMediaItemUpdated(item);

            
            co_await safeThis->refreshAndBroadcastItem(item.id);

            ModernToast::showMessage(tr("Marked as Played"));
        } catch (const std::exception& e) {
            qDebug() << "BaseView mark as played failed:" << e.what();
            ModernToast::showMessage(tr("Operation failed"));
        }
    });

    connect(this, &BaseView::_internalMarkUnplayedTask, this, [this](MediaItem item) -> QCoro::Task<void> {
        QPointer<BaseView> safeThis(this);
        try {
            co_await m_core->mediaService()->markAsUnplayed(item.id);
            if (!safeThis) co_return;
            
            
            item.userData.played = false;
            item.userData.playbackPositionTicks = 0;
            item.userData.playedPercentage = 0.0;
            safeThis->onMediaItemUpdated(item);

            
            co_await safeThis->refreshAndBroadcastItem(item.id);
            
            ModernToast::showMessage(tr("Marked as Unplayed"));
        } catch (const std::exception& e) {
            qDebug() << "BaseView mark as unplayed failed:" << e.what();
            ModernToast::showMessage(tr("Operation failed"));
        }
    });

    connect(this, &BaseView::_internalRemoveFromResumeTask, this, [this](MediaItem item) -> QCoro::Task<void> {
        QPointer<BaseView> safeThis(this);
        try {
            co_await m_core->mediaService()->removeFromResume(item.id);
            if (!safeThis) co_return;
            
            
            item.userData.playbackPositionTicks = 0;
            item.userData.playedPercentage = 0.0;
            safeThis->onMediaItemUpdated(item);

            
            
            co_await safeThis->refreshAndBroadcastItem(item.id);
            
            ModernToast::showMessage(tr("Removed from Continue Watching"));
        } catch (const std::exception& e) {
            qDebug() << "BaseView remove from resume failed:" << e.what();
            ModernToast::showMessage(tr("Operation failed"));
        }
    });

    
    
    
    connect(this, &BaseView::_internalExternalPlayTask, this, [this](MediaItem item, QString playerPath) -> QCoro::Task<void> {
        if (!m_core || !m_core->mediaService()) {
            co_return;
        }

        QPointer<BaseView> safeThis(this);

        try {
            MediaItem detail = item;

            
            if (detail.type == "Series") {
                QList<MediaItem> nextUpList = co_await m_core->mediaService()->getNextUp(detail.id);
                if (!safeThis) co_return;

                if (!nextUpList.isEmpty()) {
                    detail = nextUpList.first();
                } else {
                    QList<MediaItem> seasons = co_await m_core->mediaService()->getSeasons(detail.id);
                    if (!safeThis) co_return;

                    if (!seasons.isEmpty()) {
                        QList<MediaItem> episodes = co_await m_core->mediaService()->getEpisodes(
                            detail.id, seasons.first().id);
                        if (!safeThis) co_return;

                        if (!episodes.isEmpty()) {
                            detail = episodes.first();
                        } else {
                            co_return;
                        }
                    } else {
                        co_return;
                    }
                }
            }

            if (detail.mediaSources.isEmpty()) {
                detail = co_await m_core->mediaService()->getItemDetail(detail.id);
            }
            if (!safeThis) co_return;

            QString playTitle = detail.name;
            if (detail.type == "Episode" && !detail.seriesName.isEmpty()) {
                playTitle = QString("%1 - S%2E%3 - %4")
                    .arg(detail.seriesName)
                    .arg(detail.parentIndexNumber, 2, 10, QChar('0'))
                    .arg(detail.indexNumber, 2, 10, QChar('0'))
                    .arg(detail.name);
            }

            if (!detail.mediaSources.isEmpty()) {
                int bestSourceIdx = 0;
                if (detail.mediaSources.size() > 1) {
                    QString versionPref = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed();
                    if (!versionPref.isEmpty()) {
                        QStringList keywords = versionPref.split(',', Qt::SkipEmptyParts);
                        for (const QString& kw : keywords) {
                            QString keyword = kw.trimmed();
                            if (keyword.isEmpty()) continue;
                            bool found = false;
                            for (int i = 0; i < detail.mediaSources.size(); ++i) {
                                if (detail.mediaSources[i].name.contains(keyword, Qt::CaseInsensitive)) {
                                    bestSourceIdx = i;
                                    found = true;
                                    break;
                                }
                            }
                            if (found) break;
                        }
                    }
                }

                MediaSourceInfo selectedSource = detail.mediaSources[bestSourceIdx];
                QString streamUrl = safeThis->m_core->mediaService()->getStreamUrl(detail.id, selectedSource);
                long long ticks = detail.userData.playbackPositionTicks;

                PlaybackManager::instance()->startExternalPlayback(
                    playerPath, detail.id, playTitle, streamUrl, ticks,
                    QVariant::fromValue(selectedSource));
            }
        } catch (const std::exception& e) {
            qDebug() << "BaseView external play task failed:" << e.what();
        }
    });
}

void BaseView::handlePlayRequested(const MediaItem& item)
{
    
    Q_EMIT _internalPlayTask(item);
}

void BaseView::handleFavoriteRequested(const MediaItem& item)
{
    
    Q_EMIT _internalFavoriteTask(item);
}


void BaseView::handleMarkPlayedRequested(const MediaItem& item)
{
    Q_EMIT _internalMarkPlayedTask(item);
}

void BaseView::handleMarkUnplayedRequested(const MediaItem& item)
{
    Q_EMIT _internalMarkUnplayedTask(item);
}

void BaseView::handleRemoveFromResumeRequested(const MediaItem& item)
{
    Q_EMIT _internalRemoveFromResumeTask(item);
}

void BaseView::handleMoreMenuRequested(const MediaItem& item, const QPoint& globalPos)
{
    
    MediaActionMenu menu(item, m_core, this);

    
    connect(&menu, &MediaActionMenu::playRequested, this, [this, item]() {
        handlePlayRequested(item);
    });

    connect(&menu, &MediaActionMenu::externalPlayRequested, this, [this, item](const QString &playerPath) {
        
        Q_EMIT _internalExternalPlayTask(item, playerPath);
    });

    connect(&menu, &MediaActionMenu::favoriteRequested, this, [this, item]() {
        handleFavoriteRequested(item);
    });

    connect(&menu, &MediaActionMenu::detailRequested, this, [this, item]() {
        
        Q_EMIT navigateToDetail(item.id, item.name);
    });

    
    connect(&menu, &MediaActionMenu::markPlayedRequested, this, [this, item]() {
        handleMarkPlayedRequested(item);
    });

    connect(&menu, &MediaActionMenu::markUnplayedRequested, this, [this, item]() {
        handleMarkUnplayedRequested(item);
    });

    connect(&menu, &MediaActionMenu::removeFromResumeRequested, this, [this, item]() {
        handleRemoveFromResumeRequested(item);
    });

    
    menu.exec(globalPos);
}




QCoro::Task<void> BaseView::refreshAndBroadcastItem(const QString& itemId)
{
    if (itemId.isEmpty() || !m_core || !m_core->mediaService()) {
        co_return;
    }

    
    QPointer<BaseView> safeThis(this);

    try {
        
        MediaItem latestItem = co_await m_core->mediaService()->getItemDetail(itemId);
        
        
        if (safeThis) {
            safeThis->onMediaItemUpdated(latestItem);
        }
    } catch (const std::exception& e) {
        qDebug() << "BaseView::refreshAndBroadcastItem failed for ID:" << itemId << "Error:" << e.what();
    }
}
