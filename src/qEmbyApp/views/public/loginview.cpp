#include "loginview.h"
#include "../../components/loadingoverlay.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernmessagebox.h"
#include "../../components/modernswitch.h"
#include "../../components/serverwheelview.h"
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <models/profile/serverprofile.h>
#include <qembycore.h>
#include <services/auth/authservice.h>
#include <services/manager/servermanager.h>


class CardEventFilter : public QObject {
public:
  CardEventFilter(QWidget *actionContainer, std::function<void()> onClick,
                  QObject *parent = nullptr)
      : QObject(parent), m_actionContainer(actionContainer),
        m_onClick(onClick) {}

protected:
  bool eventFilter(QObject *obj, QEvent *event) override {
    QWidget *card = static_cast<QWidget *>(obj);
    
    bool isDepthCard = card->property("isDepthCard").toBool();

    if (event->type() == QEvent::Enter) {
      
      if (!isDepthCard)
        m_actionContainer->show();
    } else if (event->type() == QEvent::Leave) {
      m_actionContainer->hide();
    } else if (event->type() == QEvent::MouseButtonRelease) {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        m_onClick();
        return true;
      }
    }

    
    if (isDepthCard && m_actionContainer->isVisible()) {
      m_actionContainer->hide();
    }

    return QObject::eventFilter(obj, event);
  }

private:
  QWidget *m_actionContainer;
  std::function<void()> m_onClick;
};

LoginView::LoginView(QEmbyCore *core, QWidget *parent)
    : QWidget(parent), m_core(core) {
  setupUi();

  
  connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &LoginView::onThemeChanged);
}

void LoginView::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  
  if (m_loadingOverlay) {
    m_loadingOverlay->hide();
  }
  refreshServerList();

  
  if (!m_autoLoginAttempted && m_loadingOverlay != nullptr) {
    m_autoLoginAttempted = true; 

    qDebug() << "LoginView::showEvent: auto login checks starting";

    bool rememberServer =
        ConfigStore::instance()->get<bool>(ConfigKeys::RememberServer, false);
    QString lastServerId =
        ConfigStore::instance()->get<QString>(ConfigKeys::LastSelectedServerId);

    qDebug() << "LoginView::showEvent: rememberServer =" << rememberServer
             << ", lastServerId =" << lastServerId;

    if (rememberServer && !lastServerId.isEmpty()) {
      
      bool serverExists = false;
      auto servers = m_core->serverManager()->servers();
      qDebug() << "LoginView::showEvent: checking if server exists among"
               << servers.size() << "servers";

      for (const auto &s : servers) {
        if (s.id == lastServerId) {
          serverExists = true;
          break;
        }
      }

      qDebug() << "LoginView::showEvent: serverExists =" << serverExists;

      if (serverExists) {
        
        
        qDebug() << "LoginView::showEvent: setting up QTimer::singleShot(0)";
        QTimer::singleShot(0, this, [this, lastServerId]() {
          qDebug() << "LoginView::showEvent delayed: starting m_loadingOverlay";
          
          m_loadingOverlay->start();

          qDebug() << "LoginView::showEvent delayed: setting up "
                      "QTimer::singleShot(800)";
          
          QTimer::singleShot(800, this, [this, lastServerId]() {
            
            qDebug() << "LoginView::showEvent 800ms delayed: checking 'this' "
                        "pointer";
            if (this) {
              qDebug() << "LoginView::showEvent 800ms delayed: calling "
                          "onServerCardClicked";
              onServerCardClicked(lastServerId);
            } else {
              qDebug() << "LoginView::showEvent 800ms delayed: 'this' pointer "
                          "is null, aborting";
            }
          });
        });
      }
    }
  }
}

void LoginView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  
  if (m_loadingOverlay) {
    m_loadingOverlay->resize(event->size());
  }
}


QString LoginView::getThemeSvgPath(const QString &iconName) const {
  return ThemeManager::instance()->isDarkMode() ? (":/svg/dark/" + iconName)
                                                : (":/svg/light/" + iconName);
}

