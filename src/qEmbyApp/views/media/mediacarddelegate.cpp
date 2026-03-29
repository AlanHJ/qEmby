#include "mediacarddelegate.h"
#include "medialistmodel.h"

#include "../../managers/thememanager.h"
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QStyle>
#include <QEvent>
#include <QMouseEvent>
#include <QIcon>
#include <QAbstractItemView>
#include <QDebug>


MediaCardThemeHelper::MediaCardThemeHelper(QWidget *parent)
    : QWidget(parent),
      m_hoverBg(229, 231, 235),          
      m_selectedBg(219, 234, 254),
      m_titleColor(31, 41, 55),
      m_subTitleColor(107, 114, 128),
      m_placeholderBg(229, 231, 235),
      m_placeholderTextColor(156, 163, 175)
{
    hide(); 
}



MediaCardDelegate::MediaCardDelegate(CardStyle style, QObject *parent)
    : QStyledItemDelegate(parent), m_style(style), m_tileSize(160, 270), m_themeHelper(nullptr) {}

QSize MediaCardDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    Q_UNUSED(option);
    Q_UNUSED(index);
    
    return m_tileSize;
}


bool MediaCardDelegate::eventFilter(QObject *object, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress || 
        event->type() == QEvent::MouseButtonRelease || 
        event->type() == QEvent::MouseButtonDblClick) {
        
        QWidget *viewport = qobject_cast<QWidget*>(object);
        if (viewport) {
            
            QAbstractItemView *view = qobject_cast<QAbstractItemView*>(viewport->parentWidget());
            if (view) {
                QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
                QPoint pos = mouseEvent->pos();
                QModelIndex index = view->indexAt(pos); 
                
                if (index.isValid()) {
                    MediaItem item = index.data(MediaListModel::ItemDataRole).value<MediaItem>();
                    bool isPlayable = (item.mediaType == "Video" || 
                                       item.type == "Movie" || 
                                       item.type == "Episode" || 
                                       item.type == "Video" || 
                                       item.type == "MusicVideo");
                    
                    bool hasHoverButtons = isPlayable || item.type == "Series" || item.type == "Season";
                    
                    if (hasHoverButtons) {
                        
                        QRect rect = view->visualRect(index); 

                        
                        
                        
                        if (m_style == EpisodeList) {
                            int padding = 10;
                            int imgWidth = 250;
                            int imgHeight = 140;
                            QRect baseImgRect(rect.x() + padding, rect.y() + (rect.height() - imgHeight)/2, imgWidth, imgHeight);
                            
                            
                            int favBtnSize = 24;
                            int rightMargin = 20;
                            QRect favRect(rect.right() - rightMargin - favBtnSize, rect.center().y() - favBtnSize/2, favBtnSize, favBtnSize);

                            bool inFav = favRect.contains(pos);
                            
                            
                            int playSize = 48;
                            QRect playRectCenter(baseImgRect.center().x() - playSize / 2,
                                           baseImgRect.center().y() - playSize / 2,
                                           playSize, playSize);
                            bool inThumbPlay = playRectCenter.contains(pos);

                            if (inFav || inThumbPlay) {
                                if (event->type() == QEvent::MouseButtonRelease) {
                                    if (inThumbPlay) emit playRequested(item);
                                    else if (inFav) emit favoriteRequested(item);
                                }
                                return true;
                            }
                        } else {
                            int padding = 8;
                            QRect baseImgRect;

                            if (m_style == Poster) {
                                int imgWidth = rect.width() - padding * 2;
                                int imgHeight = qRound(imgWidth * 1.5);
                                baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
                            } else if (m_style == LibraryTile) {
                                int imgWidth = rect.width() - padding * 2;
                                int imgHeight = qRound(imgWidth * 9.0 / 16.0);
                                baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
                            } else if (m_style == Cast) {
                                int imgWidth = rect.width() - padding * 2;
                                int imgHeight = qRound(imgWidth * 1.5); 
                                baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
                            }

                            int expandW = qRound(baseImgRect.width() * 0.035);
                            int expandH = qRound(baseImgRect.height() * 0.035);
                            QRect targetImgRect = baseImgRect.adjusted(-expandW, -expandH, expandW, expandH);

                            int playSize = 48;
                            QRect playRect(targetImgRect.center().x() - playSize / 2,
                                           targetImgRect.center().y() - playSize / 2,
                                           playSize, playSize);

                            int btnWidth = 28;
                            int btnHeight = 28;
                            int spacing = 6;
                            QRect moreRect(targetImgRect.right() - btnWidth - 8,
                                           targetImgRect.bottom() - btnHeight - 8,
                                           btnWidth, btnHeight);
                            QRect favRect(moreRect.left() - btnWidth - spacing,
                                          moreRect.top(),
                                          btnWidth, btnHeight);

                            
                            bool inPlay = playRect.contains(pos);
                            bool inFav  = favRect.contains(pos);
                            bool inMore = moreRect.contains(pos);

                            if (inPlay || inFav || inMore) {
                                
                                if (event->type() == QEvent::MouseButtonRelease) {
                                    if (inPlay) emit playRequested(item);
                                    else if (inFav) emit favoriteRequested(item);
                                    else if (inMore) emit moreMenuRequested(item, mouseEvent->globalPosition().toPoint());
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

void MediaCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    
    if (option.widget) {
        
        QAbstractItemView *view = qobject_cast<QAbstractItemView*>(const_cast<QWidget*>(option.widget));
        if (view) {
            QWidget *viewport = view->viewport(); 
            if (!m_installedViewports.contains(viewport)) {
                viewport->installEventFilter(const_cast<MediaCardDelegate*>(this));
                m_installedViewports.insert(viewport);
                
                connect(viewport, &QObject::destroyed, const_cast<MediaCardDelegate*>(this), [this, viewport]() {
                    m_installedViewports.remove(viewport);
                });
            }
        }
    }

    if (!m_themeHelper && option.widget) {
        m_themeHelper = new MediaCardThemeHelper(const_cast<QWidget*>(option.widget));
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
    bool isHovered = option.state & QStyle::State_MouseOver;
    bool isPlayable = (item.mediaType == "Video" || 
                       item.type == "Movie" || 
                       item.type == "Episode" || 
                       item.type == "Video" || 
                       item.type == "MusicVideo");
    
    bool hasHoverButtons = isPlayable || item.type == "Series" || item.type == "Season";

    
    
    
    if (m_style == EpisodeList) {
        if (isHovered) {
            
            painter->setPen(Qt::NoPen);
            painter->setBrush(isDarkTheme ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 15)); 
            painter->drawRoundedRect(rect.adjusted(5, 5, -5, -5), 8, 8);
        }

        int padding = 10;
        int imgWidth = 250;
        int imgHeight = 140; 
        QRect baseImgRect(rect.x() + padding, rect.y() + (rect.height() - imgHeight)/2, imgWidth, imgHeight);

        int imgRadius = 8;
        painter->setPen(Qt::NoPen);
        painter->setBrush(placeholderBg);
        painter->drawRoundedRect(baseImgRect, imgRadius, imgRadius);

        if (!poster.isNull()) {
            QPainterPath path;
            path.addRoundedRect(baseImgRect, imgRadius, imgRadius); 
            painter->setClipPath(path);
            QPixmap scaled = poster.scaled(baseImgRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int px = baseImgRect.x() + (baseImgRect.width() - scaled.width()) / 2;
            int py = baseImgRect.y() + (baseImgRect.height() - scaled.height()) / 2;
            painter->drawPixmap(px, py, scaled);
            painter->setClipping(false);
        } else {
            painter->setPen(placeholderTextColor);
            painter->drawText(baseImgRect, Qt::AlignCenter, tr("No Image"));
        }

        if (isHovered && isPlayable) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(0, 0, 0, 70));
            painter->drawRoundedRect(baseImgRect, imgRadius, imgRadius);

            int playSize = 48;
            QRect playRectCenter(baseImgRect.center().x() - playSize / 2, baseImgRect.center().y() - playSize / 2, playSize, playSize);
            painter->setBrush(QColor(0, 0, 0, 160)); 
            painter->drawEllipse(playRectCenter);
            QIcon playIcon(":/svg/player/play.svg");
            if (!playIcon.isNull()) {
                playIcon.paint(painter, playRectCenter.adjusted(12, 12, -12, -12), Qt::AlignCenter);
            } else {
                QPainterPath playPath;
                playPath.moveTo(playRectCenter.center().x() - 4, playRectCenter.center().y() - 8);
                playPath.lineTo(playRectCenter.center().x() + 10, playRectCenter.center().y());
                playPath.lineTo(playRectCenter.center().x() - 4, playRectCenter.center().y() + 8);
                playPath.closeSubpath();
                painter->setBrush(Qt::white);
                painter->drawPath(playPath);
            }
        }

        
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
        painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, fmTitle.elidedText(epTitle, Qt::ElideRight, titleRect.width()));

        QFont metaFont = option.font;
        metaFont.setPixelSize(qMax(8, qRound(13 * fontScale)));
        painter->setFont(metaFont);
        painter->setPen(subTitleColor);
        QStringList metas;
        if (!item.premiereDate.isEmpty()) metas << item.premiereDate.left(10);
        if (item.runTimeTicks > 0) {
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
        if (maxLines < 1) maxLines = 1;
        
        QString elidedDesc = fmDesc.elidedText(desc, Qt::ElideRight, descRect.width() * maxLines);
        painter->drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, elidedDesc);

        
        QRect favRect(rect.right() - rightMargin - favBtnSize, rect.center().y() - favBtnSize/2, favBtnSize, favBtnSize);
        {
            QString themeFolder = isDarkTheme ? "dark" : "light";
            QString favIconPath = item.isFavorite() ? QString(":/svg/%1/heart-fill.svg").arg(themeFolder) 
                                                    : QString(":/svg/%1/heart-outline.svg").arg(themeFolder);
            QIcon favIcon(favIconPath);
            if(!favIcon.isNull()) favIcon.paint(painter, favRect.adjusted(2, 2, -2, -2));
        }

        painter->restore();
        return; 
    }

    
    
    

    
    if (option.state & QStyle::State_Selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(selectedBg);
        painter->drawRoundedRect(rect, 8, 8);
    }

    int padding = 8;
    QRect baseImgRect, titleRect, yearRect;

    
    if (m_style == Poster) {
        int imgWidth = rect.width() - padding * 2;
        int imgHeight = qRound(imgWidth * 1.5);
        baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
        titleRect = QRect(rect.x() + padding, baseImgRect.bottom() + 6, imgWidth, 20);
        yearRect = QRect(rect.x() + padding, titleRect.bottom() + 2, imgWidth, 16);
    } else if (m_style == LibraryTile) {
        int imgWidth = rect.width() - padding * 2;
        int imgHeight = qRound(imgWidth * 9.0 / 16.0);
        baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
        titleRect = QRect(rect.x() + padding, baseImgRect.bottom() + 6, imgWidth, 20);
    } else if (m_style == Cast) {
        int imgWidth = rect.width() - padding * 2;
        int imgHeight = qRound(imgWidth * 1.5); 
        baseImgRect = QRect(rect.x() + padding, rect.y() + padding, imgWidth, imgHeight);
        titleRect = QRect(rect.x() + padding, baseImgRect.bottom() + 8, imgWidth, 18);
        yearRect = QRect(rect.x() + padding, titleRect.bottom() + 2, imgWidth, 16); 
    }

    
    QRect targetImgRect = baseImgRect;

    if (isHovered) {
        
        int expandW = qRound(baseImgRect.width() * 0.035);
        int expandH = qRound(baseImgRect.height() * 0.035);
        
        targetImgRect = baseImgRect.adjusted(-expandW, -expandH, expandW, expandH);

        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 60)); 
        
        painter->drawRoundedRect(targetImgRect.translated(0, 6), 12, 12);
    }

    
    int imgRadius = 8;
    painter->setPen(Qt::NoPen);
    painter->setBrush(placeholderBg);
    painter->drawRoundedRect(targetImgRect, imgRadius, imgRadius);

    
    if (!poster.isNull()) {
        QPainterPath path;
        path.addRoundedRect(targetImgRect, imgRadius, imgRadius); 
        painter->setClipPath(path);

        
        QPixmap scaled = poster.scaled(targetImgRect.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        int px = targetImgRect.x() + (targetImgRect.width() - scaled.width()) / 2;
        int py = targetImgRect.y() + (targetImgRect.height() - scaled.height()) / 2;

        painter->drawPixmap(px, py, scaled);
        painter->setClipping(false);
    } else {
        painter->setPen(placeholderTextColor);
        painter->drawText(targetImgRect, Qt::AlignCenter, tr("No Image"));
    }

    
    
    
    if ((item.type == "Series" || item.type == "Season") && item.recursiveItemCount > 0) {
        QFont badgeFont = option.font;
        badgeFont.setPixelSize(qMax(8, qRound(11 * fontScale)));
        badgeFont.setBold(true);
        
        
        QString countStr = item.recursiveItemCount > 999 ? "999+" : QString::number(item.recursiveItemCount);
        
        
        QFontMetrics fm(badgeFont);
        int textWidth = fm.horizontalAdvance(countStr);
        
        
        int badgeHeight = 22;
        int badgeWidth = qMax(badgeHeight, textWidth + 10); 
        
        
        int badgeMargin = 4;
        QRect badgeRect(targetImgRect.right() - badgeWidth - badgeMargin,
                        targetImgRect.top() + badgeMargin,
                        badgeWidth, badgeHeight);

        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 160));
        
        painter->drawRoundedRect(badgeRect, badgeHeight / 2, badgeHeight / 2);

        
        painter->setFont(badgeFont);
        painter->setPen(Qt::white);
        painter->drawText(badgeRect, Qt::AlignCenter, countStr);
    }

    
    
    
    if (isHovered && hasHoverButtons) {
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 70));
        painter->drawRoundedRect(targetImgRect, imgRadius, imgRadius);

        
        int playSize = 48;
        QRect playRect(targetImgRect.center().x() - playSize / 2,
                       targetImgRect.center().y() - playSize / 2,
                       playSize, playSize);
        
        painter->setBrush(QColor(0, 0, 0, 160)); 
        painter->drawEllipse(playRect);
        
        QIcon playIcon(":/svg/player/play.svg");
        if (!playIcon.isNull()) {
            
            playIcon.paint(painter, playRect.adjusted(12, 12, -12, -12), Qt::AlignCenter);
        } else {
            
            QPainterPath playPath;
            playPath.moveTo(playRect.center().x() - 4, playRect.center().y() - 8);
            playPath.lineTo(playRect.center().x() + 10, playRect.center().y());
            playPath.lineTo(playRect.center().x() - 4, playRect.center().y() + 8);
            playPath.closeSubpath();
            painter->setBrush(Qt::white);
            painter->drawPath(playPath);
        }

        
        int btnWidth = 28;
        int btnHeight = 28;
        int spacing = 6;
        
        QRect moreRect(targetImgRect.right() - btnWidth - 8,
                       targetImgRect.bottom() - btnHeight - 8,
                       btnWidth, btnHeight);
                       
        QRect favRect(moreRect.left() - btnWidth - spacing,
                      moreRect.top(),
                      btnWidth, btnHeight);
        
        
        painter->setBrush(QColor(0, 0, 0, 160));
        painter->drawRoundedRect(moreRect, 6, 6);
        painter->drawRoundedRect(favRect, 6, 6);

        
        QIcon moreIcon(":/svg/dark/more-line.svg"); 
        if (!moreIcon.isNull()) {
            moreIcon.paint(painter, moreRect.adjusted(4, 4, -4, -4), Qt::AlignCenter);
        } else {
            painter->setBrush(Qt::white);
            int cx = moreRect.center().x();
            int cy = moreRect.center().y();
            painter->drawEllipse(cx - 2, cy - 8, 4, 4);
            painter->drawEllipse(cx - 2, cy - 2, 4, 4);
            painter->drawEllipse(cx - 2, cy + 4, 4, 4);
        }
        
        
        QString favIconPath = item.isFavorite() ? ":/svg/dark/heart-fill.svg" : ":/svg/dark/heart-outline.svg";
        QIcon favIcon(favIconPath);
        if (!favIcon.isNull()) {
            
            favIcon.paint(painter, favRect.adjusted(5, 5, -5, -5), Qt::AlignCenter);
        } else {
            painter->setBrush(Qt::white);
            painter->drawEllipse(favRect.center(), 4, 4); 
        }
    }

    
    
    
    if (!m_highlightedItemId.isEmpty() && item.id == m_highlightedItemId) {
        QColor borderColor = isDarkTheme ? QColor(96, 165, 250, 220)   
                                         : QColor(59, 130, 246, 220);  
        QPen highlightPen(borderColor, 2.0);
        painter->setPen(highlightPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(targetImgRect.adjusted(-2, -2, 2, 2), 10, 10);
    }

    
    QFont titleFont = option.font;
    titleFont.setPixelSize(qMax(8, qRound(13 * fontScale)));
    titleFont.setBold(true);
    painter->setFont(titleFont);
    painter->setPen(titleColor);

    QFontMetrics fm(titleFont);
    
    QString displayTitle = item.name;
    if (item.type == "Episode" && item.parentIndexNumber >= 0 && item.indexNumber >= 0) {
        displayTitle = QString("S%1E%2  %3")
            .arg(item.parentIndexNumber, 2, 10, QChar('0'))
            .arg(item.indexNumber, 2, 10, QChar('0'))
            .arg(item.name);
    }
    QString elidedTitle = fm.elidedText(displayTitle, Qt::ElideRight, titleRect.width());
    Qt::Alignment titleAlign = (m_style == Poster) ? (Qt::AlignHCenter | Qt::AlignVCenter) : (Qt::AlignHCenter | Qt::AlignVCenter);
    painter->drawText(titleRect, titleAlign, elidedTitle);

    
    if (m_style == Poster && item.productionYear > 0) {
        QFont subFont = option.font;
        subFont.setPixelSize(qMax(8, qRound(12 * fontScale)));
        painter->setFont(subFont);
        painter->setPen(subTitleColor);
        painter->drawText(yearRect, Qt::AlignHCenter | Qt::AlignVCenter, QString::number(item.productionYear));
    } else if (m_style == Cast) {
        QFont subFont = option.font;
        subFont.setPixelSize(qMax(8, qRound(12 * fontScale)));
        painter->setFont(subFont);
        painter->setPen(subTitleColor);
        QFontMetrics fmSub(subFont);
        QString elidedRole = fmSub.elidedText(item.overview, Qt::ElideRight, yearRect.width());
        painter->drawText(yearRect, Qt::AlignHCenter | Qt::AlignVCenter, elidedRole);
    }

    painter->restore();
}
