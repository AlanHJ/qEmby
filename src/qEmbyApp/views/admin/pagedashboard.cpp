#include "pagedashboard.h"
#include "../../components/horizontalwidgetgallery.h"
#include "../../components/modernmessagebox.h"
#include "../../components/moderntoast.h"
#include "activitylogdelegate.h"
#include "nowplayingcard.h"
#include <models/admin/adminmodels.h>
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>
#include "../../utils/layoututils.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QScroller>
#include <QScrollerProperties>
#include <QSet>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariantAnimation>

PageDashboard::PageDashboard(QEmbyCore *core, QWidget *parent)
    : ManagePageBase(core, tr("Dashboard"), parent) {
  
  
  
  auto *infoCard = new QFrame(this);
  infoCard->setObjectName("ManageInfoCard");

  auto *infoLayout = new QVBoxLayout(infoCard);
  infoLayout->setSpacing(10);

  
  auto *titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, 0, 0);

  auto *infoTitle = new QLabel(tr("Server Information"), infoCard);
  infoTitle->setObjectName("ManageCardTitle");
  titleRow->addWidget(infoTitle);
  titleRow->addStretch(1);

  
  m_btnRefresh = new QPushButton(infoCard);
  m_btnRefresh->setObjectName("ManageRefreshBtn");
  m_btnRefresh->setCursor(Qt::PointingHandCursor);
  m_btnRefresh->setFixedSize(32, 32);
  m_btnRefresh->setToolTip(tr("Refresh"));
  titleRow->addWidget(m_btnRefresh);

  titleRow->addSpacing(8);

  
  m_btnRestart = new QPushButton(infoCard);
  m_btnRestart->setObjectName("ManageRestartBtn");
  m_btnRestart->setCursor(Qt::PointingHandCursor);
  m_btnRestart->setFixedSize(32, 32);
  m_btnRestart->setToolTip(tr("Restart Server"));
  titleRow->addWidget(m_btnRestart);

  titleRow->addSpacing(4);

  
  m_btnShutdown = new QPushButton(infoCard);
  m_btnShutdown->setObjectName("ManageShutdownBtn");
  m_btnShutdown->setCursor(Qt::PointingHandCursor);
  m_btnShutdown->setFixedSize(32, 32);
  m_btnShutdown->setToolTip(tr("Shutdown Server"));
  titleRow->addWidget(m_btnShutdown);

  infoLayout->addLayout(titleRow);
  infoLayout->addSpacing(4);

  infoLayout->addLayout(LayoutUtils::createInfoRow(tr("Server Name"), m_serverNameValue));
  infoLayout->addLayout(LayoutUtils::createInfoRow(tr("Version"), m_versionValue));
  infoLayout->addLayout(LayoutUtils::createInfoRow(tr("Operating System"), m_osValue));
  infoLayout->addLayout(LayoutUtils::createInfoRow(tr("Address"), m_addressValue));

  m_mainLayout->addWidget(infoCard);

  
  
  
  auto *nowPlayingCard = new QFrame(this);
  nowPlayingCard->setObjectName("ManageInfoCard");

  auto *nowPlayingOuterLayout = new QVBoxLayout(nowPlayingCard);
  nowPlayingOuterLayout->setSpacing(12);

  auto *nowPlayingTitle = new QLabel(tr("Now Playing"), nowPlayingCard);
  nowPlayingTitle->setObjectName("ManageCardTitle");
  nowPlayingOuterLayout->addWidget(nowPlayingTitle);

  m_nowPlayingGallery = new HorizontalWidgetGallery(nowPlayingCard);
  m_nowPlayingGallery->setSpacing(12);
  nowPlayingOuterLayout->addWidget(m_nowPlayingGallery);

  m_nowPlayingEmptyLabel = new QLabel(tr("Nothing is playing"), nowPlayingCard);
  m_nowPlayingEmptyLabel->setObjectName("ManageEmptyLabel");
  m_nowPlayingEmptyLabel->setAlignment(Qt::AlignCenter);
  nowPlayingOuterLayout->addWidget(m_nowPlayingEmptyLabel);

  m_mainLayout->addWidget(nowPlayingCard);

  
  
  
  auto *sessionsCard = new QFrame(this);
  sessionsCard->setObjectName("ManageInfoCard");

  auto *sessionsOuterLayout = new QVBoxLayout(sessionsCard);
  sessionsOuterLayout->setSpacing(12);

  auto *sessionsTitle = new QLabel(tr("Active Sessions"), sessionsCard);
  sessionsTitle->setObjectName("ManageCardTitle");
  sessionsOuterLayout->addWidget(sessionsTitle);

  m_sessionsGrid = new QGridLayout();
  m_sessionsGrid->setSpacing(8);
  sessionsOuterLayout->addLayout(m_sessionsGrid);

  m_sessionsEmptyLabel = new QLabel(tr("No active sessions"), sessionsCard);
  m_sessionsEmptyLabel->setObjectName("ManageEmptyLabel");
  m_sessionsEmptyLabel->setAlignment(Qt::AlignCenter);
  m_sessionsGrid->addWidget(m_sessionsEmptyLabel, 0, 0, 1, 2);

  m_mainLayout->addWidget(sessionsCard);

  
  
  
  auto *activityCard = new QFrame(this);
  activityCard->setObjectName("ManageActivityCard");

  auto *activityOuterLayout = new QVBoxLayout(activityCard);
  activityOuterLayout->setSpacing(8);
  activityOuterLayout->setContentsMargins(20, 20, 0, 20);

  auto *activityTitle = new QLabel(tr("Activity Log"), activityCard);
  activityTitle->setObjectName("ManageCardTitle");
  activityOuterLayout->addWidget(activityTitle);

  m_activityModel = new QStandardItemModel(this);

  m_activityListView = new QListView(activityCard);
  m_activityListView->setObjectName("ActivityLogList");
  m_activityListView->setModel(m_activityModel);
  m_activityListView->setItemDelegate(
      new ActivityLogDelegate(m_activityListView));
  m_activityListView->setSelectionMode(QAbstractItemView::NoSelection);
  m_activityListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_activityListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  m_activityListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_activityListView->setFixedHeight(400);

  
  QScroller::grabGesture(m_activityListView->viewport(),
                         QScroller::LeftMouseButtonGesture);
  QScroller *scroller = QScroller::scroller(m_activityListView->viewport());
  QScrollerProperties props = scroller->scrollerProperties();
  props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                        QScrollerProperties::OvershootAlwaysOff);
  props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                        QScrollerProperties::OvershootAlwaysOff);
  props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
  scroller->setScrollerProperties(props);

  activityOuterLayout->addWidget(m_activityListView);

  m_activityEmptyLabel = new QLabel(tr("No activity records"), activityCard);
  m_activityEmptyLabel->setObjectName("ManageEmptyLabel");
  m_activityEmptyLabel->setAlignment(Qt::AlignCenter);
  m_activityEmptyLabel->hide();
  activityOuterLayout->addWidget(m_activityEmptyLabel);

  m_mainLayout->addWidget(activityCard);
  m_mainLayout->addStretch(1);

  
  
  
  connect(m_btnRefresh, &QPushButton::clicked, this,
          [this]() { loadData(true); });
  connect(m_btnRestart, &QPushButton::clicked, this,
          [this]() { onRestartClicked(); });
  connect(m_btnShutdown, &QPushButton::clicked, this,
          [this]() { onShutdownClicked(); });

  
  
  
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(2000);
  connect(m_refreshTimer, &QTimer::timeout, this,
          [this]() { loadData(false); });

  
  loadData();
}