void LoginView::onThemeChanged(ThemeManager::Theme theme) {
  Q_UNUSED(theme);

  
  if (m_togglePwdAction) {
    if (m_togglePwdAction->isChecked()) {
      m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye-off.svg")));
    } else {
      m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye.svg")));
    }
  }

  
  
  if (m_pageSwitcher->currentWidget() == m_listPage) {
    refreshServerList();
  }
}

void LoginView::updateSslOptionsVisibility() {
    const bool isHttps =
        m_protocolInput != nullptr &&
        m_protocolInput->currentText().compare("https://", Qt::CaseInsensitive) ==
            0;
  
    if (m_sslOptionsRow != nullptr) {
      m_sslOptionsRow->setVisible(isHttps);
    }
  
    if (!isHttps && m_ignoreSslSwitch != nullptr) {
      m_ignoreSslSwitch->setChecked(false);
    }
  }

void LoginView::syncProtocolSelectionFromUrlText(const QString &text) {
  if (m_protocolInput == nullptr) {
    return;
  }

  const QString trimmedText = text.trimmed();
  int targetIndex = -1;
  if (trimmedText.startsWith("https://", Qt::CaseInsensitive)) {
    targetIndex = 1;
  } else if (trimmedText.startsWith("http://", Qt::CaseInsensitive)) {
    targetIndex = 0;
  }

  if (targetIndex != -1 && m_protocolInput->currentIndex() != targetIndex) {
    const QSignalBlocker blocker(m_protocolInput);
    m_protocolInput->setCurrentIndex(targetIndex);
    updateSslOptionsVisibility();
  }
}

QUrl LoginView::buildNormalizedServerUrl(QString *errorMessage) const {
  const QString rawAddress = m_serverAddressInput->text().trimmed();
  const QString rawPort = m_portInput != nullptr ? m_portInput->text().trimmed()
                                                 : QString();
  QString candidate = rawAddress;

  int portFromField = -1;
  if (!rawPort.isEmpty()) {
    bool ok = false;
    const int parsedPort = rawPort.toInt(&ok);
    if (!ok || parsedPort < 1 || parsedPort > 65535) {
      if (errorMessage != nullptr) {
        *errorMessage = tr("Please enter a valid port number.");
      }
      qWarning() << "[LoginView] Invalid port input" << "| port:" << rawPort;
      return QUrl();
    }
    portFromField = parsedPort;
  }

  if (!candidate.startsWith("http://", Qt::CaseInsensitive) &&
      !candidate.startsWith("https://", Qt::CaseInsensitive)) {
    candidate.prepend(m_protocolInput->currentText());
  }

  QUrl normalizedUrl(candidate, QUrl::TolerantMode);
  if (!normalizedUrl.isValid() || normalizedUrl.host().isEmpty()) {
    if (errorMessage != nullptr) {
      *errorMessage = tr("Please enter a valid server address.");
    }
    return QUrl();
  }

  const QString scheme = normalizedUrl.scheme().toLower();
  if (scheme != "http" && scheme != "https") {
    if (errorMessage != nullptr) {
      *errorMessage = tr("Server URL must use http:// or https://.");
    }
    return QUrl();
  }

  if (!normalizedUrl.query().isEmpty() || !normalizedUrl.fragment().isEmpty()) {
    qWarning() << "[LoginView] Stripping query or fragment from server URL"
               << "| original:" << candidate;
  }
  if (!normalizedUrl.userName().isEmpty() ||
      !normalizedUrl.password().isEmpty()) {
    qWarning() << "[LoginView] Stripping embedded credentials from server URL"
               << "| original:" << candidate;
  }

  normalizedUrl.setUserName(QString());
  normalizedUrl.setPassword(QString());
  normalizedUrl.setQuery(QString());
  normalizedUrl.setFragment(QString());

  const int parsedPort = normalizedUrl.port(-1);
  if (portFromField != -1 && parsedPort != portFromField) {
    if (parsedPort != -1) {
      qDebug() << "[LoginView] Overriding URL port with port field"
               << "| parsedPort:" << parsedPort
               << "| inputPort:" << portFromField;
    }
    normalizedUrl.setPort(portFromField);
  }

  QString normalizedPath = normalizedUrl.path(QUrl::FullyDecoded);
  if (normalizedPath == "/") {
    normalizedPath.clear();
  }
  while (normalizedPath.size() > 1 && normalizedPath.endsWith('/')) {
    normalizedPath.chop(1);
  }
  normalizedUrl.setPath(normalizedPath);

  qDebug() << "[LoginView] Normalized server URL"
           << "| address:" << displayServerAddress(normalizedUrl)
           << "| port:" << normalizedUrl.port(-1)
           << "| finalUrl:" << normalizedUrl.toString(QUrl::FullyEncoded);

  return normalizedUrl;
}

