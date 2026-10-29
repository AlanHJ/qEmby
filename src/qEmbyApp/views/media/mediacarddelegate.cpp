#include "mediacarddelegate.h"
#include "medialistmodel.h"
#include "../../utils/artworkpolicyutils.h"

#include "../../managers/thememanager.h"
#include <QAbstractItemView>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include "../../utils/imageutils.h"
#include "../../utils/mediaitemutils.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include <QStyle>

namespace
{

enum class HoverOverlayMode
{
    None,
    FullControls,
    FavoriteAndMenu,
    MenuOnly
};

bool isPlayableItem(const MediaItem &item)
{
    return item.type == "MusicAlbum" || item.type == "Audio" || item.type == "TvChannel" ||
           item.mediaType == "Video" || item.type == "Movie" || item.type == "Episode" || item.type == "Video" ||
           item.type == "MusicVideo";
}

bool isPlaylistItem(const MediaItem &item)
{
    return item.type == "Playlist";
}

bool canShowMenuOnlyButtonForTile(const MediaItem &item)
{
    if (item.id.trimmed().isEmpty())
    {
        return false;
    }

    const QString normalizedCollectionType = item.collectionType.trimmed().toLower();
    const QString normalizedType = item.type.trimmed().toLower();
    return normalizedCollectionType != "playlists" && normalizedCollectionType != "boxsets" &&
           normalizedType != "playlist" && normalizedType != "boxset";
}

HoverOverlayMode hoverOverlayModeForItem(const MediaItem &item, MediaCardDelegate::CardStyle style,
                                         bool showMoreButtonForNonPlayableTiles)
{
    if (item.type == "Person")
    {
        return item.id.trimmed().isEmpty() ? HoverOverlayMode::None : HoverOverlayMode::FavoriteAndMenu;
    }

    if (style == MediaCardDelegate::EpisodeList)
    {
        return isPlayableItem(item) ? HoverOverlayMode::FullControls : HoverOverlayMode::None;
    }

    if (isPlayableItem(item) || item.type == "Series" || item.type == "Season")
    {
        return HoverOverlayMode::FullControls;
    }

    if (isPlaylistItem(item))
    {
        return HoverOverlayMode::MenuOnly;
    }

    if (style == MediaCardDelegate::LibraryTile && showMoreButtonForNonPlayableTiles &&
        canShowMenuOnlyButtonForTile(item))
    {
        return HoverOverlayMode::MenuOnly;
    }

    return HoverOverlayMode::None;
}

QRect baseImageRectForCard(MediaCardDelegate::CardStyle style, const QRect &rect, int padding)
{
    const int imgWidth = rect.width() - padding * 2;

    if (style == MediaCardDelegate::LibraryTile)
    {
        const int imgHeight = qRound(imgWidth * 9.0 / 16.0);
        return QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
    }

    const int imgHeight = style == MediaCardDelegate::AlbumCover ? imgWidth : qRound(imgWidth * 1.5);
    return QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
}

QRect hoverImageRect(const QRect &baseImgRect)
{
    const int expandW = qRound(baseImgRect.width() * 0.035);
    const int expandH = qRound(baseImgRect.height() * 0.035);
    return baseImgRect.adjusted(-expandW, -expandH, expandW, expandH);
}

QRect centerPlayButtonRect(const QRect &targetImgRect)
{
    const int playSize = 48;
    return QRect(targetImgRect.center().x() - playSize / 2, targetImgRect.center().y() - playSize / 2, playSize,
                 playSize);
}

QRect moreButtonRect(const QRect &targetImgRect)
{
    const int btnWidth = 24;
    const int btnHeight = 24;
    return QRect(targetImgRect.right() - btnWidth - 8, targetImgRect.bottom() - btnHeight - 8, btnWidth, btnHeight);
}

QRect favoriteButtonRect(const QRect &moreRect)
{
    const int btnWidth = 24;
    const int spacing = 5;
    return QRect(moreRect.left() - btnWidth - spacing, moreRect.top(), btnWidth, moreRect.height());
}

bool shouldShowPlayButton(HoverOverlayMode overlayMode, MediaCardDelegate::HoverControls controls)
{
    return overlayMode == HoverOverlayMode::FullControls && controls.testFlag(MediaCardDelegate::HoverControlPlay);
}

bool shouldShowFavoriteButton(HoverOverlayMode overlayMode, MediaCardDelegate::HoverControls controls)
{
    return (overlayMode == HoverOverlayMode::FullControls || overlayMode == HoverOverlayMode::FavoriteAndMenu) &&
           controls.testFlag(MediaCardDelegate::HoverControlFavorite);
}

bool shouldShowMoreButton(HoverOverlayMode overlayMode, MediaCardDelegate::HoverControls controls)
{
    return overlayMode != HoverOverlayMode::None && controls.testFlag(MediaCardDelegate::HoverControlMore);
}

struct HoverButtonLayout
{
    QRect playRect;
    QRect favoriteRect;
    QRect moreRect;
};

HoverButtonLayout buildHoverButtonLayout(const QRect &targetImgRect, bool showPlayButton, bool showFavoriteButton,
                                         bool showMoreButton)
{
    HoverButtonLayout layout;
    if (showPlayButton)
    {
        layout.playRect = centerPlayButtonRect(targetImgRect);
    }

    const QRect trailingButtonRect = moreButtonRect(targetImgRect);
    if (showMoreButton)
    {
        layout.moreRect = trailingButtonRect;
    }
    if (showFavoriteButton)
    {
        layout.favoriteRect = showMoreButton ? favoriteButtonRect(trailingButtonRect) : trailingButtonRect;
    }

    return layout;
}

void drawPlayedBadge(QPainter *painter, const QRect &targetImgRect)
{
    const int checkSize = 16;
    const int checkMargin = 4;
    const QRect checkRect(targetImgRect.left() + checkMargin,
                          targetImgRect.top() + checkMargin, checkSize, checkSize);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(16, 185, 129, 140));
    painter->drawRoundedRect(checkRect, checkSize / 2, checkSize / 2);

