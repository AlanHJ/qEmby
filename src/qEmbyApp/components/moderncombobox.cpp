#include "moderncombobox.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QScrollBar>
#include <QShortcut>
#include <QStyleOption>
#include <QStyleOptionComboBox>
#include <QStyleOptionViewItem>
#include <QStylePainter>
#include <QVBoxLayout>

namespace {

constexpr int kArrowWidth = 32;
constexpr auto kPopupEdgeFixMarker = "/* modern-combobox-popup-edge-fix */";
constexpr auto kPopupEdgeFixQss = R"(
/* modern-combobox-popup-edge-fix */
QAbstractItemView QScrollBar:vertical {
  margin: 0px;
}
)";

} 



void CenterAlignDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const {
  QStyleOptionViewItem opt = option;
  initStyleOption(&opt, index);
  
  opt.displayAlignment = Qt::AlignCenter;
  QStyledItemDelegate::paint(painter, opt, index);
}



ModernComboBox::ModernComboBox(QWidget *parent) : QComboBox(parent) {
  
  setItemDelegate(new CenterAlignDelegate(this));

  
  setSizeAdjustPolicy(QComboBox::AdjustToContents);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  
  connect(this, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int index) {
            if (m_maxTextWidth > 0 && index >= 0) {
              setToolTip(itemText(index));
            } else {
              setToolTip({});
            }
          });

  polishPopupView();
}

void ModernComboBox::setMaxTextWidth(int pixels) {
  m_maxTextWidth = pixels;
  updateGeometry();
}

QSize ModernComboBox::sizeHint() const {
  
  QFontMetrics fm(font());
  int maxW = 60; 
  for (int i = 0; i < count(); ++i) {
    maxW = qMax(maxW, fm.horizontalAdvance(itemText(i)));
  }
  
  if (m_maxTextWidth > 0) {
    maxW = qMin(maxW, m_maxTextWidth);
  }
  
  
  int w = maxW + 16 + 16 + 32;
  int h = QComboBox::sizeHint().height();
  return QSize(w, h);
}

QSize ModernComboBox::minimumSizeHint() const { return sizeHint(); }

void ModernComboBox::showPopup() {
  
  
  
  
  
  if (window() && window()->isFullScreen()) {
    showEmbeddedPopup();
    return;
  }

  polishPopupView();
  QComboBox::showPopup();

  auto *popupView = view();
  QWidget *popupContainer = popupView ? popupView->window() : nullptr;
  if (!popupView || !popupContainer) {
    return;
  }

  
  
  auto *vScrollBar = popupView->verticalScrollBar();
  if (vScrollBar && vScrollBar->isVisible()) {
    const int targetWidth = qMax(popupContainer->width(),
                                 width() + vScrollBar->sizeHint().width());
    if (targetWidth > popupContainer->width()) {
      popupContainer->resize(targetWidth, popupContainer->height());
    }
  }
}

void ModernComboBox::hidePopup() {
  closeEmbeddedPopup();
  QComboBox::hidePopup();
}

bool ModernComboBox::eventFilter(QObject *watched, QEvent *event) {
  if (!event) {
    return QComboBox::eventFilter(watched, event);
  }

  
  if (m_embeddedPopup && watched == m_embeddedPopup.data()) {
    
    if (event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonDblClick) {
      closeEmbeddedPopup();
      return true;
    }
  }

  return QComboBox::eventFilter(watched, event);
}