QString LoginView::displayServerAddress(const QUrl &url) const {
  QString address = url.host(QUrl::FullyDecoded);
  if (address.contains(':') && !address.startsWith('[')) {
    address = QStringLiteral("[%1]").arg(address);
  }
  const QString path = url.path(QUrl::FullyDecoded);
  if (!path.isEmpty() && path != "/") {
    address += path;
  }
  return address;
}

void LoginView::applyServerUrlToForm(const QUrl &url,
                                     bool ignoreSslVerification) {
  if (m_protocolInput == nullptr || m_serverAddressInput == nullptr ||
      m_portInput == nullptr) {
    return;
  }

  {
    const QSignalBlocker blocker(m_protocolInput);
    m_protocolInput->setCurrentIndex(
        url.scheme().compare("https", Qt::CaseInsensitive) == 0 ? 1 : 0);
  }

  {
    const QSignalBlocker blocker(m_serverAddressInput);
    m_serverAddressInput->setText(displayServerAddress(url));
  }

  {
    const QSignalBlocker blocker(m_portInput);
    const int port = url.port(-1);
    m_portInput->setText(port > 0 ? QString::number(port) : QString());
  }

  if (m_ignoreSslSwitch != nullptr) {
    m_ignoreSslSwitch->setChecked(ignoreSslVerification);
  }
  updateSslOptionsVisibility();
}

void LoginView::setupUi() {
  this->setProperty("showGlobalSearch", false);
  this->setProperty("viewTitle", tr("qEmby"));
  this->setProperty("showGlobalBack", false);
  this->setProperty("showGlobalHome", false);
  this->setProperty("showGlobalFav", false);
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 8); 

  m_pageSwitcher = new QStackedWidget(this);
  m_pageSwitcher->setFixedWidth(360);

  setupListPage();
  setupAddPage();

  m_pageSwitcher->addWidget(m_listPage);
  m_pageSwitcher->addWidget(m_addPage);

  mainLayout->addStretch();
  mainLayout->addWidget(m_pageSwitcher, 0, Qt::AlignHCenter);
  mainLayout->addStretch();

  
  QString orgName = qApp->organizationName();
  QString version = qApp->applicationVersion();
  if (version.isEmpty())
    version = "1.0.0";
  if (orgName.isEmpty())
    orgName = "xiaozhe";

  auto *versionLabel =
      new QLabel(QString("%1 v%2").arg(orgName, version), this);
  versionLabel->setObjectName("app-version-label");
  mainLayout->addWidget(versionLabel, 0, Qt::AlignHCenter);

  
  m_loadingOverlay = new LoadingOverlay(this);
}

void LoginView::setupListPage() {
  m_listPage = new QWidget(this);
  auto *layout = new QVBoxLayout(m_listPage);

  
  
  
  layout->setSpacing(0);
  layout->setContentsMargins(8, 8, 8, 8);

  auto *titleLabel = new QLabel(tr("Select Server"), this);
  titleLabel->setObjectName("login-title");
  titleLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(titleLabel);
  layout->addSpacing(8);

  
  m_wheelView = new ServerWheelView(this);
  
  m_wheelView->setFixedSize(340, 220);
  layout->addWidget(m_wheelView, 0, Qt::AlignHCenter);
}

