#include "detailactionwidget.h"
#include "../managers/externalplayerdetector.h"
#include "../utils/mediasourcepreferenceutils.h"
#include "../utils/playerpreferenceutils.h"
#include "modernmenubutton.h"
#include "splitplayerbutton.h"
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>


DetailActionWidget::DetailActionWidget(QWidget *parent) : QWidget(parent) {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(4);

  
  auto *actionsLayout = new QHBoxLayout();
  actionsLayout->setSpacing(12);
  actionsLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  m_resumeBtn = new QPushButton(tr("▶ Resume"), this);
  m_resumeBtn->setObjectName("detail-resume-btn");
  m_resumeBtn->setCursor(Qt::PointingHandCursor);
  m_resumeBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_playBtn = new QPushButton(tr("▶ Play"), this);
  m_playBtn->setObjectName("detail-play-btn");
  m_playBtn->setCursor(Qt::PointingHandCursor);
  m_playBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  m_favBtn = new QPushButton(this);
  m_favBtn->setObjectName("detail-fav-btn");
  m_favBtn->setIcon(QIcon(":/svg/light/heart.svg"));
  m_favBtn->setIconSize(QSize(20, 20));
  m_favBtn->setFixedSize(36, 36);
  m_favBtn->setCursor(Qt::PointingHandCursor);

  m_progressWidget = new QWidget(this);
  m_progressWidget->setMaximumWidth(450);
  auto *progressLayout = new QHBoxLayout(m_progressWidget);
  progressLayout->setContentsMargins(0, 0, 0, 0);
  progressLayout->setSpacing(10);

  m_progressBar = new QProgressBar(m_progressWidget);
  m_progressBar->setObjectName("detail-progress-bar");
  m_progressBar->setTextVisible(false);

  m_remainingTimeLabel = new QLabel(m_progressWidget);
  m_remainingTimeLabel->setObjectName("detail-remaining-time");

  progressLayout->addWidget(m_progressBar, 1);
  progressLayout->addWidget(m_remainingTimeLabel);

  
  m_extPlayerBtn = new SplitPlayerButton(this);
  m_extPlayerBtn->setObjectName("detail-ext-player-btn");
  m_extPlayerBtn->setIconOnly(true);
  m_extPlayerBtn->hide();

  connect(m_extPlayerBtn, &SplitPlayerButton::playRequested, this,
          &DetailActionWidget::externalPlayRequested);
  connect(m_extPlayerBtn, &SplitPlayerButton::playerSelected, this,
          &DetailActionWidget::externalPlayRequested);

  actionsLayout->addWidget(m_resumeBtn);
  actionsLayout->addWidget(m_playBtn);
  actionsLayout->addWidget(m_extPlayerBtn);
  actionsLayout->addWidget(m_favBtn);
  actionsLayout->addWidget(m_progressWidget);
  actionsLayout->addStretch();

  
  auto *streamSelectorsLayout = new QHBoxLayout();
  streamSelectorsLayout->setSpacing(6);
  streamSelectorsLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  m_versionComboBox = new ModernMenuButton(this);
  m_audioComboBox = new ModernMenuButton(this);
  m_subtitleComboBox = new ModernMenuButton(this);

  streamSelectorsLayout->addWidget(m_versionComboBox);
  streamSelectorsLayout->addWidget(m_audioComboBox);
  streamSelectorsLayout->addWidget(m_subtitleComboBox);
  streamSelectorsLayout->addStretch();

  mainLayout->addLayout(actionsLayout);
  mainLayout->addLayout(streamSelectorsLayout);

  connect(m_playBtn, &QPushButton::clicked, this,
          &DetailActionWidget::playRequested);
  connect(m_resumeBtn, &QPushButton::clicked, this,
          &DetailActionWidget::resumeRequested);
  connect(m_favBtn, &QPushButton::clicked, this,
          &DetailActionWidget::favoriteRequested);
  connect(m_versionComboBox, &ModernMenuButton::currentIndexChanged, this,
          &DetailActionWidget::sourceVersionChanged);

  clear();
}

