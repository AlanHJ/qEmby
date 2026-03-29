#include "pageplayer.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernswitch.h"
#include "../../components/moderntaginput.h"
#include "../../components/playercombobox.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../managers/externalplayerdetector.h"
#include "../../managers/thememanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QStandardPaths>
#include <QTextStream>
#include <QVariantAnimation>
#include <memory>

PagePlayer::PagePlayer(QEmbyCore *core, QWidget *parent)
    : SettingsPageBase(core, tr("Player"), parent) {
  
  auto *hwCombo = new ModernComboBox(this);
  hwCombo->addItem(tr("Auto"), "auto-copy");
  hwCombo->addItem(tr("D3D11VA"), "d3d11va-copy");
  hwCombo->addItem(tr("DXVA2"), "dxva2-copy");
  hwCombo->addItem(tr("CUDA (NVIDIA)"), "cuda-copy");
  hwCombo->addItem(tr("None (Software)"), "no");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/hw-decode.svg", tr("Hardware Decoding"),
                       tr("Select GPU acceleration method for video decoding"),
                       hwCombo, ConfigKeys::PlayerHwDec, this));

  
  auto *voCombo = new ModernComboBox(this);
  voCombo->addItem(tr("libmpv (Embedded)"), "libmpv");
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/video-output.svg", tr("Video Output Driver"),
      tr("Rendering backend for embedded player (Render API mode)"), voCombo,
      ConfigKeys::PlayerVo, this));

  
  auto *vsyncCombo = new ModernComboBox(this);
  vsyncCombo->addItem(tr("Audio (Default)"), "audio");
  vsyncCombo->addItem(tr("Display Resample"), "display-resample");
  vsyncCombo->addItem(tr("Display VSync"), "display-vdrop");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/video-sync.svg", tr("Video Sync Mode"),
                       tr("Method for synchronizing video frames to display"),
                       vsyncCombo, ConfigKeys::PlayerVideoSync, this));

  
  auto *audioLangCombo = new ModernComboBox(this);
  audioLangCombo->addItem(tr("Auto"), "auto");
  audioLangCombo->addItem(tr("Chinese"), "chi");
  audioLangCombo->addItem(tr("English"), "eng");
  audioLangCombo->addItem(tr("Japanese"), "jpn");
  audioLangCombo->addItem(tr("Korean"), "kor");
  audioLangCombo->addItem(tr("French"), "fre");
  audioLangCombo->addItem(tr("German"), "ger");
  audioLangCombo->addItem(tr("Spanish"), "spa");
  audioLangCombo->addItem(tr("Russian"), "rus");
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/audio-lang.svg", tr("Preferred Audio Language"),
      tr("Automatically select audio track matching this language"),
      audioLangCombo, ConfigKeys::PlayerAudioLang, this));

  
  auto *subLangCombo = new ModernComboBox(this);
  subLangCombo->addItem(tr("Auto"), "auto");
  subLangCombo->addItem(tr("Chinese"), "chi");
  subLangCombo->addItem(tr("English"), "eng");
  subLangCombo->addItem(tr("Japanese"), "jpn");
  subLangCombo->addItem(tr("Korean"), "kor");
  subLangCombo->addItem(tr("French"), "fre");
  subLangCombo->addItem(tr("German"), "ger");
  subLangCombo->addItem(tr("Spanish"), "spa");
  subLangCombo->addItem(tr("Russian"), "rus");
  subLangCombo->addItem(tr("None"), "none");
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/subtitle-lang.svg", tr("Preferred Subtitle Language"),
      tr("Automatically select subtitle track matching this language"),
      subLangCombo, ConfigKeys::PlayerSubLang, this));

  
  auto *versionInput = new ModernTagInput(this);
  versionInput->addPreset(tr("Original"), "original");
  versionInput->addPreset(tr("4K"), "2160p");
  versionInput->addPreset(tr("1080p"), "1080p");
  versionInput->addPreset(tr("720p"), "720p");
  versionInput->addPreset(tr("480p"), "480p");
  versionInput->addPreset(tr("Remux"), "remux");
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/video-version.svg", tr("Preferred Video Version"),
      tr("Select preferred resolutions; type custom keywords and press Enter"),
      versionInput, ConfigKeys::PlayerPreferredVersion, this));

  
  auto *seekCombo = new ModernComboBox(this);
  seekCombo->addItem(tr("3 Seconds"), "3");
  seekCombo->addItem(tr("5 Seconds"), "5");
  seekCombo->addItem(tr("10 Seconds"), "10");
  seekCombo->addItem(tr("15 Seconds"), "15");
  seekCombo->addItem(tr("30 Seconds"), "30");
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/play-history.svg", tr("Seek Step"),
      tr("Number of seconds to skip when pressing forward or rewind"),
      seekCombo, ConfigKeys::PlayerSeekStep, this, QVariant("10")));

  
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/play-history.svg", tr("Long Press Seek"),
      tr("Hold forward/rewind buttons to continuously seek with acceleration"),
      new ModernSwitch(this), ConfigKeys::PlayerLongPressSeek, this,
      QVariant(true)));

  
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/click-pause.svg", tr("Click to Pause"),
      tr("Single click on the video area to toggle play/pause"),
      new ModernSwitch(this), ConfigKeys::PlayerClickToPause, this,
      QVariant(true)));

  
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/window-pop.svg", tr("Independent Player Window"),
      tr("Open video in a separate detached window"), new ModernSwitch(this),
      ConfigKeys::PlayerIndependentWindow, this));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/volume-normalize.svg", tr("Volume Normalization"),
      tr("Automatically adjust volume to prevent sudden loud noises"),
      new ModernSwitch(this), ConfigKeys::PlayerVolNormal, this));

  
  auto *mpvConfSwitch = new ModernSwitch(this);
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/mpv-conf.svg", tr("Use Custom mpv.conf"),
      tr("Load settings from an external mpv.conf configuration file"),
      mpvConfSwitch, ConfigKeys::PlayerUseMpvConf, this));

  
  auto *confPanel = new SettingsSubPanel(":/svg/dark/mpv-conf.svg", this);

  QString mpvConfDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
          .replace(QRegularExpression("[^/]+$"), "mpv");
  QString mpvConfPath = mpvConfDir + "/mpv.conf";

  auto *confPathLabel = new QLabel(
      tr("Config path: %1").arg(QDir::toNativeSeparators(mpvConfPath)), this);
  confPathLabel->setObjectName("SettingsCardDesc");
  confPathLabel->setWordWrap(true);
  confPanel->contentLayout()->addWidget(confPathLabel, 1);

  auto *openConfBtn = new QPushButton(tr("Open File"), this);
  openConfBtn->setObjectName("SettingsCardButton");
  openConfBtn->setCursor(Qt::PointingHandCursor);
  openConfBtn->setFixedHeight(30);
  confPanel->contentLayout()->addWidget(openConfBtn, 0, Qt::AlignVCenter);

  m_mainLayout->addWidget(confPanel);

  
  if (ConfigStore::instance()->get<bool>(ConfigKeys::PlayerUseMpvConf, false)) {
    confPanel->initExpanded();
  }

  
  connect(mpvConfSwitch, &ModernSwitch::toggled, confPanel,
          &SettingsSubPanel::setExpanded);

  
  connect(openConfBtn, &QPushButton::clicked, this,
          [mpvConfDir, mpvConfPath]() {
            QDir().mkpath(mpvConfDir);
            if (!QFile::exists(mpvConfPath)) {
              QFile file(mpvConfPath);
              if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "# MPV Configuration File for qEmby\n";
                out << "# Uncomment or add options below, one per line.\n";
                out << "# Reference: https://mpv.io/manual/stable/#options\n";
                out << "#\n";
                out << "# hwdec=auto-copy\n";
                out << "# video-sync=display-resample\n";
                out << "# deband=yes\n";
                file.close();
              }
            }
            QDesktopServices::openUrl(QUrl::fromLocalFile(mpvConfPath));
          });

  
  auto *extTitle = new QLabel(tr("External Player"), this);
  extTitle->setObjectName("SettingsSubTitle");
  m_mainLayout->addWidget(extTitle);

  QStringList supportedExternalPlayers;