    QIcon checkIcon(":/svg/dark/check.svg");
    if (!checkIcon.isNull())
    {
        checkIcon.paint(painter, checkRect.adjusted(3, 3, -3, -3), Qt::AlignCenter);
    }

    painter->restore();
}

} 


MediaCardThemeHelper::MediaCardThemeHelper(QWidget *parent)
    : QWidget(parent), m_hoverBg(229, 231, 235), 
      m_selectedBg(219, 234, 254), m_titleColor(31, 41, 55), m_subTitleColor(107, 114, 128),
      m_placeholderBg(229, 231, 235), m_placeholderTextColor(156, 163, 175),
      m_progressTrackColor(203, 213, 225), m_progressFillColor(22, 163, 74),
      m_ratingBackgroundColor(15, 23, 42, 140), m_ratingTextColor(Qt::white),
      m_hoverButtonBackgroundColor(0, 0, 0, 140)
{
    hide(); 
}



MediaCardDelegate::MediaCardDelegate(CardStyle style, QObject *parent)
    : QStyledItemDelegate(parent), m_style(style), m_tileSize(160, 270), m_themeHelper(nullptr)
{
    m_showPlaybackProgress = ConfigStore::instance()->get<bool>(
        ConfigKeys::ShowMediaPlaybackProgress, true);
    m_showCoverRating = ConfigStore::instance()->get<bool>(
        ConfigKeys::ShowCoverRating, true);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &value) {
                if (key == QLatin1String(ConfigKeys::ShowMediaPlaybackProgress))
                    m_showPlaybackProgress = value.toBool();
                else if (key == QLatin1String(ConfigKeys::ShowCoverRating))
                    m_showCoverRating = value.toBool();
                else
                    return;
                for (QWidget *viewport : m_installedViewports)
                    viewport->update();
            });
}

QSize MediaCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    if (m_style == Poster && effectiveStyle(index) == LibraryTile)
        return QSize(qMax(m_tileSize.width(), qRound((m_tileSize.height() - 48) * 16.0 / 9.0)), m_tileSize.height());
    if (m_style == LibraryTile && effectiveStyle(index) == Poster)
        return QSize(qMax(80, qRound((m_tileSize.height() - 60) / 1.5) + 16), m_tileSize.height());
    
    return m_tileSize;
}

MediaCardDelegate::CardGeometry MediaCardDelegate::cardGeometry(const QRect &rect) const
{
    return cardGeometry(rect, m_style);
}