void DetailActionWidget::clear() {
  m_resumeBtn->hide();
  m_playBtn->setText(tr("▶ Play"));
  m_progressWidget->hide();

  m_versionComboBox->blockSignals(true);
  m_versionComboBox->clear();
  m_versionComboBox->hide();
  m_versionComboBox->blockSignals(false);

  m_audioComboBox->blockSignals(true);
  m_audioComboBox->clear();
  m_audioComboBox->hide();
  m_audioComboBox->blockSignals(false);

  m_subtitleComboBox->blockSignals(true);
  m_subtitleComboBox->clear();
  m_subtitleComboBox->hide();
  m_subtitleComboBox->blockSignals(false);
}

void DetailActionWidget::setupNormalMode(const MediaItem &item) {
  m_playBtn->setEnabled(true);
  if (item.userData.playedPercentage > 0 &&
      item.userData.playedPercentage < 100) {
    m_resumeBtn->show();
    m_playBtn->setText(tr("↺ Play from Beginning"));

    m_progressBar->setValue(static_cast<int>(item.userData.playedPercentage));
    long long remainingTicks =
        item.runTimeTicks - item.userData.playbackPositionTicks;
    if (remainingTicks > 0) {
      m_remainingTimeLabel->setText(
          tr("%1 remaining").arg(formatRunTime(remainingTicks)));
    } else {
      m_remainingTimeLabel->clear();
    }
    m_progressWidget->show();
  } else {
    m_resumeBtn->hide();
    m_playBtn->setText(tr("▶ Play"));
    m_progressWidget->hide();
  }
}

void DetailActionWidget::setSeriesLoadingMode() {
  m_resumeBtn->hide();
  m_playBtn->setText(tr("▶ Play"));
  m_playBtn->setEnabled(false);
  m_progressWidget->hide();
}

void DetailActionWidget::setupSeriesMode(const MediaItem &nextUpItem,
                                         const QString &epTag) {
  m_playBtn->setEnabled(true);
  if (nextUpItem.userData.playbackPositionTicks > 0) {
    m_resumeBtn->show();
    m_resumeBtn->setText(tr("▶ Resume %1").arg(epTag));
    m_playBtn->setText(tr("↺ Restart %1").arg(epTag));

    if (nextUpItem.userData.playedPercentage > 0) {
      m_progressBar->setValue(
          static_cast<int>(nextUpItem.userData.playedPercentage));
      long long remainingTicks =
          nextUpItem.runTimeTicks - nextUpItem.userData.playbackPositionTicks;
      if (remainingTicks > 0) {
        m_remainingTimeLabel->setText(
            tr("%1 remaining").arg(formatRunTime(remainingTicks)));
      } else {
        m_remainingTimeLabel->clear();
      }
      m_progressWidget->show();
    }
  } else {
    m_resumeBtn->hide();
    m_playBtn->setText(tr("▶ Play %1").arg(epTag));
    m_progressWidget->hide();
  }
}

void DetailActionWidget::setFavoriteState(bool isFavorite) {
  m_favBtn->setProperty("isFavorite", isFavorite);
  m_favBtn->setIcon(QIcon(isFavorite ? ":/svg/light/heart-fill.svg"
                                     : ":/svg/light/heart-outline.svg"));
  m_favBtn->style()->unpolish(m_favBtn);
  m_favBtn->style()->polish(m_favBtn);
}