void LoginView::setupAddPage() {
    m_addPage = new QWidget(this);
    m_addPage->setObjectName("login-add-page");
    m_addPage->setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(m_addPage);
    layout->setSpacing(8);
    layout->setContentsMargins(8, 8, 8, 8);

  auto *titleLabel = new QLabel(tr("Connect to Server"), this);
  titleLabel->setObjectName("login-title");
  titleLabel->setAlignment(Qt::AlignCenter);
  layout->addWidget(titleLabel);

    m_errorLabel = new QLabel(this);
    m_errorLabel->setObjectName("login-error-label");
    m_errorLabel->setAlignment(Qt::AlignCenter);
    m_errorLabel->setWordWrap(true);
    m_errorLabel->hide();
    layout->addWidget(m_errorLabel);
  
  const QString serverAddressTooltip = tr(
      "Server Address (e.g. example.com/emby or 192.168.1.1/jellyfin)");
  m_serverAddressInput = new QLineEdit(this);
  m_serverAddressInput->setPlaceholderText(tr("Server Address"));
  m_serverAddressInput->setToolTip(serverAddressTooltip);
  m_serverAddressInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  layout->addWidget(m_serverAddressInput);

    auto *protoPortLayout = new QHBoxLayout();
    protoPortLayout->setSpacing(10);

  m_protocolInput = new ModernComboBox(this);
  m_protocolInput->setObjectName("login-protocol-combo");
  m_protocolInput->addItems({"http://", "https://"});
  m_protocolInput->setCursor(Qt::PointingHandCursor);
  m_protocolInput->setFixedWidth(qMax(126, m_protocolInput->sizeHint().width()));

  m_portInput = new QLineEdit(this);
  m_portInput->setPlaceholderText(tr("Port"));
  m_portInput->setToolTip(tr("Port (e.g. 8096)"));
  m_portInput->setValidator(new QIntValidator(1, 65535, m_portInput));
  m_portInput->setMaxLength(5);
  m_portInput->setMinimumWidth(110);
  m_portInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_portInput->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

  protoPortLayout->addWidget(m_protocolInput, 0);
  protoPortLayout->addWidget(m_portInput, 1);
  layout->addLayout(protoPortLayout);

    m_sslOptionsRow = new QWidget(this);
    m_sslOptionsRow->setObjectName("login-ssl-card");
    m_sslOptionsRow->setAttribute(Qt::WA_StyledBackground, true);
    auto *sslOptionsLayout = new QHBoxLayout(m_sslOptionsRow);
    sslOptionsLayout->setContentsMargins(14, 8, 10, 8);
    sslOptionsLayout->setSpacing(10);
  
    const QString sslTooltip = tr(
        "Use this only for trusted self-signed or private CA HTTPS servers.");

  auto *sslTitleLabel =
      new QLabel(tr("Ignore SSL certificate verification"), m_sslOptionsRow);
  sslTitleLabel->setObjectName("login-ssl-title");
  sslTitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  sslTitleLabel->setToolTip(sslTooltip);

  m_ignoreSslSwitch = new ModernSwitch(m_sslOptionsRow);
  m_ignoreSslSwitch->setToolTip(sslTooltip);

  sslOptionsLayout->addWidget(sslTitleLabel, 1);
  sslOptionsLayout->addWidget(m_ignoreSslSwitch, 0, Qt::AlignVCenter);
  m_sslOptionsRow->setToolTip(sslTooltip);
  layout->addWidget(m_sslOptionsRow);

  m_usernameInput = new QLineEdit(this);
  m_usernameInput->setPlaceholderText(tr("Username"));
  layout->addWidget(m_usernameInput);

  m_passwordInput = new QLineEdit(this);
  m_passwordInput->setPlaceholderText(tr("Password"));
  m_passwordInput->setEchoMode(QLineEdit::Password);

  
  m_togglePwdAction =
      new QAction(QIcon(getThemeSvgPath("eye.svg")), tr("Show Password"), this);
  m_togglePwdAction->setCheckable(true);
  m_passwordInput->addAction(m_togglePwdAction, QLineEdit::TrailingPosition);

  connect(m_togglePwdAction, &QAction::toggled, this, [this](bool checked) {
    if (checked) {
      m_passwordInput->setEchoMode(QLineEdit::Normal);
      m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye-off.svg")));
    } else {
      m_passwordInput->setEchoMode(QLineEdit::Password);
      m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye.svg")));
    }
  });

  layout->addWidget(m_passwordInput);

  auto *btnLayout = new QHBoxLayout();
  btnLayout->setSpacing(8);

  auto *cancelBtn = new QPushButton(tr("Cancel"), this);
  cancelBtn->setCursor(Qt::PointingHandCursor);
  connect(cancelBtn, &QPushButton::clicked, this, &LoginView::showListPage);

  m_loginButton = new QPushButton(tr("Login"), this);
  m_loginButton->setObjectName("login-button");
  m_loginButton->setCursor(Qt::PointingHandCursor);
  connect(m_loginButton, &QPushButton::clicked, this,
          &LoginView::onLoginClicked);

  btnLayout->addWidget(cancelBtn);
  btnLayout->addWidget(m_loginButton);
  layout->addLayout(btnLayout);

  layout->addStretch();

  
  
  
  connect(m_protocolInput, &QComboBox::currentTextChanged, this,
          [this](const QString &) { updateSslOptionsVisibility(); });
  connect(m_serverAddressInput, &QLineEdit::textChanged, this,
          &LoginView::syncProtocolSelectionFromUrlText);
  connect(m_serverAddressInput, &QLineEdit::returnPressed, this,
          &LoginView::onLoginClicked);
  connect(m_portInput, &QLineEdit::returnPressed, this,
          &LoginView::onLoginClicked);
  connect(m_usernameInput, &QLineEdit::returnPressed, this,
          &LoginView::onLoginClicked);
  connect(m_passwordInput, &QLineEdit::returnPressed, this,
          &LoginView::onLoginClicked);

  updateSslOptionsVisibility();
}