void PageDashboard::showEvent(QShowEvent *event) {
  ManagePageBase::showEvent(event);
  m_refreshTimer->start();
  loadData(false); 
}

void PageDashboard::hideEvent(QHideEvent *event) {
  ManagePageBase::hideEvent(event);
  m_refreshTimer->stop();
}

QCoro::Task<void> PageDashboard::loadData(bool isManual) {
  
  if (m_loadingData && !isManual)
    co_return;
  m_loadingData = true;

  
  if (isManual) {
    m_btnRefresh->setEnabled(false);

    
    QIcon originalIcon = m_btnRefresh->icon();
    QSize iconSz = m_btnRefresh->iconSize();
    QPixmap basePx = originalIcon.pixmap(iconSz);

    auto *rotAnim = new QVariantAnimation(this);
    rotAnim->setStartValue(0.0);
    rotAnim->setEndValue(360.0);
    rotAnim->setDuration(800);
    rotAnim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(rotAnim, &QVariantAnimation::valueChanged, this,
            [this, basePx, iconSz](const QVariant &val) {
              qreal angle = val.toReal();
              QTransform t;
              t.translate(iconSz.width() / 2.0, iconSz.height() / 2.0);
              t.rotate(angle);
              t.translate(-iconSz.width() / 2.0, -iconSz.height() / 2.0);
              QPixmap rotated = basePx.transformed(t, Qt::SmoothTransformation);
              
              int xOff = (rotated.width() - iconSz.width()) / 2;
              int yOff = (rotated.height() - iconSz.height()) / 2;
              QPixmap cropped =
                  rotated.copy(xOff, yOff, iconSz.width(), iconSz.height());
              m_btnRefresh->setIcon(QIcon(cropped));
            });

    connect(rotAnim, &QVariantAnimation::finished, this,
            [this, originalIcon]() {
              m_btnRefresh->setIcon(originalIcon); 
            });

    rotAnim->start(QAbstractAnimation::DeleteWhenStopped);
  }

  try {
    
    auto sysInfo = co_await m_core->adminService()->getSystemInfo();

    m_serverNameValue->setText(sysInfo.serverName);
    m_versionValue->setText(sysInfo.version);
    m_osValue->setText(sysInfo.operatingSystemName.isEmpty()
                           ? sysInfo.operatingSystem
                           : sysInfo.operatingSystemName);

    QString address = sysInfo.localAddress;
    if (!sysInfo.wanAddress.isEmpty()) {
      address += QString("  |  WAN: %1").arg(sysInfo.wanAddress);
    }
    m_addressValue->setText(address);

    
    m_btnRestart->setEnabled(sysInfo.canSelfRestart);

    
    auto sessions = co_await m_core->adminService()->getActiveSessions();

    
    QList<SessionInfo> playingSessions;
    QList<SessionInfo> allSessions;

    for (const auto &session : sessions) {
      if (session.userName.isEmpty())
        continue;
      allSessions.append(session);
      if (session.isNowPlaying()) {
        playingSessions.append(session);
      }
    }

    
    
    
    static constexpr qint64 kExpireMs = 30000; 
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    if (playingSessions.isEmpty()) {
      m_nowPlayingGallery->hide();
      m_nowPlayingEmptyLabel->show();
      for (auto *card : m_nowPlayingCards) {
        card->hide();
        card->setParent(nullptr);
        card->deleteLater();
      }
      m_nowPlayingCards.clear();
      m_cardLastSeen.clear();
    } else {
      m_nowPlayingEmptyLabel->hide();
      m_nowPlayingGallery->show();

      
      QSet<QString> activeKeys;
      for (const auto &session : playingSessions) {
        activeKeys.insert(session.id + "|" + session.nowPlayingItemId);
      }

      
      for (auto it = m_nowPlayingCards.begin();
           it != m_nowPlayingCards.end();) {
        if (!activeKeys.contains(it.key())) {
          qint64 lastSeen = m_cardLastSeen.value(it.key(), 0);
          if (nowMs - lastSeen >= kExpireMs) {
            it.value()->hide();
            it.value()->setParent(nullptr);
            it.value()->deleteLater();
            m_cardLastSeen.remove(it.key());
            it = m_nowPlayingCards.erase(it);
          } else {
            ++it;
          }
        } else {
          ++it;
        }
      }

      
      for (const auto &session : playingSessions) {
        QString compositeKey = session.id + "|" + session.nowPlayingItemId;
        m_cardLastSeen[compositeKey] = nowMs;
        if (m_nowPlayingCards.contains(compositeKey)) {
          m_nowPlayingCards[compositeKey]->updateSession(session);
        } else {
          auto *card = new NowPlayingCard(session, m_core);
          m_nowPlayingCards.insert(compositeKey, card);
          m_nowPlayingGallery->addWidget(card);
        }
      }
      m_nowPlayingGallery->adjustHeightToContent();
    }

    
    while (m_sessionsGrid->count() > 0) {
      auto *item = m_sessionsGrid->takeAt(0);
      if (item->widget())
        item->widget()->deleteLater();
      delete item;
    }

    if (allSessions.isEmpty()) {
      m_sessionsEmptyLabel = new QLabel(tr("No active sessions"), this);
      m_sessionsEmptyLabel->setObjectName("ManageEmptyLabel");
      m_sessionsEmptyLabel->setAlignment(Qt::AlignCenter);
      m_sessionsGrid->addWidget(m_sessionsEmptyLabel, 0, 0, 1, 2);
    } else {
      
      auto formatTime = [](const QString &isoDate) -> QString {
        if (isoDate.isEmpty())
          return {};
        QDateTime dt = QDateTime::fromString(isoDate, Qt::ISODate);
        if (!dt.isValid()) {
          dt = QDateTime::fromString(isoDate, "yyyy-MM-ddTHH:mm:ss.zzzzzzzZ");
        }
        if (!dt.isValid())
          return isoDate;
        dt.setTimeSpec(Qt::UTC);
        return dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
      };

      const int columns = 3;
      for (int i = 0; i < allSessions.size(); ++i) {
        const auto &session = allSessions[i];

        auto *frame = new QFrame(this);
        frame->setObjectName("ManageSessionCard");

        auto *layout = new QVBoxLayout(frame);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(4);

        
        auto *userLabel = new QLabel(session.userName);
        userLabel->setObjectName("ManageSessionUser");
        layout->addWidget(userLabel);

        
        auto *clientLabel =
            new QLabel(QString("%1 · %2 (v%3)")
                           .arg(session.client, session.deviceName,
                                session.applicationVersion));
        clientLabel->setObjectName("ManageSessionClient");
        layout->addWidget(clientLabel);

        
        QString info;
        QString timeStr = formatTime(session.lastActivityDate);
        if (!timeStr.isEmpty())
          info += timeStr;
        if (!session.remoteEndPoint.isEmpty()) {
          if (!info.isEmpty())
            info += "  |  ";
          info += session.remoteEndPoint;
        }
        if (!info.isEmpty()) {
          auto *infoLabel = new QLabel(info);
          infoLabel->setObjectName("ManageSessionTime");
          layout->addWidget(infoLabel);
        }

        m_sessionsGrid->addWidget(frame, i / columns, i % columns);
      }
    }

    
    auto entries = co_await m_core->adminService()->getActivityLog(200);

    m_activityModel->clear();
    if (entries.isEmpty()) {
      m_activityListView->hide();
      m_activityEmptyLabel->show();
    } else {
      m_activityEmptyLabel->hide();
      m_activityListView->show();

      for (const auto &entry : entries) {
        auto *item = new QStandardItem();
        item->setData(entry.name, ActivityLogDelegate::NameRole);
        item->setData(entry.overview.isEmpty() ? entry.shortOverview
                                               : entry.overview,
                      ActivityLogDelegate::OverviewRole);
        item->setData(entry.date, ActivityLogDelegate::DateRole);
        item->setData(entry.severity, ActivityLogDelegate::SeverityRole);
        item->setData(entry.type, ActivityLogDelegate::TypeRole);
        m_activityModel->appendRow(item);
      }
    }

  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Failed to load: %1").arg(QString::fromUtf8(e.what())), 3000);
  }

  if (!m_btnRefresh->isEnabled())
    m_btnRefresh->setEnabled(true);

  m_loadingData = false;
}

QCoro::Task<void> PageDashboard::onRestartClicked() {
  bool confirmed = ModernMessageBox::question(
      this, tr("Restart Server"),
      tr("Are you sure you want to restart the server? All active sessions "
         "will be terminated."),
      tr("Restart"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);

  if (!confirmed)
    co_return;

  try {
    co_await m_core->adminService()->restartServer();
    ModernToast::showMessage(tr("Server restart command sent"), 2000);
  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Restart failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageDashboard::onShutdownClicked() {
  bool confirmed = ModernMessageBox::question(
      this, tr("Shutdown Server"),
      tr("Are you sure you want to shut down the server? This will stop all "
         "services and disconnect all clients."),
      tr("Shutdown"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);

  if (!confirmed)
    co_return;

  try {
    co_await m_core->adminService()->shutdownServer();
    ModernToast::showMessage(tr("Server shutdown command sent"), 2000);
  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Shutdown failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}
