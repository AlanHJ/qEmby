#include "settingsview.h"
#include "../../components/slidingstackedwidget.h"
#include "../../managers/thememanager.h" 
#include "pageabout.h"
#include "pageappearance.h"
#include "pagegeneral.h"
#include "pageplayer.h"
#include "pagelibrary.h"
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>


SettingsView::SettingsView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent) {
  
  setAttribute(Qt::WA_StyledBackground, true);
  setObjectName("SettingsRootView");

  setupUi();
  setupConnections();

  
  
  m_navMenu->setCurrentRow(0);
}

void SettingsView::setupUi() {
  auto *mainLayout = new QHBoxLayout(this);
  
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  
  m_leftPanel = new QWidget(this);
  m_leftPanel->setFixedWidth(260);
  m_leftPanel->setObjectName("SettingsLeftPanel");
  auto *leftLayout = new QVBoxLayout(m_leftPanel);
  
  leftLayout->setContentsMargins(32, 32, 32, 32);
  leftLayout->setSpacing(24);

  
  m_titleLabel = new QLabel(tr("Settings"), m_leftPanel);
  m_titleLabel->setObjectName("SettingsMainTitle");

  
  m_navMenu = new QListWidget(m_leftPanel);
  m_navMenu->setObjectName("SettingsNavMenu");
  m_navMenu->setFocusPolicy(Qt::NoFocus);
  
  m_navMenu->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_navMenu->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  
  auto *itemGeneral = new QListWidgetItem(
      ThemeManager::getAdaptiveIcon(":/svg/dark/general.svg"), tr(" General"));
  itemGeneral->setData(Qt::UserRole, ":/svg/dark/general.svg");

  auto *itemAppearance = new QListWidgetItem(
      ThemeManager::getAdaptiveIcon(":/svg/dark/appearance.svg"),
      tr(" Appearance"));
  itemAppearance->setData(Qt::UserRole, ":/svg/dark/appearance.svg");

  auto *itemPlayer = new QListWidgetItem(
      ThemeManager::getAdaptiveIcon(":/svg/dark/player.svg"), tr(" Player"));
  itemPlayer->setData(Qt::UserRole, ":/svg/dark/player.svg");

  auto *itemLibrary = new QListWidgetItem(
      ThemeManager::getAdaptiveIcon(":/svg/dark/library.svg"), tr(" Library"));
  itemLibrary->setData(Qt::UserRole, ":/svg/dark/library.svg");

  auto *itemAbout = new QListWidgetItem(
      ThemeManager::getAdaptiveIcon(":/svg/dark/about.svg"), tr(" About"));
  itemAbout->setData(Qt::UserRole, ":/svg/dark/about.svg");

  
  itemGeneral->setSizeHint(QSize(220, 44));
  itemAppearance->setSizeHint(QSize(220, 44));
  itemPlayer->setSizeHint(QSize(220, 44));
  itemLibrary->setSizeHint(QSize(220, 44));
  itemAbout->setSizeHint(QSize(220, 44));

  m_navMenu->addItem(itemGeneral);
  m_navMenu->addItem(itemAppearance);
  m_navMenu->addItem(itemLibrary);
  m_navMenu->addItem(itemPlayer);
  m_navMenu->addItem(itemAbout);

  leftLayout->addWidget(m_titleLabel);
  leftLayout->addWidget(m_navMenu);

  
  m_stack = new SlidingStackedWidget(this);
  m_stack->setObjectName("SettingsStack");

  
  
  
  
  const int kPageCount = 5;
  m_scrollAreas.reserve(kPageCount);
  m_scrollAnims.reserve(kPageCount);
  m_scrollTargets.reserve(kPageCount);
  m_pages.reserve(kPageCount);
  for (int i = 0; i < kPageCount; ++i) {
    auto *placeholder = new QWidget(m_stack);
    placeholder->setAttribute(Qt::WA_StyledBackground, true);
    placeholder->setObjectName("SettingsPagePlaceholder");
    m_stack->addWidget(placeholder);

    m_scrollAreas.append(nullptr);
    m_scrollAnims.append(nullptr);
    m_scrollTargets.append(0);
    m_pages.append(QPointer<QWidget>());
  }

  mainLayout->addWidget(m_leftPanel);
  mainLayout->addWidget(m_stack, 1);

  
  qApp->postEvent(this, new QEvent(QEvent::StyleChange));
}