void DetailActionWidget::setSources(const QList<MediaSourceInfo> &sources,
                                    int currentIndex) {
  m_versionComboBox->blockSignals(true);
  m_versionComboBox->clear();

  if (!sources.isEmpty()) {
    for (int i = 0; i < sources.size(); ++i) {
      const auto &src = sources[i];
      QString versionName =
          src.name.isEmpty() ? tr("Version %1").arg(i + 1) : src.name;
      QString videoInfo = tr("Unknown Video");
      for (const auto &stream : src.mediaStreams) {
        if (stream.type == "Video") {
          QStringList parts;
          if (stream.width > 0)
            parts << QString("%1x%2").arg(stream.width).arg(stream.height);
          if (!stream.codec.isEmpty())
            parts << stream.codec.toUpper();
          if (stream.realFrameRate > 0)
            parts << QString::number(stream.realFrameRate, 'f', 1) + " fps";
          if (stream.bitRate > 0)
            parts << QString("%1 Mbps").arg(stream.bitRate / 1000000.0, 0, 'f',
                                            1);
          if (!parts.isEmpty())
            videoInfo = parts.join(" · ");
          break;
        }
      }
      m_versionComboBox->addItem("🎞 " + versionName, videoInfo, QString(),
                                 src.id);
    }

    int bestIndex = currentIndex;
    if (sources.size() > 1) {
      bestIndex = MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
          sources,
          ConfigStore::instance()
              ->get<QString>(ConfigKeys::PlayerPreferredVersion)
              .trimmed());
    }

    m_versionComboBox->setCurrentIndex(bestIndex);
    m_versionComboBox->show();
  } else {
    m_versionComboBox->hide();
  }
  m_versionComboBox->blockSignals(false);
}

void DetailActionWidget::setStreams(const MediaSourceInfo &source) {
  m_audioComboBox->blockSignals(true);
  m_subtitleComboBox->blockSignals(true);
  m_audioComboBox->clear();
  m_subtitleComboBox->clear();

  m_subtitleComboBox->addItem("🚫 " + tr("No Subtitles"), QString(),
                              tr("Disable subtitle display"), -1);

  
  QString prefAudioLang = ConfigStore::instance()->get<QString>(
      ConfigKeys::PlayerAudioLang, "auto");
  QString prefSubLang =
      ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");
  const int preferredAudioStreamIndex =
      PlayerPreferenceUtils::findPreferredStreamIndex(
          source.mediaStreams, "Audio", prefAudioLang);
  const int preferredSubtitleStreamIndex =
      PlayerPreferenceUtils::findPreferredStreamIndex(
          source.mediaStreams, "Subtitle", prefSubLang);
  const bool subtitleDisabled =
      PlayerPreferenceUtils::isSubtitleDisabled(prefSubLang);

  int defaultAudioIdx = 0;
  int defaultSubIdx = 0; 
  bool subMatchedByPref = false;
  bool subMatchedByDefault = false; 

  for (const auto &stream : source.mediaStreams) {
    if (stream.type == "Audio") {
      QString title =
          stream.displayTitle.isEmpty() ? stream.language : stream.displayTitle;
      if (title.isEmpty())
        title = tr("Audio Track");
      QStringList parts;
      if (!stream.codec.isEmpty())
        parts << stream.codec.toUpper();
      if (stream.channels > 0)
        parts << QString("%1 ch").arg(stream.channels);
      if (stream.sampleRate > 0)
        parts << QString("%1 kHz").arg(stream.sampleRate / 1000.0, 0, 'f', 1);
      if (stream.bitRate > 0)
        parts << QString("%1 kbps").arg(stream.bitRate / 1000);

      
      QStringList secondLineParts;
      if (!stream.title.isEmpty())
        secondLineParts << stream.title;
      if (!parts.isEmpty())
        secondLineParts << parts.join(" · ");
      m_audioComboBox->addItem("🔊 " + title, QString(),
                               secondLineParts.join("  ·  "), stream.index);

      int curIdx = m_audioComboBox->count() - 1;
      if (preferredAudioStreamIndex >= 0 &&
          stream.index == preferredAudioStreamIndex) {
        defaultAudioIdx = curIdx;
      } else if (preferredAudioStreamIndex < 0 && stream.isDefault) {
        defaultAudioIdx = curIdx;
      }
    } else if (stream.type == "Subtitle") {
      QString title =
          stream.displayTitle.isEmpty() ? stream.language : stream.displayTitle;
      if (title.isEmpty())
        title = tr("Subtitle");
      QStringList parts;
      if (!stream.codec.isEmpty())
        parts << stream.codec.toUpper();
      if (stream.isForced)
        parts << tr("Forced");
      if (stream.isExternal)
        parts << tr("External");
      if (stream.isHearingImpaired)
        parts << tr("SDH");

      
      QStringList secondLineParts;
      if (!stream.title.isEmpty())
        secondLineParts << stream.title;
      if (!parts.isEmpty())
        secondLineParts << parts.join(" · ");
      m_subtitleComboBox->addItem("💬 " + title, QString(),
                                  secondLineParts.join("  ·  "), stream.index);

      int curIdx = m_subtitleComboBox->count() - 1;
      if (preferredSubtitleStreamIndex >= 0 &&
          stream.index == preferredSubtitleStreamIndex) {
        defaultSubIdx = curIdx;
        subMatchedByPref = true;
      } else if (preferredSubtitleStreamIndex < 0 && !subMatchedByPref &&
                 stream.isDefault) {
        defaultSubIdx = curIdx;
        subMatchedByDefault = true;
      }
    }
  }

  
  if (subtitleDisabled) {
    defaultSubIdx = 0; 
  } else if (!subMatchedByPref && !subMatchedByDefault &&
             m_subtitleComboBox->count() > 1) {
    
    
    defaultSubIdx = 1;
  }

  if (m_audioComboBox->count() > 0) {
    m_audioComboBox->setCurrentIndex(defaultAudioIdx);
    m_audioComboBox->show();
  } else {
    m_audioComboBox->hide();
  }

  if (m_subtitleComboBox->count() > 1) {
    m_subtitleComboBox->setCurrentIndex(defaultSubIdx);
    m_subtitleComboBox->show();
  } else {
    m_subtitleComboBox->hide();
  }

  m_audioComboBox->blockSignals(false);
  m_subtitleComboBox->blockSignals(false);
}