MediaCardDelegate::CardStyle MediaCardDelegate::effectiveStyle(const QModelIndex& index) const
{
    if (!index.isValid()) return m_style;
    const auto item = index.data(MediaListModel::ItemDataRole).value<MediaItem>();
    if (item.type == QLatin1String("MusicAlbum")) return AlbumCover;
    if (item.type == QLatin1String("Episode") || item.type == QLatin1String("Season") ||
        item.type == QLatin1String("TvChannel")) return m_style;
    if ((m_style == Poster || m_style == LibraryTile) && index.model()->property("variableArtworkLayout").toBool()) {
        const int layout = index.data(MediaListModel::ArtworkLayoutRole).toInt();
        if (layout == int(ArtworkPolicyUtils::Layout::Landscape)) return LibraryTile;
        if (layout == int(ArtworkPolicyUtils::Layout::Portrait)) return Poster;
    }
    return m_style;
}

MediaCardDelegate::CardGeometry MediaCardDelegate::cardGeometry(const QRect &rect, CardStyle style) const
{
    CardGeometry geometry;
    geometry.image = baseImageRectForCard(style, rect, m_contentPadding);
    if (style == AlbumCover) geometry.image.setHeight(geometry.image.width());
    const int titleGap = style == Cast ? 5 : 4;
    geometry.title = QRect(rect.x() + m_contentPadding, geometry.image.bottom() + titleGap,
                           geometry.image.width(), qMax(16, m_titleFontPixelSize + 6));
    geometry.subtitle = QRect(geometry.title.left(), geometry.title.bottom() + 1,
                              geometry.image.width(), qMax(14, m_subTitleFontPixelSize + 4));
    return geometry;
}

int MediaCardDelegate::contentBottomInset() const
{
    if (m_style == EpisodeList)
        return 0;

    const QRect cardRect(QPoint(0, 0), m_tileSize);
    const CardGeometry geometry = cardGeometry(cardRect);
    
    const QRect lastRow = (m_style == Cast || geometry.subtitle.bottom() <= cardRect.bottom())
                              ? geometry.subtitle : geometry.title;
    return qMax(0, cardRect.bottom() - lastRow.bottom());
}


