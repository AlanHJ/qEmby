#include "pagegeneral.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernmessagebox.h"
#include "../../components/modernswitch.h"
#include "../../components/moderntoast.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../managers/logmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QUrl>

PageGeneral::PageGeneral(QEmbyCore *core, QWidget *parent)
    : SettingsPageBase(core, tr("General"), parent) {
  
  auto *langCombo = new ModernComboBox(this);
  langCombo->addItem(tr("System"), "system");
  langCombo->addItem(tr("简体中文"), "zh_CN");
  langCombo->addItem(tr("English"), "en_US");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/language.svg", tr("Language"),
                       tr("App interface language (requires restart)"),
                       langCombo, ConfigKeys::Language, this));

  
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/remember-server.svg", tr("Remember Last Server"),
      tr("Automatically connect to the last used server on startup"),
      new ModernSwitch(this), ConfigKeys::RememberServer, this));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/tray.svg", tr("Close to Tray"),
      tr("Minimize to system tray instead of exiting the application"),
      new ModernSwitch(this), ConfigKeys::CloseToTray, this));

  
  auto *logSwitch = new ModernSwitch(this);
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/log.svg", tr("Enable Logging"),
      tr("Record debug information to a log file for troubleshooting"),
      logSwitch, ConfigKeys::LogEnable, this));

  
  auto *logPanel = new SettingsSubPanel(":/svg/dark/log-path.svg", this);

  
  auto *logPathLabel = new QLabel(
      tr("Log path: %1")
          .arg(QDir::toNativeSeparators(LogManager::instance()->logFilePath())),
      this);
  logPathLabel->setObjectName("SettingsCardDesc");
  logPathLabel->setWordWrap(true);
  logPanel->contentLayout()->addWidget(logPathLabel, 1);

  m_mainLayout->addWidget(logPanel);

  
  auto *logActionsPanel = new SettingsSubPanel(":/svg/dark/log-tools.svg", this);

  
  auto *logSizeLabel =
      new QLabel(tr("Size: %1").arg(LogManager::instance()->logFileSize()),
                 this);
  logSizeLabel->setObjectName("SettingsCardDesc");
  logActionsPanel->contentLayout()->addWidget(logSizeLabel, 1);

  
  auto *openLogBtn = new QPushButton(tr("Open Log"), this);
  openLogBtn->setObjectName("SettingsCardButton");
  openLogBtn->setCursor(Qt::PointingHandCursor);
  openLogBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(openLogBtn, 0,
                                              Qt::AlignVCenter);

  
  auto *clearLogBtn = new QPushButton(tr("Clear Log"), this);
  clearLogBtn->setObjectName("SettingsCardButton");
  clearLogBtn->setCursor(Qt::PointingHandCursor);
  clearLogBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(clearLogBtn, 0,
                                              Qt::AlignVCenter);

  
  auto *openDirBtn = new QPushButton(tr("Open Location"), this);
  openDirBtn->setObjectName("SettingsCardButton");
  openDirBtn->setCursor(Qt::PointingHandCursor);
  openDirBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(openDirBtn, 0,
                                              Qt::AlignVCenter);

  m_mainLayout->addWidget(logActionsPanel);

  
  if (ConfigStore::instance()->get<bool>(ConfigKeys::LogEnable, false)) {
    logPanel->initExpanded();
    logActionsPanel->initExpanded();
  }

  
  connect(logSwitch, &ModernSwitch::toggled, logPanel,
          &SettingsSubPanel::setExpanded);
  connect(logSwitch, &ModernSwitch::toggled, logActionsPanel,
          &SettingsSubPanel::setExpanded);

  
  connect(logSwitch, &ModernSwitch::toggled, this,
          [logSizeLabel](bool checked) {
            if (checked) {
              LogManager::instance()->enable();
            } else {
              LogManager::instance()->disable();
            }
            logSizeLabel->setText(QObject::tr("Size: %1").arg(
                LogManager::instance()->logFileSize()));
          });

  
  connect(openLogBtn, &QPushButton::clicked, this, []() {
    QString logPath = LogManager::instance()->logFilePath();
    if (QFileInfo::exists(logPath)) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
    }
  });

  
  connect(clearLogBtn, &QPushButton::clicked, this, [this, logSizeLabel]() {
    const bool confirmed = ModernMessageBox::question(
        this, tr("Clear Log"),
        tr("Are you sure you want to clear all log files?\n\n"
           "This will remove the current log and rotated backups. This action "
           "cannot be undone."),
        tr("Clear"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!confirmed) {
      return;
    }

    LogManager::instance()->clearLog();
    logSizeLabel->setText(
        QObject::tr("Size: %1").arg(LogManager::instance()->logFileSize()));
    ModernToast::showMessage(tr("Log files cleared"), 1500);
  });

  
  connect(openDirBtn, &QPushButton::clicked, this, []() {
    QString logDir =
        QFileInfo(LogManager::instance()->logFilePath()).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
  });

  m_mainLayout->addStretch();
}
