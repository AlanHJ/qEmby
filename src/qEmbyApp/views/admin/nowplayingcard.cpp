#include "nowplayingcard.h"
#include <models/admin/adminmodels.h>
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include "../../managers/thememanager.h"
#include "../../utils/imageutils.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPointer>

NowPlayingCard::NowPlayingCard(const SessionInfo& session, QEmbyCore* core, QWidget* parent)
    : QFrame(parent), m_core(core), m_sessionId(session.id)
{
    setObjectName("NowPlayingCard");
    setFixedWidth(320);
    setupUi(session);
}

void NowPlayingCard::setupUi(const SessionInfo& session) {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 0, 8);
    mainLayout->setSpacing(10);

    
    
    
    m_imageLabel = new QLabel(this);
    m_imageLabel->setObjectName("NowPlayingImage");
    m_imageLabel->setFixedSize(140, 80);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    
    QIcon placeholderIcon = ThemeManager::getAdaptiveIcon(":/svg/dark/now-playing.svg");
    m_imageLabel->setPixmap(placeholderIcon.pixmap(QSize(32, 32)));
    mainLayout->addWidget(m_imageLabel);

    
    
    
    auto* infoLayout = new QVBoxLayout();
    infoLayout->setContentsMargins(0, 0, 10, 0);
    infoLayout->setSpacing(3);

    
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("NowPlayingTitle");
    m_titleLabel->setWordWrap(false);
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    infoLayout->addWidget(m_titleLabel);

    
    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName("NowPlayingSubtitle");
    m_subtitleLabel->setWordWrap(false);
    m_subtitleLabel->setTextFormat(Qt::PlainText);
    m_subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    infoLayout->addWidget(m_subtitleLabel);

    
    auto* progressRow = new QHBoxLayout();
    progressRow->setSpacing(4);
    progressRow->setContentsMargins(0, 0, 0, 0);

    m_stateIcon = new QLabel(this);
    m_stateIcon->setFixedSize(12, 12);
    m_stateIcon->setScaledContents(true);
    progressRow->addWidget(m_stateIcon);

    m_progressText = new QLabel(this);
    m_progressText->setObjectName("NowPlayingProgress");
    progressRow->addWidget(m_progressText);
    progressRow->addStretch(1);

    infoLayout->addLayout(progressRow);

    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName("NowPlayingProgressBar");
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumWidth(120);
    m_progressBar->setRange(0, 1000);
    infoLayout->addWidget(m_progressBar);

    infoLayout->addSpacing(2);

    
    m_userLabel = new QLabel(this);
    m_userLabel->setObjectName("NowPlayingUser");
    m_userLabel->setWordWrap(false);
    infoLayout->addWidget(m_userLabel);

    
    m_streamLabel = new QLabel(this);
    m_streamLabel->setObjectName("NowPlayingStream");
    m_streamLabel->setWordWrap(true);
    infoLayout->addWidget(m_streamLabel);

    infoLayout->addStretch(1);
    mainLayout->addLayout(infoLayout, 1);

    
    updateSession(session);
}