void LoginView::refreshServerList() {
  int lastSelectedIndex = m_wheelView->currentIndex();
  m_wheelView->clear();

  QList<ServerProfile> servers = m_core->serverManager()->servers();
  if (servers.isEmpty()) {
    showAddPage();
    return;
  }

  
  
  
  
  auto createAddCardNode = [this]() {
    auto *addCard = new QWidget(this);
    addCard->setObjectName("server-card");
    addCard->setAttribute(Qt::WA_StyledBackground, true);
    addCard->setCursor(Qt::PointingHandCursor);

    auto *addCardLayout = new QHBoxLayout(addCard);
    addCardLayout->setContentsMargins(15, 8, 15, 8);

    auto *addIconLabel = new QLabel(this);
    addIconLabel->setFixedSize(40, 40);
    addIconLabel->setScaledContents(true);
    addIconLabel->setAlignment(Qt::AlignCenter);

    
    addIconLabel->setPixmap(QPixmap(getThemeSvgPath("add.svg")));

    auto *addInfoLayout = new QVBoxLayout();
    addInfoLayout->setSpacing(2);

    
    auto *addNameLabel = new QLabel(tr("Add New Server"), this);
    addNameLabel->setObjectName("server-name-label");
    QFont nFont = addNameLabel->font();
    nFont.setPixelSize(15);
    nFont.setBold(true);
    addNameLabel->setFont(nFont);

    
    auto *addDescLabel = new QLabel(tr("Connect to another server"), this);
    addDescLabel->setObjectName("server-url-label");
    QFont uFont = addDescLabel->font();
    uFont.setPixelSize(12);
    addDescLabel->setFont(uFont);

    addInfoLayout->addWidget(addNameLabel);
    addInfoLayout->addWidget(addDescLabel);

    auto *dummyActionContainer = new QWidget(this);
    dummyActionContainer->hide();

    addCardLayout->addWidget(addIconLabel);
    addCardLayout->addSpacing(12);
    addCardLayout->addLayout(addInfoLayout);
    addCardLayout->addStretch();
    addCardLayout->addWidget(dummyActionContainer);

    int addCardIndex = m_wheelView->count();

    auto onAddClick = [this, addCardIndex]() {
      int N = m_wheelView->count();
      int currentIndex = m_wheelView->currentIndex();
      int diff = qAbs(currentIndex - addCardIndex);
      int distance = qMin(diff, N - diff);

      
      if (m_wheelView->currentIndex() != addCardIndex) {
        m_wheelView->setCurrentIndex(addCardIndex, true);
      }

      
      
      if (distance <= 1) {
        showAddPage();
      }
    };
    addCard->installEventFilter(
        new CardEventFilter(dummyActionContainer, onAddClick, addCard));

    m_wheelView->addCard(addCard);
  };

  
  
  
  
  
  int baseCount = 1 + servers.size();
  int repeatCount = 1;
  while (baseCount * repeatCount < 5) {
    repeatCount++;
  }

  
  for (int r = 0; r < repeatCount; ++r) {

    createAddCardNode(); 

    for (const auto &server : servers) {
      auto *card = new QWidget(this);
      card->setObjectName("server-card");
      card->setAttribute(Qt::WA_StyledBackground, true);
      card->setCursor(Qt::PointingHandCursor);

      auto *cardLayout = new QHBoxLayout(card);
      cardLayout->setContentsMargins(15, 8, 15, 8);

      auto *iconLabel = new QLabel(this);
      iconLabel->setFixedSize(40, 40);
      iconLabel->setScaledContents(true);
      iconLabel->setAlignment(Qt::AlignCenter);

      if (server.type == ServerProfile::Jellyfin) {
        iconLabel->setPixmap(QPixmap(":/svg/jellyfin.svg"));
      } else {
        if (!server.iconBase64.isEmpty()) {
          QPixmap pix;
          pix.loadFromData(QByteArray::fromBase64(server.iconBase64.toUtf8()));
          iconLabel->setPixmap(pix);
        } else {
          iconLabel->setObjectName("server-fallback-icon");
          iconLabel->setText(server.type == ServerProfile::Emby ? "E" : "J");

          
          QFont fallbackFont = iconLabel->font();
          fallbackFont.setPixelSize(18);
          fallbackFont.setBold(true);
          iconLabel->setFont(fallbackFont);
        }
      }

      auto *infoLayout = new QVBoxLayout();
      infoLayout->setSpacing(2);

      
      auto *nameLabel = new QLabel(server.name, this);
      nameLabel->setObjectName("server-name-label");
      QFont nFont = nameLabel->font();
      nFont.setPixelSize(15);
      nFont.setBold(true);
      nameLabel->setFont(nFont);

      
      auto *urlLabel = new QLabel(server.url, this);
      urlLabel->setObjectName("server-url-label");
      QFont uFont = urlLabel->font();
      uFont.setPixelSize(12);
      urlLabel->setFont(uFont);

      infoLayout->addWidget(nameLabel);
      infoLayout->addWidget(urlLabel);

      auto *actionContainer = new QWidget(this);
      auto *actionLayout = new QHBoxLayout(actionContainer);
      actionLayout->setContentsMargins(0, 0, 0, 0);
      actionLayout->setSpacing(8);

      auto *editBtn = new QPushButton(this);
      editBtn->setObjectName("action-edit-btn");
      editBtn->setToolTip(tr("Edit"));
      editBtn->setCursor(Qt::PointingHandCursor);
      connect(editBtn, &QPushButton::clicked, this,
              [this, id = server.id]() { onEditServerClicked(id); });

      auto *delBtn = new QPushButton(this);
      delBtn->setObjectName("action-del-btn");
      delBtn->setToolTip(tr("Delete"));
      delBtn->setCursor(Qt::PointingHandCursor);
      connect(delBtn, &QPushButton::clicked, this,
              [this, id = server.id]() { onRemoveServerClicked(id); });

      actionLayout->addWidget(editBtn);
      actionLayout->addWidget(delBtn);
      actionContainer->hide();

      cardLayout->addWidget(iconLabel);
      cardLayout->addSpacing(12);
      cardLayout->addLayout(infoLayout);
      cardLayout->addStretch();
      cardLayout->addWidget(actionContainer);

      int cardIndex = m_wheelView->count();

      auto onClick = [this, id = server.id, cardIndex]() {
        int N = m_wheelView->count();
        int currentIndex = m_wheelView->currentIndex();
        int diff = qAbs(currentIndex - cardIndex);
        int distance = qMin(diff, N - diff);

        
        if (m_wheelView->currentIndex() != cardIndex) {
          m_wheelView->setCurrentIndex(cardIndex, true);
        }

        
        if (distance <= 1) {
          onServerCardClicked(id);
        }
      };
      card->installEventFilter(
          new CardEventFilter(actionContainer, onClick, card));

      m_wheelView->addCard(card);
    }
  }

  int totalCount = m_wheelView->count();
  int targetIndex = 1;

  
  
  
  if (lastSelectedIndex > 0 && lastSelectedIndex < totalCount) {
    targetIndex = lastSelectedIndex;
  } else {
    QString lastServerId =
        ConfigStore::instance()->get<QString>(ConfigKeys::LastSelectedServerId);
    if (!lastServerId.isEmpty()) {
      for (int i = 0; i < servers.size(); ++i) {
        if (servers[i].id == lastServerId) {
          targetIndex = i + 1; 
          break;
        }
      }
    }
  }

  targetIndex = qBound(0, targetIndex, totalCount - 1);
  m_wheelView->setCurrentIndex(targetIndex, false);

  
  m_wheelView->setTransitionMode(true);
  
  
  QTimer::singleShot(450, m_wheelView, [this]() {
    if (m_wheelView)
      m_wheelView->setTransitionMode(
          false); 
  });

  m_editingServerId.clear();
  m_pageSwitcher->setCurrentWidget(m_listPage);
}