QScrollArea *SettingsView::wrapInScrollArea(QWidget *page, int row) {
  
  page->setAttribute(Qt::WA_StyledBackground, true);

  auto *scroll = new QScrollArea(m_stack);
  scroll->setObjectName("SettingsScrollArea");
  scroll->setWidget(page);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  
  scroll->viewport()->setAutoFillBackground(false);

  
  scroll->viewport()->installEventFilter(this);

  
  if (row >= 0 && row < m_scrollAreas.size()) {
    m_scrollAreas[row] = scroll;

    auto *anim =
        new QPropertyAnimation(scroll->verticalScrollBar(), "value", this);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setDuration(450);
    m_scrollAnims[row] = anim;
    m_scrollTargets[row] = 0;
  }

  return scroll;
}

void SettingsView::ensurePageAt(int row) {
  if (row < 0 || row >= m_pages.size()) {
    return;
  }
  if (m_pages[row]) {
    
    return;
  }

  qDebug() << "[SettingsView] Lazy instantiate page row=" << row;

  QWidget *page = nullptr;
  switch (row) {
  case 0:
    page = new PageGeneral(m_core, m_stack);
    break;
  case 1:
    page = new PageAppearance(m_core, m_stack);
    break;
  case 2:
    page = new PageLibrary(m_core, m_stack);
    break;
  case 3:
    page = new PagePlayer(m_core, m_stack);
    break;
  case 4:
    page = new PageAbout(m_core, m_stack);
    break;
  default:
    return;
  }

  QScrollArea *scroll = wrapInScrollArea(page, row);

  
  
  QWidget *placeholder = m_stack->widget(row);
  const bool wasBlocked = m_stack->blockSignals(true);
  m_stack->insertWidget(row, scroll); 
  m_stack->removeWidget(placeholder);
  m_stack->blockSignals(wasBlocked);
  placeholder->deleteLater();

  m_pages[row] = scroll;
}

void SettingsView::setupConnections() {
  
  connect(m_navMenu, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0) {
      ensurePageAt(row);
      m_stack->setCurrentIndex(row);
    }
  });

  
  connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
          &SettingsView::onThemeChanged);
}


void SettingsView::onThemeChanged() {
  for (int i = 0; i < m_navMenu->count(); ++i) {
    auto *item = m_navMenu->item(i);
    
    QString svgPath = item->data(Qt::UserRole).toString();
    if (!svgPath.isEmpty()) {
      
      item->setIcon(ThemeManager::getAdaptiveIcon(svgPath));
    }
  }
}

bool SettingsView::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::Wheel) {
    
    for (int i = 0; i < m_scrollAreas.size(); ++i) {
      
      if (!m_scrollAreas[i]) {
        continue;
      }
      if (obj == m_scrollAreas[i]->viewport()) {
        auto *we   = static_cast<QWheelEvent *>(event);
        auto *vBar = m_scrollAreas[i]->verticalScrollBar();
        auto *anim = m_scrollAnims[i];

        if (vBar && anim) {
          int currentVal = vBar->value();

          
          if (anim->state() == QAbstractAnimation::Running) {
            currentVal = m_scrollTargets[i];
          }

          
          int step      = we->angleDelta().y();
          int newTarget = currentVal - step;

          
          newTarget = qBound(vBar->minimum(), newTarget, vBar->maximum());

          if (newTarget != vBar->value()) {
            m_scrollTargets[i] = newTarget;
            anim->stop();
            anim->setStartValue(vBar->value());
            anim->setEndValue(newTarget);
            anim->start();
          }
        }
        return true; 
      }
    }
  }

  return QWidget::eventFilter(obj, event);
}