void NowPlayingCard::updateSession(const SessionInfo& session) {
    m_sessionId = session.id;

    
    {
        const QString &title = session.nowPlayingItemName;
        QFontMetrics fm(m_titleLabel->font());
        QString elided = fm.elidedText(title, Qt::ElideRight, m_titleLabel->width());
        m_titleLabel->setText(elided);
        m_titleLabel->setToolTip(elided != title ? title : QString());
    }

    
    QString subtitle;
    if (!session.nowPlayingSeriesName.isEmpty()) {
        subtitle = session.nowPlayingSeriesName;
        QString seLabel = session.seasonEpisodeLabel();
        if (!seLabel.isEmpty()) {
            subtitle += QString(" - %1").arg(seLabel);
        }
    } else if (!session.nowPlayingItemType.isEmpty()) {
        subtitle = session.nowPlayingItemType;
    }
    {
        QFontMetrics fm(m_subtitleLabel->font());
        QString elided = fm.elidedText(subtitle, Qt::ElideRight, m_subtitleLabel->width());
        m_subtitleLabel->setText(elided);
        m_subtitleLabel->setToolTip(elided != subtitle ? subtitle : QString());
    }
    m_subtitleLabel->setVisible(!subtitle.isEmpty());

    
    if (session.nowPlayingRunTimeTicks > 0) {
        QString posStr = SessionInfo::formatTicks(session.positionTicks);
        QString durStr = SessionInfo::formatTicks(session.nowPlayingRunTimeTicks);

        
        QString iconPath = session.isPaused
            ? QStringLiteral(":/svg/dark/now-paused.svg")
            : QStringLiteral(":/svg/dark/now-playing.svg");
        QIcon stateIcon = ThemeManager::getAdaptiveIcon(iconPath);
        m_stateIcon->setPixmap(stateIcon.pixmap(QSize(12, 12)));

        m_progressText->setText(posStr + " / " + durStr);
        m_progressBar->setValue(static_cast<int>(session.playbackProgress() * 1000));
        m_progressText->show();
        m_stateIcon->show();
        m_progressBar->show();
    } else {
        m_progressText->hide();
        m_stateIcon->hide();
        m_progressBar->hide();
    }

    
    m_userLabel->setText(QString("%1 · %2").arg(session.userName, session.client));

    
    QStringList streamParts;
    if (!session.playMethod.isEmpty()) {
        QString methodDisplay;
        if (session.playMethod == "DirectPlay") methodDisplay = tr("Direct Play");
        else if (session.playMethod == "DirectStream") methodDisplay = tr("Direct Stream");
        else if (session.playMethod == "Transcode") methodDisplay = tr("Transcode");
        else methodDisplay = session.playMethod;

        if (session.playMethod == "Transcode" && !session.transcodingVideoCodec.isEmpty()) {
            QString bitrateStr;
            if (session.transcodingBitrate > 0) {
                double mbps = session.transcodingBitrate / 1000000.0;
                bitrateStr = QString(" %1 Mbps").arg(mbps, 0, 'f', 1);
            }
            streamParts << QString("%1 → %2%3")
                .arg(methodDisplay, session.transcodingVideoCodec.toUpper(), bitrateStr);
        } else {
            streamParts << methodDisplay;
        }
    }
    if (!session.remoteEndPoint.isEmpty()) {
        streamParts << session.remoteEndPoint;
    }
    m_streamLabel->setText(streamParts.join("  ·  "));

    
    if (!session.nowPlayingItemId.isEmpty() && session.nowPlayingItemId != m_currentImageItemId) {
        m_currentImageItemId = session.nowPlayingItemId;
        
        QIcon placeholderIcon = ThemeManager::getAdaptiveIcon(":/svg/dark/now-playing.svg");
        m_imageLabel->setPixmap(placeholderIcon.pixmap(QSize(32, 32)));
        loadImage(session);
    }
}

QCoro::Task<void> NowPlayingCard::loadImage(const SessionInfo& session) {
    QPointer<NowPlayingCard> safeThis(this);

    
    
    struct ImageCandidate {
        QString itemId;
        QString imageType;
        QString imageTag;
    };

    QList<ImageCandidate> candidates;

    
    if (!session.nowPlayingThumbTag.isEmpty()) {
        candidates.append({session.nowPlayingItemId, "Thumb", session.nowPlayingThumbTag});
    }
    
    if (!session.nowPlayingBackdropTag.isEmpty()) {
        candidates.append({session.nowPlayingItemId, "Backdrop", session.nowPlayingBackdropTag});
    }
    
    if (!session.nowPlayingImageTag.isEmpty()) {
        candidates.append({session.nowPlayingItemId, "Primary", session.nowPlayingImageTag});
    }
    
    if (!session.parentThumbItemId.isEmpty() && !session.parentThumbImageTag.isEmpty()) {
        candidates.append({session.parentThumbItemId, "Thumb", session.parentThumbImageTag});
    }
    
    if (!session.parentBackdropItemId.isEmpty() && !session.parentBackdropImageTag.isEmpty()) {
        candidates.append({session.parentBackdropItemId, "Backdrop", session.parentBackdropImageTag});
    }
    
    if (!session.nowPlayingSeriesId.isEmpty() && !session.nowPlayingSeriesPrimaryTag.isEmpty()) {
        candidates.append({session.nowPlayingSeriesId, "Primary", session.nowPlayingSeriesPrimaryTag});
    }

    
    if (candidates.isEmpty()) {
        candidates.append({session.nowPlayingItemId, "Primary", ""});
    }

    
    for (const auto& candidate : candidates) {
        if (!safeThis) co_return;

        try {
            auto pixmap = co_await m_core->mediaService()->fetchImage(
                candidate.itemId, candidate.imageType, candidate.imageTag, 280);

            if (!safeThis) co_return;

            if (!pixmap.isNull()) {
                
                QPixmap scaled = pixmap.scaled(
                    m_imageLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                int x = (scaled.width() - m_imageLabel->width()) / 2;
                int y = (scaled.height() - m_imageLabel->height()) / 2;
                QPixmap cropped = scaled.copy(x, y, m_imageLabel->width(), m_imageLabel->height());
                m_imageLabel->setPixmap(ImageUtils::roundedPixmap(cropped, 10, 4));
                co_return; 
            }
        } catch (...) {
            
        }
    }

    
    if (safeThis) {
        QIcon placeholderIcon = ThemeManager::getAdaptiveIcon(":/svg/dark/now-playing.svg");
        m_imageLabel->setPixmap(placeholderIcon.pixmap(QSize(32, 32)));
    }
}