void LoginView::showAddPage() {
  m_editingServerId.clear();
  m_protocolInput->setCurrentIndex(0);
  m_serverAddressInput->clear();
  if (m_portInput) {
    m_portInput->clear();
  }
  m_usernameInput->clear();
  m_passwordInput->clear();
  if (m_ignoreSslSwitch) {
    m_ignoreSslSwitch->setChecked(false);
  }
  updateSslOptionsVisibility();

  m_passwordInput->setEchoMode(QLineEdit::Password);

  
  if (m_togglePwdAction) {
    m_togglePwdAction->setChecked(false);
    m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye.svg")));
  }

  m_loginButton->setText(tr("Login"));
  m_errorLabel->hide();
  m_pageSwitcher->setCurrentWidget(m_addPage);

  
  
  m_serverAddressInput->setFocus();
}

void LoginView::showListPage() {
  if (m_core->serverManager()->servers().isEmpty())
    return;
  refreshServerList();
}

void LoginView::onEditServerClicked(const QString &serverId) {
  QList<ServerProfile> servers = m_core->serverManager()->servers();
  for (const auto &server : servers) {
    if (server.id == serverId) {
      m_editingServerId = serverId;

      QUrl parsedUrl(server.url);
      if (parsedUrl.isValid() && !parsedUrl.scheme().isEmpty()) {
        applyServerUrlToForm(parsedUrl, server.ignoreSslVerification);
      } else {
        m_protocolInput->setCurrentIndex(
            server.url.startsWith("https://", Qt::CaseInsensitive) ? 1 : 0);
        m_serverAddressInput->setText(server.url);
        if (m_portInput) {
          m_portInput->clear();
        }
        if (m_ignoreSslSwitch) {
          m_ignoreSslSwitch->setChecked(server.ignoreSslVerification);
        }
        updateSslOptionsVisibility();
      }

      m_usernameInput->setText(server.userName);
      m_passwordInput->clear();

      m_passwordInput->setEchoMode(QLineEdit::Password);
      if (m_togglePwdAction) {
        m_togglePwdAction->setChecked(false);
        m_togglePwdAction->setIcon(QIcon(getThemeSvgPath("eye.svg")));
      }

      m_loginButton->setText(tr("Save & Login"));
      m_errorLabel->hide();
      m_pageSwitcher->setCurrentWidget(m_addPage);

      
      m_passwordInput->setFocus();
      break;
    }
  }
}

