#include "serverbrowserdlg.h"
#include "../managers/thememanager.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <qcorotask.h>
#include <qembycore.h>
#include <services/admin/adminservice.h>


class ServerBrowserTree : public QTreeWidget {
public:
  explicit ServerBrowserTree(QWidget *parent = nullptr) : QTreeWidget(parent) {}

protected:
  void drawRow(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
    
    QTreeWidget::drawRow(painter, option, index);

    
    QRect rowRect = visualRect(index);
    rowRect.setLeft(0);
    rowRect.setRight(viewport()->width());
    rowRect.adjust(0, 1, -0, -1);

    bool isDark = ThemeManager::instance()->isDarkMode();
    bool isSelected = selectionModel()->isSelected(index);
    bool isHovered = (option.state & QStyle::State_MouseOver);

    
    
    QColor bg;
    if (isSelected) {
      bg = isDark ? QColor(59, 130, 246, 51)  
                  : QColor(59, 130, 246, 46); 
    } else if (isHovered) {
      bg = isDark ? QColor(255, 255, 255, 13) 
                  : QColor(0, 0, 0, 18);      
    }

    if (bg.isValid()) {
      painter->save();
      painter->setRenderHint(QPainter::Antialiasing);
      painter->setBrush(bg);
      painter->setPen(Qt::NoPen);
      painter->drawRoundedRect(rowRect, 6, 6);
      painter->restore();
    }
  }
};

static const int PATH_ROLE = Qt::UserRole + 101;
static const int LOADED_ROLE = Qt::UserRole + 102;

ServerBrowserDialog::ServerBrowserDialog(QEmbyCore *core, QWidget *parent)
    : ModernDialogBase(parent), m_core(core) {
  setTitle(tr("Browse Server Directory"));
  setMinimumSize(420, 400);

  auto *layout = contentLayout();

  m_tree = new ServerBrowserTree(this);
  m_tree->setObjectName("ServerBrowserTree");
  m_tree->setHeaderHidden(true);
  m_tree->setAnimated(true);
  m_tree->setIndentation(20);
  m_tree->setIconSize(QSize(18, 18));
  m_tree->setMouseTracking(true); 
  layout->addWidget(m_tree, 1);

  
  auto *btnRow = new QHBoxLayout();
  btnRow->addStretch(1);

  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  m_cancelBtn->setObjectName("SettingsCardButton");
  m_cancelBtn->setCursor(Qt::PointingHandCursor);

  m_okBtn = new QPushButton(tr("Select"), this);
  m_okBtn->setObjectName("SettingsCardButton");
  m_okBtn->setCursor(Qt::PointingHandCursor);
  m_okBtn->setEnabled(false);

  btnRow->addWidget(m_cancelBtn);
  btnRow->addSpacing(8);
  btnRow->addWidget(m_okBtn);
  layout->addSpacing(8);
  layout->addLayout(btnRow);

  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_okBtn, &QPushButton::clicked, this, [this]() {
    auto *item = m_tree->currentItem();
    if (item) {
      m_selectedPath = item->data(0, PATH_ROLE).toString();
    }
    accept();
  });

  connect(m_tree, &QTreeWidget::currentItemChanged, this,
          [this](QTreeWidgetItem *current) {
            m_okBtn->setEnabled(current != nullptr);
          });

  connect(m_tree, &QTreeWidget::itemExpanded, this,
          &ServerBrowserDialog::onItemExpanded);

  QTimer::singleShot(0, this, [this]() { m_currentTask = loadDrives(); });
}

QString ServerBrowserDialog::selectedPath() const { return m_selectedPath; }

QString ServerBrowserDialog::iconPrefix() const {
  bool isDark = ThemeManager::instance()->isDarkMode();
  return isDark ? ":/svg/dark/" : ":/svg/light/";
}

QCoro::Task<void> ServerBrowserDialog::loadDrives() {
  try {
    auto drives = co_await m_core->adminService()->getServerDrives();
    QString prefix = iconPrefix();
    for (const auto &drive : drives) {
      auto *item = new QTreeWidgetItem();
      item->setText(0, drive.name.isEmpty()
                           ? drive.path
                           : QString("%1 (%2)").arg(drive.name, drive.path));
      item->setData(0, PATH_ROLE, drive.path);
      item->setData(0, LOADED_ROLE, false);
      item->setIcon(0, QIcon(prefix + "hard-drive.svg"));
      item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
      m_tree->addTopLevelItem(item);
    }
  } catch (...) {
    auto *errItem = new QTreeWidgetItem();
    errItem->setText(0, tr("Failed to load drives"));
    m_tree->addTopLevelItem(errItem);
  }
}

void ServerBrowserDialog::onItemExpanded(QTreeWidgetItem *item) {
  if (!item->data(0, LOADED_ROLE).toBool()) {
    item->setData(0, LOADED_ROLE, true);
    QString path = item->data(0, PATH_ROLE).toString();
    m_currentTask = loadChildren(item, path);
  }
}

QCoro::Task<void> ServerBrowserDialog::loadChildren(QTreeWidgetItem *parentItem,
                                                    const QString &path) {
  try {
    auto dirs = co_await m_core->adminService()->getDirectoryContents(path);
    QString prefix = iconPrefix();
    for (const auto &dirPath : dirs) {
      auto *child = new QTreeWidgetItem();
      QString displayName = dirPath;
      int lastSep = dirPath.lastIndexOf('/');
      if (lastSep < 0)
        lastSep = dirPath.lastIndexOf('\\');
      if (lastSep >= 0 && lastSep < dirPath.length() - 1) {
        displayName = dirPath.mid(lastSep + 1);
      }
      child->setText(0, displayName);
      child->setData(0, PATH_ROLE, dirPath);
      child->setData(0, LOADED_ROLE, false);
      child->setIcon(0, QIcon(prefix + "folder.svg"));
      child->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
      parentItem->addChild(child);
    }
    if (dirs.isEmpty()) {
      parentItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    }
  } catch (...) {
    auto *errItem = new QTreeWidgetItem();
    errItem->setText(0, tr("Access denied or error"));
    parentItem->addChild(errItem);
  }
}
