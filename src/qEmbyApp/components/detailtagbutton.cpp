#include "detailtagbutton.h"

#include <QSizePolicy>

DetailTagButton::DetailTagButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent) {
  setObjectName("detail-genre-tag");
  setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
  setCursor(Qt::PointingHandCursor);
  setFocusPolicy(Qt::NoFocus);
  setFlat(true);
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  setFixedHeight(kTagHeight);
  ensurePolished();
}

QSize DetailTagButton::sizeHint() const {
  QSize size = QPushButton::sizeHint();
  size.setHeight(kTagHeight);
  return size;
}

QSize DetailTagButton::minimumSizeHint() const {
  QSize size = QPushButton::minimumSizeHint();
  size.setHeight(kTagHeight);
  return size;
}