void LoginView::onRemoveServerClicked(const QString &serverId) {
  
  
  bool confirm =
      ModernMessageBox::question(this, tr("Remove Server"),
                                 tr("Are you sure you want to remove this "
                                    "server?\nThis action cannot be undone."),
                                 tr("Remove"), 
                                 tr("Cancel"), 
                                 ModernMessageBox::Danger);
  if (confirm) {
    m_core->serverManager()->removeServer(serverId);
    refreshServerList();
  }
}


QCoro::Task<void> LoginView::onServerCardClicked(const QString &serverId) {
  
  
  
  QString safeServerId = serverId;

  
  QPointer<LoginView> guard(this);

  
  m_loadingOverlay->start();

  try {
    co_await m_core->authService()->validateSession(safeServerId);
    if (!guard)
      co_return;

    m_loadingOverlay->stop();
    m_loadingOverlay->hide();

    ConfigStore::instance()->set(ConfigKeys::LastSelectedServerId, safeServerId);

    
    
    m_wheelView->setTransitionMode(true);
    Q_EMIT loginCompleted();
  } catch (const std::exception &e) {
    if (!guard)
      co_return;

    m_loadingOverlay->stop();
    refreshServerList();
    ModernMessageBox::warning(this, tr("Connection Failed"),
                              tr("Failed to connect or session expired. Please "
                                 "log in again.\nError: ") +
                                  QString::fromStdString(e.what()));
    onEditServerClicked(safeServerId);
  }
}


