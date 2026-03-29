#include "moderncombobox.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOption>
#include <QStyleOptionComboBox>
#include <QStyleOptionViewItem>
#include <QStylePainter>

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
  polishPopupView();
  QComboBox::showPopup();

  auto *popupView = view();
  auto *vScrollBar = popupView ? popupView->verticalScrollBar() : nullptr;
  QWidget *popupContainer = popupView ? popupView->window() : nullptr;
  if (!popupView || !vScrollBar || !popupContainer || !vScrollBar->isVisible()) {
    return;
  }

  
  
  const int targetWidth = qMax(popupContainer->width(), width() + vScrollBar->sizeHint().width());
  if (targetWidth > popupContainer->width()) {
    popupContainer->resize(targetWidth, popupContainer->height());
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