#ifdef Q_OS_WIN
  supportedExternalPlayers = {QStringLiteral("PotPlayer"),
                              QStringLiteral("VLC"),
                              QStringLiteral("MPC-HC"),
                              QStringLiteral("MPC-BE"),
                              QStringLiteral("MPV")};
#elif defined(Q_OS_MAC)
  supportedExternalPlayers = {QStringLiteral("MPV"),
                              QStringLiteral("VLC"),
                              QStringLiteral("IINA")};
#else
  supportedExternalPlayers = {QStringLiteral("MPV"),
                              QStringLiteral("VLC"),
                              QStringLiteral("MPC-Qt")};
#endif
  const QString externalPlayerSupportText =
      tr("Supported players on this platform: %1")
          .arg(supportedExternalPlayers.join(QStringLiteral(", ")));

  auto *extSwitch = new ModernSwitch(this);
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/external-player.svg", tr("Enable External Player"),
      externalPlayerSupportText, extSwitch, ConfigKeys::ExtPlayerEnable,
      this));

  

  
  auto *playerPanel =
      new SettingsSubPanel(":/svg/dark/external-player.svg", this);
  auto *playerCombo = new PlayerComboBox(this);

  
  auto savePlayerListToConfig = [](const QList<DetectedPlayer> &players) {
    ExternalPlayerDetector::saveToConfig(players);
  };

  
  auto syncAllPlayersToConfig = [](PlayerComboBox *combo) {
    QJsonArray arr;
    for (int i = 0; i < combo->count(); ++i) {
      QString source =
          combo->itemData(i, PlayerComboBox::SourceRole).toString();
      if (source == PlayerComboBox::SourceCustom)
        continue;
      QString path = combo->itemData(i).toString();
      if (path.isEmpty())
        continue;
      QJsonObject obj;
      obj["name"] = QFileInfo(path).baseName();
      obj["path"] = path;
      arr.append(obj);
    }
    ConfigStore::instance()->set(
        ConfigKeys::ExtPlayerDetectedList,
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
  };

  
  QList<DetectedPlayer> detectedPlayers = ExternalPlayerDetector::detect();
  QString currentPath =
      ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath);
  int selectedIdx = -1;

  for (int i = 0; i < detectedPlayers.size(); ++i) {
    const auto &p = detectedPlayers[i];
    playerCombo->addPlayer(QString("%1  (%2)").arg(p.name, p.path), p.path,
                           PlayerComboBox::SourceAuto);
    if (p.path == currentPath)
      selectedIdx = i;
  }
  
  playerCombo->addPlayer(tr("Custom..."), "custom",
                         PlayerComboBox::SourceCustom);

  
  if (selectedIdx >= 0) {
    playerCombo->setCurrentIndex(selectedIdx);
  } else if (!currentPath.isEmpty() && currentPath != "custom") {
    playerCombo->addPlayer(QDir::toNativeSeparators(currentPath), currentPath,
                           PlayerComboBox::SourceManual);
    
    int manualIdx = playerCombo->findData(currentPath);
    if (manualIdx >= 0)
      playerCombo->setCurrentIndex(manualIdx);
  }

  
  syncAllPlayersToConfig(playerCombo);

  auto *playerLabel = new QLabel(tr("Player"), this);
  playerLabel->setObjectName("SettingsCardDesc");

  
  auto *searchBtn = new QPushButton(this);
  searchBtn->setObjectName("SettingsIconBtn");
  searchBtn->setCursor(Qt::PointingHandCursor);
  searchBtn->setFixedSize(28, 28);
  searchBtn->setToolTip(tr("Re-detect installed players"));

  playerPanel->contentLayout()->addWidget(playerLabel);
  playerPanel->contentLayout()->addWidget(playerCombo);
  playerPanel->contentLayout()->addWidget(searchBtn);

  m_mainLayout->addWidget(playerPanel);

  
  connect(
      searchBtn, &QPushButton::clicked, this,
      [this, playerCombo, savePlayerListToConfig, syncAllPlayersToConfig,
       searchBtn]() {
        
        bool reduceAnimations =
            ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
        if (!reduceAnimations) {
          
          auto originalPix = std::make_shared<QPixmap>(
              searchBtn->icon().pixmap(searchBtn->iconSize()));

          auto *rotAnim = new QVariantAnimation(searchBtn);
          rotAnim->setStartValue(0.0);
          rotAnim->setEndValue(360.0);
          rotAnim->setDuration(500);
          rotAnim->setEasingCurve(QEasingCurve::InOutCubic);

          connect(rotAnim, &QVariantAnimation::valueChanged, searchBtn,
                  [searchBtn, originalPix](const QVariant &val) {
                    qreal angle = val.toReal();
                    QTransform rot;
                    rot.rotate(angle);
                    QPixmap rotated =
                        originalPix->transformed(rot, Qt::SmoothTransformation);
                    
                    int dx = (rotated.width() - originalPix->width()) / 2;
                    int dy = (rotated.height() - originalPix->height()) / 2;
                    QPixmap cropped = rotated.copy(dx, dy, originalPix->width(),
                                                   originalPix->height());
                    searchBtn->setIcon(QIcon(cropped));
                  });

          connect(rotAnim, &QVariantAnimation::finished, searchBtn,
                  [searchBtn]() {
                    
                    searchBtn->style()->unpolish(searchBtn);
                    searchBtn->style()->polish(searchBtn);
                  });

          rotAnim->start(QAbstractAnimation::DeleteWhenStopped);
        }

        
        QString prevPath = playerCombo->currentPlayerPath();

        
        
        playerCombo->blockSignals(true);
        playerCombo->removeAutoDetected();

        QList<DetectedPlayer> newPlayers = ExternalPlayerDetector::detect();
        for (const auto &p : newPlayers) {
          playerCombo->addPlayer(QString("%1  (%2)").arg(p.name, p.path),
                                 p.path, PlayerComboBox::SourceAuto);
        }

        
        int prevIdx = playerCombo->findData(prevPath);
        if (prevIdx >= 0) {
          playerCombo->setCurrentIndex(prevIdx);
        } else if (playerCombo->count() > 0) {
          playerCombo->setCurrentIndex(0);
        }
        playerCombo->blockSignals(false);

        
        syncAllPlayersToConfig(playerCombo);

        
        QString syncPath = playerCombo->currentPlayerPath();
        if (syncPath != "custom") {
          ConfigStore::instance()->set(ConfigKeys::ExtPlayerPath, syncPath);
        }
      });

  
  connect(playerCombo, &PlayerComboBox::playerRemoved, this,
          [playerCombo, syncAllPlayersToConfig]() {
            syncAllPlayersToConfig(playerCombo);
            ConfigStore::instance()->set(ConfigKeys::ExtPlayerPath,
                                         playerCombo->currentPlayerPath());
          });

  
  connect(
      playerCombo, QOverload<int>::of(&QComboBox::activated), this,
      [this, playerCombo, syncAllPlayersToConfig](int idx) {
        if (idx < 0)
          return;
        QString source =
            playerCombo->itemData(idx, PlayerComboBox::SourceRole).toString();
        QString data = playerCombo->itemData(idx).toString();

        if (source == PlayerComboBox::SourceCustom) {
          QString filePath = QFileDialog::getOpenFileName(
              this, tr("Select External Player"), QString(),
#ifdef Q_OS_WIN
              tr("Executable Files (*.exe)")
#else
              tr("All Files (*)")
#endif
          );
          if (!filePath.isEmpty()) {
            filePath = QDir::toNativeSeparators(filePath);
            playerCombo->blockSignals(true);
            playerCombo->addPlayer(filePath, filePath,
                                   PlayerComboBox::SourceManual);
            int newIdx = playerCombo->findData(filePath);
            playerCombo->setCurrentIndex(newIdx);
            playerCombo->blockSignals(false);
            syncAllPlayersToConfig(playerCombo);
            ConfigStore::instance()->set(ConfigKeys::ExtPlayerPath, filePath);
          } else {
            
            QString prevPath = ConfigStore::instance()->get<QString>(
                ConfigKeys::ExtPlayerPath);
            int prevIdx = playerCombo->findData(prevPath);
            if (prevIdx >= 0) {
              playerCombo->blockSignals(true);
              playerCombo->setCurrentIndex(prevIdx);
              playerCombo->blockSignals(false);
            }
          }
        } else {
          ConfigStore::instance()->set(ConfigKeys::ExtPlayerPath, data);
        }
      });

  
  auto *argsPanel = new SettingsSubPanel(":/svg/dark/player-args.svg", this);
  auto *argsLabel = new QLabel(tr("Arguments"), this);
  argsLabel->setObjectName("SettingsCardDesc");
  auto *argsEdit = new QLineEdit(this);
  argsEdit->setPlaceholderText(tr("e.g., --fullscreen --volume=100"));
  argsEdit->setMinimumWidth(200);
  argsEdit->setFixedHeight(32);
  argsEdit->setText(
      ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerArgs));

  connect(argsEdit, &QLineEdit::editingFinished, this, [argsEdit]() {
    ConfigStore::instance()->set(ConfigKeys::ExtPlayerArgs, argsEdit->text());
  });

  argsPanel->contentLayout()->addWidget(argsLabel);
  argsPanel->contentLayout()->addWidget(argsEdit, 1);
  m_mainLayout->addWidget(argsPanel);

  
  auto *quickPlayPanel =
      new SettingsSubPanel(":/svg/dark/quick-play.svg", this);
  auto *quickPlayLabel =
      new QLabel(tr("Quick Play with External Player"), this);
  quickPlayLabel->setObjectName("SettingsCardDesc");
  auto *quickPlayDesc = new QLabel(
      tr("Use external player for quick play actions on cards and menus"),
      this);
  quickPlayDesc->setObjectName("SettingsCardDesc");
  quickPlayDesc->setWordWrap(true);
  auto *quickPlaySwitch = new ModernSwitch(this);
  quickPlaySwitch->setChecked(ConfigStore::instance()->get<bool>(
      ConfigKeys::ExtPlayerQuickPlay, false));
  connect(quickPlaySwitch, &ModernSwitch::toggled, this, [](bool checked) {
    ConfigStore::instance()->set(ConfigKeys::ExtPlayerQuickPlay, checked);
  });

  quickPlayPanel->contentLayout()->addWidget(quickPlayLabel);
  quickPlayPanel->contentLayout()->addWidget(quickPlayDesc, 1);
  quickPlayPanel->contentLayout()->addWidget(quickPlaySwitch);
  m_mainLayout->addWidget(quickPlayPanel);

  
  auto *directPanel =
      new SettingsSubPanel(":/svg/dark/direct-stream.svg", this);
  auto *directLabel = new QLabel(tr("Direct Stream"), this);
  directLabel->setObjectName("SettingsCardDesc");
  auto *directDesc =
      new QLabel(tr("Bypass server proxy and stream directly from source. "
                    "May not work on external networks"),
                 this);
  directDesc->setObjectName("SettingsCardDesc");
  directDesc->setWordWrap(true);
  auto *directSwitch = new ModernSwitch(this);
  directSwitch->setChecked(ConfigStore::instance()->get<bool>(
      ConfigKeys::ExtPlayerDirectStream, false));
  connect(directSwitch, &ModernSwitch::toggled, this, [](bool checked) {
    ConfigStore::instance()->set(ConfigKeys::ExtPlayerDirectStream, checked);
  });

  directPanel->contentLayout()->addWidget(directLabel);
  directPanel->contentLayout()->addWidget(directDesc, 1);
  directPanel->contentLayout()->addWidget(directSwitch);
  m_mainLayout->addWidget(directPanel);

  
  
  auto *replaceFrame = new QFrame(this);
  replaceFrame->setObjectName("UrlReplaceFrame");
  auto *replaceOuterLayout = new QHBoxLayout(replaceFrame);
  replaceOuterLayout->setContentsMargins(16, 10, 16, 10);
  replaceOuterLayout->setSpacing(12);

  
  auto *replaceIcon = new QLabel(replaceFrame);
  replaceIcon->setFixedSize(16, 16);
  replaceIcon->setAlignment(Qt::AlignCenter);
  const QString replaceIconPath = ":/svg/dark/url-replace.svg";
  replaceIcon->setPixmap(
      ThemeManager::getAdaptiveIcon(replaceIconPath).pixmap(16, 16));
  replaceOuterLayout->addWidget(replaceIcon, 0, Qt::AlignTop);

  
  auto *replaceVLayout = new QVBoxLayout();
  replaceVLayout->setContentsMargins(0, 0, 0, 0);
  replaceVLayout->setSpacing(4);

  auto *replaceLabel =
      new QLabel(tr("URL Path Replacement one rule per line: source => target"),
                 replaceFrame);
  replaceLabel->setObjectName("SettingsCardDesc");
  replaceLabel->setWordWrap(true);

  auto *replaceEdit = new QPlainTextEdit(replaceFrame);
  replaceEdit->setPlaceholderText(
      tr("e.g., http://192.168.2.1:19798/path/ => W:/mount/"));
  replaceEdit->setFixedHeight(80);
  replaceEdit->setPlainText(
      ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerUrlReplace));

  
  connect(ThemeManager::instance(), &ThemeManager::themeChanged, replaceFrame,
          [replaceIcon, replaceIconPath]() {
            replaceIcon->setPixmap(
                ThemeManager::getAdaptiveIcon(replaceIconPath).pixmap(16, 16));
          });

  connect(replaceEdit, &QPlainTextEdit::textChanged, this, [replaceEdit]() {
    ConfigStore::instance()->set(ConfigKeys::ExtPlayerUrlReplace,
                                 replaceEdit->toPlainText());
  });

  replaceVLayout->addWidget(replaceLabel);
  replaceVLayout->addWidget(replaceEdit);
  replaceOuterLayout->addLayout(replaceVLayout, 1);
  m_mainLayout->addWidget(replaceFrame);

  
  bool extEnabled =
      ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerEnable, false);
  if (extEnabled) {
    playerPanel->initExpanded();
    argsPanel->initExpanded();
    directPanel->initExpanded();
    quickPlayPanel->initExpanded();
  }
  replaceFrame->setVisible(extEnabled);

  connect(extSwitch, &ModernSwitch::toggled, playerPanel,
          &SettingsSubPanel::setExpanded);
  connect(extSwitch, &ModernSwitch::toggled, argsPanel,
          &SettingsSubPanel::setExpanded);
  connect(extSwitch, &ModernSwitch::toggled, directPanel,
          &SettingsSubPanel::setExpanded);
  connect(extSwitch, &ModernSwitch::toggled, quickPlayPanel,
          &SettingsSubPanel::setExpanded);
  connect(extSwitch, &ModernSwitch::toggled, replaceFrame,
          &QWidget::setVisible);

  
  m_mainLayout->addStretch();
}