QCoro::Task<void> LoginView::onLoginClicked() {
  
  QPointer<LoginView> guard(this);

  QString user = m_usernameInput->text().trimmed();
  QString pass = m_passwordInput->text();

  if (m_serverAddressInput->text().trimmed().isEmpty() || user.isEmpty()) {
    m_errorLabel->setText(tr("Server address and username cannot be empty."));
    m_errorLabel->show();
    co_return;
  }

  QString urlError;
  const QUrl normalizedUrl = buildNormalizedServerUrl(&urlError);
  if (!urlError.isEmpty()) {
    m_errorLabel->setText(urlError);
    m_errorLabel->show();
    co_return;
  }

  const bool ignoreSslVerification =
      normalizedUrl.scheme().compare("https", Qt::CaseInsensitive) == 0 &&
      m_ignoreSslSwitch != nullptr && m_ignoreSslSwitch->isChecked();
  applyServerUrlToForm(normalizedUrl, ignoreSslVerification);

  const QString fullUrl = normalizedUrl.toString(QUrl::FullyEncoded);
  qDebug() << "[LoginView] Attempting login"
           << "| url:" << fullUrl
           << "| ignoreSslVerification:" << ignoreSslVerification;

  m_errorLabel->hide();
  m_loginButton->setEnabled(false);
  m_loginButton->setText(tr("Connecting..."));

  m_loadingOverlay->start();

  try {
    ServerProfile profile =
        co_await m_core->authService()->login(fullUrl, user, pass,
                                              ignoreSslVerification);

    if (!guard)
      co_return;

    m_loadingOverlay->stop();
    m_loadingOverlay->hide(); 

    ConfigStore::instance()->set(ConfigKeys::LastSelectedServerId, profile.id);

    if (!m_editingServerId.isEmpty()) {
      m_core->serverManager()->removeServer(m_editingServerId);
      m_editingServerId.clear();
    }

    m_loginButton->setEnabled(true);
    m_loginButton->setText(tr("Login"));

    
    m_wheelView->setTransitionMode(true);
    Q_EMIT loginCompleted();

  } catch (const std::exception &e) {
    if (!guard)
      co_return;

    m_loadingOverlay->stop();
    m_loginButton->setEnabled(true);
    m_loginButton->setText(m_editingServerId.isEmpty() ? tr("Login")
                                                       : tr("Save & Login"));
    m_errorLabel->setText(QString::fromStdString(e.what()));
    m_errorLabel->show();
  }
}