bool MediaCardDelegate::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick)
    {

        QWidget *viewport = qobject_cast<QWidget *>(object);
        if (viewport)
        {
            
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(viewport->parentWidget());
            if (view)
            {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
                QPoint pos = mouseEvent->pos();
                QModelIndex index = view->indexAt(pos); 

                if (index.isValid())
                {
                    const CardStyle style = effectiveStyle(index);
                    MediaItem item = index.data(MediaListModel::ItemDataRole).value<MediaItem>();
                    const HoverOverlayMode overlayMode =
                        hoverOverlayModeForItem(item, style, m_showMoreButtonForNonPlayableTiles);

                    if (overlayMode != HoverOverlayMode::None)
                    {
                        
                        QRect rect = view->visualRect(index);

                        
                        
                        
                        if (style == EpisodeList)
                        {
                            int padding = 10;
                            int imgWidth = 250;
                            int imgHeight = 140;
                            QRect baseImgRect(rect.x() + padding, rect.y() + (rect.height() - imgHeight) / 2, imgWidth,
                                              imgHeight);

                            
                            int favBtnSize = 24;
                            int rightMargin = 20;
                            QRect favRect(rect.right() - rightMargin - favBtnSize, rect.center().y() - favBtnSize / 2,
                                          favBtnSize, favBtnSize);

                            const bool showFavoriteButton = m_hoverControls.testFlag(HoverControlFavorite);
                            bool inFav = showFavoriteButton && favRect.contains(pos);

                            
                            int playSize = 48;
                            QRect playRectCenter(baseImgRect.center().x() - playSize / 2,
                                                 baseImgRect.center().y() - playSize / 2, playSize, playSize);
                            const bool showPlayButton = m_hoverControls.testFlag(HoverControlPlay);
                            bool inThumbPlay = showPlayButton && playRectCenter.contains(pos);

                            if (inFav || inThumbPlay)
                            {
                                if (event->type() == QEvent::MouseButtonRelease)
                                {
                                    if (inThumbPlay)
                                        emit playRequested(item);
                                    else if (inFav)
                                        emit favoriteRequested(item);
                                }
                                return true;
                            }
                        }
                        else
                        {
                            const QRect baseImgRect = baseImageRectForCard(style, rect, m_contentPadding);
                            const QRect targetImgRect = hoverImageRect(baseImgRect);
                            const bool showPlayButton = shouldShowPlayButton(overlayMode, m_hoverControls);
                            const bool showFavoriteButton = shouldShowFavoriteButton(overlayMode, m_hoverControls);
                            const bool showMoreButton = shouldShowMoreButton(overlayMode, m_hoverControls);

                            if (!showPlayButton && !showFavoriteButton && !showMoreButton)
                            {
                                return QStyledItemDelegate::eventFilter(object, event);
                            }

                            const HoverButtonLayout buttonLayout = buildHoverButtonLayout(
                                targetImgRect, showPlayButton, showFavoriteButton, showMoreButton);

                            
                            const bool inPlay = showPlayButton && buttonLayout.playRect.contains(pos);
                            const bool inFav = showFavoriteButton && buttonLayout.favoriteRect.contains(pos);
                            const bool inMore = showMoreButton && buttonLayout.moreRect.contains(pos);

                            if (inPlay || inFav || inMore)
                            {
                                
                                if (event->type() == QEvent::MouseButtonRelease)
                                {
                                    if (inPlay)
                                        emit playRequested(item);
                                    else if (inFav)
                                        emit favoriteRequested(item);
                                    else if (inMore)
                                        emit moreMenuRequested(item, mouseEvent->globalPosition().toPoint());
                                }
                                return true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return QStyledItemDelegate::eventFilter(object, event);
}

void MediaCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const CardStyle style = effectiveStyle(index);
    
    if (option.widget)
    {
        
        QAbstractItemView *view = qobject_cast<QAbstractItemView *>(const_cast<QWidget *>(option.widget));
        if (view)
        {
            QWidget *viewport = view->viewport(); 
            if (!m_installedViewports.contains(viewport))
            {
                viewport->installEventFilter(const_cast<MediaCardDelegate *>(this));
                m_installedViewports.insert(viewport);

                connect(viewport, &QObject::destroyed, const_cast<MediaCardDelegate *>(this),
                        [this, viewport]() { m_installedViewports.remove(viewport); });
            }
        }
    }

    if (!m_themeHelper && option.widget)
    {
        m_themeHelper = new MediaCardThemeHelper(const_cast<QWidget *>(option.widget));
        m_themeHelper->setObjectName("media-card-theme");
        option.widget->style()->polish(m_themeHelper); 
    }

    
    QColor selectedBg = m_themeHelper ? m_themeHelper->selectedBg() : QColor(219, 234, 254);
    QColor titleColor = m_themeHelper ? m_themeHelper->titleColor() : QColor(31, 41, 55);
    QColor subTitleColor = m_themeHelper ? m_themeHelper->subTitleColor() : QColor(107, 114, 128);
    QColor placeholderBg = m_themeHelper ? m_themeHelper->placeholderBg() : QColor(229, 231, 235);
    QColor placeholderTextColor = m_themeHelper ? m_themeHelper->placeholderTextColor() : QColor(156, 163, 175);

    
    bool isDarkTheme = ThemeManager::instance()->isDarkMode();

    
    const qreal fontScale = ThemeManager::fontScaleFactor();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true); 

    QRect rect = option.rect;
    MediaItem item = index.data(MediaListModel::ItemDataRole).value<MediaItem>();
    QPixmap poster = index.data(MediaListModel::PosterPixmapRole).value<QPixmap>();
    const double progressPercentage = MediaItemUtils::playbackProgressPercentage(item);
    const bool showPlaybackProgress =
        m_showPlaybackProgress && style != Cast && progressPercentage > 0.0;
    const auto drawPlaybackProgress = [&](const QRect &imageRect) {
        if (!showPlaybackProgress || imageRect.isEmpty())
            return;

        constexpr qreal barHeight = 4.0;
        const QRectF track(imageRect.left(), imageRect.bottom() + 1 - barHeight,
                           imageRect.width(), barHeight);
        
        const qreal fillWidth = qMin(track.width(),
                                     qMax(track.width() * progressPercentage / 100.0, 10.0));
        QPainterPath imageClip;
        imageClip.addRoundedRect(QRectF(imageRect), 8, 8);
        painter->save();
        painter->setClipPath(imageClip, Qt::IntersectClip);
        painter->setOpacity(m_themeHelper ? m_themeHelper->progressOpacity() : 0.65);
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_themeHelper ? m_themeHelper->progressTrackColor() : QColor(203, 213, 225));
        painter->drawRect(QRectF(track.left() + fillWidth, track.top(),
                                 track.width() - fillWidth, track.height()));
        painter->setBrush(m_themeHelper ? m_themeHelper->progressFillColor() : QColor(22, 163, 74));
        painter->drawRect(QRectF(track.topLeft(), QSizeF(fillWidth, track.height())));
        painter->restore();
    };
    bool isHovered = option.state & QStyle::State_MouseOver;
    const HoverOverlayMode overlayMode = hoverOverlayModeForItem(item, style, m_showMoreButtonForNonPlayableTiles);
    const bool showPlayButton = shouldShowPlayButton(overlayMode, m_hoverControls);
    const bool showFavoriteButton = shouldShowFavoriteButton(overlayMode, m_hoverControls);
    const bool showMoreButton = shouldShowMoreButton(overlayMode, m_hoverControls);
    const bool hasHoverButtons = showPlayButton || showFavoriteButton || showMoreButton;
    const bool showCoverRating = m_showCoverRating && style != Cast &&
                                 item.communityRating > 0.0;
    const auto drawCoverRating = [&](const QRect &imageRect) {
        if (!showCoverRating || imageRect.isEmpty())
            return;

        QFont ratingFont = option.font;
        ratingFont.setPixelSize(qMax(9, qRound(11 * fontScale)));
        ratingFont.setBold(true);
        const QString ratingText = QString::number(item.communityRating, 'f', 1);
        const QFontMetrics metrics(ratingFont);
        const int badgeWidth = metrics.horizontalAdvance(ratingText) + 24;
        const QRect trailingButtonRect = moreButtonRect(imageRect);
        const QRect badgeRect(imageRect.left() + 5,
                              trailingButtonRect.top() + (trailingButtonRect.height() - 20) / 2,
                              badgeWidth, 20);
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_themeHelper ? m_themeHelper->ratingBackgroundColor()
                                        : QColor(15, 23, 42, 140));
        painter->drawRoundedRect(badgeRect, 6, 6);
        const QIcon starIcon(isDarkTheme ? ":/svg/dark/star-filled.svg"
                                         : ":/svg/light/star-filled.svg");
        starIcon.paint(painter, QRect(badgeRect.left() + 5,
                                     badgeRect.top() + 4, 12, 12));
        painter->setFont(ratingFont);
        painter->setPen(m_themeHelper ? m_themeHelper->ratingTextColor() : Qt::white);
        painter->drawText(badgeRect.adjusted(20, 0, -4, 0),
                          Qt::AlignCenter, ratingText);
        painter->restore();
    };

    
    
    
    if (style == EpisodeList)
    {
        if (isHovered)
        {
            
            painter->setPen(Qt::NoPen);
            painter->setBrush(isDarkTheme ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 15));
            painter->drawRoundedRect(rect.adjusted(5, 5, -5, -5), 8, 8);
        }

        int padding = 10;
        int imgWidth = 250;
        int imgHeight = 140;
        QRect baseImgRect(rect.x() + padding, rect.y() + (rect.height() - imgHeight) / 2, imgWidth, imgHeight);

        int imgRadius = 8;
        painter->setPen(Qt::NoPen);
        painter->setBrush(placeholderBg);
        painter->drawRoundedRect(baseImgRect, imgRadius, imgRadius);

        if (!poster.isNull())
        {
            QPainterPath path;
            path.addRoundedRect(baseImgRect, imgRadius, imgRadius);
            painter->setClipPath(path);
            QPixmap scaled = ImageUtils::scaledCoverPixmap(poster, baseImgRect.size(), painter->device()->devicePixelRatioF());
            qreal px = baseImgRect.x() + (baseImgRect.width() - scaled.deviceIndependentSize().width()) / 2;
            qreal py = baseImgRect.y() + (baseImgRect.height() - scaled.deviceIndependentSize().height()) / 2;
            painter->drawPixmap(QPointF(px, py), scaled);
            painter->setClipping(false);
        }
        else
        {
            painter->setPen(placeholderTextColor);
            painter->drawText(baseImgRect, Qt::AlignCenter, tr("No Image"));
        }

        if (isHovered && showPlayButton)
        {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 70));
            painter->drawRoundedRect(baseImgRect, imgRadius, imgRadius);

            int playSize = 48;
            QRect playRectCenter(baseImgRect.center().x() - playSize / 2, baseImgRect.center().y() - playSize / 2,
                                 playSize, playSize);
            painter->setBrush(QColor(0, 0, 0, 160));
            painter->drawEllipse(playRectCenter);
            QIcon playIcon(":/svg/player/play.svg");
            if (!playIcon.isNull())
            {
                playIcon.paint(painter, playRectCenter.adjusted(12, 12, -12, -12), Qt::AlignCenter);
            }
            else
            {
                QPainterPath playPath;
                playPath.moveTo(playRectCenter.center().x() - 4, playRectCenter.center().y() - 8);
                playPath.lineTo(playRectCenter.center().x() + 10, playRectCenter.center().y());
                playPath.lineTo(playRectCenter.center().x() - 4, playRectCenter.center().y() + 8);
                playPath.closeSubpath();
                painter->setBrush(Qt::white);
                painter->drawPath(playPath);
            }
        }

        if (item.userData.played)
        {
            drawPlayedBadge(painter, baseImgRect);
        }
        drawPlaybackProgress(baseImgRect);
        drawCoverRating(baseImgRect);

        
        int textX = baseImgRect.right() + 20;
        int favBtnSize = 24;
        int rightMargin = 20;
        int btnAreaWidth = favBtnSize + rightMargin + 10;
        int textWidth = rect.right() - textX - btnAreaWidth - 10;

        QRect titleRect(textX, baseImgRect.top() + 5, textWidth, 24);
        QRect metaRect(textX, titleRect.bottom() + 5, textWidth, 18);
        QRect descRect(textX, metaRect.bottom() + 10, textWidth, baseImgRect.bottom() - metaRect.bottom() - 10);

        QFont titleFont = option.font;
        titleFont.setPixelSize(qMax(8, qRound(16 * fontScale)));
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(titleColor);
        QString epTitle = QString("%1. %2").arg(item.indexNumber).arg(item.name);
        QFontMetrics fmTitle(titleFont);
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                          fmTitle.elidedText(epTitle, Qt::ElideRight, titleRect.width()));

        QFont metaFont = option.font;
        metaFont.setPixelSize(qMax(8, qRound(13 * fontScale)));
        painter->setFont(metaFont);
        painter->setPen(subTitleColor);
        QStringList metas;
        if (!item.premiereDate.isEmpty())
        {
            const int len = item.premiereDate.length();
            if (len > 0)
                metas << item.premiereDate.left(qMin(len, 10));
        }
        if (item.runTimeTicks > 0)
        {
            long long mins = item.runTimeTicks / 10000000 / 60;
            metas << QString(tr("%1 min")).arg(mins);
        }
        painter->drawText(metaRect, Qt::AlignLeft | Qt::AlignVCenter, metas.join(" • "));

        QFont descFont = option.font;
        descFont.setPixelSize(qMax(8, qRound(13 * fontScale)));
        painter->setFont(descFont);
        painter->setPen(subTitleColor);

        QString desc = item.overview;
        desc.replace('\n', ' ');
        QFontMetrics fmDesc(descFont);
        int availableHeight = descRect.height();
        int lineSpacing = fmDesc.lineSpacing();
        int maxLines = availableHeight / lineSpacing;
        if (maxLines < 1)
            maxLines = 1;

        QString elidedDesc = fmDesc.elidedText(desc, Qt::ElideRight, descRect.width() * maxLines);
        painter->drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, elidedDesc);

        
        if (showFavoriteButton)
        {
            QRect favRect(rect.right() - rightMargin - favBtnSize, rect.center().y() - favBtnSize / 2, favBtnSize,
                          favBtnSize);
            QString themeFolder = isDarkTheme ? "dark" : "light";
            QString favIconPath = item.isFavorite() ? QString(":/svg/%1/heart-fill.svg").arg(themeFolder)
                                                    : QString(":/svg/%1/heart-outline.svg").arg(themeFolder);
            QIcon favIcon(favIconPath);
            if (!favIcon.isNull())
            {
                favIcon.paint(painter, favRect.adjusted(2, 2, -2, -2));
            }
        }

        painter->restore();
        return;
    }

    
    
    

    
    if (option.state & QStyle::State_Selected)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selectedBg);
        painter->drawRoundedRect(rect, 8, 8);
    }

    
    const CardGeometry geometry = cardGeometry(rect, style);
    QRect baseImgRect = geometry.image;
    QRect titleRect = geometry.title;
    QRect yearRect = geometry.subtitle;

    
    QRect targetImgRect = baseImgRect;

    if (isHovered)
    {
        targetImgRect = hoverImageRect(baseImgRect);
        const int expandH = targetImgRect.bottom() - baseImgRect.bottom();
        titleRect.translate(0, expandH);
        yearRect.translate(0, expandH);

        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 60)); 
        
        painter->drawRoundedRect(targetImgRect.translated(0, 6), 12, 12);
    }

    
    int imgRadius = 8;
    painter->setPen(Qt::NoPen);
    painter->setBrush(placeholderBg);
    painter->drawRoundedRect(targetImgRect, imgRadius, imgRadius);

    
    if (!poster.isNull())
    {
        QPainterPath path;
        path.addRoundedRect(targetImgRect, imgRadius, imgRadius);
        painter->setClipPath(path);

        
        QPixmap scaled = ImageUtils::scaledCoverPixmap(poster, targetImgRect.size(), painter->device()->devicePixelRatioF());
        qreal px = targetImgRect.x() + (targetImgRect.width() - scaled.deviceIndependentSize().width()) / 2;
        qreal py = targetImgRect.y() + (targetImgRect.height() - scaled.deviceIndependentSize().height()) / 2;

        painter->drawPixmap(QPointF(px, py), scaled);
        painter->setClipping(false);
    }
    else
    {
        painter->setPen(placeholderTextColor);
        painter->drawText(targetImgRect, Qt::AlignCenter, tr("No Image"));
    }

    
    
    
    if ((item.type == "Series" || item.type == "Season") && item.recursiveItemCount > 0)
    {
        QFont badgeFont = option.font;
        badgeFont.setPixelSize(qMax(8, qRound(11 * fontScale)));
        badgeFont.setBold(true);

        
        QString countStr = item.recursiveItemCount > 999 ? "999+" : QString::number(item.recursiveItemCount);

        
        QFontMetrics fm(badgeFont);
        int textWidth = fm.horizontalAdvance(countStr);

        
        int badgeHeight = 22;
        int badgeWidth = qMax(badgeHeight, textWidth + 10);

        
        int badgeMargin = 4;
        QRect badgeRect(targetImgRect.right() - badgeWidth - badgeMargin, targetImgRect.top() + badgeMargin, badgeWidth,
                        badgeHeight);

        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 160));
        
        painter->drawRoundedRect(badgeRect, badgeHeight / 2, badgeHeight / 2);

        
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, countStr);
    }

    
    
    
    if (isHovered && hasHoverButtons)
    {
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 70));
        painter->drawRoundedRect(targetImgRect, imgRadius, imgRadius);

        const HoverButtonLayout buttonLayout =
            buildHoverButtonLayout(targetImgRect, showPlayButton, showFavoriteButton, showMoreButton);

        if (showPlayButton)
        {
            painter->setBrush(QColor(0, 0, 0, 160));
            painter->drawEllipse(buttonLayout.playRect);

            QIcon playIcon(":/svg/player/play.svg");
            if (!playIcon.isNull())
            {
                playIcon.paint(painter, buttonLayout.playRect.adjusted(12, 12, -12, -12), Qt::AlignCenter);
            }
            else
            {
                QPainterPath playPath;
                playPath.moveTo(buttonLayout.playRect.center().x() - 4, buttonLayout.playRect.center().y() - 8);
                playPath.lineTo(buttonLayout.playRect.center().x() + 10, buttonLayout.playRect.center().y());
                playPath.lineTo(buttonLayout.playRect.center().x() - 4, buttonLayout.playRect.center().y() + 8);
                playPath.closeSubpath();
                painter->setBrush(Qt::white);
                painter->drawPath(playPath);
            }
        }

        if (showMoreButton)
        {
            painter->setBrush(m_themeHelper ? m_themeHelper->hoverButtonBackgroundColor()
                                            : QColor(0, 0, 0, 140));
            painter->drawRoundedRect(buttonLayout.moreRect, 5, 5);

            QIcon moreIcon(":/svg/dark/more-line.svg");
            if (!moreIcon.isNull())
            {
                moreIcon.paint(painter, buttonLayout.moreRect.adjusted(4, 4, -4, -4), Qt::AlignCenter);
            }
            else
            {
                painter->setBrush(Qt::white);
                int cx = buttonLayout.moreRect.center().x();
                int cy = buttonLayout.moreRect.center().y();
                painter->drawEllipse(cx - 2, cy - 8, 4, 4);
                painter->drawEllipse(cx - 2, cy - 2, 4, 4);
                painter->drawEllipse(cx - 2, cy + 4, 4, 4);
            }
        }

        if (showFavoriteButton)
        {
            painter->setBrush(m_themeHelper ? m_themeHelper->hoverButtonBackgroundColor()
                                            : QColor(0, 0, 0, 140));
            painter->drawRoundedRect(buttonLayout.favoriteRect, 5, 5);

            QString favIconPath = item.isFavorite() ? ":/svg/dark/heart-fill.svg" : ":/svg/dark/heart-outline.svg";
            QIcon favIcon(favIconPath);
            if (!favIcon.isNull())
            {
                favIcon.paint(painter, buttonLayout.favoriteRect.adjusted(4, 4, -4, -4), Qt::AlignCenter);
            }
            else
            {
                painter->setBrush(Qt::white);
                painter->drawEllipse(buttonLayout.favoriteRect.center(), 4, 4);
            }
        }
    }

    
    if (item.userData.played &&
        (isPlayableItem(item) || item.type == "Series" || item.type == "Season"))
    {
        drawPlayedBadge(painter, targetImgRect);
    }

    drawPlaybackProgress(targetImgRect);
    drawCoverRating(targetImgRect);

    
    
    
    if (!m_highlightedItemId.isEmpty() && item.id == m_highlightedItemId)
    {
        QColor borderColor = isDarkTheme ? QColor(96, 165, 250, 220)  
                                         : QColor(59, 130, 246, 220); 
        QPen highlightPen(borderColor, 2.0);
        painter->setPen(highlightPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(targetImgRect.adjusted(-2, -2, 2, 2), 10, 10);
    }

    
    QFont titleFont = option.font;
    titleFont.setPixelSize(qMax(8, qRound(m_titleFontPixelSize * fontScale)));
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(titleColor);

    QFontMetrics fm(titleFont);
    
    QString displayTitle = item.name;
    if (item.type == "Episode" && item.parentIndexNumber >= 0 && item.indexNumber >= 0)
    {
        displayTitle = QString("S%1E%2  %3")
                           .arg(item.parentIndexNumber, 2, 10, QChar('0'))
                           .arg(item.indexNumber, 2, 10, QChar('0'))
                           .arg(item.name);
    }
    QString elidedTitle = fm.elidedText(displayTitle, Qt::ElideRight, titleRect.width());
    Qt::Alignment titleAlign =
        (style == Poster) ? (Qt::AlignHCenter | Qt::AlignVCenter) : (Qt::AlignHCenter | Qt::AlignVCenter);
    painter->drawText(titleRect, titleAlign, elidedTitle);

    
    
    const bool canDrawYear = item.productionYear > 0 && yearRect.isValid() && yearRect.bottom() <= rect.bottom();
    if ((style == Poster || style == LibraryTile) && canDrawYear)
    {
        QFont subFont = option.font;
        subFont.setPixelSize(qMax(8, qRound(m_subTitleFontPixelSize * fontScale)));
        painter->setFont(subFont);
        painter->setPen(subTitleColor);
        painter->drawText(yearRect, Qt::AlignHCenter | Qt::AlignVCenter, QString::number(item.productionYear));
    }
    else if (style == AlbumCover)
    {
        QFont subFont = option.font;
        subFont.setPixelSize(qMax(8, qRound(m_subTitleFontPixelSize * fontScale)));
        painter->setFont(subFont);
        painter->setPen(subTitleColor);
        const QString artist = item.albumArtist.isEmpty() ? item.artists.join(QStringLiteral(", ")) : item.albumArtist;
        painter->drawText(yearRect, Qt::AlignCenter, QFontMetrics(subFont).elidedText(artist, Qt::ElideRight, yearRect.width()));
    }
    else if (style == Cast)
    {
        QFont subFont = option.font;
        subFont.setPixelSize(qMax(8, qRound(m_subTitleFontPixelSize * fontScale)));
        painter->setFont(subFont);
        painter->setPen(subTitleColor);
        QFontMetrics fmSub(subFont);
        QString elidedRole = fmSub.elidedText(item.overview, Qt::ElideRight, yearRect.width());
        painter->drawText(yearRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedRole);
    }

    painter->restore();
}
