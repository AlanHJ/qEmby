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

  
  auto wrapInScrollArea = [this](QWidget *page) -> QScrollArea * {
    
    page->setAttribute(Qt::WA_StyledBackground, true);

    auto *scroll = new QScrollArea();
    scroll->setObjectName("SettingsScrollArea");

    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    
    scroll->viewport()->setAutoFillBackground(false);

    
    m_scrollAreas.append(scroll);
    scroll->viewport()->installEventFilter(this);

    return scroll;
  };

  
  m_stack->addWidget(wrapInScrollArea(new PageGeneral(m_core, m_stack)));
  m_stack->addWidget(wrapInScrollArea(new PageAppearance(m_core, m_stack)));
  m_stack->addWidget(wrapInScrollArea(new PageLibrary(m_core, m_stack)));
  m_stack->addWidget(wrapInScrollArea(new PagePlayer(m_core, m_stack)));
  m_stack->addWidget(wrapInScrollArea(new PageAbout(m_core, m_stack)));

  mainLayout->addWidget(m_leftPanel);
  mainLayout->addWidget(m_stack, 1);

  m_navMenu->setCurrentRow(0);

  
  m_scrollTargets.resize(m_scrollAreas.size(), 0);
  for (auto *sa : m_scrollAreas) {
    auto *anim = new QPropertyAnimation(sa->verticalScrollBar(), "value", this);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setDuration(450);
    m_scrollAnims.append(anim);
  }

  
  qApp->postEvent(this, new QEvent(QEvent::StyleChange));
}

void SettingsView::setupConnections() {
  
  connect(m_navMenu, &QListWidget::currentRowChanged, this, [this](int row) {
    if (row >= 0) {
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
      if (obj == m_scrollAreas[i]->viewport()) {
        auto *we   = static_cast<QWheelEvent *>(event);
        auto *vBar = m_scrollAreas[i]->verticalScrollBar();
        auto *anim = m_scrollAnims[i];

        if (vBar) {
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