int DetailActionWidget::currentSourceIndex() const {
  return qMax(0, m_versionComboBox->currentIndex());
}
int DetailActionWidget::currentAudioIndex() const {
  return m_audioComboBox->isVisible() ? m_audioComboBox->currentData().toInt()
                                      : -1;
}
int DetailActionWidget::currentSubtitleIndex() const {
  return m_subtitleComboBox->isVisible()
             ? m_subtitleComboBox->currentData().toInt()
             : -1;
}

QString DetailActionWidget::formatRunTime(long long ticks) {
  long long totalSeconds = ticks / 10000000;
  long long hours = totalSeconds / 3600;
  long long minutes = (totalSeconds % 3600) / 60;
  if (hours > 0)
    return QString(tr("%1 hr %2 min")).arg(hours).arg(minutes);
  return QString(tr("%1 min")).arg(minutes);
}

void DetailActionWidget::refreshExtPlayerButton() {
  bool extEnabled =
      ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerEnable, false);
  if (!extEnabled) {
    m_extPlayerBtn->hide();
    return;
  }

  QString currentPath =
      ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath);

  
  QList<DetectedPlayer> allPlayers = ExternalPlayerDetector::loadFromConfig();

  
  QSet<QString> knownPaths;
  for (const auto &p : allPlayers)
    knownPaths.insert(p.path);
  if (!currentPath.isEmpty() && currentPath != "custom" &&
      !knownPaths.contains(currentPath) && QFileInfo::exists(currentPath)) {
    allPlayers.prepend({QFileInfo(currentPath).baseName(), currentPath});
  }

  if (allPlayers.isEmpty()) {
    m_extPlayerBtn->hide();
    return;
  }

  
  QString activePlayerPath = currentPath;
  if (activePlayerPath.isEmpty() || activePlayerPath == "custom") {
    activePlayerPath = allPlayers.first().path;
  }
  m_extPlayerBtn->setPlayers(allPlayers, activePlayerPath);
}