void ModernComboBox::showEmbeddedPopup() {
  closeEmbeddedPopup();

  QWidget *host = window();
  if (!host || count() == 0) {
    return;
  }

  const bool flushRightScrollBar =
      property("flush-right-scrollbar").toBool();

  
  
  
  
  auto *overlay = new QWidget(host);
  overlay->setAttribute(Qt::WA_DeleteOnClose);
  overlay->setObjectName(QStringLiteral("modernComboEmbeddedOverlay"));
  overlay->setGeometry(host->rect());
  overlay->installEventFilter(this);

  
  auto *listFrame = new QFrame(overlay);
  listFrame->setObjectName(QStringLiteral("modernComboEmbeddedFrame"));
  listFrame->setFrameStyle(QFrame::NoFrame);
  listFrame->setProperty("flush-right-scrollbar", flushRightScrollBar);

  auto *frameLayout = new QVBoxLayout(listFrame);
  frameLayout->setContentsMargins(0, 0, 0, 0);
  frameLayout->setSpacing(0);

  auto *listWidget = new QListWidget(listFrame);
  listWidget->setObjectName(QStringLiteral("modernComboEmbeddedList"));
  listWidget->setProperty("flush-right-scrollbar", flushRightScrollBar);
  listWidget->setItemDelegate(new CenterAlignDelegate(listWidget));
  listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  listWidget->setFocusPolicy(Qt::StrongFocus);
  listWidget->setFrameStyle(QFrame::NoFrame);
  listWidget->setContentsMargins(0, 0, 0, 0);

  
  for (int i = 0; i < count(); ++i) {
    auto *item = new QListWidgetItem(itemText(i));
    item->setData(Qt::UserRole, itemData(i));
    item->setTextAlignment(Qt::AlignCenter);
    listWidget->addItem(item);
  }

  
  if (currentIndex() >= 0 && currentIndex() < listWidget->count()) {
    listWidget->setCurrentRow(currentIndex());
  }

  frameLayout->addWidget(listWidget);

  
  constexpr int kMaxVisibleItems = 12;
  constexpr int kItemPadding = 12; 
  const QFontMetrics fm(listWidget->font());
  const int itemH = fm.height() + kItemPadding;
  const int visibleItems = qMin(count(), kMaxVisibleItems);
  int popupH = itemH * visibleItems + 6; 
  int popupW = qMax(width(), 240);

  
  popupH = qMin(popupH, host->height() / 2);

  
  const QPoint comboBottomLeft = mapTo(host, QPoint(0, height()));
  const QPoint comboTopLeft = mapTo(host, QPoint(0, 0));

  int popupX = comboBottomLeft.x();
  int popupY = comboBottomLeft.y() + 4; 

  
  if (popupY + popupH > host->height() - 10) {
    popupY = comboTopLeft.y() - popupH - 4;
  }

  
  if (popupX + popupW > host->width() - 10) {
    popupX = host->width() - popupW - 10;
  }

  popupX = qMax(10, popupX);
  popupY = qMax(10, popupY);

  listFrame->setGeometry(popupX, popupY, popupW, popupH);

  
  overlay->show();
  overlay->raise();
  listFrame->show();
  listWidget->setFocus();

  
  if (currentIndex() >= 0 && currentIndex() < listWidget->count()) {
    listWidget->scrollToItem(listWidget->item(currentIndex()),
                             QAbstractItemView::PositionAtCenter);
  }

  m_embeddedPopup = overlay;

  
  
  connect(listWidget, &QListWidget::itemClicked, this,
          [this, listWidget](QListWidgetItem *) {
            const int row = listWidget->currentRow();
            if (row >= 0 && row < count()) {
              setCurrentIndex(row);
            }
            closeEmbeddedPopup();
          });

  
  auto *escShortcut =
      new QShortcut(QKeySequence(Qt::Key_Escape), overlay,
                    nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
  connect(escShortcut, &QShortcut::activated, this,
          [this]() { closeEmbeddedPopup(); });

  
  auto *enterShortcut =
      new QShortcut(QKeySequence(Qt::Key_Return), listWidget,
                    nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
  connect(enterShortcut, &QShortcut::activated, this,
          [this, listWidget]() {
            const int row = listWidget->currentRow();
            if (row >= 0 && row < count()) {
              setCurrentIndex(row);
            }
            closeEmbeddedPopup();
          });
}

void ModernComboBox::closeEmbeddedPopup() {
  if (m_embeddedPopup) {
    m_embeddedPopup->removeEventFilter(this);
    m_embeddedPopup->close();
    m_embeddedPopup = nullptr;
  }
}



void ModernComboBox::polishPopupView() {
  auto *popupView = view();
  if (!popupView) {
    return;
  }

  popupView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  popupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QString popupStyleSheet = popupView->styleSheet();
  if (!popupStyleSheet.contains(QLatin1String(kPopupEdgeFixMarker))) {
    if (!popupStyleSheet.isEmpty() && !popupStyleSheet.endsWith(QLatin1Char('\n'))) {
      popupStyleSheet += QLatin1Char('\n');
    }
    popupStyleSheet += QLatin1String(kPopupEdgeFixQss);
    popupView->setStyleSheet(popupStyleSheet);
  }
}



void ModernComboBox::paintEvent(QPaintEvent *) {
  QStylePainter painter(this);

  QStyleOptionComboBox opt;
  initStyleOption(&opt);

  
  QString text = opt.currentText;
  opt.currentText = "";

  
  painter.drawComplexControl(QStyle::CC_ComboBox, opt);

  
  
  
  
  QRect textRect = rect().adjusted(0, 0, -kArrowWidth, 0);

  
  if (m_maxTextWidth > 0) {
    QFontMetrics fm(this->font());
    text = fm.elidedText(text, Qt::ElideRight, textRect.width() - 8);
  }

  
  painter.setPen(opt.palette.color(QPalette::ButtonText));
  painter.setFont(this->font());
  painter.drawText(textRect, Qt::AlignCenter | Qt::TextSingleLine, text);
}
